import logging
from enum import Enum
from pathlib import Path
from typing import cast, Type

import yaml

from .HAKCLogger import HAKCLogger, parse_log_level
from .HAKCObjects import add_yaml_constructors

logging.setLoggerClass(HAKCLogger)
logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.config'))


class TimeoutException(Exception):
    pass


class TerminateConnectionException(Exception):
    pass


class HAKCBackingType(Enum):
    NULL = "null"
    YAML = "yaml"
    KUZU = "kuzu"


def parse_backing_type(backing_type: str) -> HAKCBackingType:
    for ty in HAKCBackingType:
        if ty.name == backing_type.upper() or ty.name == backing_type.lower():
            return ty
    raise RuntimeError(f'Invalid backing type {backing_type}')


class HAKCBuildMode(Enum):
    ANALYSIS = "analysis"
    ENFORCEMENT = "enforcement"
    IR_ANALYSIS = "ir-analysis"
    IR_ENFORCEMENT = "ir-enforcement"

    @staticmethod
    def from_str(s: str) -> 'HAKCBuildMode':
        compare_value = s.lower()
        for e in HAKCBuildMode:
            if compare_value == e.value.lower():
                return e

        raise ValueError(f'Invalid string {s}')

    @property
    def is_analysis_mode(self) -> bool:
        return self in [HAKCBuildMode.ANALYSIS, HAKCBuildMode.IR_ANALYSIS]

    @property
    def is_enforcement_mode(self) -> bool:
        return self in [HAKCBuildMode.ENFORCEMENT, HAKCBuildMode.IR_ENFORCEMENT]

    @property
    def output_ir(self) -> bool:
        return self in [HAKCBuildMode.IR_ANALYSIS, HAKCBuildMode.IR_ENFORCEMENT]


class HAKCDataRequest:
    def __init__(self, Endpoint: str, **kwargs):
        self.endpoint = Endpoint
        self.parameters = kwargs.get('Parameters', dict())

    def __str__(self):
        # Note: be careful here because if there is an error or exception the thread will crash without any debug information
        if self.endpoint == 'terminate-connection':
            return 'Request to terminate-connection'
        elif self.endpoint in ['get-compartment-by-id', 'get-division-by-id', 'get-division-from-symbol',
                               'get-valid-targets-from-compartment-id']:
            return f'Request query {self.endpoint} {self.parameters}'
        elif self.endpoint == 'add-symbols':
            out = f"Request to {self.endpoint} "
            for symbol in self.parameters['allSymbols']:
                out += f'{symbol}\n'
            return out
        return f'Request unknown endpoint {self.endpoint} with parameters {self.parameters}'

    def __repr__(self):
        return self.__str__()


class HAKCBackingConfig:
    def __init__(self, **backing_config):
        self.type = parse_backing_type(get_arg_or_error('type', **backing_config))
        self.path = get_arg_or_error('path', **backing_config)
        if self.type == HAKCBackingType.NULL and len(self.path) != 0:
            raise RuntimeError(f"Backing Store Type is set to NULL, but path ({self.path}) is not empty!")

    def __str__(self):
        return f"""BackingConfig:
                        type:                   {self.type}
                        path:                   {self.path}"""


class HAKCEndpoints:
    def __init__(self, **endpoints):
        self.get_compartment_endpoint = endpoints.get('get-compartment-by-id-endpoint', "get-compartment-by-id")
        self.get_division_endpoint = endpoints.get('get-division-by-id-endpoint', "get-division-by-id")
        self.get_division_from_symbol_endpoint = endpoints.get('get-division-from-symbol-endpoint',
                                                               "get-division-from-symbol")
        self.get_valid_targets_from_compartment_id_endpoint = endpoints.get(
            'get-valid-targets-from-compartment-id-endpoint', "get-valid-targets-from-compartment-id")
        self.add_symbols_endpoint = endpoints.get('add-symbols-endpoint', "add-symbols")
        self.terminate_connection_endpoint = endpoints.get('terminate-connection-endpoint', "terminate-connection")


class HAKCServerConfig:
    def __init__(self, **hakc_server_config):
        # using safeloader because there is an associated yaml tag used to load the actual object
        self.adjustments = read_sub_config(hakc_server_config.get('adjustments-path', ''), Loader=yaml.SafeLoader)
        self.analysis_core_count = hakc_server_config.get('analysis-core-count', -1)
        self.timeout = hakc_server_config.get('timeout', 100)
        self.log_level = parse_log_level(hakc_server_config.get('log-level', 'INFO'))
        self.log_mode = hakc_server_config.get('log-mode', 'w')
        self.profile = hakc_server_config.get('profile', False)
        self.backing_config = HAKCBackingConfig(**get_arg_or_error('BackingConfig', **hakc_server_config))
        self.compilation_database = hakc_server_config.get('compilation-database-path', None)
        if self.compilation_database and len(self.compilation_database) == 0:
            self.compilation_database = None

    def __str__(self):
        return f"""ServerConfig: 
                    adjustments:            {self.adjustments}
                    analysis_core_count:    {self.analysis_core_count}
                    timeout:                {self.timeout}
                    log_level:              {self.log_level}
                    compilation_database:   {self.compilation_database}
                    {self.backing_config}"""


class HAKCConfig:
    def __init__(self, **hakc_config):
        add_yaml_constructors()
        self.server_config = HAKCServerConfig(**read_sub_config(get_arg_or_error('server-config-path', **hakc_config)))
        self.socket_dir = Path(get_arg_or_error('socket-dir', **hakc_config))
        self.temporal_analysis_enabled = hakc_config.get('temporal-analysis-enabled', False)
        self.server_core_count = hakc_config.get('server-core-count', 64)
        self.default_compartment_id = hakc_config.get('default-compartment-id', 0)
        self.default_division_id = hakc_config.get('default-division-id', 53)
        self.default_access_token = hakc_config.get('default-access-token', 5353)
        self.default_entry_token = hakc_config.get('default-entry-token', 3535)
        self.log_dir = hakc_config.get('log-dir', '')
        self.endpoints = HAKCEndpoints(**hakc_config)
        self.timeout = self.server_config.timeout
        self.root_config_path = Path(hakc_config['root-file-path']) if 'root-file-path' in hakc_config else None

    @property
    def log_level(self):
        return self.server_config.log_level

    def __str__(self):
        return f"""
        Config:
            {self.server_config}
            temporal-analysis-enabled:          {self.temporal_analysis_enabled}
            server-core-count:                  {self.server_core_count}"""


def kwargs_get(cls, name: str, default: any = None, **kwargs):
    # Function to retrieve and validate function parameter
    assert (isinstance(name, str))
    val_or_default = kwargs.get(name, default)
    # if value is already of the correct type, just return the value
    if type(val_or_default) == cls:
        return val_or_default
    try:
        if cls in [int, str, float]:
            return cls(val_or_default)
        return val_or_default
    except Exception:
        raise Exception(f"Failed to get parameter {name} of type {cls} from {kwargs}")


def get_arg_or_error(key, **kwargs):
    val = kwargs.get(key, None)
    if val is None:
        raise RuntimeError(f"{key} is missing from args {kwargs}")
    return val


def read_sub_config(path, Loader: Type[yaml.Loader | yaml.SafeLoader] = yaml.Loader):
    if len(path) == 0:
        return None
    with open(path, 'r') as f:
        try:
            return yaml.load(f, Loader=Loader)
        except Exception as e:
            raise RuntimeError(f"Unable to open sub configuration {path} with error {e}")
