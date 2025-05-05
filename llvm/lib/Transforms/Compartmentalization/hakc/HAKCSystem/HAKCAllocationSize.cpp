//
// Created by al32163 on 10/23/2024
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCAllocationSize.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

namespace llvm::hakc {

HAKCSimpleArgumentSize::HAKCSimpleArgumentSize(Function *AllocationFunction, StringRef ArgNoString)
    : HAKCAllocationSize(AllocationFunction), ArgNo(0) {
  if (ArgNoString.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCSimpleArgumentSize Argument\n";
    throw std::exception();
  }

  ArgNoString.getAsInteger(10, ArgNo);

}
ConstantInt *HAKCSimpleArgumentSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  Value *size = Val->getArgOperand(ArgNo);
  size = irBuilder.CreateZExtOrBitCast(size, irBuilder.getInt64Ty());
  ConstantInt *CI = dyn_cast<ConstantInt>(size);
  return CI;
}

// e.g., - { name: neigh_parms_alloc, type: SimpleStaticSize, arguments: [ 144 ] }
HAKCSimpleStaticSize::HAKCSimpleStaticSize(Function *AllocationFunction, StringRef StaticSizeString)
    : HAKCAllocationSize(AllocationFunction), StaticSize(0) {
  if (StaticSizeString.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCSimpleStaticSize Argument\n";
    throw std::exception();
  }

  StaticSizeString.getAsInteger(10, StaticSize);
}
ConstantInt *HAKCSimpleStaticSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  return irBuilder.getInt64(StaticSize);
}

// e.g., - { name: nlmsg_new, type: StaticPlusArgument, arguments: [ 64,0 ] }
// i.e., variable size struct = fixed size + argument size
HAKCStaticPlusArgument::HAKCStaticPlusArgument(Function *AllocationFunction, StringRef StaticSizeString, StringRef ArgNoString)
    : HAKCAllocationSize(AllocationFunction), StaticSize(0) {
  if (StaticSizeString.empty() || ArgNoString.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCStaticPlusArgument arguments\n";
    throw std::exception();
  }

  StaticSizeString.getAsInteger(10, StaticSize);
  ArgNoString.getAsInteger(10, ArgNo);
}
ConstantInt *HAKCStaticPlusArgument::GetSize(CallInst *Val) {
    IRBuilder<> irBuilder(Val);
    Value *ArgSizeVal = Val->getArgOperand(ArgNo);
    ArgSizeVal = irBuilder.CreateZExtOrBitCast(ArgSizeVal, irBuilder.getInt64Ty());
    Value *StaticSizeVal = irBuilder.getInt64(StaticSize);
    ConstantInt* CI = irBuilder.getInt64(dyn_cast<ConstantInt>(ArgSizeVal)->getZExtValue() + dyn_cast<ConstantInt>(StaticSizeVal)->getZExtValue());
    return CI;
}

HAKCMultiplyArgumentSize::HAKCMultiplyArgumentSize(Function *AllocationFunction, StringRef NObjsString, StringRef ArgSizePerObjString)
    : HAKCAllocationSize(AllocationFunction), NObjs(0), ArgSizePerObj(0) {
  if (NObjsString.empty() || ArgSizePerObjString.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCMultiplyArgumentSize arguments!\n";
    throw std::exception();
  }
  NObjsString.getAsInteger(10, NObjs);
  ArgSizePerObjString.getAsInteger(10, ArgSizePerObj);
}

ConstantInt *HAKCMultiplyArgumentSize::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);

  Value *NObjsVal = Val->getArgOperand(NObjs);
  NObjsVal = irBuilder.CreateZExtOrBitCast(NObjsVal, irBuilder.getInt64Ty());
  Value *ArgSizePerObjVal = Val->getArgOperand(ArgSizePerObj);
  ArgSizePerObjVal = irBuilder.CreateZExtOrBitCast(ArgSizePerObjVal, irBuilder.getInt64Ty());
  ConstantInt *CI = irBuilder.getInt64(dyn_cast<ConstantInt>(NObjsVal)->getZExtValue() * dyn_cast<ConstantInt>(ArgSizePerObjVal)->getZExtValue());
  return CI;
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

HAKCArgumentGEP::HAKCArgumentGEP(Function *AllocationFunction, StringRef ArgAccessNoString, ArrayRef<StringRef> IndicesString)
    : HAKCAllocationSize(AllocationFunction) {
  if (ArgAccessNoString.empty() || IndicesString.empty()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid HAKCArgumentGEP arguments!\n";
    throw std::exception();
  }
  ArgAccessNoString.getAsInteger(10, ArgAccessNo);
  for (auto IndexString : IndicesString) {
    unsigned Index;
    IndexString.getAsInteger(10, Index);
    Indices.push_back(Index);
  }
}
ConstantInt *HAKCArgumentGEP::GetSize(CallInst *Val) {
  IRBuilder<> irBuilder(Val);
  SmallVector<Value*> IndicesVal; // IncidesVal must be of type Value* to be used in CreateGEP
  for (auto Index : Indices) {
    IndicesVal.push_back(ConstantInt::get(irBuilder.getInt64Ty(), Index)); //
  }
  Value* GEP = irBuilder.CreateGEP(irBuilder.getInt64Ty(), Val->getArgOperand(ArgAccessNo), IndicesVal);
  Value* Size = irBuilder.CreateLoad(irBuilder.getInt64Ty(), GEP);
  return dyn_cast<ConstantInt>(Size);
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
  case hakc::SimpleArgumentSize:
    return std::make_shared<HAKCSimpleArgumentSize>(
        F, YamlAllocation.Arguments[0]);
  case hakc::SimpleStaticSize:
    return std::make_shared<HAKCSimpleStaticSize>(
        F, YamlAllocation.Arguments[0]);
  case hakc::StaticPlusArgument:
    return std::make_shared<HAKCStaticPlusArgument>(
        F, YamlAllocation.Arguments[0], YamlAllocation.Arguments[1]);
  case hakc::MultiplyTwoArguments:
    return std::make_shared<HAKCMultiplyArgumentSize>(
        F, YamlAllocation.Arguments[0], YamlAllocation.Arguments[1]);
  case hakc::ArgumentGEP:
    // cast from std::vector<std::string> to ArrayRef<StringRef>, skipping the first element (automatic casting is not working here, for some reason)
    SmallVector<StringRef> YamlAllocationArguments;
    std::transform(YamlAllocation.Arguments.begin() + 1, YamlAllocation.Arguments.end(), YamlAllocationArguments.begin(), [](const std::string &ArgString){return StringRef(ArgString);});

    return std::make_shared<HAKCArgumentGEP>(
        F, YamlAllocation.Arguments[0], YamlAllocationArguments);
}

}

Function *HAKCAllocationSize::GetAllocationFunction() const {
  return AllocationFunction;
}
} // namespace llvm::hakc
