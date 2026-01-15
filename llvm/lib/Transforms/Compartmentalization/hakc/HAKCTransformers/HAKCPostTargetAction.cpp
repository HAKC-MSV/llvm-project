//
// Created by al32163 on 02/27/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"

using namespace llvm;

hakc::HAKCPostTargetAction::HAKCPostTargetAction(HAKCFunctionDefinition &HAKCActionFunction, const StringRef Label,
    const SmallVector<HAKCActionArgument> &Arguments) : HAKCTransferAction(HAKCActionFunction, Label, Arguments) {
}
