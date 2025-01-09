//
// Created by derrick on 3/16/21.
//
#ifndef PMC_HAKCPASS_H
#define PMC_HAKCPASS_H

#include "llvm/IR/PassManager.h"

namespace llvm {
    class HAKCPass : public PassInfoMixin<HAKCPass> {
    public:
        PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
    };
}
#endif//PMC_HAKCPASS_H
