//
// Created by de29664 on 3/6/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

hakc::HAKCTransferState::HAKCTransferState(const HAKCCompartmentDivision &TargetDivision) : ActionValues(),
    TargetDivision(TargetDivision) {
}

Value *hakc::HAKCTransferState::GetLabeledValue(StringRef Label) const {
    for (auto &it: ActionValues) {
        if (it.first.GetLabel() == Label) {
            return it.second;
        }
    }
    return nullptr;
}

void hakc::HAKCTransferState::AddTransferActionValue(const HAKCTransferAction &Action, Value *V) {
    ActionValues[Action] = V;
}

Value *hakc::HAKCTransferState::GetTransferActionValue(const HAKCTransferAction &Action) {
    const auto it = ActionValues.find(Action);
    if (it == ActionValues.end()) {
        return nullptr;
    }
    return it->second;
}

const hakc::HAKCCompartmentDivision &hakc::HAKCTransferState::GetDivision() const {
    return TargetDivision;
}
