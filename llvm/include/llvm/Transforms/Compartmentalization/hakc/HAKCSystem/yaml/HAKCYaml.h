//
// Created by de29664 on 11/12/24.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <vector>
#include <string>

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/Module.h"

typedef std::string HAKCYAMLStringType;

template<typename Ty>
using HAKCYAMLSequence = std::vector<Ty>;

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
        RunCompartmentalization
    };

    enum HAKCTestModeTypeEnum {
        InvalidTestModeType,
        TestModeDisabled,
        TestModeDefault,
        TestModeSuppliedDAG
    };

    struct HAKCYAMLAllocationType {
        HAKCYAMLStringType FunctionName;
        HAKCAllocationTypeEnum AllocationType;
        HAKCYAMLStringSequenceType Arguments;

        HAKCYAMLAllocationType() : FunctionName(), AllocationType(InvalidAllocationType), Arguments() {
        }
    };

    struct HAKCYAMLFileType {
        HAKCYAMLStringType PathRoot;
        HAKCYAMLStringSequenceType Files;

        HAKCYAMLFileType() : PathRoot(), Files() {
        }
    };

    struct HAKCYAMLFunctionParameterType {
      unsigned idx;
      HAKCYAMLStringType name;

      HAKCYAMLFunctionParameterType() : idx(), name() {
      }
    };

    // the function argument values (the parameter values set when called)
    struct HAKCYAMLFunctionArgumentType {
      unsigned idx;
      HAKCYAMLStringType label;
      HAKCYAMLFunctionArgumentType() : idx(), label() {
      }
    };

    struct HAKCYAMLCFunctionDefinitionType {
      HAKCYAMLStringType Type;
      HAKCYAMLStringType Name;
      unsigned PointerIdx;
      unsigned SizeIdx;
      unsigned CompartmentIdx;
      unsigned DivisionIdx;
      unsigned IsCodeIdx;

      HAKCYAMLCFunctionDefinitionType()
          : Type(), Name(), PointerIdx(HAKCTransferFunction::MissingIdx),
            SizeIdx(HAKCTransferFunction::MissingIdx), CompartmentIdx(HAKCTransferFunction::MissingIdx),
            DivisionIdx(HAKCTransferFunction::MissingIdx), IsCodeIdx(HAKCTransferFunction::MissingIdx) {
      }

      bool IsValid() const { return (!Type.empty() || !Name.empty()); }

      Function *GetFunction(HAKCTypeIdentifier &TypeIdentifier) {
        if (!IsValid()) {
          return nullptr;
        }
        auto *FoundTypes = TypeIdentifier.GetTypeFromString(Type);
        if (!FoundTypes) {
          return nullptr;
        }
        auto *FType = dyn_cast<llvm::FunctionType>(FoundTypes);
        if (!FType) {
          // TODO: what is the right ostream to use here?
          errs() << "Failed to parse Function " << Name << " with Type " << Type << "\n";
          return nullptr;
        }
        auto *F = dyn_cast<Function>(TypeIdentifier.GetModule().getOrInsertFunction(Name, FType).getCallee());
        errs() << "inserted function: " << *F << "\n" ;
        return F;
      }
    };

    struct HAKCYAMLFunctionDefinitionType {
        // updated function definition (llvm only types, currently)
        HAKCYAMLStringType Type;
        HAKCYAMLStringType Name;
        HAKCYAMLSequence<HAKCYAMLFunctionParameterType> Parameters;
        std::map<HAKCYAMLStringType, uint64_t> ParameterNameToIndex;
        SMDiagnostic Err;

        HAKCYAMLFunctionDefinitionType()
            : Type(), Name(), Parameters(), ParameterNameToIndex(), Err() {
            for (auto param: Parameters) {
              ParameterNameToIndex[param.name] = param.idx;
            }
        }

        bool IsValid() const { return (!Type.empty() || !Name.empty()); }

        Function *GetFunction(HAKCTypeIdentifier &TypeIdentifier) {
            if (!IsValid()) {
                return nullptr;
            }
            errs() << "Parsing function: " << Name << " with type [" << Type << "]: ";
            auto *FoundTypes = parseType(Type, Err, TypeIdentifier.GetModule());
            // Cast from Type* to FunctionType* needed to properly insert the function
            auto *FType = dyn_cast<llvm::FunctionType>(FoundTypes);
            if (!FType) {
              // TODO: what is the right ostream to use here?
              errs() << "Failed to parse Function " << Name << " with Type " << Type << ": " << Err.getMessage() << "\n";
              return nullptr;
            }

            auto F = dyn_cast<Function>(TypeIdentifier.GetModule()
                                          .getOrInsertFunction(Name, FType)
                                          .getCallee());
            errs() << "inserted function: " << *F << "\n" ;
            return F;
        }
    };

    struct HAKCYAMLTransferType : public HAKCYAMLCFunctionDefinitionType {
        HAKCYAMLTransferType() : HAKCYAMLCFunctionDefinitionType() {}
    };

    struct HAKCYAMLCustomTransferType : public HAKCYAMLTransferType {
        HAKCYAMLStringType TypeName;

        HAKCYAMLCustomTransferType() : HAKCYAMLTransferType(), TypeName() {}
    };

    struct HAKCYAMLActionType {
      HAKCYAMLStringType Type;
      HAKCYAMLStringType Name;
      HAKCYAMLSequence<HAKCYAMLFunctionArgumentType> Arguments;
      // TODO: expand with more options, e.g., call? load? store?
      HAKCYAMLActionType() : Type(), Name() {
      }
    };

    struct HAKCYAMLPreTransferActionsType {
        HAKCYAMLSequence<HAKCYAMLActionType> Actions;
        HAKCYAMLPreTransferActionsType(): Actions() {
      }
    };

    struct HAKCYAMLPostTargetActionType {
        HAKCYAMLSequence<HAKCYAMLActionType> Actions;
        HAKCYAMLPostTargetActionType() : Actions() {
        }
    };

    struct HAKCYamlDatabaseConfig {
        HAKCYAMLStringType ServerURL;
        HAKCYAMLStringType GetCompartmentEndpoint;
        HAKCYAMLStringType GetDivisionEndpoint;
        HAKCYAMLStringType GetSymbolDivisionEndpoint;
        HAKCYAMLStringType GetValidTargetsEndpoint;
        unsigned ServerTimeout;
    };

    struct HAKCYamlConfig {
        HAKCTestModeTypeEnum TestMode;
        HAKCYAMLStringType Arch;
        HAKCYAMLStringType Platform;
        HAKCYAMLStringType SourcePath;
        HAKCYAMLStringType BuildPath;
        HAKCYAMLStringType DagAnalysisRootPath;
        HAKCYAMLFunctionDefinitionType CodeValidationFunction;
        HAKCYAMLFunctionDefinitionType DataValidationFunction;
        // TODO: need multiple?
        // HAKCYAMLSequence<HAKCYAMLFunctionDefinitionType> CodeValidationFunction;
        // HAKCYAMLSequence<HAKCYAMLFunctionDefinitionType> DataValidationFunction;
        HAKCPassModeTypeEnum PassMode;
        HAKCYAMLStringSequenceType NoTransferFunctions;
        HAKCYAMLStringSequenceType SafeTransitionFunctions;
        HAKCYAMLStringSequenceType IgnoredGlobals;
        HAKCYAMLStringSequenceType TransferFunctions;
        HAKCYAMLStringSequenceType PassDebugSymbols;
        HAKCYAMLStringSequenceType SeparateNamespacePathsList;
        HAKCYAMLStringSequenceType HAKCSourcePathsList;
        HAKCYAMLStringSequenceType IgnoredTypesList;
        HAKCYAMLStringSequenceType IncludePathsList;
        bool OutputAllDebugInfo;
        HAKCYamlDatabaseConfig DatabaseConfig;

        // HAKCYAMLSequence <HAKCYAMLFunctionDefinitionType> NoTransferFunctions;
        HAKCYAMLSequence<HAKCYAMLCustomTransferType> CustomTransferFunctions;
        HAKCYAMLSequence<HAKCYAMLFunctionDefinitionType> CompartmentalizationSupportFunctions;
        HAKCYAMLSequence<HAKCYAMLAllocationType> AllocationFunctions;
        HAKCYAMLSequence<HAKCYAMLFileType> SeparateNamespacePaths;
        HAKCYAMLSequence<HAKCYAMLFileType> HAKCSourcePaths;
        HAKCYAMLSequence<HAKCYAMLActionType> PreTransferActions;
        HAKCYAMLSequence<HAKCYAMLActionType> PostTargetActions;
        HAKCYAMLStringSequenceType IgnoredTypes;
      // TODO: revert to transfer type?
        HAKCYAMLFunctionDefinitionType DefaultCompartmentTransfer;
        HAKCYAMLFunctionDefinitionType SignWithDivision;
        HAKCYAMLFunctionDefinitionType PerCPUCompartmentTransfer;
    };
} // hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLTransferType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLCustomTransferType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDefinitionType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFileType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLPreTransferActionsType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLPostTargetActionType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionArgumentType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionParameterType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLActionType)

inline void ValidateHAKCDefinition(hakc::HAKCYAMLCFunctionDefinitionType &Definition) {
#define FieldCheck(Def, Field) if (Def.Field != hakc::HAKCTransferFunction::MissingIdx && Def.Field > hakc::HAKCTransferFunction::MaxArgIndex) { errs() << "Invalid Index Value for " << #Field << " : " << Def.Field << "\n"; throw std::exception(); }
    FieldCheck(Definition, CompartmentIdx);
    FieldCheck(Definition, DivisionIdx);
    FieldCheck(Definition, PointerIdx);
    FieldCheck(Definition, IsCodeIdx);
    FieldCheck(Definition, SizeIdx);
#undef FieldCheck
}

template<>
struct yaml::ScalarEnumerationTraits<hakc::HAKCAllocationTypeEnum> {
    static void enumeration(IO &io, hakc::HAKCAllocationTypeEnum &value) {
        io.enumCase(value, "SimpleArgumentSize", hakc::SimpleArgumentSize);
        io.enumCase(value, "SimpleStaticSize", hakc::SimpleStaticSize);
        io.enumCase(value, "StaticPlusArgument", hakc::StaticPlusArgument);
        io.enumCase(value, "MultiplyTwoArguments", hakc::MultiplyTwoArguments);
        io.enumCase(value, "ArgumentGEP", hakc::ArgumentGEP);
    }
};

template<>
struct yaml::ScalarEnumerationTraits<hakc::HAKCPassModeTypeEnum> {
    static void enumeration(IO &io, hakc::HAKCPassModeTypeEnum &value) {
        io.enumCase(value, "RunDataAccessGraphAnalysis", hakc::RunDataAccessGraphAnalysis);
        io.enumCase(value, "RunCompartmentalization", hakc::RunCompartmentalization);
    }
};

template<>
struct yaml::ScalarEnumerationTraits<hakc::HAKCTestModeTypeEnum> {
    static void enumeration(IO &io, hakc::HAKCTestModeTypeEnum &value) {
        io.enumCase(value, "InvalidTestModeType", hakc::InvalidTestModeType);
        io.enumCase(value, "TestModeDisabled", hakc::TestModeDisabled);
        io.enumCase(value, "TestModeDefault", hakc::TestModeDefault);
        io.enumCase(value, "TestModeSuppliedDAG", hakc::TestModeSuppliedDAG);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLAllocationType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLAllocationType &AllocationType) {
        io.mapRequired("name", AllocationType.FunctionName);
        io.mapRequired("type", AllocationType.AllocationType);
        io.mapRequired("args", AllocationType.Arguments);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLActionType> {
  static void mapping(yaml::IO &io, hakc::HAKCYAMLActionType &ActionType) {
    io.mapRequired("type", ActionType.Type);
    io.mapRequired("name", ActionType.Name);
    io.mapRequired("args", ActionType.Arguments);
  }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLCFunctionDefinitionType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLCFunctionDefinitionType &CFunctionDefinition) {
        io.mapRequired("name", CFunctionDefinition.Name);
        io.mapOptional("ptr-idx", CFunctionDefinition.PointerIdx);
        io.mapOptional("compartment-idx", CFunctionDefinition.CompartmentIdx);
        io.mapOptional("division-idx", CFunctionDefinition.DivisionIdx);
        io.mapOptional("size-idx", CFunctionDefinition.SizeIdx);
        io.mapOptional("is-code-idx", CFunctionDefinition.IsCodeIdx);
        ValidateHAKCDefinition(CFunctionDefinition);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionDefinitionType> {
  static void mapping(yaml::IO &io, hakc::HAKCYAMLFunctionDefinitionType &FunctionDefinition) {
    io.mapRequired("type", FunctionDefinition.Type);
    io.mapRequired("name", FunctionDefinition.Name);
    io.mapRequired("params", FunctionDefinition.Parameters);
    // TODO: add validation function?
  }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLFileType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLFileType &File) {
        io.mapRequired("path_root", File.PathRoot);
        io.mapRequired("file_names", File.Files);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLPreTransferActionsType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLPreTransferActionsType &HAKCYAMLPreTransferActions) {
        io.mapRequired("actions", HAKCYAMLPreTransferActions.Actions);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLPostTargetActionType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLPostTargetActionType &HAKCYAMLPostTargetAction) {
      io.mapRequired("actions", HAKCYAMLPostTargetAction.Actions);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionArgumentType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLFunctionArgumentType &HAKCYAMLFunctionArgument) {
        io.mapRequired("idx", HAKCYAMLFunctionArgument.idx);
        io.mapRequired("label", HAKCYAMLFunctionArgument.label);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionParameterType> {
  static void mapping(yaml::IO &io, hakc::HAKCYAMLFunctionParameterType &HAKCYAMLFunctionParameter) {
    io.mapRequired("idx", HAKCYAMLFunctionParameter.idx);
    io.mapRequired("name", HAKCYAMLFunctionParameter.name);
  }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLTransferType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLTransferType &TransferType) {
        io.mapRequired("name", TransferType.Name);
        io.mapRequired("ptr-idx", TransferType.PointerIdx);
        io.mapRequired("compartment-idx", TransferType.CompartmentIdx);
        io.mapRequired("division-idx", TransferType.DivisionIdx);
        io.mapOptional("size-idx", TransferType.SizeIdx);
        io.mapOptional("is-code-idx", TransferType.IsCodeIdx);
        ValidateHAKCDefinition(TransferType);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLCustomTransferType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLCustomTransferType &CustomTransfer) {
        io.mapRequired("name", CustomTransfer.Name);
        io.mapRequired("ptr-idx", CustomTransfer.PointerIdx);
        io.mapRequired("compartment-idx", CustomTransfer.CompartmentIdx);
        io.mapRequired("division-idx", CustomTransfer.DivisionIdx);
        io.mapRequired("type", CustomTransfer.TypeName);
        io.mapOptional("size-idx", CustomTransfer.SizeIdx);
        io.mapOptional("is-code-idx", CustomTransfer.IsCodeIdx);
        ValidateHAKCDefinition(CustomTransfer);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlDatabaseConfig> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlDatabaseConfig &YamlConfig) {
        io.mapRequired("server-url", YamlConfig.ServerURL);
        io.mapOptional("server-timeout", YamlConfig.ServerTimeout, 1000);
        io.mapOptional("get-compartment-by-id-endpoint", YamlConfig.GetCompartmentEndpoint, "get-compartment-id");
        io.mapOptional("get-division-by-id-endpoint", YamlConfig.GetDivisionEndpoint, "get-division-id");
        io.mapOptional("get-division-from-symbol-endpoint", YamlConfig.GetSymbolDivisionEndpoint,
                       "get-division-from-symbol");
        io.mapOptional("get-valid-targets-from-compartment-id-endpoint", YamlConfig.GetValidTargetsEndpoint,
                       "get-valid-targets-from-compartment-id");
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYamlConfig> {
    static void mapping(yaml::IO &io, hakc::HAKCYamlConfig &YamlConfig) {
        // setting default TestMode to TestModeDisabled
        io.mapOptional("TestMode", YamlConfig.TestMode, hakc::TestModeDisabled);

        if (YamlConfig.TestMode == hakc::InvalidTestModeType) {
            errs() << "Supplied TestMode is invalid\n";
            throw std::exception();
        } else if (YamlConfig.TestMode == hakc::TestModeDisabled) {
            io.mapRequired("Arch", YamlConfig.Arch);
            io.mapRequired("Platform", YamlConfig.Platform);
            io.mapRequired("SourcePath", YamlConfig.SourcePath);
            io.mapRequired("BuildPath", YamlConfig.BuildPath);
            io.mapRequired("DagAnalysisRootPath", YamlConfig.DagAnalysisRootPath);
            io.mapRequired("PassMode", YamlConfig.PassMode);
            io.mapRequired("IncludePaths", YamlConfig.IncludePathsList);
            io.mapRequired("CodeValidationFunction", YamlConfig.CodeValidationFunction);
            io.mapRequired("DataValidationFunction", YamlConfig.DataValidationFunction);
            io.mapRequired("DefaultCompartmentTransferFunction", YamlConfig.DefaultCompartmentTransfer);
            io.mapRequired("SignWithDivisionFunction", YamlConfig.SignWithDivision);

            io.mapOptional("CompartmentalizationSupportFunctions", YamlConfig.CompartmentalizationSupportFunctions);
            io.mapOptional("NoTransferFunctions", YamlConfig.NoTransferFunctions);
            io.mapOptional("SeparateNamespacePathList", YamlConfig.SeparateNamespacePaths);
            io.mapOptional("HAKCSourcePathList", YamlConfig.HAKCSourcePaths);
            io.mapOptional("SafeTransitionFunctions", YamlConfig.SafeTransitionFunctions);
            io.mapOptional("IgnoredTypes", YamlConfig.IgnoredTypes);
            io.mapOptional("IgnoredGlobals", YamlConfig.IgnoredGlobals);
            io.mapOptional("AllocationFunctions", YamlConfig.AllocationFunctions);
            io.mapOptional("OutputDebugInfo", YamlConfig.OutputAllDebugInfo, false);
            io.mapOptional("DebugOutputSymbols", YamlConfig.PassDebugSymbols);
            io.mapOptional("PerCPUCompartmentTransferFunction", YamlConfig.PerCPUCompartmentTransfer);
            io.mapOptional("CustomTransferFunctions", YamlConfig.CustomTransferFunctions);
            io.mapOptional("PreTransferActions", YamlConfig.PreTransferActions);
            io.mapOptional("PostTargetActions", YamlConfig.PostTargetActions);

            if (YamlConfig.PassMode == hakc::RunCompartmentalization) {
                io.mapRequired("Database", YamlConfig.DatabaseConfig);
            } else if (YamlConfig.PassMode == hakc::RunDataAccessGraphAnalysis) {
                io.mapOptional("Database", YamlConfig.DatabaseConfig);
            }
        } else if (YamlConfig.TestMode == hakc::TestModeDefault) {
            io.mapRequired("Arch", YamlConfig.Arch);
            io.mapRequired("Platform", YamlConfig.Platform);
            io.mapRequired("SourcePath", YamlConfig.SourcePath);
            io.mapRequired("BuildPath", YamlConfig.BuildPath);
            io.mapOptional("DagAnalysisRootPath", YamlConfig.DagAnalysisRootPath);
            io.mapOptional("PassMode", YamlConfig.PassMode);
            io.mapOptional("IncludePaths", YamlConfig.IncludePathsList);
            io.mapOptional("CodeValidationFunction", YamlConfig.CodeValidationFunction);
            io.mapOptional("DataValidationFunction", YamlConfig.DataValidationFunction);
            io.mapOptional("DefaultCompartmentTransferFunction", YamlConfig.DefaultCompartmentTransfer);
            io.mapOptional("SignWithDivisionFunction", YamlConfig.SignWithDivision);

            io.mapOptional("CompartmentalizationSupportFunctions", YamlConfig.CompartmentalizationSupportFunctions);
            io.mapOptional("NoTransferFunctions", YamlConfig.NoTransferFunctions);
            io.mapOptional("SeparateNamespacePathList", YamlConfig.SeparateNamespacePaths);
            io.mapOptional("HAKCSourcePathList", YamlConfig.HAKCSourcePaths);
            io.mapOptional("SafeTransitionFunctions", YamlConfig.SafeTransitionFunctions);
            io.mapOptional("IgnoredTypes", YamlConfig.IgnoredTypes);
            io.mapOptional("IgnoredGlobals", YamlConfig.IgnoredGlobals);
            io.mapOptional("AllocationFunctions", YamlConfig.AllocationFunctions);
            io.mapOptional("OutputDebugInfo", YamlConfig.OutputAllDebugInfo, false);
            io.mapOptional("DebugOutputSymbols", YamlConfig.PassDebugSymbols);
            io.mapOptional("PerCPUCompartmentTransferFunction", YamlConfig.PerCPUCompartmentTransfer);
            io.mapOptional("CustomTransferFunctions", YamlConfig.CustomTransferFunctions);
            io.mapOptional("Database", YamlConfig.DatabaseConfig);
            io.mapOptional("PreTransferActions", YamlConfig.PreTransferActions);
            io.mapOptional("PostTargetActions", YamlConfig.PostTargetActions);
        } else if (YamlConfig.TestMode == hakc::TestModeSuppliedDAG) {
            // TODO
        }
    }
};

#endif //HAKC_HAKCYAML_H
