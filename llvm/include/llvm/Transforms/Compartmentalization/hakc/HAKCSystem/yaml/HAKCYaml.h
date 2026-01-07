//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains pass configuration parsing
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 11/12/24.
//

#ifndef HAKC_HAKCYAML_H
#define HAKC_HAKCYAML_H

#include <string>
#include <vector>

#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

typedef std::string HAKCYAMLStringType;

template<typename Ty>
using HAKCYAMLSequence = std::vector<Ty>;

typedef HAKCYAMLSequence<HAKCYAMLStringType> HAKCYAMLStringSequenceType;

namespace llvm::hakc {

    enum HAKCLogLevel {
      Verbose = 1,
      Debug = 2,
      Info = 3,
      Warning = 4,
      Error = 5,
      Fatal = 10,
    };

    enum HAKCAllocationTypeEnum {
        InvalidAllocationType,
        SimpleArgumentSize,
        SimpleStaticSize,
        StaticPlusArgument,
        MultiplyTwoArguments,
        ArgumentGEP
    };

      struct HAKCYAMLSymbolDeclaration {
        HAKCYAMLStringType SymbolName;

        HAKCYAMLSymbolDeclaration() {}
    };

    struct HAKCYAMLAllocationType : HAKCYAMLSymbolDeclaration {
        HAKCAllocationTypeEnum AllocationType;
        HAKCYAMLStringSequenceType Arguments;

        HAKCYAMLAllocationType() : AllocationType(InvalidAllocationType) {}
    };

    struct HAKCYAMLFileType {
        HAKCYAMLStringType PathRoot;
        HAKCYAMLStringSequenceType Files;

        HAKCYAMLFileType() {}

        void AddAllFiles(SmallVectorImpl<std::string> &Results) {
            for (auto &FilePath: Files) {
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

        HAKCYAMLFunctionArgument() : Idx(), ArgUse(Other) {}

        Type *GetType(const HAKCTypeIdentifier &TypeIdentifier) const {
            return TypeIdentifier.GetTypeFromString(TypeStr);
        }
    };

    struct HAKCYAMLFunctionDefinition : HAKCYAMLSymbolDeclaration {
        HAKCYAMLStringType ReturnType;
        HAKCYAMLSequence<HAKCYAMLFunctionArgument> Arguments;

        HAKCYAMLFunctionDefinition(){}

        bool IsValid() {
            bool Result = !ReturnType.empty() || !SymbolName.empty();
            if (Result) {
                auto ByIndex = [&](const HAKCYAMLFunctionArgument &Arg0,
                                   const HAKCYAMLFunctionArgument &Arg1) {
                    return Arg0.Idx < Arg1.Idx;
                };
                llvm::sort(Arguments, ByIndex);
                unsigned Previous = 0;
                for (auto &Arg: Arguments) {
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
            for (auto &Arg: Arguments) {
                auto *ArgTy = Arg.GetType(TypeIdentifier);
                if (!ArgTy) {
                    return nullptr;
                }
                ArgTys.push_back(ArgTy);
            }

            auto *FType = FunctionType::get(ReturnTy, ArgTys, false);
            auto *F = dyn_cast<Function>(TypeIdentifier.GetModule()
                .getOrInsertFunction(SymbolName, FType)
                .getCallee());
            return F;
        }
    };

    struct HAKCYAMLCustomTransferType : HAKCYAMLFunctionDefinition {
        HAKCYAMLStringType TransferObjectTypeName;

        HAKCYAMLCustomTransferType() {}
    };

    struct HAKCYAMLActionArgument {
        HAKCYAMLStringType Label;
        unsigned Idx;
    };

    struct HAKCYAMLActionType : HAKCYAMLSymbolDeclaration {
        HAKCYAMLStringType Label;
        HAKCYAMLSequence<HAKCYAMLActionArgument> Arguments;

        HAKCYAMLActionType() {}
    };

    struct HAKCYAMLEndpoints {
        HAKCYAMLStringType GetCompartmentEndpoint;
        HAKCYAMLStringType GetDivisionEndpoint;
        HAKCYAMLStringType GetSymbolDivisionEndpoint;
        HAKCYAMLStringType GetSymbolTypeUseDivisionEndpoint;
        HAKCYAMLStringType GetValidTargetsEndpoint;
        HAKCYAMLStringType AddSymbolsEndpoint;
        HAKCYAMLStringType AddFunctionEndpoint;
        HAKCYAMLStringType AddGlobalVariableEndpoint;
        HAKCYAMLStringType TerminateConnectionEndpoint;
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

    struct HAKCYAMLClientConfig {
        HAKCYAMLStringType Arch;
        HAKCYAMLStringType Platform;
        HAKCLogLevel ConsoleLogLevel;
        HAKCLogLevel FileLogLevel;
        HAKCYAMLFunctionDefinition CodeValidationFunction;
        HAKCYAMLFunctionDefinition DataValidationFunction;
        HAKCYAMLSequence<HAKCYAMLSymbolDeclaration> SafeTransitionFunctions;
        HAKCYAMLSequence<HAKCYAMLSymbolDeclaration> IgnoredGlobals;
        HAKCYAMLStringSequenceType TransferFunctions;
        HAKCYAMLStringSequenceType PassDebugSymbols;
        HAKCYAMLStringSequenceType SeparateNamespacePathsList;
        HAKCYAMLStringSequenceType HAKCSourcePathsList;
        HAKCYAMLStringSequenceType TransferFunctionCandidates;
        bool DebugDatabase;
        HAKCYAMLSequence<HAKCYAMLSymbolDeclaration> NoTransferFunctions;
        HAKCYAMLSequence<HAKCYAMLCustomTransferType> CustomTransferFunctions;
        HAKCYAMLSequence<HAKCYAMLFunctionDefinition> CompartmentalizationSupportFunctions;
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
        unsigned MaxConnectionRetries;
    };

    struct HAKCYAMLConfig {
        HAKCYAMLClientConfig ClientConfig;
        HAKCYAMLStringType ClientConfigPath;
        HAKCYAMLStringType BuildDir;
        HAKCYAMLStringType SocketDir;
        HAKCYAMLStringType LogDir;
        bool TemporalAnalysisEnabled;
        unsigned ServerCoreCount;
        unsigned DefaultCompartmentID;
        unsigned DefaultDivisionID;
        unsigned DefaultEntryToken;
        unsigned DefaultAccessToken;
        HAKCYAMLEndpoints Endpoints;
        unsigned DivisionIDBitCount;
    };
} // namespace llvm::hakc

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLAllocationType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLCustomTransferType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionDefinition)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFileType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLFunctionArgument)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLActionType)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLActionArgument)

LLVM_YAML_IS_SEQUENCE_VECTOR(hakc::HAKCYAMLSymbolDeclaration)

static void YAMLFunctionDeclarationMapping(
    yaml::IO &io, hakc::HAKCYAMLSymbolDeclaration &FunctionDeclaration) {
    io.mapRequired("name", FunctionDeclaration.SymbolName);
}

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLSymbolDeclaration> {
    static void mapping(yaml::IO &io,
                        hakc::HAKCYAMLSymbolDeclaration &FunctionDefinition) {
        YAMLFunctionDeclarationMapping(io, FunctionDefinition);
    }
};

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
struct yaml::ScalarEnumerationTraits<hakc::HAKCLogLevel> {
    static void enumeration(IO &io, hakc::HAKCLogLevel &value) {
        io.enumCase(value, "Verbose", hakc::Verbose);
        io.enumCase(value, "Debug", hakc::Debug);
        io.enumCase(value, "Info", hakc::Info);
        io.enumCase(value, "Warning", hakc::Warning);
        io.enumCase(value, "Error", hakc::Error);
        io.enumCase(value, "Fatal", hakc::Fatal);
    }
};

template<>
struct yaml::ScalarEnumerationTraits<hakc::HAKCFunctionArgumentUse> {
    static void enumeration(IO &io, hakc::HAKCFunctionArgumentUse &value) {
        for (auto &it: hakc::HAKCArgumentArgumentUseStringMap()) {
            io.enumCase(value, it.second, it.first);
        }
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

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLActionArgument> {
    static void mapping(yaml::IO &io, hakc::HAKCYAMLActionArgument &ActionArg) {
        io.mapRequired("label", ActionArg.Label);
        io.mapRequired("idx", ActionArg.Idx);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLActionType> {
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

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionDefinition> {
    static void mapping(yaml::IO &io,
                        hakc::HAKCYAMLFunctionDefinition &FunctionDefinition) {
        YAMLFunctionMapping(io, FunctionDefinition);
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
struct yaml::MappingTraits<hakc::HAKCYAMLFunctionArgument> {
    static void
    mapping(yaml::IO &io,
            hakc::HAKCYAMLFunctionArgument &HAKCYAMLFunctionArgument) {
        io.mapRequired("idx", HAKCYAMLFunctionArgument.Idx);
        io.mapRequired("type", HAKCYAMLFunctionArgument.TypeStr);
        io.mapRequired("arg-use", HAKCYAMLFunctionArgument.ArgUse);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLCustomTransferType> {
    static void mapping(yaml::IO &io,
                        hakc::HAKCYAMLCustomTransferType &CustomTransfer) {
        YAMLFunctionMapping(io, CustomTransfer);
        io.mapRequired("transfer-obj-type", CustomTransfer.TransferObjectTypeName);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLEndpoints> {
    static void mapping(yaml::IO &Io, hakc::HAKCYAMLEndpoints &Endpoints) {
        Io.mapOptional("get-compartment-by-id-endpoint",
                       Endpoints.GetCompartmentEndpoint, "get-compartment-id");
        Io.mapOptional("get-division-by-id-endpoint",
                       Endpoints.GetDivisionEndpoint, "get-division-id");
        Io.mapOptional("get-division-from-symbol-endpoint",
                       Endpoints.GetSymbolDivisionEndpoint,
                       "get-division-from-symbol");
        Io.mapOptional("get-division-from-symbol-type-use-endpoint",
                     Endpoints.GetSymbolTypeUseDivisionEndpoint,
                     "get-division-from-symbol-type-use");
        Io.mapOptional("get-valid-targets-from-compartment-id-endpoint",
                       Endpoints.GetValidTargetsEndpoint,
                       "get-valid-targets-from-compartment-id");
        Io.mapOptional("add-function-endpoint",
                       Endpoints.AddFunctionEndpoint,
                       "add-function");
        Io.mapOptional("add-symbols-endpoint",
                       Endpoints.AddSymbolsEndpoint,
                       "add-symbols");
        Io.mapOptional("add-global-variable-endpoint",
                       Endpoints.AddGlobalVariableEndpoint,
                       "add-global-variable");
        Io.mapOptional("terminate-connection-endpoint",
                       Endpoints.TerminateConnectionEndpoint,
                       "terminate-connection");
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLClientConfig> {
    static void mapping(IO &io, hakc::HAKCYAMLClientConfig &ClientConfig) {
        io.mapRequired("arch", ClientConfig.Arch);
        io.mapRequired("platform", ClientConfig.Platform);
        io.mapOptional("console-log-level", ClientConfig.ConsoleLogLevel,
                       hakc::HAKCLogLevel::Error);
        io.mapOptional("file-log-level", ClientConfig.FileLogLevel,
                       hakc::HAKCLogLevel::Error);

        io.mapOptional("max-connection-retries", ClientConfig.MaxConnectionRetries, 1);
        io.mapRequired("CodeValidationFunction",
                       ClientConfig.CodeValidationFunction);
        io.mapRequired("DataValidationFunction",
                       ClientConfig.DataValidationFunction);
        io.mapRequired("DefaultCompartmentTransferFunction",
                       ClientConfig.DefaultCompartmentTransfer);
        io.mapRequired("SignWithDivisionFunction", ClientConfig.SignWithDivision);
        io.mapOptional("CompartmentalizationSupportFunctions",
                       ClientConfig.CompartmentalizationSupportFunctions);
        io.mapOptional("NoTransferFunctions", ClientConfig.NoTransferFunctions);
        io.mapOptional("SeparateNamespacePaths",
                       ClientConfig.SeparateNamespacePaths);
        io.mapOptional("HAKCSourcePaths", ClientConfig.HAKCSourcePaths);
        io.mapOptional("SafeTransitionFunctions",
                       ClientConfig.SafeTransitionFunctions);
        io.mapOptional("IgnoredTypes", ClientConfig.IgnoredTypes);
        io.mapOptional("IgnoredGlobals", ClientConfig.IgnoredGlobals);
        io.mapOptional("AllocationFunctions", ClientConfig.AllocationFunctions);
        io.mapOptional("DebugDatabase", ClientConfig.DebugDatabase, false);
        io.mapOptional("DebugOutputSymbols", ClientConfig.PassDebugSymbols);
        io.mapOptional("PerCPUCompartmentTransferFunction",
                       ClientConfig.PerCPUCompartmentTransfer);
        io.mapOptional("CustomTransferFunctions",
                       ClientConfig.CustomTransferFunctions);
        io.mapOptional("PreTargetActions", ClientConfig.PreTargetActions);
        io.mapOptional("PostTargetActions", ClientConfig.PostTargetActions);
        io.mapOptional("TransferFunctionCandidates",
                       ClientConfig.TransferFunctionCandidates);
    }
};

template<>
struct yaml::MappingTraits<hakc::HAKCYAMLConfig> {
    static void mapping(IO &io, hakc::HAKCYAMLConfig &YamlConfig) {
        std::string ignore0;
        io.mapOptional("server-config-path", ignore0);
        io.mapOptional("client-config-path", YamlConfig.ClientConfigPath);
        // read in server config (we only get the path from the yaml file)
        ErrorOr<std::unique_ptr<MemoryBuffer> > mb = MemoryBuffer::getFile(YamlConfig.ClientConfigPath);
        Input yin(mb.get()->getMemBufferRef().getBuffer());
        yin >> YamlConfig.ClientConfig;
        if (yin.error()) {
            errs() << "Error parsing config file " << YamlConfig.ClientConfigPath << "\n";
            throw std::exception();
        }
        io.mapRequired("build-dir", YamlConfig.BuildDir);
        io.mapRequired("socket-dir", YamlConfig.SocketDir);
        io.mapRequired("log-dir", YamlConfig.LogDir);
        io.mapOptional("server-core-count", YamlConfig.ServerCoreCount, 64);
        io.mapOptional("default-compartment-id", YamlConfig.DefaultCompartmentID);
        io.mapOptional("default-division-id", YamlConfig.DefaultDivisionID);
        io.mapOptional("default-entry-token", YamlConfig.DefaultEntryToken);
        io.mapOptional("default-access-token", YamlConfig.DefaultAccessToken);
        io.mapRequired("Endpoints", YamlConfig.Endpoints);
        io.mapOptional("division-id-bit-count", YamlConfig.DivisionIDBitCount, 16);
    }
};

#endif // HAKC_HAKCYAML_H
