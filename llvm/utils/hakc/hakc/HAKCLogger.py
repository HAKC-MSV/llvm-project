import logging
import os
from enum import Enum

import tqdm


class LoggingLevelEnum(Enum):
    CRITICAL = logging.CRITICAL
    ERROR = logging.ERROR
    WARNING = logging.WARNING
    INFO = logging.INFO
    DEBUG = logging.DEBUG


def parse_log_level(level_string: str) -> LoggingLevelEnum:
    """
    parse string into LoggingLevelEnum
    :param level_string:
    :return: LoggingLevelEnum
    """
    for level in LoggingLevelEnum:
        if level.name == level_string.upper():
            return level
    raise RuntimeError(f'Invalid log level {level_string}')


class HAKCLogger(logging.Logger):
    """
    Custom HAKC logging.Logger
    """
    log_fmt = "%(asctime)s [%(processName)s, %(threadName)-10.10s] [%(levelname)-3.3s] %(message)s"

    @staticmethod
    def get_formatter() -> logging.Formatter:
        return logging.Formatter(HAKCLogger.log_fmt)

    def __init__(self, name):
        super().__init__(name=name)
        self.console_handler = logging.StreamHandler()
        self.console_handler.setFormatter(HAKCLogger.get_formatter())
        self.addHandler(self.console_handler)

    def progress_bar(self, **kwargs) -> tqdm.tqdm:
        kwargs['file'] = self.console_handler.stream
        # print(f"Log level: {self.level}")
        # disable progress bar printing if only errors or higher should be printed
        if self.level >= LoggingLevelEnum.INFO.value:
            kwargs['file'] = open(os.devnull, 'w')
            kwargs['disable'] = True

        return tqdm.tqdm(**kwargs)

    def add_file_handler(self, log_file: str, log_mode: str = 'w'):
        file_handler = logging.FileHandler(log_file, mode=log_mode)
        file_handler.setFormatter(HAKCLogger.get_formatter())
        self.addHandler(file_handler)
        # returning file_handler so threads can delete the per thread logger on completion
        return file_handler


def setup_logging(logger: HAKCLogger | logging.Logger, log_file: str = "",
                  log_level: LoggingLevelEnum = LoggingLevelEnum.WARNING,
                  log_mode: str = 'w') -> None:
    logger.setLevel(log_level.value)
    if log_file and len(log_file) > 0:
        os.makedirs(os.path.dirname(log_file), exist_ok=True)
        # RootLogger does not inherit add_file_handler from HAKCLogger or something
        for name, logger in logging.root.manager.loggerDict.items():
            # Other loggers exist, e.g., logging.PlaceHolder, but don't update these
            if isinstance(logger, logging.RootLogger):
                file_handler = logging.FileHandler(log_file, mode=log_mode)
                file_handler.setFormatter(HAKCLogger.get_formatter())
                logger.addHandler(file_handler)
            elif isinstance(logger, HAKCLogger):
                logger.add_file_handler(log_file, log_mode)
