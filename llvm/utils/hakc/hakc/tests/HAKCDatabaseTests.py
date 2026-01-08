import logging
import os
import unittest
from typing import cast

from .HAKCTestUtils import get_random_function, get_random_global_variable, get_random_string
from ..HAKCConfig import HAKCConfig, HAKCBackingType
from ..HAKCDatabase import HAKCDatabase
from ..HAKCLogger import HAKCLogger, setup_logging
from ..HAKCUtils import delete_database, copy_database

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.spatial-tests'))


class HAKCDatabaseTests(unittest.TestCase):
    def __init__(self, methodName='runTest', config: HAKCConfig = None):
        super().__init__(methodName)
        self.config: HAKCConfig = config
        root_logger = logging.getLogger()
        setup_logging(root_logger, log_level=config.log_level.INFO)
        logger.debug(f"Loaded {self.config}")

    def test_linux_database(self):
        print()
        if self.config.server_config.backing_config.type != HAKCBackingType.KUZU:
            logger.info(
                f"!!!    Spatial test_linux_database: Unsupported backing type {self.config.server_config.backing_config.type}     !!!")
            return

        logger.info("!!!    Starting Spatial test_linux_database    !!!")
        db_dir_in = self.config.server_config.backing_config.path + "-adjusted"
        db_dir_out = self.config.server_config.backing_config.path + "-adjusted-test"
        if not os.path.exists(db_dir_in):
            return
        delete_database(db_dir_out)
        copy_database(db_dir_in, db_dir_out)
        conn = HAKCDatabase(db_dir_out)
        conn.set_timeout(100000)
        logger.debug(f"Loaded {conn}")

        # find symbol 'hakc_foo'
        sym = conn.get_symbols_by_name("hakc_foo")
        self.assertEqual(len(sym), 1)
        hakc_foo = sym[0]
        print(f"hakc_foo hash: {hash(hakc_foo)}")
        comp = conn.get_division_id_compartment_id_from_symbol(hakc_foo)
        self.assertIsNotNone(comp)
        logger.error(f"Found hakc_foo: {hakc_foo} in compartment {comp}")

        # How to add test custom query:
        # 1. Add desired query below
        # 2. Remove the 'pass' command below
        pass

        cmd = f""""""
        print(f"Executing command: {cmd}")
        data = conn.execute(cmd, symbol_hashes=[hash(sym[0])])
        for _, entry in data.iterrows():
            print(f"{entry}")

        logger.info("!!!    Ending Spatial test_linux_database    !!!")
        print()


if __name__ == '__main__':
    unittest.main()
