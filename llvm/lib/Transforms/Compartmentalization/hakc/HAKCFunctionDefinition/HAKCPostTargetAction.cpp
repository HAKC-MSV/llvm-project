//
// Created by al32163 on 02/27/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCPostTargetAction.h"

using namespace llvm;

namespace llvm::hakc {
    HAKCPostTargetAction::HAKCPostTargetAction(Function *PostTargetAction, unsigned Idx, StringRef Val) : HAKCFunctionDefinition(PostTargetAction), Idx(Idx), Val(Val){};

    unsigned HAKCPostTargetAction::GetIdx() const {
      return Idx;
    }

    StringRef HAKCPostTargetAction::GetVal() const {
        return Val;
    }

} // hakc
