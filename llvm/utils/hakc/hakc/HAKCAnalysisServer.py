import logging
import multiprocessing as mp
import os
import signal
import socketserver
from typing import cast

import yaml

from .HAKCBase import HAKCResponse, HAKCResultSuccess, HAKCResultFail
from .HAKCCompartmentalization import HAKCCompartmentalization
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCFunction, HAKCGlobalVariable
from .HAKCServerConfig import HAKCServerConfig, kwargs_get
from .HAKCServerThread import HAKCServerThread

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-analysis-server'))

# create global thread safe structure to share sub compartmentalizations
global_queue = mp.Queue()
SaveDAGEvent = mp.Event()

# noinspection PyTypeChecker
class HAKCAnalysisServerThread(HAKCServerThread):
    size_fmt = "@L"
    def init(self):
        HAKCServerThread.init(self)
        # Note: handler() can be called before the actual __init__() function is called, so make a custom blocking init() for use in handler()
        logger.debug(f"Spinning up Analysis Server Thread")
        self.endpoints = {self.hakc_server.config.set_dag_filename_endpoint: self.set_dag_filename,
                          self.hakc_server.config.add_symbols_endpoint: self.add_symbols,
                          self.hakc_server.config.add_function_endpoint: self.add_function,
                          self.hakc_server.config.add_global_variable_endpoint: self.add_global_variable,
                          self.hakc_server.config.terminate_connection_endpoint: self.terminate_connection}
        # Initialize thread specific data
        self.compartmentalization = HAKCCompartmentalization()
        self.compartmentalization.add_yaml_constructors()
        self.dag_filename = None

    @property
    def hakc_server(self) -> 'HAKCAnalysisServer':
        return cast(HAKCAnalysisServer, self.server)

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
class HAKCAnalysisServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, config: HAKCServerConfig, logger: HAKCLogger, **kwargs):
        self.logger = logger
        self.config = config
        os.makedirs(os.path.dirname(config.socket_path), exist_ok=True)
        self.logger.debug(f'Starting HAKC Analysis Server with socket {config.socket_path}')
        socketserver.ThreadingUnixStreamServer.__init__(self, str(config.socket_path), HAKCAnalysisServerThread)

    def __del__(self):
        logger.info(f"Closing HAKCAnalysisServer")

    def handle_error(self, request, client):
        self.logger.info(f"Error handling request {request} for client {client}")
        # do a server shutdown, rather than a server_close()
        self.shutdown()

    def reset_alarm(self):
        # cancel existing alarm
        signal.alarm(0)
        if self.config.server_timeout > 0:
            signal.alarm(self.config.server_timeout)