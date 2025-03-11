//
// Created by de29664 on 3/6/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

hakc::HAKCTransferAction::HAKCTransferAction(HAKCFunctionDefinition &HAKCActionFunction,
                                             StringRef Label) : Label(Label), HAKCActionFunction(HAKCActionFunction) {
}

StringRef hakc::HAKCTransferAction::GetLabel() const {
    return Label;
}

hakc::HAKCFunctionDefinition &hakc::HAKCTransferAction::GetHAKCActionFunction() const {
    return HAKCActionFunction;
}

bool hakc::HAKCTransferAction::operator<(const HAKCTransferAction &rhs) const {
    return HAKCActionFunction.GetName() < rhs.GetHAKCActionFunction().GetName();
}
