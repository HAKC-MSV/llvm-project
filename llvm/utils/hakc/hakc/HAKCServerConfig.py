import logging
from enum import Enum
from pathlib import Path
from typing import cast, Optional
from .HAKCLogger import HAKCLogger, LoggingLevelEnum, parse_log_level

logging.setLoggerClass(HAKCLogger)
logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-policy-server'))

class TimeoutException(Exception):
    pass

class TerminateConnectionException(Exception):
    pass

class SupportedBackingStore(Enum):
    NULL = "null"
    YAML = "yaml"
    KUZU = "kuzu"

class HAKCDataRequest:
    def __init__(self, Endpoint: str, **kwargs):
        self.endpoint = Endpoint
        self.parameters = kwargs.get('Parameters', dict())

    def __str__(self):
        # TODO: update these if the default endpoints are changed
        # Note: be careful here because if there is an error or exception the thread will crash without any debug information
        if self.endpoint == 'terminate-connection':
            return 'Request to terminate-connection'
        elif self.endpoint in ['get-compartment-by-id','get-division-by-id','get-division-from-symbol','get-valid-targets-from-compartment-id']:
            return f'Request query {self.endpoint} {self.parameters}'
        elif self.endpoint == 'add-symbols':
            out = f"Request to {self.endpoint} "
            for symbol in self.parameters['allSymbols']:
                out += f'{symbol}\n'
            return out
        return f'Request unknown endpoint {self.endpoint} with parameters {self.parameters}'

    def __repr__(self):
        return self.__str__()

class HAKCAnalysisConfig:
    def __init__(self, **kwargs):
        analysis_config = kwargs_get_or_error('AnalysisConfig', **kwargs)
        self.adjustments_path = analysis_config.get('adjustments-path', None)
        self.max_dag_processes = analysis_config.get('max-dag-processes', -1)
        self.timeout = analysis_config.get('timeout', 100)

class HAKCPolicyConfig:
    def __init__(self, **kwargs):
        policy_config = kwargs_get_or_error('PolicyConfig', **kwargs)
        self.timeout = policy_config.get('timeout', 1000)
        self.type = policy_config.get('type', 'kuzu')
        self.path = policy_config.get('path', None)
        if self.path and self.type == SupportedBackingStore.NULL.value:
            raise RuntimeError(f"PolicyConfig type is set to NULL, but path is set ({self.path}) but should be empty!")
        self.default_compartment_id = policy_config.get('default-compartment', 0)
        self.default_division_id = policy_config.get('default-division', 53)
        self.default_access_token = policy_config.get("default_access_token", 5353)
        self.default_entry_token = policy_config.get("default_entry_token", 3535)

class HAKCEndpoints:
    def __init__(self, **kwargs):
        endpoints = kwargs_get_or_error('Endpoints', **kwargs)
        self.get_compartment_endpoint = endpoints.get('get-compartment-by-id-endpoint', "get-compartment-by-id")
        self.get_division_endpoint = endpoints.get('get-division-by-id-endpoint', "get-division-by-id")
        self.get_division_from_symbol_endpoint = endpoints.get('get-division-from-symbol-endpoint',
                                                            "get-division-from-symbol")
        self.get_valid_targets_from_compartment_id_endpoint = endpoints.get(
            'get-valid-targets-from-compartment-id-endpoint', "get-valid-targets-from-compartment-id")
        self.add_symbols_endpoint = endpoints.get('add-symbols-endpoint', "add-symbols")
        self.terminate_connection_endpoint = endpoints.get('terminate-connection-endpoint', "terminate-connection")

class HAKCServerConfig:
    def __init__(self, **kwargs):
        # ServerConfig struct
        self.root_path = Path(kwargs_get_or_error('root-path', **kwargs))
        self.socket_path = Path(kwargs_get_or_error('socket-path', **kwargs))
        self.database_path = Path(kwargs.get('database-path'))
        self.reuse_path = kwargs.get('reuse-path', True)

        self.test_mode = kwargs.get('test-mode', False)
        self.max_server_processes = kwargs.get('max-server-processes', -1)
        self.log_path = kwargs.get('log-path', None)
        self.log_level = parse_log_level(kwargs.get('log-level', 'INFO'))

        # AnalysisConfig struct
        self.analysis_config = HAKCAnalysisConfig(**kwargs)

        # PolicyConfig struct
        self.policy_config = HAKCPolicyConfig(**kwargs)

        # Endpoints struct
        self.endpoints = HAKCEndpoints(**kwargs)

        # process args
        # server_timeout is set later to either analysis timeout or policy timeout depending on current mode
        self.server_timeout = -1
        if not self.test_mode and self.database_path == "":
            raise RuntimeError(f"Server is not in test mode, but database path is missing")

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
        # logger.warning(f"Unable to cast parameter {name} to type {cls}, so just returning {name} of type {type(val_or_default)}")
        return val_or_default
    except Exception:
        raise Exception(f"Failed to get parameter {name} of type {cls} from {kwargs}")

def kwargs_get_or_error(key, **kwargs):
    val = kwargs.get(key, None)
    if val is None:
        raise RuntimeError(f"{key} is missing from hakc-server.yaml")
    return val
