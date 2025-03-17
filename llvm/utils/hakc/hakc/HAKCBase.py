import hashlib
from enum import Enum
from typing import Type

import yaml


class QuotedString(str):
    pass


class HAKCDivisionEnum(Enum):
    NO_DIVISION = 0
    SILVER_DIVISION = 1  # SIL
    GREEN_DIVISION = 2  # GRN
    RED_DIVISION = 3  # RED
    ORANGE_DIVISION = 4  # ORN
    YELLOW_DIVISION = 5  # YEL
    PURPLE_DIVISION = 6  # PUR
    BLUE_DIVISION = 7  # BLU
    GREY_DIVISION = 8  # GRY
    PINK_DIVISION = 9  # PNK
    BROWN_DIVISION = 10  # BWN
    WHITE_DIVISION = 11  # WHI
    BLACK_DIVISION = 12  # BLK
    TEAL_DIVISION = 13  # TEA
    VIOLET_DIVISION = 14  # VLT
    CRIMSON_DIVISION = 15  # CRI
    GOLD_DIVISION = 16  # GLD


class HAKCHashValue:
    ByteOrder = 'big'

    @classmethod
    def from_int(cls, hash_value: int):
        result = cls([])
        result.final_hash = hash_value
        return result

    def __init__(self, values: list[object]):
        if len(values) > 0:
            h = hashlib.sha256()
            for value in values:
                h.update(HAKCHashValue.get_bytes(value))
            self.final_hash = int.from_bytes(h.digest()[:8], byteorder=HAKCHashValue.ByteOrder)

    @staticmethod
    def get_bytes(value) -> bytes:
        if isinstance(value, str):
            return value.encode('utf-8')
        elif isinstance(value, int):
            return value.to_bytes(8, byteorder=HAKCHashValue.ByteOrder)
        else:
            hash_value = hash(value)
            return hash_value.to_bytes(8, byteorder=HAKCHashValue.ByteOrder)

    def __hash__(self):
        return self.final_hash

    def __str__(self):
        return f'{self.final_hash:0x}'

    def __repr__(self):
        return str(self)


class HAKCPrintableObj:
    def __init__(self, **kwargs):
        self.computed_hash = None

    def __str__(self):
        cls = self.__class__.__name__
        inside_strings = [f'{key}={str(value)}' for key, value in self.get_info_tokens(convert_hash=False).items()]
        return f'{cls}({", ".join(sorted(inside_strings))})'

    def get_info_tokens(self, convert_hash=True) -> dict[str, object]:
        raise NotImplementedError

    def to_yaml_dict(self) -> dict[str, object]:
        result = dict()
        for key, value in self.get_info_tokens(convert_hash=False).items():
            if isinstance(value, HAKCPrintableObj):
                result[key] = value.to_yaml_dict()
            elif isinstance(value, HAKCHashValue):
                result[key] = str(value)
            else:
                result[key] = value
        return result

    def compute_hash(self):
        if self.computed_hash is None:
            self.computed_hash = HAKCHashValue(self.get_hash_inputs())

    def get_computed_hash(self) -> HAKCHashValue:
        self.compute_hash()
        return self.computed_hash

    def __hash__(self):
        self.compute_hash()
        return hash(self.computed_hash)

    def get_hash_inputs(self) -> list[object]:
        raise NotImplementedError


class HAKCDBColumn(HAKCPrintableObj):
    def __init__(self, column_name: str, column_type: str, **kwargs):
        HAKCPrintableObj.__init__(self, **kwargs)
        self.column_name = column_name
        self.column_type = column_type

    def __str__(self):
        return f'{self.column_name}'

    def __eq__(self, other):
        if isinstance(other, HAKCDBColumn):
            return self.column_name == other.column_name and self.column_type == other.column_type
        return False

    def __hash__(self):
        return HAKCPrintableObj.__hash__(self)

    def get_info_tokens(self, convert_hash=True) -> dict[str, object]:
        return {
            'column_name': self.column_name,
            'column_type': self.column_type
        }

    def get_hash_inputs(self) -> list[object]:
        return list(self.get_info_tokens().values())


class HAKCDBNode(HAKCPrintableObj):
    def __init__(self, **kwargs):
        HAKCPrintableObj.__init__(self, **kwargs)
        for key, value in kwargs.items():
            if key == self.get_primary_key().column_name and self.uses_hashed_key():
                self.computed_hash = HAKCHashValue.from_int(int(value, 16))

    @classmethod
    def to_yaml(cls, dumper: yaml.Dumper, data):
        return dumper.represent_mapping(data.yaml_tag, data.to_yaml_dict())

    def get_info_tokens(self, convert_hash=True) -> dict[str, object]:
        result = {column.column_name: value for column, value in self.get_db_data(convert_hash).items()}
        return result

    def get_db_data(self, convert_hash=True) -> dict[HAKCDBColumn, object]:
        raise NotImplementedError

    def uses_hashed_key(self) -> bool:
        return False

    @staticmethod
    def get_primary_key() -> HAKCDBColumn:
        raise NotImplementedError

    @classmethod
    def get_db_table_columns(cls) -> list[HAKCDBColumn]:
        columns = [cls.get_primary_key()]
        for column in cls.get_data_columns():
            columns.append(column)
        return columns

    @staticmethod
    def get_data_columns() -> list[HAKCDBColumn]:
        raise NotImplementedError

    @staticmethod
    def get_db_relations() -> list['HAKCDBRelation']:
        return []

    @staticmethod
    def get_table_name() -> str:
        raise NotImplementedError

    @classmethod
    def get_table_definition(cls) -> str:
        columns = cls.get_db_table_columns()
        primary_key = cls.get_primary_key()
        member_str = ", ".join([" ".join([column.column_name, column.column_type]) for column in columns])
        return f'{cls.get_table_name()}({member_str}, PRIMARY KEY ({primary_key.column_name}))'

    def get_primary_key_data(self, convert_hash=True) -> object:
        primary_key = self.get_primary_key()
        for column, data in self.get_db_data(convert_hash).items():
            if column == primary_key:
                return data
        raise RuntimeError(
            f'Could not find data for primary key {primary_key} in object for table {self.get_table_name()}')


class HAKCPayload(HAKCPrintableObj):
    def __init__(self, payload: dict[str, object] = None, **kwargs):
        HAKCPrintableObj.__init__(self, **kwargs)
        self.payload = payload

    @classmethod
    def to_yaml(cls, dumper: yaml.Dumper, data):
        return dumper.represent_dict(data.to_yaml_dict())

    def get_info_tokens(self, convert_hash=True) -> dict[str, object]:
        return self.payload

    def add(self, key, val):
        if key not in self.payload:
            self.payload[key] = val


class HashedHAKCDBNode(HAKCDBNode):
    def __init__(self, **kwargs):
        HAKCDBNode.__init__(self, **kwargs)

    def uses_hashed_key(self) -> bool:
        return True


class HAKCDBRelation:
    def __init__(self, relation_name: str, from_class: Type[HAKCDBNode], to_class: Type[HAKCDBNode], **kwargs):
        self.relation_name = relation_name
        self.from_class = from_class
        self.to_class = to_class
        self.properties = None
        if len(kwargs) > 0:
            struct_def = ",".join([" ".join([name, db_type]) for name, db_type in kwargs.items()])
            self.properties = struct_def

    def __str__(self):
        return self.get_definition()

    def get_definition(self) -> str:
        definition_string = f'{self.relation_name}(FROM {self.from_class.get_table_name()} TO {self.to_class.get_table_name()}'
        if self.properties is not None:
            definition_string = ", ".join([definition_string, self.properties])
        definition_string += ")"
        return definition_string
