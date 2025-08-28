import logging
from enum import Enum
from pathlib import Path
from typing import cast
from .HAKCLogger import HAKCLogger

logging.setLoggerClass(HAKCLogger)
logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-policy-server'))

class TimeoutException(Exception):
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
        return f"Endpoint: {self.endpoint}\nwith parameters: {self.parameters}"

    def __repr__(self):
        return self.__str__()

class HAKCServerConfig:
    def __init__(self, **kwargs):
        self.socket_path = kwargs.get('socket_path', None)
        if self.socket_path is None:
            raise RuntimeError("ERROR: socket_path is missing from policy_config.yml")
        self.socket_path = Path(self.socket_path)
        self.reuse_path = kwargs.get('reuse_path', False)
        self.log_path = kwargs.get('log_path', None)
        self.test_mode = kwargs.get("test-mode", False)
        self.server_timeout = int(kwargs.get('server_timeout', -1))
        if self.reuse_path and self.socket_path.exists():
            self.socket_path.unlink()
        backing_store_config = kwargs.get('backing_policy_config', dict())
        if len(backing_store_config) == 0:
            raise RuntimeError(f'Missing backing store configuration')
        self.type = backing_store_config.get("type", None)
        if self.type is None:
            raise RuntimeError("ERROR: type (for data store) is missing")
        self.data_path = backing_store_config.get("path", None)
        if self.data_path is None and self.type != SupportedBackingStore.NULL.value:
            raise RuntimeError("ERROR: path (for data store) is missing")
        self.default_compartment_id = backing_store_config.get("default_compartment", 0)
        self.default_division_id = backing_store_config.get("default_division", 53)
        self.default_access_token = backing_store_config.get("default_access_token", 5353)
        self.default_entry_token = backing_store_config.get("default_entry_token", 3535)
        self.get_compartment_endpoint = kwargs.get('get-compartment-by-id-endpoint', "get-compartment-by-id")
        self.get_division_endpoint = kwargs.get('get-division-by-id-endpoint', "get-division-by-id")
        self.get_division_from_symbol_endpoint = kwargs.get('get-division-from-symbol-endpoint',
                                                            "get-division-from-symbol")
        self.get_valid_targets_from_compartment_id_endpoint = kwargs.get(
            'get-valid-targets-from-compartment-id-endpoint', "get-valid-targets-from-compartment-id")
        self.add_function_endpoint = kwargs.get('add-function-endpoint', "add-function")
        self.add_global_variable = kwargs.get('add-global-variable-endpoint', "add-global-variable")


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
        logger.warning(
            f"Unable to cast parameter {name} to type {cls}, so just returning {name} of type {type(val_or_default)}")
        return val_or_default
    except Exception:
        raise Exception(f"Failed to get parameter {name} of type {cls} from {kwargs}")
