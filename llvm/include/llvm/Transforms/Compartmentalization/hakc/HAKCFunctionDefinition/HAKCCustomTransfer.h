//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the information needed to create a custom transfer function
/// i.e., allowing data to be transferred correctly outside a boundary.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 6/21/23.
//

#ifndef HAKC_HAKCCUSTOMTRANSFER_H
#define HAKC_HAKCCUSTOMTRANSFER_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

using namespace llvm;

namespace llvm::hakc {
class HAKCCustomTransfer : public HAKCFunctionDefinition {
public:
  HAKCCustomTransfer(Function *CustomFunction, const HAKCTypeP &TargetType,
                     SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args);

  ~HAKCCustomTransfer() override = default;

  HAKCTypeP GetTargetType() const;

  virtual Instruction *
  CreateTransfer(IRBuilder<> &HAKCIRBuilder,
                 HAKCCompartmentDivision &CompartmentDivision,
                 hakc::HAKCPointerBase &HAKCPointer, Value *Size, bool IsData);

  virtual Instruction *
  CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder,
                          HAKCCompartmentDivision &CompartmentDivision,
                          hakc::HAKCPointerBase &HAKCPointer, Value *Size,
                          HAKCTypeP srcTy, HAKCTypeP dstTy, bool IsData);

protected:
  HAKCTypeP TypeToTransfer;

  virtual Instruction *
  CreateTransfer(IRBuilder<> &HAKCIRBuilder,
                 HAKCCompartmentDivision &CompartmentDivision, Value *Pointer,
                 Value *Size, bool IsData);
};

typedef std::shared_ptr<HAKCCustomTransfer> custom_transfer_def_t;
} // namespace llvm::hakc

#endif // HAKC_HAKCCUSTOMTRANSFER_H
