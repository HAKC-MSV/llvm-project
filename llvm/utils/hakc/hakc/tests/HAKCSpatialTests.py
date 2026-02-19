import logging
import unittest
from typing import cast

from hakc.HAKCServer import KUZUHAKCEnforcementDataStore
from .HAKCTestUtils import get_random_function, get_random_string, get_random_global_variable
from ..HAKCCompartmentalization import HAKCCompartmentalization
from ..HAKCLogger import HAKCLogger, setup_logging
from ..HAKCObjects import HAKCDefinitionLocation, HAKCType, HAKCAdjustments, HAKCCompartment, compute_static_access_token
from ..HAKCConfig import HAKCConfig, HAKCDataRequest

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.spatial-tests'))


class HAKCDatabaseTests(unittest.TestCase):
    def test0(self):
        compartmentalization = HAKCCompartmentalization(db_dir='')

        for i in range(16):
            function0 = get_random_function(Name=get_random_string(16))
            function1 = get_random_function(Name=get_random_string(16))
            function0.direct_calls.append(function1)
            compartmentalization.add_function(function0)
            compartmentalization.add_function(function1)

        for i in range(8):
            compartmentalization.add_global_variable(get_random_global_variable(Name=get_random_string(16)))

        compartmentalization.add_default_compartmentalization(create_schema=True)
        compartmentalization.create_dag()

        # initially there should be N divisions for N symbols
        self.assertEqual(len(compartmentalization.conn.get_all_divisions()), 40)

        compartmentalization.perform_adjustments(
            HAKCAdjustments(**{'no-enforcement-compartment': True, 'adjustments': []}))

        # after placing everything in the NEC there should only be 1
        self.assertEqual(len(compartmentalization.conn.get_all_divisions()), 1)

        self.assertEqual(len(compartmentalization.get_symbols()), len(compartmentalization.conn.get_symbols()))
        self.assertEqual(len(compartmentalization.get_types()), len(compartmentalization.conn.get_types()))
        self.assertEqual(len(compartmentalization.get_scopes()), len(compartmentalization.conn.get_scopes()))
        self.assertEqual(len(compartmentalization.get_definition_locations()),
                         len(compartmentalization.conn.get_definition_locations()))

    def test1(self):
        # testing construction of access tokens
        print()
        logger.info(f"!!!\t\t\tStarting {self._testMethodName}\t\t\t!!!")

        # test valid compartment and division_ids
        self.assertEqual(compute_static_access_token(compartment_id=0, division_ids={0, 1, 2, 3}), 0b01111)
        self.assertEqual(compute_static_access_token(compartment_id=1, division_ids={0, 1, 2, 3}), 0b10000000000001111)
        self.assertEqual(compute_static_access_token(compartment_id=16, division_ids={0, 1, 2, 3}), 0b100000000000000001111)
        self.assertEqual(compute_static_access_token(compartment_id=16, division_ids={division_id for division_id in range(16)}),
                         0b100001111111111111111)
        self.assertEqual(compute_static_access_token(compartment_id=((1 << 48) - 1), division_ids={division_id for division_id in range(16)}),
                         (1 << 64) - 1)

        # test valid compartment and invalid division_ids
        try:
            compute_static_access_token(compartment_id=0, division_ids={17})
        except AssertionError as e:
            self.assertEqual(str(e), "Invalid access 0 <= 17 < 16")
        try:
            compute_static_access_token(compartment_id=0, division_ids={-1})
        except AssertionError as e:
            self.assertEqual(str(e), "Invalid access 0 <= -1 < 16")

        # test invalid compartment and valid division_ids
        try:
            compute_static_access_token(compartment_id=(1 << 48), division_ids={0})
        except AssertionError as e:
            self.assertEqual(str(e), f"Invalid v0: 0 <= {1 << 48} <= {(1 << 48) - 1}")
        try:
            compute_static_access_token(compartment_id=-1, division_ids={0})
        except AssertionError as e:
            self.assertEqual(str(e), f"Invalid v0: 0 <= -1 <= {(1 << 48) - 1}")

        logger.info(f"!!!\t\t\tEnding {self._testMethodName}\t\t\t!!!")
        print()


if __name__ == '__main__':
    unittest.main()
