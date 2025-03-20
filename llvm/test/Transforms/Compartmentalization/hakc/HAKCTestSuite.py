import copy
import os

import lit
from lit.formats.shtest import ShTest


class HAKCTestSuite(ShTest):
    # TODO: Add in kuzu tests
    test_types = ['yaml']

    def __init__(self):
        super(HAKCTestSuite, self).__init__()
        self.execute_external = True

    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig, localConfig):
        for test_type in HAKCTestSuite.test_types:
            for local_config_name in litConfig.local_config_names:
                generated_config = os.path.join(localConfig.test_exec_root, *path_in_suite, test_type,
                                                local_config_name)
                if os.path.exists(generated_config):
                    localConfig = copy.deepcopy(localConfig)
                    localConfig = litConfig.load_config(localConfig, generated_config)
                    for local_config_name2 in litConfig.local_config_names:
                        local_config_path = os.path.join(localConfig.test_source_root, local_config_name2)
                        if os.path.exists(local_config_path):
                            localConfig = litConfig.load_config(localConfig, local_config_path)

                    for filename in os.listdir(localConfig.test_source_root):
                        test_path = os.path.join(localConfig.test_source_root, filename)
                        _, extension = os.path.splitext(test_path)
                        if os.path.isfile(test_path) and extension in localConfig.suffixes:
                            file_path = list(path_in_suite)
                            file_path.append(filename)
                            yield lit.Test.Test(testSuite, file_path, localConfig)
