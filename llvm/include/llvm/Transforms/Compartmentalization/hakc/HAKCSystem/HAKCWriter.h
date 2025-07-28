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

#include "llvm/IR/Verifier.h"

using namespace llvm;

namespace llvm::hakc {

class HAKCWriter {
protected:
  // raw_ostream *os;
  std::shared_ptr<raw_ostream> os;
  std::shared_ptr<raw_fd_ostream> _os;
  std::string log_path;
  // HAKCLogLevel ConfiguredLogLevel;
  // HAKCLogLevel TempLogLevel;
  std::error_code EC;

  void CreateLog() {
    os = std::make_shared<raw_fd_ostream>(log_path, EC);
    // return std::shared_ptr<llvm::raw_ostream>(
    //   &llvm::errs(),  // non-owning raw pointer
    //   [](llvm::raw_ostream *) {
    //     // no-op deleter to prevent delete on singleton
    //   });
    // os = new raw_fd_ostream(log_path, EC);
    // os = _os.get();
    // os = std::move(_os.get());
    // os = std::move(_os);
  }

public:
  explicit HAKCWriter() : os(), _os(nullptr), log_path("") {
    errs() << "CREATING HAKC WRITER0\n";
    os = std::shared_ptr<raw_ostream>(
      &errs(),  // non-owning raw pointer
      [](raw_ostream *) {
        // no-op deleter to prevent delete on singleton
      });
  }
  explicit HAKCWriter(StringRef log_path) : os(), _os(nullptr), log_path(log_path) {
    errs() << "CREATING HAKC WRITER1\n";
    CreateLog();
  }
  ~HAKCWriter() {
    errs() << "DESTROYING HAKC WRITER\n";
  }

  // this could be corrupting the os
  // bool verifyFunction(Function *F) { return llvm::verifyFunction(*F, os); }
  bool verifyFunction(Function *F) { return llvm::verifyFunction(*F, nullptr); }

  StringRef GetLogPath() const { return log_path; }

  raw_ostream &GetOS() { return *os; }

  void printDIType(const DIType *type, unsigned indents) const;

  HAKCWriter &operator<<(Value *V);

  HAKCWriter &operator<<(Use &U);

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

  // template <typename T>
  // HAKCWriter &operator<<(const T &value) {
  //   *os << value;
  //   // operator<<(value);
  //   // os << value;
  //   // os.GetOS() << value;
  //   return *this;
  // }

  // template <typename T>
  // HAKCWriter &operator<<(const T &value) {
  //   // if (os) {
  //   //   *os << value;
  //   // }
  //   // *this << value;
  //   // this->operator<<(value);
  //   // return *this;
  //   return this->operator<<(value);
  // }
  //
  // template <typename T>
  // HAKCWriter &operator<<(const T *value) {
  //   // *this << *value;
  //   // this->operator<<(*value);
  //   // return *this;
  //   return this->operator<<(*value);
  // }


};

class HAKCLogger {
protected:
  // std::vector<HAKCWriter> HAKCStreams;
  HAKCLogLevel ConfiguredLogLevel;
  HAKCLogLevel TempLogLevel;

public:
  std::vector<std::shared_ptr<HAKCWriter>> HAKCStreams;
  explicit HAKCLogger()
      : HAKCStreams(), ConfiguredLogLevel(Verbose), TempLogLevel(Verbose) {
    errs() << "CREATING HAKC LOGGER\n";
    // HAKCWriter writer;
    auto writer = std::make_shared<HAKCWriter>();
    HAKCStreams.push_back(writer);
  }
  ~HAKCLogger() {
    errs() << "DESTROYING HAKC LOGGER\n";
    HAKCStreams.clear();
  }

  // Add a stream after construction
  void addStream(StringRef log_path) {
    // HAKCWriter writer(log_path);
    auto writer = std::make_shared<HAKCWriter>(log_path);
    HAKCStreams.push_back(writer);
  }

  HAKCLogLevel GetConfiguredLogLevel() const { return ConfiguredLogLevel; }

  HAKCLogLevel GetTempLogLevel() const { return TempLogLevel; }

  void SetConfiguredLogLevel(HAKCLogLevel log_level) {
    ConfiguredLogLevel = log_level;
  }
  void SetTempLogLevel(HAKCLogLevel log_level) { TempLogLevel = log_level; }

  // Output generic types
  // template <typename T> HAKCLogger &operator<<(const T &value) {
  //   // Any non disabled value of TempLogLevel is greater than 0
  //   if (TempLogLevel && (TempLogLevel >= ConfiguredLogLevel)) {
  //     for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
  //       // gets further than: stream << value; but still segfaults
  //       // *stream.get() << value;
  //       // ::operator<<(writer, value);
  //       // stream->operator<<(value);
  //       // using ::operator<<; // bring in overloads
  //       // operator<<(stream.get(), value);  // ADL dispatch
  //       // stream->operator<<("\n");
  //
  //       // *stream << value;
  //       stream->operator<<(value);
  //       // *stream = stream->operator<<(value);
  //       // *stream = (*stream << value);
  //     }
  //   }
  //   return *this;
  // }
  //
  // template <typename T> HAKCLogger &operator<<(const T *value) {
  //   // pointer overloaded required for correct value overload (e.g., *F needs to call F)
  //   // Any non disabled value of TempLogLevel is greater than 0
  //   if (TempLogLevel && (TempLogLevel >= ConfiguredLogLevel)) {
  //     for (std::shared_ptr<HAKCWriter> &stream : HAKCStreams) {
  //       stream->operator<<(*value);
  //     }
  //   }
  //   return *this;
  // }

  // Output manipulators like std::endl
  HAKCLogger& operator<<(raw_ostream& (*manip)(raw_ostream&)) {
    for (auto &stream : HAKCStreams) {
      if (TempLogLevel && (TempLogLevel >= ConfiguredLogLevel)) {
        *stream.get() << manip;
      }
    }
    return *this;
  }

  bool verifyFunction(Function *F) {
    bool passed = true;
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
      passed = stream->verifyFunction(F);
    }
    return passed;
  }

  HAKCLogger &operator<<(Value *V){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
      stream->operator<<(V);
    }
    return *this;
  };

  HAKCLogger &operator<<(Use &U){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
      stream->operator<<(U);
    }
    return *this;
  };

  HAKCLogger &operator<<(Value &V){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(V);
    }
    return *this;
  };

  HAKCLogger &operator<<(StringRef str){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(str);
    }
    return *this;
  };

  HAKCLogger &operator<<(unsigned int i){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(i);
    }
    return *this;
  };

  HAKCLogger &operator<<(unsigned long i){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(i);
    }
    return *this;
  };

  HAKCLogger &operator<<(ssize_t i){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(i);
    }
    return *this;
  };

  HAKCLogger &operator<<(bool b){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(b);
    }
    return *this;
  };

  HAKCLogger &operator<<(const std::string &str){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(str);
    }
    return *this;
  };

  HAKCLogger &operator<<(const char *s){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(s);
    }
    return *this;
  };

  HAKCLogger &operator<<(const Function &F){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(F);
    }
    return *this;
  };

  HAKCLogger &operator<<(const Module &M){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(M);
    }
    return *this;
  };

  HAKCLogger &operator<<(const Module *M){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(M);
    }
    return *this;
  };

  HAKCLogger &operator<<(const Type *Ty){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(Ty);
    }
    return *this;
  };

  HAKCLogger &operator<<(const Type &Ty){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(Ty);
    }
    return *this;
  };

  HAKCLogger &operator<<(const DINode *DiNode){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(DiNode);
    }
    return *this;
  };

  HAKCLogger &operator<<(const DINode &DiNode){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(DiNode);
    }
    return *this;
  };

  HAKCLogger &operator<<(const DIType *DiType){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(DiType);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCCompartment &Compartment){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(Compartment);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCCompartmentDivision &Division){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(Division);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCTypeInfo &TypeInfo){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(TypeInfo);
    }
    return *this;
  };

  HAKCLogger &operator<<(const enum HAKCAllocationTypeEnum AllocationType){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(AllocationType);
    }
    return *this;
  };

  HAKCLogger &operator<<(const ManagedHAKCPointerUse &HAKCPointerUse){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(HAKCPointerUse);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCPointerBase &ManagedPointer){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(ManagedPointer);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCPointerBaseP &ManagedPointer){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(ManagedPointer);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCFunctionInfo &HAKCFuncInfo){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(HAKCFuncInfo);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCPreTransferAction &PreTransferAction){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(PreTransferAction);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCPostTargetAction &PostTargetAction){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(PostTargetAction);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCFunctionDefinition &FunctionDefinition){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(FunctionDefinition);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCFunctionArgumentDefinition &Arg){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(Arg);
    }
    return *this;
  };

  HAKCLogger &operator<<(const enum HAKCFunctionArgumentUse ArgUse){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(ArgUse);
    }
    return *this;
  };

  HAKCLogger &operator<<(const HAKCTransferAction &TransferAction){
    for (std::shared_ptr<HAKCWriter>& stream : HAKCStreams) {
    stream->operator<<(TransferAction);
    }
    return *this;
  };


};

} // namespace llvm::hakc

#endif // HAKCWRITER_H
