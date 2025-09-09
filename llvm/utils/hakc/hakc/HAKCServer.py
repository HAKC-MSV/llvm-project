import logging
import multiprocessing as mp
import os
import signal
import socketserver
from enum import Enum
from typing import Optional, cast, Tuple

import yaml

from .HAKCBase import HAKCResponse, HAKCResultSuccess, HAKCResultFail, HAKCPrintableObj
from .HAKCCompartmentalization import HAKCCompartmentalization
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCCompartment, HAKCDivision, HAKCDivisionPayload, HAKCCompartmentPayload, \
    HAKCDivisionCompartmentPayload, HAKCValidTargetsPayload, HAKCFunction, HAKCGlobalVariable
from .HAKCServerConfig import HAKCServerConfig, HAKCDataRequest, kwargs_get
from .HAKCServerThread import HAKCServerThread
from .HAKCDatabase import HAKCDatabase

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-server'))

# create global thread safe structure to share sub compartmentalizations
# add if statement
global_queue = mp.Queue()
SaveDAGEvent = mp.Event()

mp_conn: HAKCDatabase | None = None

class SupportedBackingStore(Enum):
    NULL = "null"
    YAML = "yaml"
    KUZU = "kuzu"

class HAKCServerMode(Enum):
    ANALYSIS = "analysis"
    POLICY = "policy"

def parse_server_mode(server_mode: str) -> HAKCServerMode:
    for mode in HAKCServerMode:
        if mode.name == server_mode.upper():
            return mode
    raise RuntimeError(f'Invalid server mode {server_mode}')


class HAKCPolicyDataSource:
    def __init__(self, config: HAKCServerConfig, yaml_loader=yaml.Loader, **kwargs):
        self.config = config
        self.endpoints = {config.get_compartment_endpoint: self.get_compartment_by_id,
                          config.get_division_endpoint: self.get_division_by_id,
                          config.get_division_from_symbol_endpoint: self.get_symbol_division,
                          config.get_valid_targets_from_compartment_id_endpoint: self.get_valid_targets_from_compartment_id}
        self.yaml_loader = yaml_loader
        self.default_compartment = HAKCCompartment(config.default_compartment_id, **{'EntryToken':0}) # need to use kwargs to set values
        self.default_division = HAKCDivision(config.default_division_id, **{'Salt':0, 'AccessToken':0})
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
        return HAKCResponse(HAKCResultSuccess(data=HAKCDivisionPayload(division)), self.config.get_division_endpoint)

    def get_compartment_by_id(self, **kwargs) -> HAKCResponse:
        # get-compartment-by-id endpoint
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        compartment = self._get_compartment_from_backing_store(compartment_id)
        if compartment is None:
            compartment = self._get_default_compartment()
        logger.debug(f"Returning Compartment {compartment} from input compartment {compartment_id}")
        return HAKCResponse(HAKCResultSuccess(HAKCCompartmentPayload(compartment)), self.config.get_compartment_endpoint)

    def get_symbol_division(self, **kwargs) -> HAKCResponse:
        # get-division-from-symbol endpoint
        symbol = yaml.load(kwargs_get(str, "object", None, **kwargs), Loader=self.yaml_loader)
        division_compartment_tuple = self._get_symbol_division_from_backing_store(symbol)
        # set the access token and entry token
        if not division_compartment_tuple:
            logger.warning(f"Unable to find Compartment and Division from symbol {symbol}, so creating default Compartment and Division! (should we crash here?)")
            return HAKCResponse(HAKCResultSuccess(HAKCDivisionCompartmentPayload(self._get_default_division(), self._get_default_compartment())),
                                self.config.get_division_from_symbol_endpoint)
        logger.debug(f"Returning Division {division_compartment_tuple[0]} Compartment {division_compartment_tuple[1]} for symbol {symbol}")
        return HAKCResponse(HAKCResultSuccess(HAKCDivisionCompartmentPayload(division_compartment_tuple[0], division_compartment_tuple[1])),
                            self.config.get_division_from_symbol_endpoint)

    def get_valid_targets_from_compartment_id(self, **kwargs) -> HAKCResponse:
        # get-valid-targets-from-compartment-id
        compartment_id = kwargs_get(int, "compartment-id", None, **kwargs)
        logger.debug(f'Calling _get_valid_targets_from_compartment_id with compartment_id = {compartment_id}')
        return HAKCResponse(HAKCResultSuccess(HAKCValidTargetsPayload(self._get_valid_targets_from_compartment_id(compartment_id))),
                            self.config.get_valid_targets_from_compartment_id_endpoint)


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
        self.deserialize_compartmentalization(config.data_path)

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
        self.connect(config.data_path)
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
        self.endpoints = {self.hakc_server.config.set_dag_filename_endpoint: self.set_dag_filename,
                          self.hakc_server.config.add_symbols_endpoint: self.add_symbols,
                          self.hakc_server.config.add_function_endpoint: self.add_function,
                          self.hakc_server.config.add_global_variable_endpoint: self.add_global_variable,
                          self.hakc_server.config.terminate_connection_endpoint: self.terminate_connection}
        # self.query_cache = {self.hakc_server.config.set_dag_filename_endpoint: {},
        #                     self.hakc_server.config.add_symbols_endpoint: {},
        #                     self.hakc_server.config.add_function_endpoint: {},
        #                     self.hakc_server.config.add_global_variable_endpoint: {},
        #                     self.hakc_server.config.terminate_connection_endpoint: {}}
        if self.policy_mode:
            for endpoint_str, endpoint_fn in self.policy_source.endpoints.items():
                self.endpoints[endpoint_str] = endpoint_fn
                # self.query_cache[endpoint_str] = {}
        if self.analysis_mode:
            self.compartmentalization = HAKCCompartmentalization()
            self.compartmentalization.add_yaml_constructors()
        self.dag_filename = None
        logger.debug(f"Finished initializing Server Thread in {'ANALYSIS' if self.analysis_mode else 'POLICY'} mode")

    def handle(self):
        self.init()
        logger.debug(f"Finished handle init subclass")
        HAKCServerThread.handle(self)
        logger.debug(f"Finished handle subclass ")

    @property
    def hakc_server(self) -> 'HAKCServer':
        return cast(HAKCServer, self.server)

    def terminate_connection(self, **kwargs):
        # override terminate_connection function to save graph
        if self.dag_filename:
            # self.compartmentalization.save_as_yaml(self.dag_filename)
            global_queue.put(self.compartmentalization)
            logger.info(f"Adding {self.compartmentalization}")
        HAKCServerThread.terminate_connection(self)

    def set_dag_filename(self, **kwargs) -> HAKCResponse:
        self.dag_filename = kwargs_get(str, "dag-filename", None, **kwargs)
        self.file_handler = self.logger.add_file_handler(self.dag_filename.replace(".dag.yml", ".analysis-server.log"))
        logger.debug(f"Processing {self.dag_filename}")
        return HAKCResponse(HAKCResultSuccess(), self.hakc_server.config.set_dag_filename_endpoint)

    def add_symbols(self, **kwargs) -> HAKCResponse:
        symbols = kwargs_get(list[str], "allSymbols", None, **kwargs)
        logger.debug(f"Adding {len(symbols)}\t symbols")
        for sym in symbols:
            symbol = yaml.load(sym, Loader=self.yaml_loader)
            if isinstance(symbol, HAKCFunction):
                self.compartmentalization.add_function(symbol)
            elif isinstance(symbol, HAKCGlobalVariable):
                self.compartmentalization.add_global_variable(symbol)
            else:
                logger.fatal(f"Invalid symbol: {symbol}")
                return HAKCResponse(HAKCResultFail('Invalid request: object is not HAKCSymbol!'), self.hakc_server.config.add_symbols_endpoint)
        return HAKCResponse(HAKCResultSuccess(), self.hakc_server.config.add_symbols_endpoint)

    def add_function(self, **kwargs) -> HAKCResponse:
        func = yaml.load(kwargs_get(str, "object", None, **kwargs), Loader=self.yaml_loader)
        self.compartmentalization.add_function(func)
        return HAKCResponse(HAKCResultSuccess(), self.hakc_server.config.add_function_endpoint)

    def add_global_variable(self, **kwargs) -> HAKCResponse:
        gv = yaml.load(kwargs_get(str, "object", None, **kwargs), Loader=self.yaml_loader)
        self.compartmentalization.add_global_variable(gv)
        return HAKCResponse(HAKCResultSuccess(), self.hakc_server.config.add_global_variable_endpoint)

# noinspection PyTypeChecker
class HAKCServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, config: HAKCServerConfig, server_mode: HAKCServerMode,  logger: HAKCLogger, **kwargs):
        self.logger = logger
        assert self.logger
        self.config = config
        assert self.config
        self.analysis_mode = server_mode == HAKCServerMode.ANALYSIS
        self.timeout = config.server_timeout
        self.policy_mode = not self.analysis_mode
        self.init_mp_database(self.config.data_path)
        self.logger.debug(f'Starting Server with socket {config.socket_path}')
        os.makedirs(os.path.dirname(config.socket_path), exist_ok=True)
        socketserver.ThreadingUnixStreamServer.__init__(self, str(config.socket_path), HAKCServerThreadInstance)

    def __del__(self):
        logger.debug(f"Closing HAKCServer")

    def handle_error(self, request, client):
        import traceback
        traceback.print_exc()
        self.logger.error(f"Error handling request {request} for client {client}")
        # do a server shutdown, rather than a server_close()
        self.shutdown()

    def reset_alarm(self):
        # cancel existing alarm
        signal.alarm(0)
        if self.timeout > 0:
            signal.alarm(self.timeout)

    @staticmethod
    def init_data_source(config: HAKCServerConfig) -> Optional[HAKCPolicyDataSource]:
        if config.type == SupportedBackingStore.NULL.value:
            logger.debug(f'Creating NullHAKCPolicyDataStore')
            return NullHAKCPolicyDataStore(config)
        elif config.type == SupportedBackingStore.YAML.value:
            logger.debug(f'Creating YAMLPolicyDataStore')
            return YAMLHAKCPolicyDataStore(config)
        elif config.type == SupportedBackingStore.KUZU.value:
            logger.debug(f'Creating KUZUPolicyDataStore')
            return KUZUHAKCPolicyDataStore(config)
        raise RuntimeError(f"Unsupported data store type: {config.type}")

    @staticmethod
    def init_mp_database(db_dir: str):
        global mp_conn
        mp_conn = HAKCDatabase(db_dir, read_only=True)
        logger.debug(f"Created global connection to kuzu db")
