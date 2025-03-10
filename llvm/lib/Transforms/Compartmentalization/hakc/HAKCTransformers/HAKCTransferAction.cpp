//
// Created by de29664 on 3/6/25.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

llvm::hakc::HAKCTransferAction::HAKCTransferAction(
    Function *F, std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel)
    : ActionFunction(F), ArgToLabel(ArgToLabel) {}

llvm::Function *llvm::hakc::HAKCTransferAction::GetActionFunction() const {
    return ActionFunction;
}

llvm::hakc::hakc_label_ref_t
llvm::hakc::HAKCTransferAction::GetActionLabel(hakc_arg_t arg) const {
  auto it = ArgToLabel.find(arg);
  if (it == ArgToLabel.end()) {
    // return StringRef("");
    throw std::exception();
  }
  return StringRef(it->second);
}

const std::map<llvm::hakc::hakc_arg_t, llvm::hakc::hakc_label_ref_t> &
llvm::hakc::HAKCTransferAction::GetArgToLabel() {
  return ArgToLabel;
}