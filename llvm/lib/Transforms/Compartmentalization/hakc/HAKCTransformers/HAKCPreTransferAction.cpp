//
// Created by al32163 on 02/27/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

llvm::hakc::HAKCPreTransferAction::HAKCPreTransferAction(
    Function *PreTransferAction,
    std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel)
    : HAKCTransferAction(PreTransferAction, ArgToLabel) {}

void llvm::hakc::HAKCPreTransferAction::AddValue(hakc_label_ref_t Label,
                                                 Value *val) {
  LabelToValue[Label] = val;
}