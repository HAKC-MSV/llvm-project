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
  SmallVector<Value*> Parameters;
};

class HAKCSimpleArgumentSize : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCSimpleArgumentSize(Function *AllocationFunction, std::vector<std::string> ArgStrings);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  std::vector<unsigned> Args;
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
  HAKCStaticPlusArgument(Function *AllocationFunction, std::vector<StringRef> ArgStrings);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  unsigned StaticSize;
  std::vector<unsigned> Args;
};


class HAKCMultiplyArgumentSize : public HAKCAllocationSize {
public:
  HAKCMultiplyArgumentSize(Function *AllocationFunction, std::vector<StringRef> Args);
  ConstantInt *GetSize(CallInst *Val) override;

protected:
  std::vector<unsigned> Args;
};


class HAKCArgumentGEP : public HAKCAllocationSize {
  friend class HAKCAllocationSize;

public:
  HAKCArgumentGEP(Function *AllocationFunction, std::vector<StringRef> Args);

  ConstantInt *GetSize(CallInst *Val) override;

protected:
  std::vector<unsigned> Args;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCALLOCATIONSIZE_H
