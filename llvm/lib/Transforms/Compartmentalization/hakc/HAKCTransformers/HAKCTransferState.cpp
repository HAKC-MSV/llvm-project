//
// Created by de29664 on 3/6/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

hakc::HAKCTransferState::HAKCTransferState()
  : TransferredArgumentValues() {
}

void hakc::HAKCTransferState::AddTransferredArgument(Value *Arg) {
  TransferredArgumentValues.push_back(Arg);
}

void hakc::HAKCTransferState::GetTransferredArguments(SmallVectorImpl<Value *> &Results) const {
  Results.append(TransferredArgumentValues.begin(), TransferredArgumentValues.end());
}
