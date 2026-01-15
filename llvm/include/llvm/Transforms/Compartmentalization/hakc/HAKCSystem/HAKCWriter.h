//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the HAKC Writer, which is a class that can print to
/// either the console or a specified file.
///
//===----------------------------------------------------------------------===//
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

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm::hakc {

class HAKCWriter {
protected:
  std::shared_ptr<raw_ostream> os;
  std::string log_path;
  HAKCLogLevel ConfiguredLogLevel;
  std::error_code EC;
  bool disabled;

  void CreateLog() { os = std::make_shared<raw_fd_ostream>(log_path, EC); }

public:
  explicit HAKCWriter(HAKCLogLevel log_level)
      : log_path(""), ConfiguredLogLevel(log_level), disabled(false) {
    os = std::shared_ptr<raw_ostream>(&errs(), // non-owning raw pointer
                                      [](raw_ostream *) {
                                        // no-op deleter to prevent delete on
                                        // singleton
                                      });
  }
  explicit HAKCWriter(StringRef log_path, HAKCLogLevel log_level)
      : log_path(log_path), ConfiguredLogLevel(log_level), disabled(false) {
    CreateLog();
  }

  ~HAKCWriter() {}

  void Disable() { disabled = true; }

  void Enable() { disabled = false; }

  bool IsDisabled() const { return disabled; }

  void SetConfiguredLogLevel(HAKCLogLevel log_level) {
    ConfiguredLogLevel = log_level;
  }

  HAKCLogLevel GetConfiguredLogLevel() const { return ConfiguredLogLevel; }

  StringRef GetLogPath() const { return log_path; }

  raw_ostream &GetOS() const { return *os; }

  explicit operator bool() const {
    if (EC) {
      return false;
    }
    return true;
  }

  void printDIType(const DIType *type, unsigned indents) const;

  HAKCWriter &operator<<(Value *V);

  HAKCWriter &operator<<(const Use &U);

  HAKCWriter &operator<<(User *User);

  HAKCWriter &operator<<(Value &V);

  HAKCWriter &operator<<(StringRef str);

  HAKCWriter &operator<<(unsigned int i);

  HAKCWriter &operator<<(unsigned long i);

  HAKCWriter &operator<<(double d);

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

  HAKCWriter &operator<<(const HAKCCompartment &Compartment);

  HAKCWriter &operator<<(const HAKCCompartmentDivision &Division);

  HAKCWriter &operator<<(const HAKCTypeInfo &TypeInfo);

  HAKCWriter &operator<<(const enum HAKCAllocationTypeEnum AllocationType);

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

  HAKCWriter &operator<<(const DbgVariableRecord &DVR);

  HAKCWriter &operator<<(const DbgVariableIntrinsic &DVI);

  HAKCWriter &operator<<(const DILocalVariable &DLV);

  HAKCWriter &operator<<(const DILocation &DL);
};

} // namespace llvm::hakc

#endif // HAKCWRITER_H
