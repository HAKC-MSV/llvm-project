import logging
import multiprocessing as mp
import os
import signal
import socketserver
import time
from enum import Enum
from typing import Optional, cast, Tuple

import yaml

from .HAKCBase import HAKCResponse, HAKCResultSuccess, HAKCResultFail
from .HAKCCompartmentalization import HAKCCompartmentalization
from .HAKCDatabase import HAKCDatabase
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCCompartment, HAKCDivision, HAKCDivisionPayload, HAKCCompartmentPayload, \
    HAKCDivisionCompartmentPayload, HAKCValidTargetsPayload, HAKCFunction, HAKCGlobalVariable
from .HAKCServerConfig import HAKCServerConfig, kwargs_get
from .HAKCServerThread import HAKCServerThread

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-server'))

mp_conn: HAKCDatabase | None = None

class SupportedBackingStore(Enum):
    NULL = "null"
    YAML = "yaml"
    KUZU = "kuzu"

class HAKCServerMode(Enum):
    ANALYSIS = "analysis"
    POLICY = "policy"


class HAKCPolicyDataSource:
    def __init__(self, config: HAKCServerConfig, yaml_loader=yaml.Loader, **kwargs):
        self.config = config
        self.endpoints = {config.endpoints.get_compartment_endpoint: self.get_compartment_by_id,
                          config.endpoints.get_division_endpoint: self.get_division_by_id,
                          config.endpoints.get_division_from_symbol_endpoint: self.get_symbol_division,
                          config.endpoints.get_valid_targets_from_compartment_id_endpoint: self.get_valid_targets_from_compartment_id}
        self.yaml_loader = yaml_loader
        self.default_compartment = HAKCCompartment(config.analysis_config.default_compartment_id, **{'EntryToken':0}) # need to use kwargs to set values
        self.default_division = HAKCDivision(config.analysis_config.default_division_id, **{'Salt':0, 'AccessToken':0})
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

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[Tuple[HAKCDivision, HAKCCompartment]]:
        raise NotImplementedError

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[list[int]]:
        raise NotImplementedError

    def get_division_by_id(self, **kwargs) -> HAKCResponse:
        # get-division-by-id endpoint
        # no need to do try except block because the caller of this function does that, and will accept an exception raised here
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        division_id = kwargs_get(int, "division-id", None, **kwargs)
        division = self._get_division_from_backing_store(division_id, compartment_id)
        if division is None:
            division = self._get_default_division()
        logger.debug(f"Returning Division {division} from (compartment_id, division_id): ({compartment_id}, {division_id})")
        return HAKCResponse(HAKCResultSuccess(data=HAKCDivisionPayload(division)), self.config.endpoints.get_division_endpoint)

    def get_compartment_by_id(self, **kwargs) -> HAKCResponse:
        # get-compartment-by-id endpoint
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        compartment = self._get_compartment_from_backing_store(compartment_id)
        if compartment is None:
            compartment = self._get_default_compartment()
        logger.debug(f"Returning Compartment {compartment} from input compartment {compartment_id}")
        return HAKCResponse(HAKCResultSuccess(HAKCCompartmentPayload(compartment)), self.config.endpoints.get_compartment_endpoint)

    def get_symbol_division(self, **kwargs) -> HAKCResponse:
        # get-division-from-symbol endpoint
        symbol = yaml.load(kwargs_get(str, "object", None, **kwargs), Loader=self.yaml_loader)
        division_compartment_tuple = self._get_symbol_division_from_backing_store(symbol)
        # set the access token and entry token
        if not division_compartment_tuple:
            logger.warning(f"Unable to find Compartment and Division from symbol {symbol}, so creating default Compartment and Division! (should we crash here?)")
            return HAKCResponse(HAKCResultSuccess(HAKCDivisionCompartmentPayload(self._get_default_division(), self._get_default_compartment())),
                                self.config.endpoints.get_division_from_symbol_endpoint)
        logger.debug(f"Returning Division {division_compartment_tuple[0]} Compartment {division_compartment_tuple[1]} for symbol {symbol}")
        return HAKCResponse(HAKCResultSuccess(HAKCDivisionCompartmentPayload(division_compartment_tuple[0], division_compartment_tuple[1])),
                            self.config.endpoints.get_division_from_symbol_endpoint)

    def get_valid_targets_from_compartment_id(self, **kwargs) -> HAKCResponse:
        # get-valid-targets-from-compartment-id
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        logger.debug(f'Calling _get_valid_targets_from_compartment_id with compartment_id = {compartment_id}')
        return HAKCResponse(HAKCResultSuccess(HAKCValidTargetsPayload(self._get_valid_targets_from_compartment_id(compartment_id))),
                            self.config.endpoints.get_valid_targets_from_compartment_id_endpoint)


class NullHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCServerConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, **kwargs)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self._get_default_compartment()

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        return self._get_default_division()

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[Tuple[HAKCDivision, HAKCCompartment]]:
        return self._get_default_division(), self._get_default_compartment()

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[list[int]]:
        return [self._get_default_compartment().compartment_id]


class YAMLHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCServerConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, yaml_loader=yaml.Loader, **kwargs)
        self.compartmentalization = None
        self.deserialize_compartmentalization(config.policy_config.path)

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        return self.compartmentalization.get_compartment_node(compartment_id)

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        return self.compartmentalization.get_division_node(division_id, compartment_id)

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[Tuple[HAKCDivision, HAKCCompartment]]:
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

        return division, compartment

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[list[int]]:
        logger.debug(f'Finding valid targets in YAML for {compartment_id}')
        return self.compartmentalization.get_valid_targets_from_compartment_id(compartment_id)

    def deserialize_compartmentalization(self, yamlin):
        if yamlin is None:
            raise RuntimeError(f'yamlin is None')
        HAKCCompartmentalization.add_yaml_constructors()

        with open(yamlin, 'r') as file:
            self.compartmentalization = yaml.load(file, Loader=self.yaml_loader)

        if len(self.compartmentalization.nodes) == 0:
            raise RuntimeError(f'{yamlin} does not contain a compartmentalization policy')
        logger.debug(f'Successfully deserialized {self.compartmentalization}!')


class KUZUHAKCPolicyDataStore(HAKCPolicyDataSource):
    def __init__(self, config: HAKCServerConfig, **kwargs):
        HAKCPolicyDataSource.__init__(self, config, **kwargs)
        self.conn = None
        self.connect(config.policy_config.path)
        logger.debug(f"Finished setting up KUZU Policy Source")

    def _get_compartment_from_backing_store(self, compartment_id: int) -> Optional[HAKCCompartment]:
        logger.debug(f"Trying to get compartment_id: {compartment_id} from backing store")
        entry_token = self.conn.get_compartment_entry_token_from_id(compartment_id)
        if entry_token is None:
            return None
        return HAKCCompartment(compartment_id, EntryToken=entry_token)

    def _get_division_from_backing_store(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        logger.debug(f"Trying to get division {division_id} in compartment {compartment_id}")
        division = self.conn.get_division(division_id, compartment_id)
        if division is None:
            division = self._get_default_division()
            logger.debug(f"Unable to find division for division_id {division_id}, so using default value of {division}!")
        return division

    def _get_symbol_division_from_backing_store(self, symbol: HAKCSymbol) -> Optional[Tuple[HAKCDivision, HAKCCompartment]]:
        assert (isinstance(symbol, HAKCSymbol))
        logger.debug(f"Trying to get HAKCDivision object from backing store with symbol: {symbol}")
        compartment_id_division_id_tuple = self.conn.get_division_id_compartment_id_from_symbol(symbol)

        if compartment_id_division_id_tuple is None:
            logger.debug(f"get_division_id_compartment_id_from_symbol returned None for symbol: {symbol}")
            return None
        return compartment_id_division_id_tuple

    def _get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[list[int]]:
        assert (isinstance(compartment_id, int))
        return self.conn.get_valid_targets_from_compartment_id(compartment_id)

    def connect(self, kuzu_in):
        global mp_conn
        self.conn = mp_conn
        logger.debug(f"Thread connected to Kuzu")
        # logger.debug(f"Kuzu opening connection to {kuzu_in}")
        # open kuzu database connection in read only mode (multithreading)
        # self.database = HAKCDatabase(kuzu_in, True)
        # self.database.open(True)
        # logger.debug(f"Successfully connected to Kuzu")

# noinspection PyTypeChecker
class HAKCServerThreadInstance(HAKCServerThread):
    size_fmt = "@L"
    def init(self):
        HAKCServerThread.init(self)
        # Note: handler() can be called before the actual __init__() function is called, so make a custom blocking init() for use in handler()
        # Initialize thread specific data
        logger.debug(f"Spinning up Server Thread Instance")
        self.config = self.hakc_server.config
        self.analysis_mode = self.hakc_server.analysis_mode
        self.policy_mode = self.hakc_server.policy_mode
        self.policy_source = HAKCServer.init_data_source(self.hakc_server.config) if self.policy_mode else None
        self.endpoints = {self.hakc_server.config.endpoints.add_symbols_endpoint: self.add_symbols,
                          self.hakc_server.config.endpoints.terminate_connection_endpoint: self.terminate_connection}
        if self.policy_mode:
            for endpoint_str, endpoint_fn in self.policy_source.endpoints.items():
                self.endpoints[endpoint_str] = endpoint_fn
        if self.analysis_mode:
            self.compartmentalization = HAKCCompartmentalization()
            self.compartmentalization.add_yaml_constructors()
        logger.debug(f"Finished initializing Server Thread in {'ANALYSIS' if self.analysis_mode else 'POLICY'} mode")

    def handle(self):
        self.init()
        HAKCServerThread.handle(self)

    @property
    def hakc_server(self) -> 'HAKCServer':
        return cast(HAKCServer, self.server)

    def terminate_connection(self, **kwargs):
        # override terminate_connection function to add compartmentalization to queue
        assert len(self.compartmentalization) > 0
        self.hakc_server.server_gather_buckets[self.hakc_server.id].put(self.compartmentalization)
        HAKCServerThread.terminate_connection(self)

    def add_symbols(self, **kwargs) -> HAKCResponse:
        symbols = kwargs_get(list[str], "allSymbols", None, **kwargs)
        # logger.debug(f"Adding {len(symbols)}\t symbols")
        for sym in symbols:
            symbol = yaml.load(sym, Loader=self.yaml_loader)
            if isinstance(symbol, HAKCFunction):
                self.compartmentalization.add_function(symbol)
            elif isinstance(symbol, HAKCGlobalVariable):
                self.compartmentalization.add_global_variable(symbol)
            else:
                # TODO: probably make this a runtime error
                logger.fatal(f"Invalid symbol: {symbol}")
                return HAKCResponse(HAKCResultFail('Invalid request: object is not HAKCSymbol!'), self.hakc_server.config.endpoints.add_symbols_endpoint)
        # self.hakc_server.check_timeout()
        return HAKCResponse(HAKCResultSuccess(), self.hakc_server.config.endpoints.add_symbols_endpoint)

# noinspection PyTypeChecker
class HAKCServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, id: int, server_gather_buckets, config: HAKCServerConfig, server_mode: HAKCServerMode, logger: HAKCLogger, **kwargs):
        self.id = id
        self.server_gather_buckets = server_gather_buckets
        self.logger = logger
        self.config = config
        self.analysis_mode = server_mode == HAKCServerMode.ANALYSIS
        self.policy_mode = server_mode == HAKCServerMode.POLICY
        self.timeout = config.server_timeout
        self.last_alive = time.time()
        if self.policy_mode:
            self.init_mp_database(self.config.database_path)
        self.logger.debug(f'Starting Server with socket {config.socket_path}')
        os.makedirs(os.path.dirname(config.socket_path), exist_ok=True)
        socketserver.ThreadingUnixStreamServer.__init__(self, str(config.socket_path), HAKCServerThreadInstance)

    def __del__(self):
        # global_queues_events[self.pid].set()
        logger.debug(f"Closing HAKCServer")

    def handle_error(self, request, client):
        import traceback
        traceback.print_exc()
        self.logger.error(f"Error handling request {request} for client {client}")
        # do a server shutdown, rather than a server_close()
        self.shutdown()

    @staticmethod
    def init_data_source(config: HAKCServerConfig) -> Optional[HAKCPolicyDataSource]:
        if config.policy_config.type == SupportedBackingStore.NULL.value:
            logger.debug(f'Creating NullHAKCPolicyDataStore')
            return NullHAKCPolicyDataStore(config)
        elif config.policy_config.type == SupportedBackingStore.YAML.value:
            logger.debug(f'Creating YAMLPolicyDataStore')
            return YAMLHAKCPolicyDataStore(config)
        elif config.policy_config.type == SupportedBackingStore.KUZU.value:
            logger.debug(f'Creating KUZUPolicyDataStore')
            return KUZUHAKCPolicyDataStore(config)
        raise RuntimeError(f"Unsupported data store type: {config.policy_config.type}")

    @staticmethod
    def init_mp_database(db_dir: str):
        global mp_conn
        mp_conn = HAKCDatabase(db_dir, read_only=True)
        logger.debug(f"Created global connection to kuzu db")
