//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLTYPE_H
#define HAKC_HAKCYAMLTYPE_H

#include <string>

#include "llvm/Support/YAMLTraits.h"

namespace llvm::hakc {
class HAKCYamlType {
public:
  HAKCYamlType() = default;

  std::string DebugType;
  std::string LLVMType;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCYAMLTYPE_H
