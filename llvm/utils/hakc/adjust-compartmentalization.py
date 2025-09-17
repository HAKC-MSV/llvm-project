#!/usr/bin/env python3
import argparse
import logging
import os
import shutil
from typing import cast

from hakc.HAKCCompartmentalization import HAKCCompartmentalization
from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging, HAKCLogger

logging.setLoggerClass(HAKCLogger)


def main():
    parser = argparse.ArgumentParser(description='HAKC Analysis Server Process')
    parser.add_argument('--adjust-path', dest='adjust_path', help='Path to adjustment YAML', required=True)
    parser.add_argument('--log-level', required=False, dest='log_level',
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level, default=LoggingLevelEnum.INFO)
    parser.add_argument('--db-dir', help='Directory to use for the kuzu database', dest='db_dir', required=True)
    parser.add_argument('--dest-dir', help='Directory to use for adjusted compartmentalization', dest="dest_dir",
                        default=None)
    args = parser.parse_args()

    root_logger = logging.getLogger()
    setup_logging(root_logger, log_level=args.log_level, log_file='')
    logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-server-process'))

    dest_dir = args.db_dir + "-adjusted" if args.dest_dir is None else args.dest_dir
    os.makedirs(dest_dir, exist_ok=True)

    logger.info(f'Copying database from {args.db_dir} to {dest_dir}')
    shutil.copytree(args.db_dir, dest_dir, dirs_exist_ok=True)

    compartmentalization = HAKCCompartmentalization()
    compartmentalization.add_yaml_constructors()

    logger.info(f'Opening connection to {dest_dir}')
    compartmentalization.open_conn(dest_dir)
    compartmentalization.adjust_compartmentalization(dest_dir, args.adjust_path)


if __name__ == "__main__":
    main()
