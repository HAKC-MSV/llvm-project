//
// Created by al32163 on 02/27/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"

using namespace llvm;

hakc::HAKCPostTargetAction::HAKCPostTargetAction(Function *PostTargetAction,
                                                 std::map<unsigned, StringRef> &
                                                 PretransferReferences) : HAKCTransferAction(PostTargetAction),
                                                                          PretransferReferences(PretransferReferences) {
}
