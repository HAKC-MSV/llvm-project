//
// Created by de29664 on 12/5/24.
//

#ifndef HAKCWRITER_H
#define HAKCWRITER_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/Value.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm::hakc {
class HAKCWriter {
public:
  HAKCWriter();
  ~HAKCWriter();

  raw_ostream &ostream() const;

protected:
  raw_ostream &os;
  static raw_fd_ostream *fd_os;
  static HAKCLogLevel ConfiguredLogLevel;
  static HAKCLogLevel TempLogLevel;
  static std::string log_path;

  void printDIType(const DIType *type, unsigned indents) const;

public:
  static void SetLogPath(std::string);
  static std::string GetLogPath();
  static HAKCLogLevel GetConfiguredLogLevel();
  static HAKCLogLevel GetTempLogLevel();

  static void SetConfiguredLogLevel(HAKCLogLevel log_level);
  static void SetTempLogLevel(HAKCLogLevel log_level);
  static raw_fd_ostream &GetFdOstream();

  static void CreateLog();

  HAKCWriter &operator<<(Value *V);

  HAKCWriter &operator<<(Value &V);

  HAKCWriter &operator<<(Use &U);

  HAKCWriter &operator<<(StringRef str);

  HAKCWriter &operator<<(unsigned int i);

  HAKCWriter &operator<<(unsigned long i);

  HAKCWriter &operator<<(ssize_t i);

  HAKCWriter &operator<<(bool b);

  HAKCWriter &operator<<(const std::string &str);

  HAKCWriter &operator<<(const char *s);

  HAKCWriter &operator<<(const Function &F);

  HAKCWriter &operator<<(const Module &M);

  HAKCWriter &operator<<(const Module *M);

  HAKCWriter &operator<<(const Type *Ty);

  HAKCWriter &operator<<(const Type &Ty);

  HAKCWriter &operator<<(const DINode *DiNode);

  HAKCWriter &operator<<(const DINode &DiNode);

  HAKCWriter &operator<<(const DIType *DiType);

  HAKCWriter &operator<<(const hakc::HAKCCompartment &Compartment);

  HAKCWriter &operator<<(const hakc::HAKCCompartmentDivision &Division);

  HAKCWriter &operator<<(const hakc::HAKCTypeInfo &TypeInfo);

  HAKCWriter &operator<<(enum HAKCAllocationTypeEnum AllocationType);

  HAKCWriter &operator<<(const ManagedHAKCPointerUse &HAKCPointerUse);

  HAKCWriter &operator<<(const HAKCPointerBase &ManagedPointer);

  HAKCWriter &operator<<(const HAKCPointerBaseP &ManagedPointer);

  HAKCWriter &operator<<(const HAKCFunctionInfo &HAKCFuncInfo);

  HAKCWriter &operator<<(const HAKCPreTransferAction &PreTransferAction);

  HAKCWriter &operator<<(const HAKCPostTargetAction &PostTargetAction);

  HAKCWriter &operator<<(const HAKCFunctionDefinition &FunctionDefinition);

  HAKCWriter &operator<<(const HAKCFunctionArgumentDefinition &Arg);

  HAKCWriter &operator<<(const enum HAKCFunctionArgumentUse ArgUse);

  HAKCWriter &operator<<(const HAKCTransferAction &TransferAction);
};
} // namespace llvm::hakc

#endif // HAKCWRITER_H
