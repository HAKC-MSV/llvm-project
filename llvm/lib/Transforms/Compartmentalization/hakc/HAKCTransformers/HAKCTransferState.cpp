//
// Created by de29664 on 3/6/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

hakc::HAKCTransferState::HAKCTransferState() : ActionValues() {
}

void hakc::HAKCTransferState::AddTransferActionValue(HAKCTransferAction &Action, Value *V) {
    ActionValues[Action] = V;
}

Value *hakc::HAKCTransferState::GetTransferActionValue(HAKCTransferAction &Action) {
    auto it = ActionValues.find(Action);
    if (it == ActionValues.end()) {
        return nullptr;
    }
    return it->second;
}
//
// hakc::HAKCTransferAction* GetTransferActions() {
//   return
// }
