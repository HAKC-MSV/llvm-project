import json
import logging
import os
import socketserver
import struct
import signal
from enum import Enum
from pathlib import Path
from typing import Optional, cast

import yaml

from .HAKCBase import HAKCPrintableObj, HAKCPayload
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCCompartment, HAKCDivision, HAKCDivisionCompartmentPayload
from .HAKCCompartmentalization import HAKCCompartmentalization
from .HAKCServerConfig import HAKCServerConfig, HAKCDataRequest, kwargs_get, TimeoutException, TerminateConnectionException
from .HAKCServerThread import HAKCServerThread
logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-policy-server'))

class SupportedBackingStore(Enum):
    NULL = "null"
    YAML = "yaml"
    KUZU = "kuzu"


class HAKCPolicyDataSource:
    def __init__(self, config: HAKCServerConfig, yaml_loader=yaml.Loader, **kwargs):
        self.endpoints = {config.get_compartment_endpoint: self.get_compartment_by_id,
                          config.get_division_endpoint: self.get_division_by_id,
                          config.get_division_from_symbol_endpoint: self.get_symbol_division,
                          config.get_valid_targets_from_compartment_id_endpoint: self.get_valid_targets_from_compartment_id}
        self.yaml_loader = yaml_loader
        self.default_compartment = HAKCCompartment(config.default_compartment_id, EntryToken=0)
        self.default_division = HAKCDivision(config.default_division_id, AccessToken=0, Salt=0)
        self.socket_path = config.socket_path
        self.test_mode = config.test_mode

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
        # get-division-by-id endpoint
        # no need to do try except block because the caller of this function does that, and will accept an exception raised here
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        division_id = kwargs_get(int, "division-id", None, **kwargs)
        division = self._get_division_from_backing_store(division_id, compartment_id)
        if division is None:
            division = self._get_default_division()
        logger.debug(
            f"Returning Division {division} from (compartment_id, division_id): ({compartment_id}, {division_id})")
        return division

    def get_compartment_by_id(self, **kwargs) -> HAKCCompartment:
        # get-compartment-by-id endpoint
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        compartment = self._get_compartment_from_backing_store(compartment_id)
        if compartment is None:
            compartment = self._get_default_compartment()
        logger.debug(f"Returning Compartment {compartment} from input compartment {compartment_id}")
        return compartment

    def get_symbol_division(self, **kwargs) -> HAKCDivisionCompartmentPayload:
        # get-division-from-symbol endpoint
        _symbol = kwargs_get(str, "object", None, **kwargs)
        symbol = yaml.load(_symbol, Loader=self.yaml_loader)
        # if the symbol hash was not set (meaning it wasn't sent with the query) we need to find the correct hash value
        # if symbol.set_hash == False:

        # TODO: important: make sure that the symbol that is being loaded from yaml.load is complete (e.g., the hash has to exist and be correct or something)
        found_symbol = self._get_symbol_division_from_backing_store(symbol)
        if not found_symbol:
            logger.warning(f"Unable to find Compartment and Division from symbol {symbol}, so creating default Compartment and Division! (should we crash here?)")
            found_symbol = HAKCDivisionCompartmentPayload(division=self._get_default_division(), compartment=self._get_default_compartment())

        logger.debug(f"Returning Compartment Division {found_symbol} for symbol {symbol}")
        return found_symbol

    def get_valid_targets_from_compartment_id(self, **kwargs) -> HAKCPayload:
        # get-valid-targets-from-compartment-id"
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        logger.debug(f'Calling _get_valid_targets_from_compartment_id with compartment_id = {compartment_id}')
        payload = self._get_valid_targets_from_compartment_id(compartment_id)
        if payload is None:
            payload = HAKCPayload({"ValidTargets": None})
        return payload

    def handle_request(self, request: HAKCDataRequest) -> HAKCPrintableObj:
        assert (isinstance(request, HAKCDataRequest))
        logger.debug(f"handle_request processing endpoint: {request.endpoint}")
        if request.endpoint not in self.endpoints:
            raise RuntimeError(f'Invalid Endpoint {request.endpoint}, endpoints available: {self.endpoints.keys()}')
        try:
            return self.endpoints[request.endpoint](**request.parameters)
        except Exception as e:
            raise RuntimeError(f"Exception: {e}")


class NullHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCServerConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, **kwargs)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self._get_default_compartment()

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        return self._get_default_division()

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        return HAKCDivisionCompartmentPayload(division=self._get_default_division(),
                                              compartment=self._get_default_compartment())

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[HAKCPayload]:
        return HAKCPayload({'ValidTargets': [self._get_default_compartment().compartment_id]})


class YAMLHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCServerConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, yaml_loader=yaml.Loader, **kwargs)
        self.compartmentalization = None
        self.deserialize_compartmentalization(config.data_path)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self.compartmentalization.get_compartment_node(compartment_id)

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        return self.compartmentalization.get_division_node(division_id, compartment_id)

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        logger.debug(f"Trying to find symbol division, compartment for {symbol}")
        division = self.compartmentalization.get_division(symbol)
        if division is None:
            logger.debug(f'Failed to find division for {symbol}')
            return None
        # compartment = self.compartmentalization.get_compartment_node(division.compartment_id)
        compartment = self.compartmentalization.get_compartment_from_division(division)
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
        HAKCCompartmentalization.add_yaml_constructors()

        with open(yamlin, 'r') as file:
            self.compartmentalization = yaml.load(file, Loader=self.yaml_loader)

        logger.error(f"type of comp: {type(self.compartmentalization.nodes)}, {self.compartmentalization.nodes}")
        logger.error(f"{self.compartmentalization.edges}")
        if len(self.compartmentalization.nodes) == 0:
            raise RuntimeError(f'{yamlin} does not contain a compartmentalization policy')
        logger.debug(f'Successfully deserialized compartmentalization info! {self.compartmentalization}')


class KUZUHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCServerConfig, **kwargs):
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
        logger.debug(f"Trying to get division {division_id} in compartment {compartment_id}")
        division = self.database.get_division(division_id, compartment_id)
        if division is None:
            logger.error(
                f"Unable to find access_token for division_id {division_id}, so using default value of {self.default_division}!")
            division = self.default_division
        return division

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[HAKCDivisionCompartmentPayload]:
        assert (isinstance(symbol, HAKCSymbol))
        logger.debug(f"Trying to get HAKCDivision object from backing store with symbol: {symbol}")
        compartment_id_division_id_tuple = self.database.get_division_id_compartment_id_from_symbol(symbol)
        if compartment_id_division_id_tuple is None:
            logger.error(f"get_division_id_compartment_id_from_symbol returned None for symbol: {symbol}")
            return None
        return HAKCDivisionCompartmentPayload(division=compartment_id_division_id_tuple[0],
                                              compartment=compartment_id_division_id_tuple[1])

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[HAKCPayload]:
        assert (isinstance(compartment_id, int))
        target_id_entry_token = self.database.get_valid_targets_from_compartment_id(compartment_id)
        if len(target_id_entry_token) == 0:
            logger.fatal(f"Unable to find entry_token for compartment_id = {compartment_id}")
            return None
        return HAKCPayload({'ValidTargets': target_id_entry_token})

    def connect(self, kuzu_in):
        from .HAKCDatabase import HAKCDatabase
        logger.debug(f"Kuzu opening connection to {kuzu_in}")
        # open kuzu database connection in read only mode (multithreading)
        self.database = HAKCDatabase(kuzu_in, True)
        self.database.open(True)

# noinspection PyTypeChecker
class HAKCPolicyServerThread(HAKCServerThread):
    def init(self):
        # Note: handler() can be called before the actual __init__() function is called, so make a custom blocking init() for use in handler()
        HAKCServerThread.init(self)
        logger.debug(f"Spinning up Policy Server Thread")
        self.endpoints = self.hakc_server.data_source.endpoints

    @property
    def hakc_server(self) -> 'HAKCPolicyServer':
        return cast(HAKCPolicyServer, self.server)

# noinspection PyTypeChecker
class HAKCPolicyServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, config: HAKCServerConfig, data_source: HAKCPolicyDataSource, **kwargs):
        # TODO: maybe put config in data_source
        self.config = config
        self.data_source = data_source
        os.makedirs(os.path.dirname(config.socket_path), exist_ok=True)
        logger.debug(f'Starting Socket Server at {config.socket_path}')
        socketserver.ThreadingUnixStreamServer.__init__(self, str(config.socket_path), HAKCPolicyServerThread)

    def handle_error(self, _a, _b):
        logger.info(f"Shutting down server")
        # do a server shutdown, rather than a server_close()
        self.shutdown()

    def reset_alarm(self):
        # cancel existing alarm
        signal.alarm(0)
        if self.config.server_timeout > 0:
            signal.alarm(self.config.server_timeout)