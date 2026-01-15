//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCYAMLCOMPARTMENTALIZATIONPOLICY_H

#include "HAKCYamlCompartment.h"
#include "HAKCYamlSymbol.h"
#include <vector>

namespace llvm::hakc {

class HAKCYamlCompartmentalizationPolicy {
public:
  HAKCYamlCompartmentalizationPolicy() = default;

  std::vector<HAKCYamlCompartment> Compartments;
  std::vector<HAKCYamlSymbol> Symbols;
};

} // namespace llvm::hakc

#endif // HAKC_HAKCYAMLCOMPARTMENTALIZATIONPOLICY_H
