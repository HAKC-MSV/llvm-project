//
// Created by al32163 on 10/23/2024
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCAllocationSize.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

namespace llvm::hakc {
ConstantInt *HAKCSingleArgumentSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  Value *size = Val->getArgOperand(ArgNo);
  size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
  auto *CI = dyn_cast<ConstantInt>(size);
  return CI;
}

HAKCSingleArgumentSize::HAKCSingleArgumentSize(Function *AllocationFunction,
                                               StringRef Argument)
    : HAKCAllocationSize(AllocationFunction), ArgNo(0) {
  if (Argument.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCSingleArgumentSize Argument\n";
    throw std::exception();
  }

  Argument.getAsInteger(10, ArgNo);
}

HAKCMultiplyArgumentSize::HAKCMultiplyArgumentSize(Function *AllocationFunction,
                                                   StringRef Argument0,
                                                   StringRef Argument1)
    : HAKCAllocationSize(AllocationFunction), Arg0(0), Arg1(0) {
  if (Argument0.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCMultiplyArgumentSize Argument 0\n";
    throw std::exception();
  }
  if (Argument1.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCMultiplyArgumentSize Argument 1\n";
    throw std::exception();
  }

  Argument0.getAsInteger(10, Arg0);
  Argument1.getAsInteger(10, Arg1);
}

ConstantInt *HAKCMultiplyArgumentSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  auto *Arg0Size = Val->getArgOperand(Arg0);
  auto *Arg1Size = Val->getArgOperand(Arg1);
  if (isa<ConstantInt>(Arg0Size) && isa<ConstantInt>(Arg1Size)) {
    auto Arg1SizeVal = dyn_cast<ConstantInt>(Arg1Size)->getZExtValue();
    auto Arg0SizeVal = dyn_cast<ConstantInt>(Arg0Size)->getZExtValue();
    return irBuilder.getInt64(Arg0SizeVal * Arg1SizeVal);
  }
  CommonHAKCAnalysis::getWriter(true)
      << "Call " << *Val << " does not have ConstantInt arguments\n";
  throw std::exception();
}

HAKCAllocationSize::HAKCAllocationSize(Function *AllocationFunction)
    : AllocationFunction(AllocationFunction) {}

std::shared_ptr<HAKCAllocationSize>
HAKCAllocationSize::FromYaml(const hakc::HAKCYAMLAllocationType &YamlAllocation,
                             Module &M) {
  auto *F = M.getFunction(YamlAllocation.FunctionName);
  if (!F) {
    return nullptr;
  }

  switch (YamlAllocation.AllocationType) {
  default:
    CommonHAKCAnalysis::getWriter(true)
        << "HAKCAllocation Type " << YamlAllocation.AllocationType
        << " is not supported\n";
    throw std::exception();
  case hakc::SimpleArgumentSize:
    return std::make_shared<HAKCSingleArgumentSize>(
        F, YamlAllocation.Arguments[0]);
  case hakc::MultiplyTwoArguments:
    return std::make_shared<HAKCMultiplyArgumentSize>(
        F, YamlAllocation.Arguments[0], YamlAllocation.Arguments[1]);
  }
}

Function *HAKCAllocationSize::GetAllocationFunction() const {
  return AllocationFunction;
}
} // namespace llvm::hakc
