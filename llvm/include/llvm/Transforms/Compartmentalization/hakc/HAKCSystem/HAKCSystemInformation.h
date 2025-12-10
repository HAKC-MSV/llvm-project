//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains all the configuration information that was parsed in
/// yaml.h and performs some validation (e.g., checking if HAKC functions
/// specified actually exist in the kernel)
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 11/7/24.
//

#ifndef HAKC_HAKCSYSTEMINFORMATION_H
#define HAKC_HAKCSYSTEMINFORMATION_H

#include "llvm/IR/Module.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCAllocationSize.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

typedef std::shared_ptr<hakc::HAKCAllocationSize> HAKCCustomAllocation;
typedef SmallVector<HAKCCustomAllocation> HAKCCustomAllocationList;
typedef SmallVector<Function *> FunctionList;
typedef SmallVector<GlobalValue *> HAKCSymbolList;
typedef SmallVector<GlobalVariable *> HAKCGlobalVariableList;
typedef SmallVector<hakc::HAKCTypeP> HAKCStructList;
typedef SmallVector<hakc::arg_def_t> HAKCArgumentsList;
typedef SmallVector<hakc::custom_transfer_def_t> HAKCCustomTransferList;
typedef SmallVector<hakc::function_def_t> HAKCFunctionList;
typedef SmallVector<hakc::function_def_t> HAKCTransferList;
typedef SmallVector<hakc::post_target_action_def_t> HAKCPostTargetActionList;
typedef SmallVector<hakc::pre_transfer_action_def_t> HAKCPreTransferActionList;
typedef SmallVector<hakc::transfer_action_def_t> HAKCTransferActionList;
typedef SmallVector<std::string, 16> HAKCStringList;

namespace llvm::hakc {
    class CommonHAKCAnalysis;

    class HAKCSystemInformation {
    public:
        explicit HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis);

        HAKCLogLevel GetConsoleLogLevel() const;

        HAKCLogLevel GetFileLogLevel() const;

        HAKCStructList GetStructList() const;

        HAKCTypeIdentifier &GetTypeIdentifier();

        Module &GetModule() const;

        StringRef GetAddFunctionEndpoint() const;

        StringRef GetAddGlobalVariableEndpoint() const;

        StringRef GetAddSymbolsEndpoint() const;

        StringRef GetArch() const;

        StringRef GetBuildPath() const;

        StringRef GetCompartmentEndpoint() const;

        StringRef GetDivisionEndpoint() const;

        StringRef GetPlatform() const;

        StringRef GetRootPath() const;

        StringRef GetSocketPath() const;

        unsigned GetDefaultDivisionID() const;

        unsigned GetDefaultCompartmentID() const;

        unsigned GetDefaultEntryToken() const;

        unsigned GetDefaultAccessToken() const;

        StringRef GetLogPath() const;

        StringRef GetSetDagFilenameEndpoint() const;

        StringRef GetSingleSourceFile();

        StringRef GetSymbolDivisionEndpoint() const;

        StringRef GetTerminateConnectionEndpoint() const;

        StringRef GetValidTargetsEndpoint() const;

        bool GetDebugDatabase() const;

        bool OutputDebugInfo(GlobalValue *GV) const;

        bool OutputDebugInfo(StringRef SymbolName) const;

        function_def_t CodeValidation() const;

        function_def_t CompartmentTransfer(bool PerCPU) const;

        function_def_t DataValidation() const;

        function_def_t SignWithDivision() const;

        iterator_range<FunctionList::iterator> NoTransferFunctions();

        iterator_range<FunctionList::iterator> SafeTransitionFunctions();

        iterator_range<HAKCCustomAllocationList::iterator> AllocationFunctions();

        iterator_range<HAKCCustomTransferList::iterator> HAKCCustomTransfers();

        iterator_range<HAKCFunctionList::iterator>
        CompartmentalizationSupportFunctions();

        iterator_range<HAKCGlobalVariableList::iterator> IgnoredGlobals();

        iterator_range<HAKCPostTargetActionList::iterator> PostTargetActions();

        iterator_range<HAKCPreTransferActionList::iterator> PreTransferActions();

        iterator_range<HAKCStringList::iterator> HAKCSourcePaths();

        iterator_range<HAKCStringList::iterator> IncludePaths();

        iterator_range<HAKCStringList::iterator> SeparateNamespacePaths();

        iterator_range<HAKCTransferList::iterator> CompartmentTransferFunctions();

        unsigned GetMaxRetries() const;

        unsigned GetServerCoreCount() const;

        void operator<<(HAKCYAMLConfig &Config);

        void SetSocketPath(StringRef SocketPath);

        unsigned GetDivisionIDBitCount() const;

    protected:
        function_def_t
        CreateHAKCFunction(HAKCYAMLFunctionDefinition &YAMLFunctionDef) const;

        custom_transfer_def_t
        CreateCustomTransferFunction(HAKCYAMLCustomTransferType &YAMLCustomTransfer,
                                     HAKCTypeP HAKCTy);

        void PopulateHAKCFunctionArgs(
            SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args,
            HAKCYAMLFunctionDefinition &YAMLFunctionDef) const;

        void GetAllDefinedHAKCFunctions(SmallVectorImpl<function_def_t> &Results);

        CommonHAKCAnalysis &CommonAnalysis;
        FunctionList NoTransferFunctionList;
        FunctionList SafeTransitionFunctionList;
        HAKCCustomAllocationList AllocationFunctionList;
        HAKCCustomTransferList CustomTransferList;
        HAKCFunctionList CompartmentalizationSupportFunctionList;
        HAKCGlobalVariableList IgnoredGlobalList;
        HAKCLogLevel ConsoleLogLevel;
        HAKCLogLevel FileLogLevel;
        HAKCPostTargetActionList PostTargetActionList;
        HAKCPreTransferActionList PreTransferActionList;
        HAKCStringList HAKCSourcePathList;
        HAKCStringList IncludePathsList;
        HAKCStringList SeparateNamespacePathList;
        HAKCStructList StructList;
        HAKCSymbolList SymbolsToOutputDebugInfo;
        HAKCTransferList CompartmentTransferFunctionList;
        HAKCTypeIdentifier TypeIdentifier;
        bool DebugDatabase;
        function_def_t CodeValidationFunction;
        function_def_t DataValidationFunction;
        function_def_t DefaultCompartmentTransfer;
        function_def_t PerCPUCompartmentTransfer;
        function_def_t SignWithDivisionFunction;
        std::string AddFunctionEndpoint;
        std::string AddGlobalVariableEndpoint;
        std::string AddSymbolsEndpoint;
        std::string Arch;
        std::string BuildPath;
        std::string CompartmentEndpoint;
        std::string DivisionEndpoint;
        std::string Platform;
        std::string RootPath;
        std::string SetDagFilenameEndpoint;
        std::string SingleSourceFile;
        std::string SocketPath;
        std::string LogPath;
        std::string SymbolDivisionEndpoint;
        std::string TerminateConnectionEndpoint;
        std::string ValidTargetsEndpoint;
        unsigned DefaultCompartmentID;
        unsigned DefaultDivisionID;
        unsigned MaxConnectionRetries;
        unsigned ServerCoreCount;
        unsigned DivisionIDBitCount;
    };
} // namespace llvm::hakc

#endif // HAKC_HAKCSYSTEMINFORMATION_H
