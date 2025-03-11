//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERSTATE_H
#define HAKCTRANSFERSTATE_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCTransferState {
    public:
        HAKCTransferState();

        void AddTransferredArgument(Value *Arg);

        void GetTransferredArguments(SmallVectorImpl<Value *> &Results) const;

        void AddTransferActionValue(HAKCTransferAction &Action, Value *V);

        Value *GetTransferActionValue(HAKCTransferAction &Action);

    protected:
        SmallVector<Value *> TransferredArgumentValues;
        std::map<HAKCTransferAction, Value *> ActionValues;
    };
} // namespace llvm::hakc

#endif //HAKCTRANSFERSTATE_H
