//
// Created by de29664 on 3/6/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"

hakc::HAKCTransferState::HAKCTransferState() : TransferredArguments(), PreActionSteps() {
}

void hakc::HAKCTransferState::AddTransferredArgument(Value *Arg) {
    TransferredArguments.push_back(Arg);
}

SmallVectorImpl<Value *> &hakc::HAKCTransferState::GetTransferredArguments() {
    return TransferredArguments;
}

void hakc::HAKCTransferState::AddPretransferAction(hakc_pre_transfer_action_def_t Action, Value *Value) {
    PreActionSteps[Action] = Value;
}
