//
// Created by al32163 on 10/23/2024
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCAllocationSize.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

#include <llvm/Support/AMDGPUMetadata.h>

namespace llvm::hakc {

HAKCSimpleArgumentSize::HAKCSimpleArgumentSize(Function *AllocationFunction, SmallVector<StringRef> ArgStrings)
    : HAKCAllocationSize(AllocationFunction) {
  if (ArgStrings.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCSimpleArgumentSize Argument\n";
    throw std::exception();
  }

  for (StringRef Arg : ArgStrings) {
      unsigned ArgNo;
      Arg.getAsInteger(10, ArgNo);
      Args.push_back(ArgNo);
  }
}
ConstantInt *HAKCSimpleArgumentSize::GetSize(CallInst *Val) {
  // TODO: double check this
  IRBuilder<> irBuilder(Val);
  Value* Accumulator = irBuilder.getInt64(0);
  for (unsigned Arg : Args) {
    Value *size = Val->getArgOperand(Arg);
    size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
    ConstantInt *CI = dyn_cast<ConstantInt>(size);
    // TODO: should this use an add instruction, or this casting thing
    // auto *AddInst = irBuilder.CreateAdd(Accumulator, CI);
    auto* AddInst = irBuilder.getInt64(dyn_cast<ConstantInt>(Accumulator)->getZExtValue() + dyn_cast<ConstantInt>(CI)->getZExtValue());
    Accumulator = AddInst;
  }
  return dyn_cast<ConstantInt>(Accumulator);
}

// e.g., - { name: neigh_parms_alloc, type: SimpleStaticSize, arguments: [ 144 ] }
HAKCSimpleStaticSize::HAKCSimpleStaticSize(Function *AllocationFunction,
                                               StringRef SizeString)
    : HAKCAllocationSize(AllocationFunction), StaticSize(0) {
  if (SizeString.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCSimpleStaticSize Argument\n";
    throw std::exception();
  }

  SizeString.getAsInteger(10, StaticSize);
}
ConstantInt *HAKCSimpleStaticSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  return irBuilder.getInt64(StaticSize);
}

// e.g., - { name: nlmsg_new, type: StaticPlusArgument, arguments: [ 64,0 ] }
// i.e., variable size struct = fixed size + argument size
HAKCStaticPlusArgument::HAKCStaticPlusArgument(Function *AllocationFunction, SmallVector<StringRef> ArgStrings)
    : HAKCAllocationSize(AllocationFunction), StaticSize(0) {
  if (ArgStrings.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCStaticPlusArgument Argument\n";
    throw std::exception();
  }

  ArgStrings[0].getAsInteger(10, StaticSize);
  for (unsigned i = 0; i < ArgStrings.size(); ++i){
    if (i == 0){ continue;};
    unsigned ArgNo;
    ArgStrings[i].getAsInteger(10, ArgNo);
    Args.push_back(ArgNo);
  }
}
ConstantInt *HAKCStaticPlusArgument::GetSize(CallInst *Val) {
    IRBuilder<> irBuilder(Val);
    Value* Accumulator = irBuilder.getInt64(StaticSize);
    for (unsigned Arg : Args) {
      Value *size = Val->getArgOperand(Arg);
      size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
      ConstantInt *CI = dyn_cast<ConstantInt>(size);
      // auto *AddInst = irBuilder.CreateAdd(Accumulator, CI);
      auto* AddInst = irBuilder.getInt64(dyn_cast<ConstantInt>(Accumulator)->getZExtValue() + dyn_cast<ConstantInt>(CI)->getZExtValue());
      Accumulator = AddInst;
    }
    return dyn_cast<ConstantInt>(Accumulator);
}

HAKCMultiplyArgumentSize::HAKCMultiplyArgumentSize(Function *AllocationFunction, SmallVector<StringRef> ArgStrings)
    : HAKCAllocationSize(AllocationFunction) {
  if (ArgStrings.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "ArgStrings in HAKCMultiplyArgumentSize is empty!\n";
    throw std::exception();
  }

  for (StringRef Arg : ArgStrings) {
    unsigned ArgNo;
    Arg.getAsInteger(10, ArgNo);
    Args.push_back(ArgNo);
  }
}

ConstantInt *HAKCMultiplyArgumentSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  Value* Accumulator = irBuilder.getInt64(0);
  for (unsigned Arg : Args) {
    Value *size = Val->getArgOperand(Arg);
    size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
    ConstantInt *CI = dyn_cast<ConstantInt>(size);
    auto* AddInst = irBuilder.getInt64(dyn_cast<ConstantInt>(Accumulator)->getZExtValue() * dyn_cast<ConstantInt>(CI)->getZExtValue());
    Accumulator = AddInst;
  }
  return dyn_cast<ConstantInt>(Accumulator);
}

// e.g.,  - { name: kmem_cache_alloc, type: ArgumentGEP, arguments: [ 0,1 ] }
// https://www.kernel.org/doc/html/next/core-api/mm-api.html#c.kmem_cache_alloc
// void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags)¶
// struct kmem_cache_args {
//  unsigned int align;
//  unsigned int useroffset;
//  unsigned int usersize;
//  unsigned int freeptr_offset;
//  bool use_freeptr_offset;
//  void (*ctor)(void *);
//};

HAKCArgumentGEP::HAKCArgumentGEP(Function *AllocationFunction,
                                 SmallVector<StringRef> ArgStrings)
    : HAKCAllocationSize(AllocationFunction) {
  if (ArgStrings.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCArgumentGEP Argument\n";
    throw std::exception();
  }

  for (StringRef Arg : ArgStrings) {
    unsigned ArgNo;
    Arg.getAsInteger(10, ArgNo);
    Args.push_back(ArgNo);
  }
}
ConstantInt *HAKCArgumentGEP::GetSize(CallInst *Val) {
  // TODO: Derrick - please check this GEP struct dereference logic

  // TODO: figure out size logic for multiple indices

  //            std::vector<Value*> indices;
  //            indices.push_back(ConstantInt::get(sizeTy, args[1], false));
  //            Value *gep = irBuilder.CreateGEP(sizeTy, call->getArgOperand(args[0]), indices);
  //            Value *size = irBuilder.CreateLoad(sizeTy, gep);
  //            return size;*/
  //
  //            return ConstantInt::get(Type::getInt64Ty(allocation->getContext()), 64, false);

  // IRBuilder<> irBuilder(Val);
  // Value *Arg0Val = Val->getArgOperand(Arg0);
  // auto *StructSize = irBuilder.CreateStructGEP(Arg0Val->getType(), Arg0Val, 2);  // idx2 corresponds to 'usersize'
  // auto *CI = dyn_cast<ConstantInt>(StructSize);
  // return CI;
}

HAKCAllocationSize::HAKCAllocationSize(Function *AllocationFunction)
    : AllocationFunction(AllocationFunction) {}

std::shared_ptr<HAKCAllocationSize>
HAKCAllocationSize::FromYaml(const hakc::HAKCYAMLAllocationType &YamlAllocation,
                             Module &M) {
  auto *F = M.getFunction(YamlAllocation.SymbolName);
  if (!F) {
    return nullptr;
  }
  switch (YamlAllocation.AllocationType) {
  default:
    CommonHAKCAnalysis::getWriter(true)
        << "HAKCAllocation Type " << YamlAllocation.AllocationType
        << " is not supported\n";
    throw std::exception();
    // TODO: should we use std::vector, or SmallVector? Also, std::string, or StringRef
    //    -> hakc yaml stores using strings, but we need stringref functionality
  case hakc::SimpleArgumentSize:
    return std::make_shared<HAKCSimpleArgumentSize>(
        F, YamlAllocation.Arguments);
  case hakc::SimpleStaticSize:
    return std::make_shared<HAKCSimpleStaticSize>(
        F, YamlAllocation.Arguments[0]);
  case hakc::StaticPlusArgument:
    return std::make_shared<HAKCStaticPlusArgument>(
        F, YamlAllocation.Arguments);
  case hakc::MultiplyTwoArguments:
    return std::make_shared<HAKCMultiplyArgumentSize>(
        F, YamlAllocation.Arguments);
  case hakc::ArgumentGEP:
    return std::make_shared<HAKCArgumentGEP>(
        F, YamlAllocation.Arguments);
}

}

Function *HAKCAllocationSize::GetAllocationFunction() const {
  return AllocationFunction;
}
} // namespace llvm::hakc
