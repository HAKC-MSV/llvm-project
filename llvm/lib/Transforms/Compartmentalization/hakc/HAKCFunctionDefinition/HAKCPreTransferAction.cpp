//
// Created by al32163 on 02/27/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCPreTransferAction.h"

using namespace llvm;

namespace llvm::hakc {
    HAKCPreTransferAction::HAKCPreTransferAction(Function *PreTransferAction, StringRef Label) : HAKCFunctionDefinition(PreTransferAction), Label(Label){};

    StringRef HAKCPreTransferAction::GetLabel() const {
        return Label;
    }

} // hakc
