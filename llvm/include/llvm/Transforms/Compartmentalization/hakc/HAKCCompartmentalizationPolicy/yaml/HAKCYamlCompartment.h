//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLCOMPARTMENT_H
#define HAKC_HAKCYAMLCOMPARTMENT_H

#include "HAKC-defs.h"
#include "HAKCYamlClique.h"

namespace llvm::hakc {

class HAKCYamlCompartment {
public:
  HAKCYamlCompartment();

  std::vector<HAKCYamlClique> Cliques;
  hakc_access_token_t EntryToken;
  hakc_compartment_id_t CompartmentID;
  std::vector<hakc_compartment_id_t> Targets;
};

} // namespace llvm::hakc

#endif // HAKC_HAKCYAMLCOMPARTMENT_H
