import argparse
import json
import logging
import signal
from typing import Optional

from hakc.HAKCLogger import LoggingLevelEnum, parse_log_level, setup_logging
from hakc.HAKCPolicyServer import HAKCPolicyServer, NullHAKCPolicyDataStore, HAKCPolicyDataSource, \
    YAMLHAKCPolicyDataStore, KUZUHAKCPolicyDataStore, TimeoutException, HAKCPolicyProcessConfig, SupportedBackingStore

logger = logging.getLogger('hakc-policy-process')


def init_data_source(config: HAKCPolicyProcessConfig) -> Optional[HAKCPolicyDataSource]:
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


def timeout_handler(signum, frame):
    raise TimeoutException


# noinspection PyTypeChecker
def main():
    parser = argparse.ArgumentParser(description='HAKC Policy Process')
    parser.add_argument('--config', help='Path to config file', required=True)
    parser.add_argument('--log-level', required=False, dest='log_level', default=LoggingLevelEnum.INFO,
                        help=f'Log level to display, can be lower case {[level.name for level in LoggingLevelEnum]}',
                        type=parse_log_level)
    parser.add_argument('-l', '--log', default=None, dest='log_path')
    parser.add_argument('--log-mode', default='w', dest='log_mode')
    parser.add_argument('--no-timeout', required=False, dest='no_timeout', action='store_true',
                        help='Ignore timeout in config file')

    args = parser.parse_args()
    setup_logging(logger, log_file=args.log_path, log_level=args.log_level, log_mode=args.log_mode)
    with open(args.config, 'r') as f:
        parsed_config = json.load(f)
        config = HAKCPolicyProcessConfig(**parsed_config)
        if args.no_timeout:
            config.server_timeout = -1

    data_source = init_data_source(config)
    with HAKCPolicyServer(data_source=data_source, log_level=args.log_level, log_file=config.log_path,
                          log_mode=args.log_mode) as server:
        try:
            if config.server_timeout > 0:
                signal.signal(signal.SIGALRM, timeout_handler)
                signal.alarm(config.server_timeout)
            server.serve_forever()
        except KeyboardInterrupt:
            logger.info('User requested to stop server')
        except TimeoutException:
            logger.info(f'Timeout received')
        except Exception as e:
            logger.error(f'Error: {e}')
        finally:
            server.server_close()


if __name__ == "__main__":
    main()
