import logging
import re
from typing import Optional

import yaml

from .HAKCBase import HAKCDivisionEnum, HAKCDBColumn, HAKCDBRelation, HAKCDBNode, HAKCPrintableObj, HashedHAKCDBNode, \
    HAKCPayload

logger = logging.getLogger('hakc-dag')


class HAKCCompilationUnit(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCCompilationUnit"

    def __init__(self, filename: Optional[str] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCDBNode.__init__(self, **kwargs)
        self.filename = filename

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCCompilationUnit):
            return self.filename == other.filename
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        return [self.filename]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('filename', "STRING")

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return []

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompilationUnit.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        return {
            HAKCCompilationUnit.get_primary_key(): self.filename
        }


class HAKCDivision(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = u'!HAKCDivision'
    InCompartmentTable = 'InCompartment'

    def __init__(self, DivisionID: Optional[int] = None, compartment_id: Optional[int] = None,
                 division_count: int = len(HAKCDivisionEnum) - 1, AccessToken: Optional[int] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.division_id = DivisionID
        self.compartment_id = compartment_id
        self.division_count = division_count
        self.access_token = AccessToken if AccessToken is not None else self.compute_access_token([])

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCDivision):
            return self.compartment_id == other.compartment_id and self.division_id == other.division_id
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCDivision):
            return self.compartment_id < other.compartment_id and self.division_id < other.division_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    def get_hash_inputs(self) -> list[object]:
        return [self.division_id, self.compartment_id]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('division_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DivisionID', 'UINT64'),
                HAKCDBColumn('AccessToken', 'UINT64')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCDivision.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCDivision.InCompartmentTable, HAKCDivision, HAKCCompartment)
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCDivision.get_db_table_columns()
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.division_id,
            schema[2]: self.access_token
        }

    def compute_access_token(self, allowable_accesses: list['HAKCDivision']) -> int:
        if self.division_id != HAKCDivisionEnum.NO_DIVISION.value:
            access_token = (self.compartment_id << self.division_count) | (1 << self.division_id)
            for division in allowable_accesses:
                if division.compartment_id != self.compartment_id:
                    raise RuntimeError(f'Trying to add access to Compartment {division.compartment_id} to {self}')
                access_token |= (1 << division.division_id)
        else:
            access_token = 0xFFFF
        return access_token


class HAKCCompartment(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = u'!HAKCCompartment'

    def __init__(self, CompartmentID: Optional[int] = None, division_count: int = len(HAKCDivisionEnum) - 1,
                 Divisions: Optional[set[HAKCDivision]] = None,
                 EntryToken: Optional[int] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        kwargs["Name"] = kwargs.get("Name", str(CompartmentID))
        HAKCDBNode.__init__(self, **kwargs)
        self.compartment_id = CompartmentID
        self.division_count = division_count
        self.divisions = Divisions if Divisions is not None else set()
        self.entry_token = EntryToken if EntryToken is not None else self.compute_entry_token()

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id == other.compartment_id
        return False

    def __hash__(self):
        return self.compartment_id

    def add_division(self, division: HAKCDivision):
        self.divisions.add(division)
        self.entry_token = self.compute_entry_token()

    def __lt__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id < other.compartment_id
        raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')

    @staticmethod
    def compute_access_token(division_id: int, compartment_id: int, division_count: int) -> int:
        if division_id != HAKCDivisionEnum.NO_DIVISION.value:
            access_token = (compartment_id << division_count) | (1 << division_id)
        else:
            access_token = 0xFFFF
        return access_token

    def compute_entry_token(self) -> int:
        token = self.compartment_id << self.division_count
        for division in self.divisions:
            token |= (1 << division.division_id)
        return token

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('CompartmentID', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('EntryToken', 'UINT64')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompartment.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return []

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCCompartment.get_db_table_columns()
        return {
            schema[0]: self.compartment_id,
            schema[1]: self.entry_token
        }


class HAKCType(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, DebugType: Optional[str] = None, LLVMType: Optional[str] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            kwargs['Name'] = DebugType if DebugType is not None and DebugType != HAKCType.unknown_type else LLVMType
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.debug_type = DebugType
        self.llvm_type = LLVMType
        self._debug_type_transformed = HAKCType.transform_type_str(self.debug_type)
        self._debug_type_is_known = self.debug_type != HAKCType.unknown_type
        self._llvm_type_is_known = self.llvm_type != HAKCType.unknown_type

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCType):
            if self._debug_type_is_known and other._debug_type_is_known:
                return self._debug_type_transformed == other._debug_type_transformed
            elif self._llvm_type_is_known and other._llvm_type_is_known:
                return self.llvm_type == other.llvm_type
            else:
                return False
        return False

    def get_hash_inputs(self) -> list[object]:
        if self._debug_type_is_known:
            return [self._debug_type_transformed]
        else:
            return [self.llvm_type]

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    @staticmethod
    def transform_type_str(type_str: str) -> str:
        transforms = {
            "struct anon.[0-9]*": "struct anon.#",
        }

        result = type_str
        for regex, sub in transforms.items():
            result = re.sub(regex, sub, result)

        return result

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('type_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DebugType', "STRING"), HAKCDBColumn('LLVMType', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCType.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCType.get_db_table_columns()
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.debug_type,
            schema[2]: self.llvm_type
        }


class HAKCScope(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCScope"
    global_scope = "global"
    local_scope = "local"

    def __init__(self, Scope: Optional[str] = None, LocalScopeName: Optional[str] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.scope = Scope
        kwargs["Name"] = LocalScopeName if LocalScopeName is not None else self.scope
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.local_scope_name = LocalScopeName if LocalScopeName is not None else HAKCScope.global_scope
        self.is_global_scope = self.scope == HAKCScope.global_scope
        self.is_local_scope = self.scope == HAKCScope.local_scope

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCScope):
            if self.is_local_scope:
                return other.is_local_scope and self.local_scope_name == other.local_scope_name
            else:
                return other.is_global_scope
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCScope):
            if self.is_local_scope and other.is_local_scope:
                return self.local_scope_name < other.local_scope_name
            else:
                return self.scope < other.scope
        elif isinstance(other, HAKCType):
            return str(self) < str(other)
        # I guess at some point a HAKCType is being compared to HAKCScope
        logger.error(f"{other} is of type: {type(other)}")
        raise RuntimeError(f'{other} is not a {self.__class__.__name__}')

    def get_hash_inputs(self) -> list[object]:
        if self.is_global_scope:
            return [self.scope]
        else:
            return [self.scope, self.local_scope_name]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('scope_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('Scope', "STRING"), HAKCDBColumn('LocalScopeName', "STRING")]

    @staticmethod
    def get_table_name() -> str:
        return HAKCScope.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCScope.get_db_table_columns()
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.scope,
            schema[2]: self.local_scope_name
        }


class HAKCSymbol(HashedHAKCDBNode, yaml.YAMLObject):
    IsTypeTable = "IsType"
    HasScopeTable = "HasScope"
    UsesSymbolTable = "UsesSymbol"
    SymbolCompilationUnitTable = "UsedInCompilationUnit"
    DagEdgeTable = "DagEdge"
    InDivisionTable = "InDivision"
    DefinedInTable = "DefinedIn"

    def __init__(self, Name: Optional[str] = None, Type: Optional[HAKCType] = None, Scope: Optional[HAKCScope] = None,
                 DefiningFile: Optional[str] = None, DefiningLine: Optional[str] = None,
                 UsedSymbols: Optional[list] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)

        if UsedSymbols is None:
            UsedSymbols = list()
        self.name = Name
        self.type = Type
        self.scope = Scope
        self.defining_file = DefiningFile
        self.defining_line = DefiningLine
        self.used_symbols = UsedSymbols

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return hash(self) == hash(other)
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        return [self.name, self.type, self.scope]

    @property
    def is_definition(self):
        return self.defining_file is not None

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('symbol_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DefiningFile', 'STRING'),
                HAKCDBColumn('DefiningLine', 'INT32'), HAKCDBColumn('is_function', 'BOOL'),
                HAKCDBColumn('Name', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return "HAKCSymbol"

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCSymbol.IsTypeTable, HAKCSymbol, HAKCType),
            HAKCDBRelation(HAKCSymbol.HasScopeTable, HAKCSymbol, HAKCScope),
            HAKCDBRelation(HAKCSymbol.UsesSymbolTable, HAKCSymbol, HAKCSymbol),
            HAKCDBRelation(HAKCSymbol.SymbolCompilationUnitTable, HAKCSymbol, HAKCCompilationUnit),
            HAKCDBRelation(HAKCSymbol.DagEdgeTable, HAKCSymbol, HAKCSymbol, weight="INT32"),
            HAKCDBRelation(HAKCSymbol.InDivisionTable, HAKCSymbol, HAKCDivision),
            HAKCDBRelation(HAKCSymbol.DefinedInTable, HAKCSymbol, HAKCCompilationUnit, line="INT64")
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCSymbol.get_db_table_columns()
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.defining_file,
            schema[2]: self.defining_line,
            schema[3]: isinstance(self, HAKCFunction),
            schema[4]: self.name
        }


class HAKCIndirectSourceLink(HAKCPrintableObj, yaml.YAMLObject):
    yaml_tag = "!HAKCIndirectSourceLink"

    def __init__(self, LinkType: Optional[str], Type: Optional[str] = None, GlobalName: Optional[str] = None,
                 Offset: Optional[int] = None, ArgNumber: Optional[int] = None, FunctionName: Optional[str] = None,
                 **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.link_type = LinkType
        self.source_type = Type
        self.global_name = GlobalName
        self.offset = Offset
        self.arg_number = ArgNumber
        self.function_name = FunctionName

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_hash_inputs(self) -> list[object]:
        result = [self.link_type]
        if self.source_type:
            result.append(self.source_type)
        if self.global_name:
            result.append(self.global_name)
        if self.offset:
            result.append(self.offset)
        if self.arg_number:
            result.append(self.arg_number)
        if self.function_name:
            result.append(self.function_name)
        return result


class HAKCIndirectCallSource(HAKCPrintableObj, yaml.YAMLObject):
    yaml_tag = "!HAKCIndirectSource"

    def __init__(self, Type: Optional[HAKCType] = None, Source: Optional[list] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.source = Source
        self.type = Type

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_hash_inputs(self) -> list[object]:
        result = [self.type]
        for link in self.source:
            result.append(link)
        return result


class HAKCFunction(HAKCSymbol):
    yaml_tag = "!HAKCFunction"
    IndirectCallTable = "IndirectCall"
    DirectCallTable = "DirectCall"

    def __init__(self, DirectCalls: Optional[list] = None, IndirectCalls: Optional[list] = None, **kwargs):
        HAKCSymbol.__init__(self, **kwargs)
        self.direct_calls = DirectCalls if DirectCalls is not None else list()
        self.indirect_calls = IndirectCalls if IndirectCalls is not None else list()

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCFunction.IndirectCallTable, HAKCFunction, HAKCType),
            HAKCDBRelation(HAKCFunction.DirectCallTable, HAKCFunction, HAKCSymbol)
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        return HAKCSymbol.get_db_data(self, convert_hash)


class HAKCGlobalVariable(HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        HAKCSymbol.__init__(self, **kwargs)

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        return HAKCSymbol.get_db_data(self, convert_hash)


class HAKCAdjustment(yaml.YAMLObject):
    yaml_tag = "!HAKCAdjustment"

    def __init__(self, path: str, division_id: int, compartment_id: int):
        yaml.YAMLObject.__init__(self)
        self.path = path
        self.division = HAKCDivision(division_id, compartment_id)

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))


class HAKCCompartmentalizationAdjustment(yaml.YAMLObject):
    yaml_tag = "!HAKCAdjustments"
    compartmentalize_entry = 'compartmentalize'
    add_kernel_compartment_entry = 'add-kernel-division'

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.adjustment_regexes = dict()
        self.add_kernel_division = kwargs.get(HAKCCompartmentalizationAdjustment.add_kernel_compartment_entry, False)
        for adjustment in sorted(kwargs.get(HAKCCompartmentalizationAdjustment.compartmentalize_entry, set()),
                                 key=lambda e: e.path):
            escaped_path = re.escape(adjustment.path)
            self.adjustment_regexes[re.compile(escaped_path)] = adjustment

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_adjusted_compartment(self, defining_path: str) -> HAKCDivision | None:
        if defining_path is None:
            return None

        adjusted_division = None
        for adjustment_regex, adjustment in self.adjustment_regexes.items():
            match = adjustment_regex.search(defining_path)
            if match:
                adjusted_division = adjustment.division

        return adjusted_division


class HAKCDivisionCompartmentPayload(HAKCPayload):
    def __init__(self, division: HAKCDivision, compartment: HAKCCompartment, **kwargs):
        HAKCPayload.__init__(self, {'Division': division, 'Compartment': compartment}, **kwargs)
