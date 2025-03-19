import copy
import os

import lit
from lit.formats.shtest import ShTest


class HAKCTestSuite(ShTest):
    # TODO: Add in kuzu tests
    test_types = ['yaml']

    def __init__(self, build_path: str):
        super(HAKCTestSuite, self).__init__()
        self.build_path = build_path
        self.execute_external = True

    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig, localConfig):
        for test_type in HAKCTestSuite.test_types:
            for local_config_name in litConfig.local_config_names:
                generated_config = os.path.join(self.build_path, *path_in_suite, test_type, local_config_name)
                if os.path.exists(generated_config):
                    test_config = copy.deepcopy(localConfig)
                    test_config.load_from_path(generated_config, litConfig)

                    test_suite = copy.deepcopy(testSuite)
                    test_suite.source_root = test_config.test_source_root
                    test_suite.config = test_config
                    test_suite.name = path_in_suite[-1]
                    test_suite.exec_root = os.path.join(self.build_path, *path_in_suite)
                    test_suite.test_times = None

                    # Overwrite generated options with specified options
                    for local_config_name2 in litConfig.local_config_names:
                        existing_config = os.path.join(test_config.test_source_root, local_config_name2)
                        if os.path.exists(existing_config):
                            test_config.load_from_path(existing_config, litConfig)

                    for filename in os.listdir(test_config.test_source_root):
                        test_path = os.path.join(test_config.test_source_root, filename)
                        _, extension = os.path.splitext(test_path)
                        if os.path.isfile(test_path) and extension in test_config.suffixes:
                            # source_path_in_suite = os.path.join(os.path.dirname(os.path.join(*path_in_suite)),
                            #                                     filename).split(os.sep)
                            source_path_in_suite = [filename]
                            yield lit.Test.Test(test_suite, source_path_in_suite, test_config)
