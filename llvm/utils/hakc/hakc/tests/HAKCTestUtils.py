import logging
from typing import cast

from .. import HAKCCompartmentalization
from ..HAKCLogger import HAKCLogger
from ..HAKCObjects import HAKCDefinitionLocation, HAKCFunction, HAKCType, HAKCScope, HAKCGlobalVariable

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.test.utils'))

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
