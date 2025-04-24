//
// Created by al32163 on 10/23/2024
//

#ifndef HAKC_HAKCALLOCATIONSIZE_H
#define HAKC_HAKCALLOCATIONSIZE_H

#include "llvm/IR/Constants.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include <memory>

using namespace llvm;

namespace llvm::hakc {
class HAKCAllocationSize {
public:
  virtual ~HAKCAllocationSize() = default;

  static std::shared_ptr<HAKCAllocationSize>
  FromYaml(const HAKCYAMLAllocationType &YamlLine, Module &M);

  virtual ConstantInt *GetSize(CallInst *val) = 0;

  Function *GetAllocationFunction() const;

protected:
  explicit HAKCAllocationSize(Function *AllocationFunction);

  HAKCAllocationSize() = default;

  Function *AllocationFunction;
};

class HAKCSingleArgumentSize : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCSingleArgumentSize(Function *AllocationFunction, StringRef Argument);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned ArgNo;
};

class HAKCSimpleStaticSize : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCSimpleStaticSize(Function *AllocationFunction, StringRef SizeString);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned StaticSize;
};

class HAKCStaticPlusArgument : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCStaticPlusArgument(Function *AllocationFunction, StringRef SizeString, StringRef Argument);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned StaticSize;
  unsigned ArgNo;
};


class HAKCMultiplyArgumentSize : public HAKCAllocationSize {
public:
  HAKCMultiplyArgumentSize(Function *AllocationFunction, StringRef Argument0,
                           StringRef Argument1);
  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned Arg0;
  unsigned Arg1;
};


class HAKCArgumentGEP : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCArgumentGEP(Function *AllocationFunction, StringRef Argument0, StringRef Argument1);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned Arg0;
  unsigned Arg1;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCALLOCATIONSIZE_H
