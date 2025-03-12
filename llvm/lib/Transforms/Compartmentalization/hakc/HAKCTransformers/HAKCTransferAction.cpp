//
// Created by de29664 on 3/6/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

hakc::HAKCTransferAction::HAKCTransferAction(HAKCFunctionDefinition &HAKCActionFunction,
                                             StringRef Label, SmallVector<HAKCActionArgument> Arguments) : Label(Label),
    HAKCActionFunction(HAKCActionFunction), Arguments(Arguments) {
}

hakc::HAKCTransferAction::HAKCTransferAction(HAKCFunctionDefinition &HAKCActionFunction,
                                             StringRef Label) : Label(Label), HAKCActionFunction(HAKCActionFunction),
                                                                Arguments() {
}

StringRef hakc::HAKCTransferAction::GetLabel() const {
    return Label;
}

iterator_range<SmallVector<hakc::HAKCActionArgument>::iterator> hakc::HAKCTransferAction::GetArguments() {
    return make_range(Arguments.begin(), Arguments.end());
}

hakc::HAKCFunctionDefinition &hakc::HAKCTransferAction::GetHAKCActionFunction() const {
    return HAKCActionFunction;
}

bool hakc::HAKCTransferAction::operator<(const HAKCTransferAction &rhs) const {
    return HAKCActionFunction.GetName() < rhs.GetHAKCActionFunction().GetName();
}
