import logging
import random
import re
import sys
from typing import Optional, cast

import yaml

from .HAKCBase import HAKCDBColumn, HAKCDBRelation, HAKCDBNode, HAKCPrintableObj, HashedHAKCDBNode, \
    HAKCPayload
from .HAKCLogger import HAKCLogger
logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-dag'))


class HAKCDefinitionLocation(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCDefinitionLocation"

    def __init__(self, DefiningFile: str, DefiningLine: Optional[int] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCDBNode.__init__(self, **kwargs)
        self.defining_file = DefiningFile
        self.defining_line = DefiningLine # make defining line an edge in the networkx object

    def pretty_print(self):
        return f"{self.get_table_name()}({self.defining_file})"

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCDefinitionLocation):
            return self.defining_file == other.defining_file
        return False

    def __hash__(self):
        # TODO check hash is correct
        return HAKCDBNode.__hash__(self)

    def debug_print(self, whitespace=""):
        return f"{whitespace}{self.yaml_tag} file {self.defining_file}\n"

    def get_hash_inputs(self) -> list[object]:
        return [self.defining_file]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('DefiningFile', "STRING")

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return []

    @staticmethod
    def get_table_name() -> str:
        return HAKCDefinitionLocation.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCDefinitionLocation.get_db_table_columns()
        return {
            schema[0]: self.defining_file,
        }

class HAKCDivision(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCDivision"
    relation_compartment = "has_compartment"

    def __init__(self, DivisionID: int, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.division_id = DivisionID
        self.salt = kwargs.get("Salt", random.randint(0, sys.maxsize))
        self.access_token = kwargs.get("AccessToken", None)

    @staticmethod
    def compute_access_token(compartment_id: int, division_ids: set[int]) -> int:
        token = compartment_id << HAKCCompartment.max_compartments
        for division_id in division_ids:
            assert 0 <= division_id < HAKCCompartment.max_compartments, f"Invalid division ID {division_id}"
            token |= (1 << division_id)
        return token

    def pretty_print(self):
        return f"{self.get_table_name()}({self.division_id})"

    def __str__(self):
        return f"{self.get_table_name()}(division_id={self.division_id}, hash={self.get_computed_hash()})"

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCDivision):
            return hash(self) == hash(other)
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCDivision):
            return self.division_id < other.division_id
        return hash(self) < hash(other)

    def get_hash_inputs(self) -> list[object]:
        return [self.division_id, self.salt]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('division_hash', 'UINT64')

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return [HAKCDBColumn('DivisionID', 'UINT64'), HAKCDBColumn("Salt", 'UINT64')]

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
        return {
            # schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[0]: self.get_computed_hash().final_hash,
            schema[1]: self.division_id,
            schema[2]: self.salt
        }

    def get_info_tokens(self, convert_hash=True) -> dict[str, object]:
        tokens = {
            "DivisionID": self.division_id,
            "Salt": self.salt,
        }
        if self.access_token is not None:
            tokens["AccessToken"] = self.access_token
        return tokens


class HAKCCompartment(HAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCCompartment"
    max_compartments = 16

    def __init__(self, CompartmentID: int, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCDBNode.__init__(self, **kwargs)
        self.compartment_id = CompartmentID
        self.entry_token = kwargs.get("EntryToken", None)

    @staticmethod
    def compute_entry_token(compartment_id: int, entry_divisions: set[int]):
        token = compartment_id << HAKCCompartment.max_compartments
        for division_id in entry_divisions:
            assert 0 <= division_id < HAKCCompartment.max_compartments
            token |= (1 << division_id)
        return token

    def pretty_print(self):
        return f"{self.get_table_name()}({self.compartment_id})"

    def __str__(self):
        return f"{self.get_table_name()}(compartment_id={self.compartment_id})"

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id == other.compartment_id
        return False

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def __lt__(self, other):
        if isinstance(other, HAKCCompartment):
            return self.compartment_id < other.compartment_id
        return hash(self) < hash(other)

    def get_hash_inputs(self) -> list[object]:
        return [self.compartment_id]

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        return HAKCDBColumn('CompartmentID', 'UINT64')

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
            schema[0]: self.compartment_id,
        }

    @classmethod
    def get_data_columns(cls) -> list[HAKCDBColumn]:
        return []

    def get_info_tokens(self, convert_hash=True) -> dict[str, object]:
        tokens = {self.get_primary_key().column_name: self.compartment_id}
        if self.entry_token is not None:
            tokens['EntryToken'] = self.entry_token
        return tokens


class HAKCType(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCType"
    unknown_type = "@UNKNOWN@"

    def __init__(self, LLVMType: str, DebugType: Optional[str] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        if 'Name' not in kwargs:
            kwargs['Name'] = DebugType if DebugType is not None and DebugType != HAKCType.unknown_type else LLVMType
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.debug_type = DebugType
        self.llvm_type = LLVMType
        self._debug_type_transformed = HAKCType.transform_type_str(self.debug_type)
        self._debug_type_is_known = self.debug_type != HAKCType.unknown_type
        self._llvm_type_is_known = self.llvm_type != HAKCType.unknown_type
        assert (isinstance(self.llvm_type, str) and self.llvm_type != "")
        if "type_hash" in kwargs:
            assert kwargs[
                       "type_hash"] == self.get_computed_hash(), f"type_hash ({kwargs['type_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def pretty_print(self):
        return f"{self.get_table_name()}({self.debug_type})"

    def debug_print(self):
        return f"{self.yaml_tag}(DebugType: {self.debug_type}, LLVMType: {self.llvm_type})"

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
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

    def __lt__(self, other):
        if isinstance(other, HAKCType):
            if self._debug_type_is_known and other._debug_type_is_known:
                return self._debug_type_transformed < other._debug_type_transformed
            elif self._llvm_type_is_known and other._llvm_type_is_known:
                return self.llvm_type < other.llvm_type
            else:
                return False
        return False

    def get_hash_inputs(self) -> list[object]:
        # a type 'int ()' might be converted to 'int', causing an improper alias
        if self._debug_type_is_known:
            return [self._debug_type_transformed]
        else:
            return [self.llvm_type]
        # return self.get_data_columns()

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
        return [HAKCDBColumn('DebugType', "STRING"),
                HAKCDBColumn('LLVMType', 'STRING')]

    @staticmethod
    def get_table_name() -> str:
        return HAKCType.yaml_tag[1:]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCType.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
        return {
            # schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[0]: self.get_computed_hash().final_hash,
            schema[1]: self.debug_type,
            schema[2]: self.llvm_type
        }

class HAKCScope(HashedHAKCDBNode, yaml.YAMLObject):
    yaml_tag = "!HAKCScope"
    global_scope = "global"
    local_scope = "local"

    def __init__(self, Scope: str, LocalScopeName: Optional[str] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.scope = Scope
        # kwargs["Name"] = LocalScopeName if LocalScopeName is not None else self.scope
        self.local_scope_name = LocalScopeName if LocalScopeName is not None else HAKCScope.global_scope
        self.is_global_scope = self.scope == HAKCScope.global_scope
        self.is_local_scope = self.scope == HAKCScope.local_scope
        assert (isinstance(self.scope, str))
        if "scope_hash" in kwargs:
            assert kwargs[
                       "scope_hash"] == self.get_computed_hash(), f"scope_hash ({kwargs['scope_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def pretty_print(self):
        return f"HAKCScope({self.scope})"

    def debug_print(self):
        return f"{self.yaml_tag}({self.local_scope_name})" if self.local_scope_name else ''

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
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
            # schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[0]: self.get_computed_hash().final_hash,
            schema[1]: self.scope,
            schema[2]: self.local_scope_name
        }


class HAKCSymbol(HashedHAKCDBNode, yaml.YAMLObject):
    relation_type = "has_type"
    relation_symbol = "has_symbol"
    relation_scope = "has_scope"
    relation_division = "has_division"
    relation_dag = "has_dag"
    relation_definition_location = "has_definition_location"

    # Init takes the attributes directly from the original dag.yml file the pass creates
    # Also, enforcing that some minimum amount of data is present (e.g., symbol needs a name and a type)
    def __init__(self, Name: str, Type: HAKCType = None, Scope: HAKCScope = None,
                 DefinitionLocation: Optional[HAKCDefinitionLocation] = None,
                 UsedSymbols: Optional[list] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HashedHAKCDBNode.__init__(self, **kwargs)
        self.name = Name
        self.type = Type
        self.scope = Scope
        self.definition_location = DefinitionLocation
        self.used_symbols = UsedSymbols if UsedSymbols else list()
        assert len(self.name) != 0, "Name cannot be empty"
        # assert isinstance(self.type, HAKCType), f"{self.type} is not a HAKCType"
        # assert isinstance(self.scope, HAKCScope), f"{self.scope} is not a HAKCScope"
        if "type_hash" in kwargs:
            assert kwargs[
                       "type_hash"] == self.get_computed_hash(), f"type_hash ({kwargs['type_hash']}) =?= hash(self) ({self.get_computed_hash()})"

    def debug_print(self, root=True, whitespace=""):
        out = f"{whitespace}{self.name} of {self.type.debug_print() if self.type else 'NOT FOUND'} in {self.scope.debug_print() if self.scope else 'NOT FOUND'}" + (
            f" with {self.definition_location.debug_print()}" if self.definition_location else '\n')

        if root:
            if len(self.used_symbols) > 0:
                out += f"{whitespace}  UsedSymbols:\n"
                for used_symbol in self.used_symbols:
                    out += f"{whitespace}  {used_symbol.debug_print(False)}" if used_symbol else ''
        return out

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        logger.fatal(f"Constructing symbol with: {node}")
        return cls(**loader.construct_mapping(node, deep=True))

    def __eq__(self, other):
        if isinstance(other, HAKCSymbol):
            return hash(self) == hash(other)
        return False

    def __lt__(self, other):
        return hash(self) < hash(other)

    def __hash__(self):
        return HAKCDBNode.__hash__(self)

    def get_hash_inputs(self) -> list[object]:
        result: list[object] = [self.name]
        if self.type:
            result.append(self.type)
        if self.scope:
            result.append(self.scope)
        return result

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
            HAKCDBRelation(HAKCSymbol.relation_definition_location, HAKCSymbol, HAKCDefinitionLocation,
                           DefiningLine="UINT64"),
            HAKCDBRelation(HAKCSymbol.relation_division, HAKCSymbol, HAKCDivision),
            HAKCDBRelation(HAKCSymbol.relation_dag, HAKCSymbol, HAKCSymbol, weight="INT32")
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        schema = HAKCSymbol.get_db_table_columns()
        assert (len(schema) == (len(self.get_data_columns()) + 1))
        return {
            # schema[0]: hash(self) if convert_hash else self.get_computed_hash(),
            schema[0]: self.get_computed_hash().final_hash,
            schema[1]: isinstance(self, HAKCFunction),
            schema[2]: self.name
        }


class HAKCFunction(HAKCSymbol):
    yaml_tag = "!HAKCFunction"
    relation_direct_calls = "has_direct_calls"
    relation_indirect_calls = "has_indirect_calls"

    def __init__(self, DirectCalls: Optional[list] = None, IndirectCalls: Optional[list] = None,
                 TypesUsed: Optional[list] = None, **kwargs):
        HAKCSymbol.__init__(self, **kwargs)
        # assert that none of these are none
        self.direct_calls = DirectCalls if DirectCalls is not None else list()
        self.indirect_calls = IndirectCalls if IndirectCalls is not None else list()

    def pretty_print(self, epoch=None):
        if epoch != -1 and epoch is not None:
            return f"HAKCFunction({self.name} in Epoch {epoch})"
        return f"HAKCFunction({self.name})"

    def __str__(self):
        return f"HAKCFunction({self.name})"

    def __repr__(self):
        return self.__str__()

    # need non_recursive parameter or else this will cause infinite recursion
    def debug_print(self, root=True, whitespace=""):
        out = f"{self.yaml_tag}\n" if root else ''
        out += f"{HAKCSymbol.debug_print(self, root, '  ')}"
        if root:
            if len(self.direct_calls) > 0:
                out += f"{whitespace}    DirectCalls:\n"
                for direct_call in self.direct_calls:
                    out += f"{whitespace}    {direct_call.debug_print(False, f'{whitespace}')}\n" if direct_call else ''
            if len(self.indirect_calls) > 0:
                out += f"{whitespace}    InDirectCalls:\n"
                for indirect_call in self.indirect_calls:
                    out += f"{whitespace}    {indirect_call.debug_print(False, f'{whitespace}')}\n" if indirect_call else ''
        return out

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        # logger.fatal(f"Constructing symbol with: {node}")
        return cls(**loader.construct_mapping(node, deep=True))

    @staticmethod
    def get_db_relations() -> list[HAKCDBRelation]:
        return [
            HAKCDBRelation(HAKCFunction.relation_indirect_calls, HAKCFunction, HAKCType),
            HAKCDBRelation(HAKCFunction.relation_direct_calls, HAKCFunction, HAKCSymbol)
        ]

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        return HAKCSymbol.get_db_data(self, convert_hash)

class HAKCGlobalVariable(HAKCSymbol):
    yaml_tag = "!HAKCGlobalVariable"

    def __init__(self, **kwargs):
        HAKCSymbol.__init__(self, **kwargs)

    def pretty_print(self):
        return f"HAKCGlobalVariable({self.name})"

    def debug_print(self, root=True, whitespace=""):
        return HAKCSymbol.debug_print(self)

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        return HAKCSymbol.get_db_data(self, convert_hash)


class HAKCAdjustment(yaml.YAMLObject):
    yaml_tag = "!HAKCAdjustment"

    def __init__(self, path: str, division_id: int, compartment_id: int):
        yaml.YAMLObject.__init__(self)
        self.path = path
        self.division = HAKCDivision(division_id)
        self.compartment = HAKCCompartment(compartment_id)

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        # return cls(loader.construct_mapping(node, deep=True)[''])
        return cls(**loader.construct_mapping(node, deep=True))


class HAKCCompartmentalizationAdjustment(yaml.YAMLObject):
    yaml_tag = "!HAKCAdjustments"
    compartmentalize_entry = 'compartmentalize'
    add_no_enforcement_compartment = 'add-no-enforcement-compartment'

    def __init__(self, **kwargs):
        yaml.YAMLObject.__init__(self)
        self.adjustment_regexes = dict()
        self.add_no_enforcement_compartment = kwargs.get(
            HAKCCompartmentalizationAdjustment.add_no_enforcement_compartment, False)
        for adjustment in sorted(kwargs.get(HAKCCompartmentalizationAdjustment.compartmentalize_entry, set()),
                                 key=lambda e: e.path):
            self.adjustment_regexes[re.compile(adjustment.path)] = adjustment

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        logger.fatal(f"node: {node}")
        return cls(**loader.construct_mapping(node, deep=True))

    def get_adjusted_division_and_compartment(self, defining_path: str) -> Optional[
        tuple[HAKCDivision, HAKCCompartment]]:
        if defining_path is None:
            return None

        adjustment = None
        for adjustment_regex, adjustment in self.adjustment_regexes.items():
            match = adjustment_regex.search(defining_path)
            if match:
                adjustment = (adjustment.division, adjustment.compartment)

        return adjustment


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
    def from_yaml(cls, loader: yaml.CLoader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def get_hash_inputs(self) -> list[object]:
        result: list[object] = [self.link_type]
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

    def debug_print(self, root=True, whitespace=""):
        out = f"{whitespace} {self.yaml_tag}\n" if root else ''
        out += f"{whitespace} "
        if self.source_type:
            out += f"{self.source_type}"
        if self.global_name:
            out += f", {self.global_name}"
        if self.offset:
            out += f", {self.offset}"
        if self.arg_number:
            out += f", {self.arg_number}"
        if self.function_name:
            out += f", {self.function_name}"
        out += f"\n"
        return out


class HAKCIndirectCallSource(HAKCPrintableObj, yaml.YAMLObject):
    yaml_tag = "!HAKCIndirectSource"
    kuzu_name = "indirect_call"

    def __init__(self, Type: Optional[HAKCType] = None, Source: Optional[list] = None, **kwargs):
        yaml.YAMLObject.__init__(self)
        HAKCPrintableObj.__init__(self, **kwargs)
        self.source = Source
        self.type = Type
        assert (isinstance(self.type, HAKCType))

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        return cls(**loader.construct_mapping(node, deep=True))

    def debug_print(self, root=True, whitespace=""):
        out = f"{whitespace}{self.yaml_tag}\n" if root else ''
        out += f"{whitespace}    {self.type.debug_print()}\n"
        if self.source:
            for src in self.source:
                out += f"{whitespace}    {src.debug_print(whitespace=f'{whitespace}')}\n"
        return out

    def get_hash_inputs(self) -> list[object]:
        result: list[object] = []
        if self.type:
            result.append(self.type)
        for link in self.source:
            result.append(link)
        return result
