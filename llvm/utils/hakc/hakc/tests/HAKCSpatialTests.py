import logging
import unittest
from typing import cast

from ..HAKCCompartmentalization import HAKCCompartmentalization
from ..HAKCLogger import HAKCLogger
from ..HAKCObjects import HAKCDefinitionLocation, HAKCFunction, HAKCType, HAKCScope, HAKCGlobalVariable, \
    HAKCAdjustments, HAKCCompartmentAdjustment, HAKCDivision, HAKCCompartment, HAKCSymbol

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.spatial-tests'))

import random
import string

# must prevent collisions
dl_count = 1
random_strings = set()


def get_random_string(length: int = 8) -> str:
    global random_strings
    while True:
        random_string = ''.join(random.choices(string.ascii_letters + string.digits, k=length))
        if random_string not in random_strings:
            random_strings.add(random_string)
            return random_string


def get_random_function(Name: str,
                        Type: HAKCType = HAKCType(LLVMType='int'),
                        Scope: HAKCScope = HAKCScope(Scope='local'),
                        DefinitionLocation: HAKCDefinitionLocation = HAKCDefinitionLocation(DefiningFile='null.c',
                                                                                            DefiningLine=dl_count),
                        **kwargs):
    global dl_count
    dl_count += 1
    return HAKCFunction(Name=Name,
                        Type=Type,
                        Scope=Scope,
                        DefinitionLocation=DefinitionLocation, **kwargs)


def get_random_global_variable(Name: str,
                               Type: HAKCType = HAKCType(LLVMType='int'),
                               Scope: HAKCScope = HAKCScope(Scope='local'),
                               DefinitionLocation: HAKCDefinitionLocation = HAKCDefinitionLocation(
                                   DefiningFile='null.c', DefiningLine=dl_count),
                               **kwargs):
    global dl_count
    dl_count += 1
    return HAKCGlobalVariable(Name=Name,
                              Type=Type,
                              Scope=Scope,
                              DefinitionLocation=DefinitionLocation, **kwargs)


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

        compartmentalization.perform_adjustments(HAKCAdjustments(**{'no-enforcement-compartment': True, 'adjustments': []}))

        # after placing everything in the NEC there should only be 1
        self.assertEqual(len(compartmentalization.conn.get_all_divisions()), 1)

        self.assertEqual(len(compartmentalization.get_symbols()), len(compartmentalization.conn.get_symbols()))
        self.assertEqual(len(compartmentalization.get_types()), len(compartmentalization.conn.get_types()))
        self.assertEqual(len(compartmentalization.get_scopes()), len(compartmentalization.conn.get_scopes()))
        self.assertEqual(len(compartmentalization.get_definition_locations()), len(compartmentalization.conn.get_definition_locations()))


if __name__ == '__main__':
    unittest.main()
