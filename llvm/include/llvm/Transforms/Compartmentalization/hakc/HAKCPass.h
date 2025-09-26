//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the top level declaration of the module level HAKC
/// compartmentalization pass.
///
//===----------------------------------------------------------------------===//
#ifndef PMC_HAKCPASS_H
#define PMC_HAKCPASS_H

#include "llvm/IR/PassManager.h"

namespace llvm {
    class HAKCPass : public PassInfoMixin<HAKCPass> {
    public:
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
    };
} // namespace llvm
#endif // PMC_HAKCPASS_H
