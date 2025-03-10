//
// Created by al32163 on 02/27/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"

using namespace llvm;

hakc::HAKCPostTargetAction::HAKCPostTargetAction(
    Function *PostTargetAction,
    std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel)
    : HAKCTransferAction(PostTargetAction, ArgToLabel) {}

void llvm::hakc::HAKCPostTargetAction::AddValue(hakc_label_ref_t Label,
                                                Value *val) {
  // auto it = LabelToValue.find(Label);
  LabelToValue[Label] = val;
}