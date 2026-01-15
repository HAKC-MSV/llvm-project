//
// Created by al32163 on 02/27/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

hakc::HAKCPreTransferAction::HAKCPreTransferAction(
    HAKCFunctionDefinition &HAKCActionFunction, const StringRef Label)
    : HAKCTransferAction(HAKCActionFunction, Label) {}
