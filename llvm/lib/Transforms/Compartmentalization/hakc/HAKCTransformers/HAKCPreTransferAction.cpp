//
// Created by al32163 on 02/27/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

llvm::hakc::HAKCPreTransferAction::HAKCPreTransferAction(Function *PreTransferAction,
                                                         StringRef Label) : HAKCTransferAction(PreTransferAction),
                                                                            Label(Label) {
}

llvm::StringRef llvm::hakc::HAKCPreTransferAction::GetLabel() const {
    return Label;
}
