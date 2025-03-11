//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERSTATE_H
#define HAKCTRANSFERSTATE_H

#include "llvm/IR/Value.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCTransferState {
    public:
        HAKCTransferState();

        void AddTransferActionValue(HAKCTransferAction &Action, Value *V);

        Value *GetTransferActionValue(HAKCTransferAction &Action);

    protected:
        std::map<HAKCTransferAction, Value *> ActionValues;
    };
} // namespace llvm::hakc

#endif //HAKCTRANSFERSTATE_H
