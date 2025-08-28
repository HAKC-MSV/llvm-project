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
    for level in LoggingLevelEnum:
        if level.name == level_string.upper():
            return level
    raise RuntimeError(f'Invalid log level {level_string}')


class HAKCLogger(logging.Logger):
    log_fmt = "%(asctime)s [%(threadName)-12.12s] [%(levelname)-5.5s]  %(message)s"

    @staticmethod
    def get_formatter() -> logging.Formatter:
        return logging.Formatter(HAKCLogger.log_fmt)

    def __init__(self, name):
        super().__init__(name=name)
        self.console_handler = logging.StreamHandler()
        self.console_handler.setFormatter(HAKCLogger.get_formatter())
        self.addHandler(self.console_handler)

    def progress_bar(self, **kwargs) -> tqdm.tqdm:
        disabled = self.level not in {logging.INFO, logging.DEBUG}
        kwargs['file'] = self.console_handler.stream
        if disabled:
            kwargs['file'] = open(os.devnull, 'w')
            kwargs['disable'] = True

        return tqdm.tqdm(**kwargs)

    def add_file_handler(self, log_file: str, log_mode: str = 'w'):
        file_handler = logging.FileHandler(log_file, mode=log_mode)
        file_handler.setFormatter(HAKCLogger.get_formatter())
        self.addHandler(file_handler)


def setup_logging(logger: HAKCLogger, log_file: str = "", log_level: LoggingLevelEnum = LoggingLevelEnum.WARNING,
                  log_mode: str = 'w') -> None:
    logger.setLevel(log_level.value)
    if log_file and len(log_file) > 0:
        os.makedirs(os.path.dirname(log_file), exist_ok=True)
        logger.add_file_handler(log_file, log_mode)
