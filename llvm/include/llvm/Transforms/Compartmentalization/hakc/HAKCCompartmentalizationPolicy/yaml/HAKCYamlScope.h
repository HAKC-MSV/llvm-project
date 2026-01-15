//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLSCOPE_H
#define HAKC_HAKCYAMLSCOPE_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <string>

namespace llvm::hakc {
class HAKCYamlScope {
public:
  HAKCYamlScope(hakc_scope_t Scope, std::string LocalScope);

  HAKCYamlScope() = default;

  std::string LocalScope;
  hakc_scope_t Scope;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCYAMLSCOPE_H
