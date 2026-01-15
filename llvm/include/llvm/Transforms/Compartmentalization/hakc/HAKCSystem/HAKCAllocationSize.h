//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains code which finds the allocation size of a memory
/// allocation function call, e.g., to malloc
///
//===----------------------------------------------------------------------===//
//
// Created by al32163 on 10/23/2024
//

#ifndef HAKC_HAKCALLOCATIONSIZE_H
#define HAKC_HAKCALLOCATIONSIZE_H

#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include <memory>

using namespace llvm;

namespace llvm::hakc {
class HAKCAllocationSize {
public:
  virtual ~HAKCAllocationSize() = default;

  static std::shared_ptr<HAKCAllocationSize>
  FromYaml(const HAKCYAMLAllocationType &YamlLine, const Module &M);

  virtual ConstantInt *GetSize(CallInst *val) = 0;

  Function *GetAllocationFunction() const;

protected:
  explicit HAKCAllocationSize(Function *AllocationFunction);

  HAKCAllocationSize() = default;

  Function *AllocationFunction;
};

class HAKCSimpleArgumentSize : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCSimpleArgumentSize(Function *AllocationFunction, StringRef ArgNoString);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned ArgNo;
};

class HAKCSimpleStaticSize : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCSimpleStaticSize(Function *AllocationFunction,
                       StringRef StaticSizeString);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned StaticSize;
};

class HAKCStaticPlusArgument : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCStaticPlusArgument(Function *AllocationFunction,
                         StringRef StaticSizeString, StringRef ArgNoString);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned StaticSize;
  unsigned ArgNo;
};

class HAKCMultiplyArgumentSize : public HAKCAllocationSize {
public:
  // eg alloc n args of size s
  HAKCMultiplyArgumentSize(Function *AllocationFunction, StringRef NObjsString,
                           StringRef ArgSizePerObjString);
  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned NObjs;
  unsigned ArgSizePerObj;
};

class HAKCArgumentGEP : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  // abc->index0->index1->index2
  // ArgAccessNo is the arg of the struct
  HAKCArgumentGEP(Function *AllocationFunction, StringRef ArgAccessNoString,
                  ArrayRef<StringRef> IndicesString);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned ArgAccessNo;
  SmallVector<unsigned> Indices;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCALLOCATIONSIZE_H
