import json
import logging
import os
import socketserver
import struct
from enum import Enum
from pathlib import Path
from typing import Optional, cast

import yaml

from .HAKCBase import HAKCPrintableObj, HAKCPayload
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCCompartment, HAKCDivision, HAKCDivisionCompartmentPayload
from .HAKCCompartmentalization import HAKCCompartmentalization
from .HAKCServerConfig import HAKCServerConfig, HAKCDataRequest, kwargs_get

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-analysis-server'))


class TimeoutException(Exception):
    pass

class HAKCAnalysisServerInstance:
    def __init__(self, config: HAKCServerConfig, yaml_loader=yaml.Loader, **kwargs):
        self.endpoints = {config.add_function_endpoint: self.add_function,
                          config.add_global_variable: self.add_global_variable}
        self.yaml_loader = yaml_loader
        self.socket_path = config.socket_path
        self.data_path = config.data_path
        self.test_mode = config.test_mode
        self.compartmentalization = HAKCCompartmentalization()
        logger.debug(f"Spinning up HAKCAnalysisServerInstance")

    def __del__(self):
        self.compartmentalization.save_as_yaml(self.data_path)

    def add_function(self, **kwargs):
        func = yaml.load(kwargs_get(str, "object", None, **kwargs), Loader=self.yaml_loader)
        self.compartmentalization.add_function(func)
        return HAKCPayload({'Success': True})

    def add_global_variable(self, **kwargs):
        gv = yaml.load(kwargs_get(str, "object", None, **kwargs), Loader=self.yaml_loader)
        self.compartmentalization.add_global_variable(gv)
        return HAKCPayload({'Success': True})

    def handle_request(self, request: HAKCDataRequest) -> HAKCPrintableObj:
        assert (isinstance(request, HAKCDataRequest))
        logger.debug(f"handle_request processing endpoint: {request.endpoint}")
        if request.endpoint not in self.endpoints:
            raise RuntimeError(f'Invalid Endpoint {request.endpoint}, endpoints available: {self.endpoints.keys()}')
        try:
            return self.endpoints[request.endpoint](**request.parameters)
        except Exception as e:
            raise RuntimeError(f"Exception: {e}")

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
            return self.wfile.write(raw_bytes)
        except OSError:
            raise ConnectionAbortedError

    @property
    def hakc_analysis_server(self) -> 'HAKCAnalysisServer':
        return cast(HAKCAnalysisServer, self.server)

    def gracefully_terminate_connection(self, e):
        err_msg = {"TERMINATING CONNECTION": e}
        err_data = json.dumps(err_msg, default=str).encode('utf-8')
        logger.fatal(f"Sending termination message to client with error: {e}")
        self.write_raw_bytes(struct.pack(HAKCRequestHandler.size_fmt, len(err_data)))
        self.write_raw_bytes(err_data)

    def handle(self):
        logger.debug(f'Handling request')
        hakc_request = None
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
                data = self.hakc_analysis_server.data_source.handle_request(hakc_request)

                if not (isinstance(data, HAKCPrintableObj)):
                    logger.error(
                        f"Generated response to request is not a HAKCPrintableObj and is invalid: {data}")
                    raise Exception
                logger.debug(f"data got from handle request: {data}")
                response_data = json.dumps(data.to_yaml_dict(), default=str)
                encoded_data = response_data.encode('utf-8')
                logger.debug(f"dumping json {encoded_data}")

                bytes_written = self.write_raw_bytes(struct.pack(
                    HAKCRequestHandler.size_fmt, len(encoded_data)))
                bytes_written += self.write_raw_bytes(encoded_data)
                logger.debug(f'dumped {bytes_written} bytes')
        # the 'raise' will call 'handle_error' in HAKCAnalysisServer
        except ConnectionAbortedError:
            logger.fatal(f'Client Aborted Connection while handling request: {hakc_request}')
            if self.hakc_analysis_server.data_source.test_mode:
                raise TimeoutException
            return
        except ConnectionResetError:
            logger.fatal(f'Client Reset Connection while handling request: {hakc_request}')
            return
        except TimeoutException:
            logger.fatal(f'Timeout received while handling request: {hakc_request}')
            return
        except Exception as e:
            logger.fatal(f"Error handling request: {hakc_request} with error: {e}")
            self.gracefully_terminate_connection(e)
            raise e


class HAKCAnalysisServer(socketserver.ThreadingUnixStreamServer):
    def __init__(self, data_source: HAKCAnalysisServerInstance, **kwargs):
        self.data_source = data_source
        os.makedirs(os.path.dirname(data_source.socket_path), exist_ok=True)
        logger.debug(f'Starting Socket Server at {data_source.socket_path}')
        socketserver.ThreadingUnixStreamServer.__init__(self, str(data_source.socket_path), HAKCRequestHandler)

    def handle_error(self, _a, _b):
        logger.info(f"Shutting down server")
        # do a server shutdown, rather than a server_close()
        self.shutdown()
