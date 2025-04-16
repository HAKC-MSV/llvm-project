//
// Created by de29664 on 11/12/24.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <string>
#include <vector>

#include "llvm/AsmParser/Parser.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

typedef std::string HAKCYAMLStringType;

template <typename Ty> using HAKCYAMLSequence = std::vector<Ty>;

typedef HAKCYAMLSequence<HAKCYAMLStringType> HAKCYAMLStringSequenceType;

namespace llvm::hakc {
enum HAKCAllocationTypeEnum {
  InvalidAllocationType,
  SimpleArgumentSize,
  SimpleStaticSize,
  StaticPlusArgument,
  MultiplyTwoArguments,
  ArgumentGEP
};

enum HAKCPassModeTypeEnum {
  InvalidPassModeType,
  RunDataAccessGraphAnalysis,
  RunCompartmentalization,
  RunConfigAndExit
};

enum HAKCTestModeTypeEnum {
  InvalidTestModeType,
  TestModeDisabled,
  TestModeDefault,
  TestModeSuppliedDAG
};

struct HAKCYAMLFunctionDeclaration {
  HAKCYAMLStringType FunctionName;

  HAKCYAMLFunctionDeclaration() : FunctionName() {}
};

struct HAKCYAMLAllocationType : public HAKCYAMLFunctionDeclaration {
  HAKCAllocationTypeEnum AllocationType;
  HAKCYAMLStringSequenceType Arguments;

  HAKCYAMLAllocationType()
      : HAKCYAMLFunctionDeclaration(), AllocationType(InvalidAllocationType),
        Arguments() {}
};

struct HAKCYAMLFileType {
  HAKCYAMLStringType PathRoot;
  HAKCYAMLStringSequenceType Files;

  HAKCYAMLFileType() : PathRoot(), Files() {}

  void AddAllFiles(SmallVectorImpl<std::string> &Results) {
    for (auto &FilePath : Files) {
      SmallString<256> Filename;
      llvm::sys::path::append(Filename, PathRoot);
      llvm::sys::path::append(Filename, FilePath);

      Results.push_back(Filename.str().str());
    }
  }
};

// the function argument values (the parameter values set when called)
struct HAKCYAMLFunctionArgument {
  unsigned Idx;
  HAKCYAMLStringType TypeStr;
  HAKCFunctionArgumentUse ArgUse;

  HAKCYAMLFunctionArgument() : Idx(), TypeStr(), ArgUse(Other) {}

  Type *GetType(const HAKCTypeIdentifier &TypeIdentifier) const {
    return TypeIdentifier.GetTypeFromString(TypeStr);
  }
};

struct HAKCYAMLFunctionDefinition : public HAKCYAMLFunctionDeclaration {
  HAKCYAMLStringType ReturnType;
  HAKCYAMLSequence<HAKCYAMLFunctionArgument> Arguments;

  HAKCYAMLFunctionDefinition()
      : HAKCYAMLFunctionDeclaration(), ReturnType(), Arguments() {}

  bool IsValid() {
    bool Result = !ReturnType.empty() || !FunctionName.empty();
    if (Result) {
      auto ByIndex = [&](const HAKCYAMLFunctionArgument &Arg0,
                         const HAKCYAMLFunctionArgument &Arg1) {
        return Arg0.Idx < Arg1.Idx;
      };
      llvm::sort(Arguments, ByIndex);
      unsigned Previous = 0;
      for (auto &Arg : Arguments) {
        if (Arg.Idx > 0) {
          if (Arg.Idx != Previous + 1) {
            return false;
          }
          Previous = Arg.Idx;
        }
      }
    }
    return Result;
  }

  Function *GetFunction(const HAKCTypeIdentifier &TypeIdentifier) {
    if (!IsValid()) {
      return nullptr;
    }

    auto *ReturnTy = TypeIdentifier.GetTypeFromString(ReturnType);
    if (!ReturnTy) {
      return nullptr;
    }
    SmallVector<Type *> ArgTys;
    for (auto &Arg : Arguments) {
      auto *ArgTy = Arg.GetType(TypeIdentifier);
      if (!ArgTy) {
        return nullptr;
      }
      ArgTys.push_back(ArgTy);
    }

    auto *FType = FunctionType::get(ReturnTy, ArgTys, false);
    auto *F = dyn_cast<Function>(TypeIdentifier.GetModule()
                                     .getOrInsertFunction(FunctionName, FType)
                                     .getCallee());
    return F;
  }
};

struct HAKCYAMLCustomTransferType : public HAKCYAMLFunctionDefinition {
  HAKCYAMLStringType TransferObjectTypeName;

  HAKCYAMLCustomTransferType()
      : HAKCYAMLFunctionDefinition(), TransferObjectTypeName() {}
};

struct HAKCYAMLActionArgument {
  HAKCYAMLStringType Label;
  unsigned Idx;
};

struct HAKCYAMLActionType : public HAKCYAMLFunctionDeclaration {
  HAKCYAMLStringType Label;
  HAKCYAMLSequence<HAKCYAMLActionArgument> Arguments;

  HAKCYAMLActionType() : HAKCYAMLFunctionDeclaration(), Label(), Arguments() {}
};

struct HAKCYamlDatabaseConfig {
  HAKCYAMLStringType ServerURL;
  HAKCYAMLStringType GetCompartmentEndpoint;
  HAKCYAMLStringType GetDivisionEndpoint;
  HAKCYAMLStringType GetSymbolDivisionEndpoint;
  HAKCYAMLStringType GetValidTargetsEndpoint;
  unsigned ServerTimeout;
};

// TODO: need to figure out how we will specify symbols in the config
struct HAKCYAMLEpochPerms {
  epoch_perms_options_t perm;
  uint64_t offset;
};

struct HAKCYAMLEpoch {
  uint64_t epoch_id;
  uint64_t next_epoch_id;
  HAKCYAMLStringType entry_symbol;
  HAKCYAMLStringType exit_symbol;
  HAKCYAMLStringType type;
  HAKCYAMLSequence<HAKCYAMLEpochPerms> perms;
};

struct HAKCYamlConfig {
  HAKCTestModeTypeEnum TestMode;
  HAKCYAMLStringType Arch;
  HAKCYAMLStringType Platform;
  HAKCYAMLStringType DagAnalysisRootPath;
  HAKCYAMLFunctionDefinition CodeValidationFunction;
  HAKCYAMLFunctionDefinition DataValidationFunction;
  HAKCPassModeTypeEnum PassMode;
  HAKCYAMLSequence<HAKCYAMLFunctionDeclaration> SafeTransitionFunctions;
  HAKCYAMLSequence<HAKCYAMLFunctionDeclaration> IgnoredGlobals;
  HAKCYAMLStringSequenceType TransferFunctions;
  HAKCYAMLStringSequenceType PassDebugSymbols;
  HAKCYAMLStringSequenceType SeparateNamespacePathsList;
  HAKCYAMLStringSequenceType HAKCSourcePathsList;
  HAKCYAMLStringSequenceType IncludePathsList;
  HAKCYAMLStringSequenceType TransferFunctionCandidates;
  bool OutputAllDebugInfo;
  HAKCYamlDatabaseConfig DatabaseConfig;

  HAKCYAMLSequence<HAKCYAMLFunctionDeclaration> NoTransferFunctions;
  HAKCYAMLSequence<HAKCYAMLCustomTransferType> CustomTransferFunctions;
  HAKCYAMLSequence<HAKCYAMLFunctionDefinition>
      CompartmentalizationSupportFunctions;
  HAKCYAMLSequence<HAKCYAMLAllocationType> AllocationFunctions;
  HAKCYAMLSequence<HAKCYAMLFileType> SeparateNamespacePaths;
  HAKCYAMLSequence<HAKCYAMLFileType> HAKCSourcePaths;
  HAKCYAMLSequence<HAKCYAMLActionType> PreTargetActions;
  HAKCYAMLSequence<HAKCYAMLActionType> PostTargetActions;
  HAKCYAMLSequence<HAKCYAMLEpoch> Epochs;
  HAKCYAMLStringSequenceType IgnoredTypes;
  // TODO: revert to transfer type?
  HAKCYAMLFunctionDefinition DefaultCompartmentTransfer;
  HAKCYAMLFunctionDefinition SignWithDivision;
  HAKCYAMLFunctionDefinition PerCPUCompartmentTransfer;
};
} // namespace llvm::hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLCustomTransferType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDefinition)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFileType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionArgument)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLActionType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLActionArgument)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDeclaration)

static void YAMLFunctionDeclarationMapping(
    yaml::IO &io, hakc::HAKCYAMLFunctionDeclaration &FunctionDeclaration) {
  io.mapRequired("name", FunctionDeclaration.FunctionName);
}

template <> struct yaml::MappingTraits<hakc::HAKCYAMLFunctionDeclaration> {
  static void mapping(yaml::IO &io,
                      hakc::HAKCYAMLFunctionDeclaration &FunctionDefinition) {
    YAMLFunctionDeclarationMapping(io, FunctionDefinition);
  }
};

template <> struct yaml::ScalarEnumerationTraits<hakc::HAKCAllocationTypeEnum> {
  static void enumeration(IO &io, hakc::HAKCAllocationTypeEnum &value) {
    io.enumCase(value, "SimpleArgumentSize", hakc::SimpleArgumentSize);
    io.enumCase(value, "SimpleStaticSize", hakc::SimpleStaticSize);
    io.enumCase(value, "StaticPlusArgument", hakc::StaticPlusArgument);
    io.enumCase(value, "MultiplyTwoArguments", hakc::MultiplyTwoArguments);
    io.enumCase(value, "ArgumentGEP", hakc::ArgumentGEP);
  }
};

template <>
struct yaml::ScalarEnumerationTraits<hakc::HAKCFunctionArgumentUse> {
  static void enumeration(IO &io, hakc::HAKCFunctionArgumentUse &value) {
    for (auto &it : hakc::HAKCArgumentArgumentUseStringMap()) {
      io.enumCase(value, it.second, it.first);
    }
  }
};

template <> struct yaml::ScalarEnumerationTraits<hakc::HAKCPassModeTypeEnum> {
  static void enumeration(IO &io, hakc::HAKCPassModeTypeEnum &value) {
    io.enumCase(value, "RunDataAccessGraphAnalysis",
                hakc::RunDataAccessGraphAnalysis);
    io.enumCase(value, "RunCompartmentalization",
                hakc::RunCompartmentalization);
  }
};

template <> struct yaml::ScalarEnumerationTraits<hakc::HAKCTestModeTypeEnum> {
  static void enumeration(IO &io, hakc::HAKCTestModeTypeEnum &value) {
    io.enumCase(value, "InvalidTestModeType", hakc::InvalidTestModeType);
    io.enumCase(value, "TestModeDisabled", hakc::TestModeDisabled);
    io.enumCase(value, "TestModeDefault", hakc::TestModeDefault);
    io.enumCase(value, "TestModeSuppliedDAG", hakc::TestModeSuppliedDAG);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYAMLAllocationType> {
  static void mapping(yaml::IO &io,
                      hakc::HAKCYAMLAllocationType &AllocationType) {
    YAMLFunctionDeclarationMapping(io, AllocationType);
    io.mapRequired("type", AllocationType.AllocationType);
    io.mapRequired("arguments", AllocationType.Arguments);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYAMLActionArgument> {
  static void mapping(yaml::IO &io, hakc::HAKCYAMLActionArgument &ActionArg) {
    io.mapRequired("label", ActionArg.Label);
    io.mapRequired("idx", ActionArg.Idx);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYAMLActionType> {
  static void mapping(yaml::IO &io, hakc::HAKCYAMLActionType &ActionType) {
    YAMLFunctionDeclarationMapping(io, ActionType);
    io.mapOptional("label", ActionType.Label, "");
    io.mapOptional("arguments", ActionType.Arguments);
  }
};

static void
YAMLFunctionMapping(yaml::IO &io,
                    hakc::HAKCYAMLFunctionDefinition &FunctionDefinition) {
  YAMLFunctionDeclarationMapping(io, FunctionDefinition);
  io.mapRequired("return-type", FunctionDefinition.ReturnType);
  io.mapOptional("arguments", FunctionDefinition.Arguments);
}

template <> struct yaml::MappingTraits<hakc::HAKCYAMLFunctionDefinition> {
  static void mapping(yaml::IO &io,
                      hakc::HAKCYAMLFunctionDefinition &FunctionDefinition) {
    YAMLFunctionMapping(io, FunctionDefinition);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYAMLFileType> {
  static void mapping(yaml::IO &io, hakc::HAKCYAMLFileType &File) {
    io.mapRequired("path_root", File.PathRoot);
    io.mapRequired("file_names", File.Files);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYAMLFunctionArgument> {
  static void
  mapping(yaml::IO &io,
          hakc::HAKCYAMLFunctionArgument &HAKCYAMLFunctionArgument) {
    io.mapRequired("idx", HAKCYAMLFunctionArgument.Idx);
    io.mapRequired("type", HAKCYAMLFunctionArgument.TypeStr);
    io.mapRequired("arg-use", HAKCYAMLFunctionArgument.ArgUse);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYAMLCustomTransferType> {
  static void mapping(yaml::IO &io,
                      hakc::HAKCYAMLCustomTransferType &CustomTransfer) {
    YAMLFunctionMapping(io, CustomTransfer);
    io.mapRequired("transfer-obj-type", CustomTransfer.TransferObjectTypeName);
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYamlDatabaseConfig> {
  static void mapping(yaml::IO &Io, hakc::HAKCYamlDatabaseConfig &YamlConfig) {
    Io.mapRequired("server-url", YamlConfig.ServerURL);
    Io.mapOptional("server-timeout", YamlConfig.ServerTimeout, 1000);
    Io.mapOptional("get-compartment-by-id-endpoint",
                   YamlConfig.GetCompartmentEndpoint, "get-compartment-id");
    Io.mapOptional("get-division-by-id-endpoint",
                   YamlConfig.GetDivisionEndpoint, "get-division-id");
    Io.mapOptional("get-division-from-symbol-endpoint",
                   YamlConfig.GetSymbolDivisionEndpoint,
                   "get-division-from-symbol");
    Io.mapOptional("get-valid-targets-from-compartment-id-endpoint",
                   YamlConfig.GetValidTargetsEndpoint,
                   "get-valid-targets-from-compartment-id");
  }
};

template <> struct yaml::MappingTraits<hakc::HAKCYamlConfig> {
  static void mapping(yaml::IO &io, hakc::HAKCYamlConfig &YamlConfig) {
    io.mapOptional("TestMode", YamlConfig.TestMode, hakc::TestModeDisabled);

    if (YamlConfig.TestMode == hakc::InvalidTestModeType) {
      errs() << "Supplied TestMode is invalid\n";
      throw std::exception();
    } else if (YamlConfig.TestMode == hakc::TestModeDisabled) {
      io.mapRequired("Arch", YamlConfig.Arch);
      io.mapRequired("Platform", YamlConfig.Platform);
      io.mapRequired("DagAnalysisRootPath", YamlConfig.DagAnalysisRootPath);
      io.mapRequired("PassMode", YamlConfig.PassMode);
      io.mapRequired("CodeValidationFunction",
                     YamlConfig.CodeValidationFunction);
      io.mapRequired("DataValidationFunction",
                     YamlConfig.DataValidationFunction);
      io.mapRequired("DefaultCompartmentTransferFunction",
                     YamlConfig.DefaultCompartmentTransfer);
      io.mapRequired("SignWithDivisionFunction", YamlConfig.SignWithDivision);

      io.mapOptional("CompartmentalizationSupportFunctions",
                     YamlConfig.CompartmentalizationSupportFunctions);
      io.mapOptional("NoTransferFunctions", YamlConfig.NoTransferFunctions);
      io.mapOptional("SeparateNamespacePaths",
                     YamlConfig.SeparateNamespacePaths);
      io.mapOptional("HAKCSourcePaths", YamlConfig.HAKCSourcePaths);
      io.mapOptional("SafeTransitionFunctions",
                     YamlConfig.SafeTransitionFunctions);
      io.mapOptional("IgnoredTypes", YamlConfig.IgnoredTypes);
      io.mapOptional("IgnoredGlobals", YamlConfig.IgnoredGlobals);
      io.mapOptional("AllocationFunctions", YamlConfig.AllocationFunctions);
      io.mapOptional("OutputDebugInfo", YamlConfig.OutputAllDebugInfo, false);
      io.mapOptional("DebugOutputSymbols", YamlConfig.PassDebugSymbols);
      io.mapOptional("PerCPUCompartmentTransferFunction",
                     YamlConfig.PerCPUCompartmentTransfer);
      io.mapOptional("CustomTransferFunctions",
                     YamlConfig.CustomTransferFunctions);
      io.mapOptional("PreTargetActions", YamlConfig.PreTargetActions);
      io.mapOptional("PostTargetActions", YamlConfig.PostTargetActions);
      io.mapOptional("TransferFunctionCandidates",
                     YamlConfig.TransferFunctionCandidates);
      // io.mapOptional("Epochs", YamlConfig.Epochs);
      if (YamlConfig.PassMode == hakc::RunCompartmentalization) {
        io.mapRequired("Database", YamlConfig.DatabaseConfig);
      } else if (YamlConfig.PassMode == hakc::RunDataAccessGraphAnalysis) {
        io.mapOptional("Database", YamlConfig.DatabaseConfig);
      }
    } else if (YamlConfig.TestMode == hakc::TestModeDefault) {
      io.mapRequired("Arch", YamlConfig.Arch);
      io.mapRequired("Platform", YamlConfig.Platform);
      io.mapOptional("DagAnalysisRootPath", YamlConfig.DagAnalysisRootPath);
      io.mapOptional("PassMode", YamlConfig.PassMode);
      io.mapOptional("CodeValidationFunction",
                     YamlConfig.CodeValidationFunction);
      io.mapOptional("DataValidationFunction",
                     YamlConfig.DataValidationFunction);
      io.mapOptional("DefaultCompartmentTransferFunction",
                     YamlConfig.DefaultCompartmentTransfer);
      io.mapOptional("SignWithDivisionFunction", YamlConfig.SignWithDivision);

      io.mapOptional("CompartmentalizationSupportFunctions",
                     YamlConfig.CompartmentalizationSupportFunctions);
      io.mapOptional("NoTransferFunctions", YamlConfig.NoTransferFunctions);
      io.mapOptional("SeparateNamespacePathList",
                     YamlConfig.SeparateNamespacePaths);
      io.mapOptional("HAKCSourcePathList", YamlConfig.HAKCSourcePaths);
      io.mapOptional("SafeTransitionFunctions",
                     YamlConfig.SafeTransitionFunctions);
      io.mapOptional("IgnoredTypes", YamlConfig.IgnoredTypes);
      io.mapOptional("IgnoredGlobals", YamlConfig.IgnoredGlobals);
      io.mapOptional("AllocationFunctions", YamlConfig.AllocationFunctions);
      io.mapOptional("OutputDebugInfo", YamlConfig.OutputAllDebugInfo, false);
      io.mapOptional("DebugOutputSymbols", YamlConfig.PassDebugSymbols);
      io.mapOptional("PerCPUCompartmentTransferFunction",
                     YamlConfig.PerCPUCompartmentTransfer);
      io.mapOptional("CustomTransferFunctions",
                     YamlConfig.CustomTransferFunctions);
      io.mapOptional("Database", YamlConfig.DatabaseConfig);
      io.mapOptional("PreTargetActions", YamlConfig.PreTargetActions);
      io.mapOptional("PostTargetActions", YamlConfig.PostTargetActions);
      io.mapOptional("TransferFunctionCandidates",
                     YamlConfig.TransferFunctionCandidates);
    } else if (YamlConfig.TestMode == hakc::TestModeSuppliedDAG) {
      // TODO
    }
  }
};

#endif // HAKC_HAKCYAML_H
