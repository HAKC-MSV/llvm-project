//
// Created by de29664 on 3/6/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"

#include "../../../../../../../install/include/llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

hakc::HAKCTransferState::HAKCTransferState()
    : TransferredArgumentValues(), ActionArgumentLabels(),
      ActionArgumentLabelToValue(), PreTransferActionList(),
      PostTargetActionList() {}

void hakc::HAKCTransferState::AddTransferredArgument(Value *Arg) {
  TransferredArgumentValues.push_back(Arg);
}

SmallVectorImpl<Value *> &hakc::HAKCTransferState::GetTransferredArguments() {
  return TransferredArgumentValues;
}

void hakc::HAKCTransferState::AddPreTransferAction(
    hakc_pre_transfer_action_def_t Action) {
  PreTransferActionList.push_back(Action);
}

void hakc::HAKCTransferState::AddPostTargetAction(
    hakc_post_target_action_def_t Action) {
  PostTargetActionList.push_back(Action);
}

iterator_range<
    SmallVector<llvm::hakc::hakc_pre_transfer_action_def_t>::iterator>
hakc::HAKCTransferState::GetPreTransferActions() {
  return make_range(PreTransferActionList.begin(), PreTransferActionList.end());
}

iterator_range<SmallVector<llvm::hakc::hakc_post_target_action_def_t>::iterator>
hakc::HAKCTransferState::GetPostTargetActions() {
  return make_range(PostTargetActionList.begin(), PostTargetActionList.end());
}

void hakc::HAKCTransferState::AddPreTransferValue(
    llvm::hakc::hakc_label_ref_t Label, Value *Value) {
  // loop through all the PreTransferAction to match label and add value to the
  // action locally and globally
  for (auto PreTransferAction : GetPreTransferActions()) {
    // PreTransferAction->AddValue will only succeed if the label is already
    // present
    PreTransferAction->AddValue(Label, Value);
  }
}

void hakc::HAKCTransferState::AddPostTargetValue(hakc_label_ref_t Label,
                                                 Value *Value) {
  for (auto PostTargetAction : GetPostTargetActions()) {
    // PostTargetAction->AddValue will only succeed if the label is already
    // present
    PostTargetAction->AddValue(Label, Value);
  }
}

void hakc::HAKCTransferState::AddActionArgumentValue(hakc_label_ref_t Label,
                                                     Value *Value) {
  // TODO: maybe don't do this, its a bit weird
  AddPreTransferValue(Label, Value);
  AddPostTargetValue(Label, Value);
  ActionArgumentLabelToValue.insert({Label, Value});
  // AddActionArgumentLabel(Label);
}

Value *hakc::HAKCTransferState::GetActionArgumentValue(hakc_label_ref_t Label) {
  auto it = ActionArgumentLabelToValue.find(Label);
  if (it == ActionArgumentLabelToValue.end()) {
    return nullptr;
  }
  return it->second;
}

llvm::hakc::hakc_label_ref_t
hakc::HAKCTransferState::AddActionArgumentLabel(hakc_label_t Label) {
  for (auto it = ActionArgumentLabels.begin(); it != ActionArgumentLabels.end();
       it++) {
    if (*it == Label) {
      return StringRef(*it);
    }
  }
  ActionArgumentLabels.push_back(Label);
  return StringRef(ActionArgumentLabels.back());
}

SmallVector<llvm::hakc::hakc_label_t>
hakc::HAKCTransferState::GetActionArgumentLabels() {
  return ActionArgumentLabels;
}
