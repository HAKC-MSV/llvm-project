//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains; TODO
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERSTATE_H
#define HAKCTRANSFERSTATE_H

#include "llvm/IR/Value.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

using namespace llvm;

namespace llvm::hakc {
class HAKCTransferState {
public:
  HAKCTransferState(const HAKCCompartmentDivision &TargetDivision,
                    HAKCPointerBase &HAKCPointer);

  void AddTransferActionValue(const HAKCTransferAction &Action, Value *V);

  Value *GetTransferActionValue(const HAKCTransferAction &Action);

  Value *GetLabeledValue(StringRef Label) const;

  const HAKCCompartmentDivision &GetDivision() const;

  HAKCPointerBase &GetManagedPointer();

  HAKC_Access_Token GetAccessToken() const;

  operator bool() const;

protected:
  std::map<HAKCTransferAction, Value *> ActionValues;
  const HAKCCompartmentDivision TargetDivision;
  HAKCPointerBase &HAKCPointer;
};
} // namespace llvm::hakc

#endif // HAKCTRANSFERSTATE_H
