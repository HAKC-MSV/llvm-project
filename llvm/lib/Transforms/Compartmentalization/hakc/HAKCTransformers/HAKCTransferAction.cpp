//
// Created by de29664 on 3/6/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

llvm::hakc::HAKCTransferAction::HAKCTransferAction(Function *F) : ActionFunction(F) {
}

llvm::Function *llvm::hakc::HAKCTransferAction::GetActionFunction() const {
    return ActionFunction;
}
