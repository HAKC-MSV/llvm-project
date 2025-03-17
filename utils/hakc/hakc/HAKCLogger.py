import logging
from enum import Enum


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


def setup_logging(logger, log_file: str = "", log_level: LoggingLevelEnum = LoggingLevelEnum.WARNING,
                  log_mode: str = 'w') -> None:
    log_formatter = logging.Formatter("%(asctime)s [%(threadName)-12.12s] [%(levelname)-5.5s]  %(message)s")
    logger.setLevel(log_level.value)
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(log_formatter)
    logger.addHandler(console_handler)
    if log_file and len(log_file) > 0:
        file_handler = logging.FileHandler(log_file, mode=log_mode)
        file_handler.setFormatter(log_formatter)
        logger.addHandler(file_handler)
