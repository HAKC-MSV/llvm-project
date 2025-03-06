//
// Created by al32163 on 02/27/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"

using namespace llvm;

hakc::HAKCPostTargetAction::HAKCPostTargetAction(Function *PostTargetAction, unsigned Idx,
                                                 StringRef Val) : HAKCTransferAction(PostTargetAction), Idx(Idx),
                                                                  Val(Val) {
}

unsigned hakc::HAKCPostTargetAction::GetIdx() const {
    return Idx;
}

StringRef hakc::HAKCPostTargetAction::GetVal() const {
    return Val;
}
