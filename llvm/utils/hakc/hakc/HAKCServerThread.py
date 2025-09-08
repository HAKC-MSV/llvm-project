import json
import logging
import socketserver
import struct
import threading
from typing import cast

import yaml

from .HAKCBase import HAKCPrintableObj, HAKCResponse
from .HAKCLogger import HAKCLogger
from .HAKCServerConfig import HAKCServerConfig, HAKCDataRequest, TimeoutException, TerminateConnectionException

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-server'))

# noinspection PyTypeChecker
class HAKCServerThread(socketserver.StreamRequestHandler):
    size_fmt = "@L"

    def init(self):
        # Note: handler() can be called before the actual __init__() function is called, so make a custom blocking init() for use in handler()
        logger.debug(f"Spinning up Server Thread")
        # Initialize thread specific data
        self.logger = cast(HAKCLogger, logging.getLogger(str(threading.get_ident())))
        # Maybe use CLoader because it's faster (but may have some compatability issues)

        self.yaml_loader = yaml.CLoader
        self.file_handler = None
        self.size_in_bytes = struct.calcsize(HAKCServerThread.size_fmt)
        self.config: HAKCServerConfig = None
        self.endpoints = dict()

    @property
    def hakc_server(self):
        return self.server

    def read_raw_bytes(self, size: int) -> bytes:
        raw_bytes = self.rfile.read(size)
        if len(raw_bytes) != size:
            raise ConnectionAbortedError
        return raw_bytes

    def write_raw_bytes(self, raw_bytes: bytes):
        try:
            return self.wfile.write(raw_bytes)
        except OSError:
            raise ConnectionAbortedError

    def send_terminate_connection(self, e):
        err_msg = {"SERVER TERMINATING CONNECTION": e}
        err_data = json.dumps(err_msg, default=str).encode('utf-8')
        self.write_raw_bytes(struct.pack(HAKCServerThread.size_fmt, len(err_data)))
        self.write_raw_bytes(err_data)

    def read_request_from_socket(self):
        raw_size_bytes = self.read_raw_bytes(self.size_in_bytes)
        msg_size = struct.unpack(HAKCServerThread.size_fmt, raw_size_bytes)[0]
        raw_msg_bytes = self.read_raw_bytes(msg_size)
        json_request = json.loads(raw_msg_bytes)
        hakc_request = HAKCDataRequest(**json_request)
        self.logger.debug(hakc_request)
        return hakc_request

    def write_response_to_socket(self, response: HAKCPrintableObj):
        if not (isinstance(response, HAKCPrintableObj)):
            raise Exception(f"Generated response to request is not a HAKCPrintableObj and is invalid: {response}")
        # self.logger.debug(f"Sending response {response}")
        response_data = json.dumps(response.to_yaml_dict(), default=str)
        # self.logger.debug(f"Sending response data {response_data}")
        encoded_data = response_data.encode('utf-8')
        self.logger.debug(f"Sending encoded data {encoded_data}")
        bytes_written = self.write_raw_bytes(struct.pack(
            HAKCServerThread.size_fmt, len(encoded_data)))
        # self.logger.debug(f"Sending {bytes_written} bytes")
        bytes_written += self.write_raw_bytes(encoded_data)
        return bytes_written

    # noinspection PyTypeChecker
    def handle_endpoint(self, hakc_request: HAKCDataRequest) -> HAKCResponse:
        if hakc_request.endpoint not in self.endpoints:
            raise RuntimeError(f'Invalid Endpoint {hakc_request.endpoint}, endpoints available: {self.endpoints.keys()}')
        return self.endpoints[hakc_request.endpoint](**hakc_request.parameters)

    def __del__(self):
        # This seems to be executed by the main thread
        if self.logger:
            self.logger.debug("Killing Analysis Server Thread")

    def terminate_connection(self, **kwargs):
        self.logger.debug(f"Raising Terminate Connection Signal")
        raise TerminateConnectionException

    def handle(self):
        self.init()
        logger.debug(f'Handling request')
        hakc_request = None
        response = None
        try:
            while True:
                hakc_request = self.read_request_from_socket()
                response = self.handle_endpoint(hakc_request)
                bytes_written = self.write_response_to_socket(response)
                assert bytes_written > 0, f"Bytes written to socket should always be >0: bytes_written = {bytes_written}"

        # the 'raise' will call 'handle_error' in HAKCAnalysisServer
        except ConnectionAbortedError:
            self.logger.fatal(f'Client Aborted Connection after returning {response}')
            if self.hakc_server.config.test_mode:
                raise TimeoutException
            self.hakc_server.reset_alarm()
            return
        except ConnectionResetError:
            self.logger.fatal(f'Client Reset Connection after returning {response}')
            self.hakc_server.reset_alarm()
            return
        except TimeoutException:
            self.logger.fatal(f'Timeout received after returning {response}')
            self.hakc_server.reset_alarm()
            return
        except TerminateConnectionException:
            self.logger.debug(f'Analysis Server Thread received terminate connection from Client; killing thread (TerminateConnectionException)')
            self.logger.removeHandler(self.file_handler)
            self.hakc_server.reset_alarm()
            return
        except Exception as e:
            if hakc_request.endpoint == "terminate-connection":
                self.logger.debug(f'Analysis Server Thread received terminate connection from Client; killing thread (General Fallthrough Exception)')
                self.logger.removeHandler(self.file_handler)
                return
            self.logger.fatal(f"Error handling request: {hakc_request} with error: {e}")
            self.hakc_server.reset_alarm()
            raise e
