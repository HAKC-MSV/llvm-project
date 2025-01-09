//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLSYMBOL_H
#define HAKC_HAKCYAMLSYMBOL_H

#include "HAKCYamlType.h"
#include "HAKCYamlScope.h"

namespace llvm::hakc {
    class HAKCYamlSymbol {
    public:
        HAKCYamlSymbol();

        HAKCYamlType Type;
        HAKCYamlScope Scope;
        std::string Name;
        std::string Definition;
        hakc_compartment_id_t CompartmentID;
        hakc_compartment_division_t DivisionID;
        std::vector<std::string> CompilationUnits;

        friend raw_ostream &operator<<(raw_ostream &os, HAKCYamlSymbol &YamlSymbol);
    };
} // hakc

#endif //HAKC_HAKCYAMLSYMBOL_H
