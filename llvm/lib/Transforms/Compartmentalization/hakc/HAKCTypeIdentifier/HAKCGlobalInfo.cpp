//
// Created by de29664 on 5/2/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCGlobalInfo.h"

namespace llvm::hakc {
HAKCGlobalInfo::HAKCGlobalInfo(CommonHAKCAnalysis &Analysis, StringRef Name,
                               bool DebugActive)
    : HAKCSymbolInfo(Analysis, Name, DebugActive) {}

void HAKCGlobalInfo::SetGlobalVariable(GlobalVariable *GV) {
  HAKCSymbolInfo::SetGlobalObj(GV);
}

GlobalVariable *HAKCGlobalInfo::GetGlobalVariable() const {
  return dyn_cast<GlobalVariable>(GetGlobalObj());
}

StringRef HAKCGlobalInfo::GetYamlIdentifier() const {
  return "!HAKCGlobalVariable";
}
} // namespace llvm::hakc
