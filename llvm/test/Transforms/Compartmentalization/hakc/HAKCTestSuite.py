import os

import lit
from lit.formats.shtest import ShTest


class HAKCTestSuite(ShTest):
    def __init__(self, build_path: str):
        super(HAKCTestSuite, self).__init__()
        self.build_path = build_path

    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig, localConfig):
        for local_config_name in litConfig.local_config_names:
            generated_config = os.path.join(self.build_path, *path_in_suite, local_config_name)
            if os.path.exists(generated_config):
                test_config = localConfig
                test_config.load_from_path(generated_config, litConfig)
                for filename in os.listdir(test_config.test_source_root):
                    test_path = os.path.join(test_config.test_source_root, filename)
                    _, extension = os.path.splitext(test_path)
                    if os.path.isfile(test_path) and extension in test_config.suffixes:
                        source_path_in_suite = os.path.join(os.path.dirname(os.path.join(*path_in_suite)),
                                                            filename).split(os.sep)
                        yield lit.Test.Test(testSuite, source_path_in_suite, test_config)
