//
// Created by de29664 on 11/12/24.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <vector>
#include <string>

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCTransferFunction.h"

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

    struct HAKCYAMLStructType {
        HAKCYAMLStringType StructType;
        HAKCYAMLStringSequenceType StructSubType;

        HAKCYAMLStructType() : StructType(), StructSubType() {
        }
    };


    struct HAKCYAMLFunctionDefinitionType {
        HAKCYAMLStringType FunctionName;
        unsigned PointerIdx;
        unsigned SizeIdx;
        unsigned CompartmentIdx;
        unsigned DivisionIdx;
        unsigned IsCodeIdx;

        HAKCYAMLFunctionDefinitionType()
            : FunctionName(), PointerIdx(HAKCTransferFunction::MissingIdx),
              SizeIdx(HAKCTransferFunction::MissingIdx), CompartmentIdx(HAKCTransferFunction::MissingIdx),
              DivisionIdx(HAKCTransferFunction::MissingIdx), IsCodeIdx(HAKCTransferFunction::MissingIdx) {
        }

        bool IsValid() const { return !FunctionName.empty(); }

        Function *GetFunction(Module &M) {
            if (!IsValid()) {
                return nullptr;
            }
            unsigned BitCount = 64;
            SmallVector<Type *, HAKCTransferFunction::MaxArgIndex> ArgTypes = {
                PointerType::get(M.getContext(), 0)
            };
            if (PointerIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[PointerIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (SizeIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[SizeIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (CompartmentIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[CompartmentIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (DivisionIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[DivisionIdx] = IntegerType::get(M.getContext(), BitCount);
            }
            if (IsCodeIdx != HAKCTransferFunction::MissingIdx) {
                ArgTypes[IsCodeIdx] = IntegerType::get(M.getContext(), 1);
            }

            auto *FuncType = FunctionType::get(PointerType::get(M.getContext(), 0), ArgTypes, false);
            return dyn_cast<Function>(M.getOrInsertFunction(FunctionName, FuncType).getCallee());
        }
    };

    struct HAKCYAMLTransferType : public HAKCYAMLFunctionDefinitionType {
        HAKCYAMLTransferType() : HAKCYAMLFunctionDefinitionType() {
        }
    };

    struct HAKCYAMLCustomTransferType : public HAKCYAMLTransferType {
        HAKCYAMLStringType TypeName;

        HAKCYAMLCustomTransferType() : HAKCYAMLTransferType(), TypeName() {
        }
    };

    struct HAKCYamlDatabaseConfig {
        HAKCYAMLStringType ServerURL;
        HAKCYAMLStringType GetCompartmentEndpoint;
        unsigned ServerTimeout;
    };

    struct HAKCYamlConfig {
        HAKCTestModeTypeEnum TestMode;
        HAKCYAMLStringType Arch;
        HAKCYAMLStringType Platform;
        HAKCYAMLStringType SourcePath;
        HAKCYAMLStringType BuildPath;
        HAKCYAMLStringType DagAnalysisRootPath;
        HAKCYAMLStringType CodeValidationFunction;
        HAKCYAMLStringType DataValidationFunction;
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
        HAKCYAMLSequence<HAKCYAMLStructType> IgnoredTypes;
        HAKCYAMLTransferType DefaultCompartmentTransfer;
        HAKCYAMLFunctionDefinitionType SignWithDivision;
        HAKCYAMLTransferType PerCPUCompartmentTransfer;
    };
} // hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLTransferType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLCustomTransferType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDefinitionType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFileType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLStructType)

inline void ValidateHAKCDefinition(hakc::HAKCYAMLFunctionDefinitionType &Definition) {
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
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionDefinitionType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLFunctionDefinitionType &FunctionDefinition) {
        io.mapRequired("name", FunctionDefinition.FunctionName);
        io.mapOptional("ptr-idx", FunctionDefinition.PointerIdx);
        io.mapOptional("compartment-idx", FunctionDefinition.CompartmentIdx);
        io.mapOptional("division-idx", FunctionDefinition.DivisionIdx);
        io.mapOptional("size-idx", FunctionDefinition.SizeIdx);
        io.mapOptional("is-code-idx", FunctionDefinition.IsCodeIdx);
        ValidateHAKCDefinition(FunctionDefinition);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLStructType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLStructType &Struct) {
        io.mapRequired("type", Struct.StructType);
        io.mapRequired("subtypes", Struct.StructSubType);
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
struct yaml::MappingTraits<hakc::HAKCYAMLTransferType> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLTransferType &TransferType) {
        io.mapRequired("name", TransferType.FunctionName);
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
        io.mapRequired("name", CustomTransfer.FunctionName);
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
        io.mapOptional("get-compartment-endpoint", YamlConfig.GetCompartmentEndpoint, "get-compartment");
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
        } else if (YamlConfig.TestMode == hakc::TestModeSuppliedDAG) {
            // TODO
        }
    }
};

#endif //HAKC_HAKCYAML_H
