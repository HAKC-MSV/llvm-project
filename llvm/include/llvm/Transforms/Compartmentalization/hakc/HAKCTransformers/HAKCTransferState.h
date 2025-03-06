//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERSTATE_H
#define HAKCTRANSFERSTATE_H

#include "llvm/IR/Value.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"
#include <map>

using namespace llvm;

namespace llvm::hakc {
    class HAKCTransferState {
    public:
        HAKCTransferState();

        void AddTransferredArgument(Value *Arg);

        SmallVectorImpl<Value *> &GetTransferredArguments();

        void AddPretransferAction(hakc_pre_transfer_action_def_t Action, Value *Value);

        Value *GetPreactionValue(hakc_pre_transfer_action_def_t Action);

    protected:
        SmallVector<Value *> TransferredArguments;
        std::map<hakc_pre_transfer_action_def_t, Value *> PreActionSteps;
    };
} // namespace llvm::hakc

#endif //HAKCTRANSFERSTATE_H
