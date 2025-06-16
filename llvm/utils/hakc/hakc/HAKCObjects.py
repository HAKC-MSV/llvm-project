import logging
import re
from typing import Optional

import yaml

from .HAKCBase import HAKCDivisionEnum, HAKCDBColumn, HAKCDBRelation, HAKCDBNode, HAKCPrintableObj, HashedHAKCDBNode, \
    HAKCPayload

logger = logging.getLogger('hakc-dag')


class HAKCCompilationUnit(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCCompilationUnit"

    def __init__(self, DefiningFile: str, DefiningLine: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCDBNode.__init__(self, **kwargs)
        self.defining_file = DefiningFile
        self.defining_line = DefiningLine
        # if "cu_hash" in kwargs:
        #     assert kwargs["cu_hash"] == self.get_computed_hash(), f"cu_hash ({kwargs['cu_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def pretty_print(self):
        return f"HAKCCompilationUnit({self.defining_file}:{self.defining_line})"

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCCompilationUnit):
            return (self.defining_file == other.defining_file) and (self.defining_line == other.defining_line)
        return False

    def __hash__(self):
        # TODO check hash is correct
        return HAKCDBNode.__hash__(self)

    def debug_print(self, root=True, whitespace=""):
        return f"{whitespace}{self.yaml_tag} defined in line {self.defining_line} of file {self.defining_file}\n"

    def get_hash_inputs(self) -> list[object]:
        return [self.defining_file, self.defining_line]
        # return self.get_data_columns()

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('cu_hash', "UINT64")

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        # HAKCDBColumn('DefiningLine', "STRING")
        return [HAKCDBColumn('DefiningFile', "STRING")]

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompilationUnit.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCCompilationUnit.get_db_table_columns()
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.defining_file,
        }
        # schema[2]: self.defining_line


class HAKCDivision(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCDivision"
    relation_compartment = "has_compartment"

    def __init__(self, DivisionID: int, CompartmentID: Optional[int] = None,
                 DivisionCount: int = len(HAKCDivisionEnum) - 1, AccessToken: Optional[int] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.division_id = DivisionID
        self.compartment_id = CompartmentID
        self.division_count = DivisionCount
        self.access_token = AccessToken if AccessToken is not None else self.compute_access_token([])
        if "division_hash" in kwargs:
            assert kwargs["division_hash"] == self.get_computed_hash(), f"division_hash ({kwargs['division_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def pretty_print(self):
        return f"HAKCDivision({self.division_id})"

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCDivision):
            # TODO: check equality
            return self.compartment_id == other.compartment_id and self.division_id == other.division_id
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        # TODO: why is it a runtime error to compare these two classes?
        # Dumping the dag seems to sort hakc objects (requiring lt gt comparison)
        # for now, lets just compare hashes
        if isinstance(other, HAKCDivision):
            return self.compartment_id < other.compartment_id and self.division_id < other.division_id
        # raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')
        return hash(self) < hash(other)

    def get_hash_inputs(self) -> list[object]:
        return [self.division_id, self.access_token]
        # return self.get_data_columns()

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
            HAKCDBRelation(HAKCDivision.relation_compartment, HAKCDivision, HAKCCompartment)
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCDivision.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
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

# HAKCCompartment(CompartmentID=1, EntryToken=65536, compartment_hash=cd2662154e6d76b2)
# HAKCCompartment(CompartmentID=2, EntryToken=131072, compartment_hash=cd04a4754498e06d)
# HAKCCompartment(CompartmentID=3, EntryToken=196608, compartment_hash=d5688a52d55a02ec)
# HAKCCompartment(CompartmentID=4, EntryToken=262144, compartment_hash=8005f02d43fa06e7)
# HAKCCompartment(CompartmentID=5, EntryToken=327680, compartment_hash=5dee4dd60ff8d0ba)
# HAKCCompartment(CompartmentID=6, EntryToken=393216, compartment_hash=14ac577cdb2ef6d9)
# HAKCCompartment(CompartmentID=3, EntryToken=204800, compartment_hash=d5688a52d55a02ec)

class HAKCCompartment(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCCompartment"

    def __init__(self, CompartmentID: int, division_count: int = len(HAKCDivisionEnum) - 1,
                 Divisions: Optional[set[HAKCDivision]] = None,
                 EntryToken: Optional[int] = None, **kwargs):
        print(f"kwargs {kwargs}")
        yaml.YAMLObject.__init__(self)
        kwargs["Name"] = kwargs.get("Name", str(CompartmentID))
        HAKCDBNode.__init__(self, **kwargs)
        self.compartment_id = CompartmentID
        self.division_count = division_count
        self.divisions = Divisions if Divisions is not None else set()
        self.entry_token = EntryToken if EntryToken is not None else self.compute_entry_token()
        print(self)
        # TODO: figure out why the hash seems to match but fails the assertion
        # if "compartment_hash" in kwargs:
        #     assert kwargs["compartment_hash"] == self.get_computed_hash(), f"compartment_hash ({kwargs['compartment_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def pretty_print(self):
        return f"HAKCCompartment({self.compartment_id})"

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id == other.compartment_id and self.entry_token == other.entry_token
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def add_division(self, division: HAKCDivision):
        self.divisions.add(division)
        self.entry_token = self.compute_entry_token()

    def __lt__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id < other.compartment_id
        # raise RuntimeError(f'{other} is not a class of {self.__class__.__name__}!')
        return hash(self) < hash(other)

    def get_hash_inputs(self) -> list[object]:
        return [self.compartment_id, self.entry_token]

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
        return HAKCDBColumn('compartment_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('CompartmentID', 'UINT64'),
                HAKCDBColumn('EntryToken', 'UINT64')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCCompartment.yaml_tag[1:]

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return []

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCCompartment.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.compartment_id,
            schema[2]: self.entry_token
        }


class HAKCType(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, LLVMType: str, DebugType: Optional[str] = None, **kwargs):
        # del kwargs["type_hash"]
        # print(f"LLVMType: {LLVMType}, DebugType: {DebugType}, kwargs: {kwargs}")
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            kwargs['Name'] = DebugType if DebugType is not None and DebugType != HAKCType.unknown_type else LLVMType
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.debug_type = DebugType
        self.llvm_type = LLVMType
        self._debug_type_transformed = HAKCType.transform_type_str(self.debug_type)
        self._debug_type_is_known = self.debug_type != HAKCType.unknown_type
        self._llvm_type_is_known = self.llvm_type != HAKCType.unknown_type
        assert(isinstance(self.llvm_type, str) and self.llvm_type != "")
        if "type_hash" in kwargs:
            assert kwargs["type_hash"] == self.get_computed_hash(), f"type_hash ({kwargs['type_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def pretty_print(self):
        return f"HAKCType({self.debug_type})"

    def debug_print(self):
        return f"{self.yaml_tag}(DebugType: {self.debug_type}, LLVMType:  {self.llvm_type})"

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
        # return self.get_data_columns()

    def __hash__(self):
        return HAKCDBNode.__hash__(self)
        # return hash(self.llvm_type)

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
        return [HAKCDBColumn('DebugType', "STRING"),
                HAKCDBColumn('LLVMType', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCType.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCType.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.debug_type,
            schema[2]: self.llvm_type
        }


class HAKCTypePerm(yaml.YAMLObject):
    # purely an edge between function and type
    yaml_tag = "!HAKCTypePerm"

    # note: the yaml keys seem to line up with exactly the parameters in __init__
    def __init__(self, Type: HAKCType, RWX: str, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.RWX = RWX
        self.perm_type = Type
        assert (isinstance(RWX, str))
        assert (isinstance(self.perm_type, HAKCType))

    def pretty_print(self):
        return f"HAKCTypePerm(RWX({self.RWX} of {self.perm_type.debug_type}))"

    def debug_print(self):
        return f"""\t{self.yaml_tag} [:- {self.RWX} -> {self.perm_type.debug_print()}\n"""

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    @staticmethod
    def get_relation_perm_type():
        return "has_perm_type"

    def __hash__(self):
        return HAKCDBNode.__hash__(self.perm_type) + hash(self.RWX)

    def get_hash_inputs(self) -> list[object]:
        return [self.RWX, self.perm_type]

    def __eq__(self, other):
        if isinstance(other, HAKCTypePerm):
            return (self.RWX == other.RWX) and (self.perm_type == other.perm_type)
        return False


class HAKCScope(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCScope"
    global_scope = "global"
    local_scope = "local"

    def __init__(self, Scope: str, LocalScopeName: Optional[str] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.scope = Scope
        kwargs["Name"] = LocalScopeName if LocalScopeName is not None else self.scope
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.local_scope_name = LocalScopeName if LocalScopeName is not None else HAKCScope.global_scope
        self.is_global_scope = self.scope == HAKCScope.global_scope
        self.is_local_scope = self.scope == HAKCScope.local_scope
        assert (isinstance(self.scope, str))

    def pretty_print(self):
        return f"HAKCScope({self.scope})"

    def debug_print(self):
        return f"{self.yaml_tag}({self.local_scope_name})" if self.local_scope_name else ''

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
        # raise RuntimeError(f'{other} is not a {self.__class__.__name__}')
        return hash(self) < hash(other)

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
        return [HAKCDBColumn('Scope', "STRING"),
                HAKCDBColumn('LocalScopeName', "STRING")]

    @staticmethod
    def get_table_name() -> str:
        return HAKCScope.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCScope.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: self.scope,
            schema[2]: self.local_scope_name
        }


class HAKCSymbol(HashedHAKCDBNode, yaml.YAMLObject):
    relation_type="has_type"
    relation_symbol="has_symbol"
    relation_scope="has_scope"
    relation_compilation_unit="has_compilation_unit"
    relation_division="has_division"
    relation_dag="has_dag"


    # Init takes the attributes directly from the original dag.yml file the pass creates
    # Also, enforcing that some minimum amount of data is present (e.g., symbol needs a name and a type)
    # TODO add asserts here
    def __init__(self, Name: str, Type: HAKCType, Scope: HAKCScope,
                 CompilationUnit: Optional[HAKCCompilationUnit] = None,
                 UsedSymbols: Optional[list] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.name = Name
        self.type = Type
        self.scope = Scope
        self.compilation_unit = CompilationUnit if CompilationUnit else None
        # TODO: ignoring used symbols for now
        self.used_symbols = UsedSymbols if UsedSymbols else list()
        assert (self.name != "")
        assert (isinstance(self.type, HAKCType))
        assert (isinstance(self.scope, HAKCScope))
        if "type_hash" in kwargs:
            assert kwargs["type_hash"] == self.get_computed_hash(), f"type_hash ({kwargs['type_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def debug_print(self, root=True, whitespace=""):
        out = f"{whitespace}{self.name} of {self.type.debug_print()} in {self.scope.debug_print()}" + (
            f" with {self.compilation_unit.debug_print()}" if self.compilation_unit else '\n')

        if root:
            if len(self.used_symbols) > 0:
                out += f"{whitespace}  UsedSymbols:\n"
                for used_symbol in self.used_symbols:
                    out += f"{whitespace}  {used_symbol.debug_print(False)}" if used_symbol else ''
        return out

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

    # @property
    # def is_definition(self):
    #     return self.defining_file is not None

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('symbol_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('IsFunction', 'BOOL'),
                HAKCDBColumn('Name', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return "HAKCSymbol"

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCSymbol.relation_type, HAKCSymbol, HAKCType),
            HAKCDBRelation(HAKCSymbol.relation_scope, HAKCSymbol, HAKCScope),
            HAKCDBRelation(HAKCSymbol.relation_symbol, HAKCSymbol, HAKCSymbol),
            HAKCDBRelation(HAKCSymbol.relation_compilation_unit, HAKCSymbol, HAKCCompilationUnit, defining_line="UINT64"),
            HAKCDBRelation(HAKCSymbol.relation_division, HAKCSymbol, HAKCDivision),
            HAKCDBRelation(HAKCSymbol.relation_dag, HAKCSymbol, HAKCSymbol, weight="INT32")
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCSymbol.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
        return {
            schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[1]: isinstance(self, HAKCFunction),
            schema[2]: self.name
        }


class HAKCFunction(HAKCSymbol):
    yaml_tag = "!HAKCFunction"
    relation_direct_calls = "has_direct_calls"
    relation_indirect_calls = "has_indirect_calls"
    relation_types_used = "has_types_used"


    def __init__(self, DirectCalls: Optional[list] = None, IndirectCalls: Optional[list] = None,
                 TypesUsed: Optional[list] = None, **kwargs):
        HAKCSymbol.__init__(self, **kwargs)
        # assert that none of these are none
        self.direct_calls = DirectCalls if DirectCalls is not None else list()
        self.indirect_calls = IndirectCalls if IndirectCalls is not None else list()
        self.types_used = TypesUsed if TypesUsed is not None else list()

    def pretty_print(self):
        return f"HAKCFunction({self.name})"

    # need non_recursive parameter or else this will cause infinite recursion
    def debug_print(self, root=True, whitespace=""):
        out = f"{self.yaml_tag}\n" if root else ''
        out += f"{HAKCSymbol.debug_print(self, root, '  ')}"
        if root:
            if len(self.direct_calls) > 0:
                out += f"{whitespace}    DirectCalls:\n"
                for direct_call in self.direct_calls:
                    out += f"{whitespace}    {direct_call.debug_print(False, '')}" if direct_call else ''
            if len(self.indirect_calls) > 0:
                out += f"{whitespace}    InDirectCalls:\n"
                for indirect_call in self.indirect_calls:
                    out += f"{whitespace}    {indirect_call.debug_print(False, '')}" if indirect_call else ''
            if len(self.types_used) > 0:
                out += f"{whitespace}    TypesUsed:\n"
                for type_used in self.types_used:
                    out += f"{whitespace}    {type_used.debug_print()}" if type_used else ''
        return out

    # def get_hash_inputs(self) -> list[object]:
    #     return [HAKCSymbol.get_hash_inputs(self), self.direct_calls, self.indirect_calls, self.types_used]
    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCFunction.relation_indirect_calls, HAKCFunction, HAKCType),
            HAKCDBRelation(HAKCFunction.relation_direct_calls, HAKCFunction, HAKCSymbol),
            HAKCDBRelation(HAKCFunction.relation_types_used, HAKCFunction, HAKCType, R="UINT64", W="UINT64", X="UINT64")
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        # TODO: add assert
        return HAKCSymbol.get_db_data(self, convert_hash)


class HAKCGlobalVariable(HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        HAKCSymbol.__init__(self, **kwargs)

    def debug_print(self, root=True, whitespace=""):
        return "TODO IMPLEMENT GLOBAL VARIABLE DEBUG PRINT"

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        # TODO add assert
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
            self.adjustment_regexes[re.compile(adjustment.path)] = adjustment

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


class HAKCIndirectSourceLink(HAKCPrintableObj, yaml.YAMLObject):
    yaml_tag = "!HAKCIndirectSourceLink"
    kuzu_name = "ind_src"

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
    kuzu_name = "indirect_call"

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
