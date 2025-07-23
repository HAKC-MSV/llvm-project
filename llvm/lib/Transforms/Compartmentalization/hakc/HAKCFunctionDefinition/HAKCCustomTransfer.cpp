//
// Created by de29664 on 6/21/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include <map>

hakc::HAKCCustomTransfer::HAKCCustomTransfer(
    Function *CustomFunction, const HAKCTypeP &TargetType,
    SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args)
    : HAKCFunctionDefinition(CustomFunction, Args), TypeToTransfer(TargetType) {
}

hakc::HAKCTypeP hakc::HAKCCustomTransfer::GetTargetType() const {
  return TypeToTransfer;
}

Instruction *hakc::HAKCCustomTransfer::CreateTransfer(
    IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
    Value *Pointer, Value *ObjectSize, bool IsData) {
  SmallVector<Value *> CallArgs;
  std::map<HAKCFunctionArgumentUse, Value *> ArgMap = {
      {SignedPtr, Pointer},
      {Comp, CompartmentDivision.GetHAKCCompartment().GetCompartmentID()},
      {Div, CompartmentDivision.GetDivisionID()},
      {Size, ObjectSize},
      {IsCode, ConstantInt::getBool(HAKCIRBuilder.getContext(), !IsData)}};
  for (auto &Arg : Args()) {
    auto it = ArgMap.find(Arg.ArgUse);
    if (it == ArgMap.end()) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Unsupported argument use in index " << Arg.Idx
          << " for custom transfer " << GetName() << "\n";
      throw std::exception();
    }
    CallArgs.push_back(it->second);
  }

  auto *TransferCall = HAKCIRBuilder.CreateCall(GetFunction(), CallArgs);
  CommonHAKCAnalysis::getWriter(Debug)
      << "Created Custom Transfer " << *TransferCall << " for " << *Pointer
      << "\n";
  return TransferCall;
}

Instruction *hakc::HAKCCustomTransfer::CreateTransfer(
    IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
    hakc::HAKCPointerBase &HAKCPointer, Value *Size, bool IsData) {
  return CreateTransfer(HAKCIRBuilder, CompartmentDivision,
                        HAKCPointer.GetBaseDefinition(), Size, IsData);
}

Instruction *hakc::HAKCCustomTransfer::CreateTransferWithCasts(
    IRBuilder<> &HAKCIRBuilder, HAKCCompartmentDivision &CompartmentDivision,
    hakc::HAKCPointerBase &HAKCPointer, Value *Size, HAKCTypeP srcTy,
    HAKCTypeP dstTy, bool IsData) {
  auto *BitcastArgForTransferCall = HAKCIRBuilder.CreateBitCast(
      HAKCPointer.GetBaseDefinition(), dstTy->GetLLVMType());
  auto *TransferCall = CreateTransfer(HAKCIRBuilder, CompartmentDivision,
                                      BitcastArgForTransferCall, Size, IsData);
  auto *BitcastArgForTargetCall =
      HAKCIRBuilder.CreateBitCast(TransferCall, srcTy->GetLLVMType());
  auto *Result = dyn_cast<Instruction>(BitcastArgForTargetCall);
  return Result;
}
