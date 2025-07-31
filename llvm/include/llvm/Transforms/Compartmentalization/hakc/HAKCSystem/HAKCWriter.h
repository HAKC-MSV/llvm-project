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
  void CreateLog() { os = std::make_shared<raw_fd_ostream>(log_path, EC); }

public:
  std::error_code EC;
  explicit HAKCWriter(HAKCLogLevel log_level)
      : os(), log_path(""), ConfiguredLogLevel(log_level) {
    // errs() << "CREATING HAKC WRITER0\n";
    // allow using shared ptr to errs() but force it to not try to destroy errs()
    os = std::shared_ptr<raw_ostream>(&errs(), // non-owning raw pointer
                                      [](raw_ostream *) {
                                        // no-op deleter to prevent delete on
                                        // singleton
                                      });
  }
  explicit HAKCWriter(StringRef log_path, HAKCLogLevel log_level)
      : os(), log_path(log_path), ConfiguredLogLevel(log_level) {
    // errs() << "CREATING HAKC WRITER1\n";
    CreateLog();
  }
  ~HAKCWriter() {
    // errs() << "DESTROYING HAKC WRITER\n";
  }

  void SetConfiguredLogLevel(HAKCLogLevel log_level) {
    ConfiguredLogLevel = log_level;
  }

  HAKCLogLevel GetConfiguredLogLevel() { return ConfiguredLogLevel; }

  StringRef GetLogPath() const { return log_path; }

  raw_ostream &GetOS() { return *os; }

  void printDIType(const DIType *type, unsigned indents) const;

  HAKCWriter &operator<<(Value *V);

  HAKCWriter &operator<<(Use &U);

  HAKCWriter &operator<<(User *User);

  HAKCWriter &operator<<(Value &V);

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

class HAKCLogger {
protected:
  std::vector<std::shared_ptr<HAKCWriter>> HAKCStreams;
  HAKCLogLevel LogLevel;
  bool disabled;

public:
  explicit HAKCLogger(HAKCLogLevel log_level)
      : HAKCStreams(), LogLevel(Verbose), disabled(log_level == Disabled) {
    // errs() << "CREATING HAKC LOGGER\n";
    auto writer = std::make_shared<HAKCWriter>(log_level);
    HAKCStreams.push_back(writer);
  }
  ~HAKCLogger() {
    // errs() << "DESTROYING HAKC LOGGER\n";
    HAKCStreams.clear();
  }

  // Add a stream after construction
  void addStream(StringRef log_path, HAKCLogLevel log_level) {
    if (log_level == Disabled) {
      disabled = true;
    }
    else {
      auto writer = std::make_shared<HAKCWriter>(log_path, log_level);
      if (writer->EC) {
        errs() << "Failed to create log file: " << writer->EC.message() << "\n";
        return;
      }
      HAKCStreams.push_back(writer);
    }
  }

  HAKCLogLevel GetLogLevel() const { return LogLevel; }
  void SetLogLevel(HAKCLogLevel log_level) {
    LogLevel = log_level;
    disabled = (log_level == Disabled);
  }

  void SetConsoleConfiguredLogLevels(HAKCLogLevel log_level) {
    HAKCStreams[0]->SetConfiguredLogLevel(log_level);
  }

  void SetFileConfiguredLogLevel(HAKCLogLevel log_level) {
    for (unsigned long i = 0; i < HAKCStreams.size(); ++i) {
      if (i != 0) {
        HAKCStreams[i]->SetConfiguredLogLevel(log_level);
      }
    }
  }

  HAKCLogger &operator<<(Value *V) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(V);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(Use &U) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(U);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(Value &V) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(V);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(StringRef str) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(str);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(unsigned int i) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(i);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(unsigned long i) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(i);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(ssize_t i) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(i);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(bool b) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(b);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const std::string &str) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(str);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const char *s) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(s);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const Function &F) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(F);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const Module &M) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(M);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const Module *M) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(M);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const Type *Ty) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(Ty);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const Type &Ty) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(Ty);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const DINode *DiNode) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(DiNode);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const DINode &DiNode) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(DiNode);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const DIType *DiType) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(DiType);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCCompartment &Compartment) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(Compartment);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCCompartmentDivision &Division) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(Division);
      }
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCTypeInfo &TypeInfo) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(TypeInfo);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const enum HAKCAllocationTypeEnum AllocationType) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(AllocationType);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const ManagedHAKCPointerUse &HAKCPointerUse) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(HAKCPointerUse);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCPointerBase &ManagedPointer) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(ManagedPointer);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCPointerBaseP &ManagedPointer) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(ManagedPointer);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCFunctionInfo &HAKCFuncInfo) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(HAKCFuncInfo);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCPreTransferAction &PreTransferAction) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(PreTransferAction);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCPostTargetAction &PostTargetAction) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(PostTargetAction);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCFunctionDefinition &FunctionDefinition) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(FunctionDefinition);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCFunctionArgumentDefinition &Arg) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(Arg);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const enum HAKCFunctionArgumentUse ArgUse) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(ArgUse);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const HAKCTransferAction &TransferAction) {
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(TransferAction);
      }
    }
    return *this;
  }



  HAKCLogger &operator<<(const DbgVariableRecord &DVR){
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(DVR);
      }
    }
    return *this;
  }
  HAKCLogger &operator<<(const DbgVariableIntrinsic &DVI){
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(DVI);
      }
    }
    return *this;
  }

  HAKCLogger &operator<<(const DILocalVariable &DLV){
    for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
      if (!disabled && (LogLevel >= stream->GetConfiguredLogLevel())) {
        stream->operator<<(DLV);
      }
    }
    return *this;
  }
};

} // namespace llvm::hakc

#endif // HAKCWRITER_H
