import json
import logging
import socketserver
import struct
from enum import Enum
from pathlib import Path
from typing import Optional

import yaml

from .HAKCBase import HAKCPrintableObj, HAKCPayload
from .HAKCDatabase import HAKCDatabase
from .HAKCLogger import setup_logging, LoggingLevelEnum
from .HAKCObjects import HAKCSymbol, HAKCCompartment, HAKCDivision, HAKCDivisionCompartmentPayload

logger = logging.getLogger('hakc-policy-server')


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


class HAKCPolicyProcessConfig:
    def __init__(self, **kwargs):
        self.socket_path = kwargs.get('socket_path', None)
        if self.socket_path is None:
            raise RuntimeError("ERROR: socket_path is missing from policy_config.yml")
        self.socket_path = Path(self.socket_path)
        self.reuse_path = kwargs.get('reuse_path', False)
        self.log_path = kwargs.get('log_path', None)
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
        self.default_division_id = backing_store_config.get("default_division", 0)
        self.get_compartment_endpoint = kwargs.get('get-compartment-by-id-endpoint', "get-compartment-by-id")
        self.get_division_endpoint = kwargs.get('get-division-by-id-endpoint', "get-division-by-id")
        self.get_division_from_symbol_endpoint = kwargs.get('get-division-from-symbol-endpoint',
                                                            "get-division-from-symbol")
        self.get_valid_targets_from_compartment_id_endpoint = kwargs.get(
            'get-valid-targets-from-compartment-id-endpoint', "get-valid-targets-from-compartment-id")


class HAKCPolicyDataSource:
    def __init__(self, config: HAKCPolicyProcessConfig, yaml_loader=yaml.Loader, **kwargs):
        self.endpoints = {config.get_compartment_endpoint: self.get_compartment_by_id,
                          config.get_division_endpoint: self.get_division_by_id,
                          config.get_division_from_symbol_endpoint: self.get_symbol_division,
                          config.get_valid_targets_from_compartment_id_endpoint: self.get_valid_targets_from_compartment_id}
        self.yaml_loader = yaml_loader
        self.default_compartment = HAKCCompartment(config.default_compartment_id)
        self.default_division = HAKCDivision(config.default_division_id, config.default_compartment_id)
        self.socket_path = config.socket_path

    def _get_default_division(self) -> HAKCDivision:
        return self.default_division

    def _get_default_compartment(self) -> HAKCCompartment:
        return self.default_compartment

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        raise NotImplementedError

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        raise NotImplementedError

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        raise NotImplementedError

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> list[int]:
        raise NotImplementedError

    def get_division_by_id(self, **kwargs) -> HAKCDivision:
        compartment_id = kwargs.get("compartment-id", None)
        division_id = kwargs.get("division-id", None)
        if compartment_id is None or division_id is None:
            raise Exception(f"ERROR: get_division_by_id did not receive a compartment_id and/or a division_id")

        division = self._get_division_from_backing_store(int(division_id), int(compartment_id))
        if division is None:
            division = self._get_default_division()
        logger.debug(
            f"Returning Division {division} from (compartment_id, division_id): ({compartment_id}, {division_id})")
        return division

    def get_compartment_by_id(self, **kwargs) -> HAKCCompartment:
        compartment_id = kwargs.get("compartment-id", None)
        if compartment_id is None:
            raise Exception("ERROR: get_compartment_by_id did not receive a compartment_id")
        compartment = self._get_compartment_from_backing_store(int(compartment_id))
        if compartment is None:
            compartment = self._get_default_compartment()
        logger.debug(f"Returning Compartment {compartment} from input compartment {compartment_id}")
        return compartment

    def get_symbol_division(self, **kwargs) -> HAKCDivisionCompartmentPayload:
        symbol = kwargs.get('object', None)
        if symbol is None:
            raise Exception("ERROR: get_symbol_division did not receive a symbol object")
        symbol = yaml.load(symbol, Loader=self.yaml_loader)
        ret = self._get_symbol_division_from_backing_store(symbol)
        if ret is None:
            ret = HAKCDivisionCompartmentPayload(division=self._get_default_division(),
                                                 compartment=self._get_default_compartment())
        logger.debug(f"Returning Division {ret} for symbol {symbol}")
        return ret

    def get_valid_targets_from_compartment_id(self, **kwargs) -> HAKCPayload:
        compartment_id = kwargs.get("compartment-id", None)
        if compartment_id is None:
            raise Exception("ERROR: get_valid_targets_from_compartment_id did not receive a compartment_id")
        logger.debug(f'Calling _get_valid_targets_from_compartment_id with {compartment_id}')
        payload = self._get_valid_targets_from_compartment_id(int(compartment_id))
        if (payload is None):
            return HAKCPayload({"ValidTargets": None})
        return payload

    def handle_request(self, request: HAKCDataRequest) -> HAKCPrintableObj:
        logger.debug(f"handle_request processing endpoint: {request.endpoint}")
        if request.endpoint in self.endpoints:
            return self.endpoints[request.endpoint](**request.parameters)
        raise RuntimeError(f'Invalid Endpoint {request.endpoint}, endpoints available: {self.endpoints.keys()}')


class NullHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCPolicyProcessConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, **kwargs)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self._get_default_compartment()

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        return self._get_default_division()

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        return HAKCDivisionCompartmentPayload(division=self._get_default_division(),
                                              compartment=self._get_default_compartment())

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[HAKCPayload]:
        return list()


class YAMLHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCPolicyProcessConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, yaml_loader=yaml.Loader, **kwargs)
        self.compartmentalization = None
        self.deserialize_compartmentalization(config.data_path)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self.compartmentalization.get_compartment_node(compartment_id)

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        return self.compartmentalization.get_division_node(division_id, compartment_id)

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        division = self.compartmentalization.get_division(symbol)
        if division is None:
            logger.debug(f'Failed to find division for {symbol}')
            return None
        compartment = self.compartmentalization.get_compartment_node(division.compartment_id)
        if compartment is None:
            logger.error(f'Could not find compartment for {division}')
            raise RuntimeError()

        return HAKCDivisionCompartmentPayload(division=division, compartment=compartment)

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[HAKCPayload]:
        logger.debug(f'Finding valid targets in YAML for {compartment_id}')
        return HAKCPayload(
            {"ValidTargets": self.compartmentalization.get_valid_targets_from_compartment_id(compartment_id)})

    def deserialize_compartmentalization(self, yamlin):
        if yamlin is None:
            raise RuntimeError(f'yamlin is None')
        with open(yamlin, 'r') as file:
            self.compartmentalization = yaml.load(file, Loader=self.yaml_loader)

        if len(self.compartmentalization) == 0:
            raise RuntimeError(f'{yamlin} does not contain a compartmentalization policy')
        logger.debug(f'Successfully deserialized compartmentalization info! {self.compartmentalization}')


class KUZUHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCPolicyProcessConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, **kwargs)
        self.database = None
        self.connect(config.data_path)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        logger.debug(f"Trying to get compartment_id: {compartment_id} from backing store")
        entry_token = self.database.get_compartment_entry_token_from_id(compartment_id)
        if entry_token is None:
            return None
        return HAKCCompartment(compartment_id, EntryToken=entry_token)

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        logger.debug(f"Trying to get division_id: {division_id} from backing store")
        access_token = self.database.get_division_access_token_from_id(division_id, compartment_id)
        if access_token is None:
            return None
        return HAKCDivision(division_id, compartment_id, AccessToken=access_token)

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        logger.debug(f"Trying to get HAKCDivision object from backing store with symbol: {symbol}")
        compartment_id_division_id_tuple = self.database.get_division_id_compartment_id_from_symbol(symbol)
        if compartment_id_division_id_tuple is None:
            logger.error(f"get_division_id_compartment_id_from_symbol returned None for symbol: {symbol}")
            return None
        logger.debug(f"compartment_id_division_id_tuple: {compartment_id_division_id_tuple}")
        division_id = compartment_id_division_id_tuple[0]
        access_token = compartment_id_division_id_tuple[1]
        compartment_id = compartment_id_division_id_tuple[2]
        entry_token = compartment_id_division_id_tuple[3]
        ret = HAKCDivisionCompartmentPayload(
            division=HAKCDivision(DivisionID=division_id, CompartmentID=compartment_id, AccessToken=access_token),
            compartment=HAKCCompartment(CompartmentID=compartment_id, EntryToken=entry_token))
        logger.debug(f"get symbol division returning: {ret}")
        return ret

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[HAKCPayload]:
        target_id_entry_token = self.database.get_valid_targets_from_compartment_id(compartment_id)
        if len(target_id_entry_token) == 0:
            return None
        return HAKCPayload({'ValidTargets': target_id_entry_token})

    def connect(self, kuzuin):
        logger.debug(f"Kuzu opening connection to {kuzuin}")
        # open kuzu database connection in read only mode (multithreading)
        self.database = HAKCDatabase(kuzuin, True)
        self.database.open(True)


class HAKCRequestHandler(socketserver.StreamRequestHandler):
    size_fmt = "@L"

    def read_raw_bytes(self, size: int) -> bytes:
        logger.debug(f'Trying to read {size} bytes')
        raw_bytes = self.rfile.read(size)
        if len(raw_bytes) != size:
            raise ConnectionAbortedError
        return raw_bytes

    def write_raw_bytes(self, raw_bytes: bytes):
        try:
            self.wfile.write(raw_bytes)
        except OSError:
            raise ConnectionAbortedError

    @property
    def hakc_policy_server(self) -> 'HAKCPolicyServer':
        return self.server

    def handle(self):
        logger.debug(f'Handling request')
        size_in_bytes = struct.calcsize(HAKCRequestHandler.size_fmt)
        try:
            while True:
                raw_size_bytes = self.read_raw_bytes(size_in_bytes)
                msg_size = struct.unpack(HAKCRequestHandler.size_fmt, raw_size_bytes)[0]
                raw_msg_bytes = self.read_raw_bytes(msg_size)
                logger.debug(f'Received message of length {len(raw_msg_bytes)} bytes, contains {raw_msg_bytes}')
                json_request = json.loads(raw_msg_bytes)
                logger.debug(f"loaded json")
                hakc_request = HAKCDataRequest(**json_request)
                data = self.hakc_policy_server.data_source.handle_request(hakc_request)

                if not (isinstance(data, HAKCPrintableObj)):
                    logger.error(
                        f"Generated response to request is not a HAKCPrintableObj or list, and is invalid: {data}")
                    raise Exception
                logger.debug(f"data got from handle request: {data}")
                response_data = json.dumps(data.to_yaml_dict(), default=str)
                encoded_data = response_data.encode('utf-8')
                logger.debug(f"dumped json {encoded_data}")

                self.write_raw_bytes(struct.pack(HAKCRequestHandler.size_fmt, len(encoded_data)))
                self.write_raw_bytes(encoded_data)
        # the 'raise' will call 'handle_error' in HAKCPolicyServer 
        except ConnectionAbortedError:
            logger.debug(f'Client Aborted Connection')
            return
        except ConnectionResetError:
            logger.debug(f'Client Reset Connection')
            raise
        except TimeoutException:
            logger.debug(f'Timeout received')
            return
        except Exception as e:
            logger.error(f"Error handling request: {e}")


class HAKCPolicyServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, data_source: HAKCPolicyDataSource, log_level=LoggingLevelEnum.INFO,
                 log_file: str = "", log_mode: str = 'w', **kwargs):
        self.data_source = data_source
        setup_logging(logger, log_level=log_level, log_file=log_file, log_mode=log_mode)
        logger.debug(f'Starting Socket Server at {data_source.socket_path}')
        socketserver.ThreadingUnixStreamServer.__init__(self, str(data_source.socket_path),
                                                        RequestHandlerClass=HAKCRequestHandler)

    def handle_error(self, _a, _b):
        logger.error(f"Shutting down server")
        # do a server shutdown, rather than a server_close()
        self.shutdown()
