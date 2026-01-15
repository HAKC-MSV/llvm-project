//
// Created by de29664 on 3/6/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

hakc::HAKCTransferState::HAKCTransferState(
    const HAKCCompartmentDivision &TargetDivision, HAKCPointerBase &HAKCPointer)
    : TargetDivision(TargetDivision), HAKCPointer(HAKCPointer) {}

Value *hakc::HAKCTransferState::GetLabeledValue(const StringRef Label) const {
  for (const auto &[key, val] : ActionValues) {
    if (key.GetLabel() == Label) {
      return val;
    }
  }
  return nullptr;
}

void hakc::HAKCTransferState::AddTransferActionValue(
    const HAKCTransferAction &Action, Value *V) {
  ActionValues[Action] = V;
}

Value *hakc::HAKCTransferState::GetTransferActionValue(
    const HAKCTransferAction &Action) {
  const auto it = ActionValues.find(Action);
  if (it == ActionValues.end()) {
    return nullptr;
  }
  return it->second;
}

const hakc::HAKCCompartmentDivision &
hakc::HAKCTransferState::GetDivision() const {
  return TargetDivision;
}

hakc::HAKCPointerBase &hakc::HAKCTransferState::GetManagedPointer() const {
  return HAKCPointer;
}

hakc::HAKC_Access_Token hakc::HAKCTransferState::GetAccessToken() const {
  return TargetDivision.GetAccessToken();
}

hakc::HAKCTransferState::operator bool() const {
  if (HAKCPointer.GetType() && HAKCPointer.GetType()->IsIgnoredType()) {
    return false;
  }
  return true;
}
