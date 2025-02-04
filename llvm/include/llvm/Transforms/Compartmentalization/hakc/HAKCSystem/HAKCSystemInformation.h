//
// Created by de29664 on 11/7/24.
//

#ifndef HAKC_HAKCSYSTEMINFORMATION_H
#define HAKC_HAKCSYSTEMINFORMATION_H

#include <string>
#include "llvm/Support/YAMLTraits.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCAllocationSize.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

typedef std::shared_ptr<llvm::hakc::HAKCAllocationSize> HAKCCustomAllocation;

typedef SmallVector<llvm::hakc::hakc_function_def_t> HAKCFunctionList;
typedef SmallVector<llvm::hakc::hakc_transfer_def_t> HAKCTransferList;
typedef SmallVector<llvm::hakc::hakc_custom_transfer_def_t> HAKCCustomTransferList;
typedef SmallVector<GlobalVariable *> HAKCGlobalVariableList;
typedef SmallVector<GlobalValue *> HAKCSymbolList;
typedef SmallVector<Function *> FunctionList;
typedef SmallPtrSet<Type *, 16> HAKCTypeSet;
typedef SmallVector<std::string, 16> HAKCStringList;
typedef SmallVector<HAKCCustomAllocation> HAKCCustomAllocationList;

namespace llvm::hakc {
    class CommonHAKCAnalysis;
    class HAKCSystemInformation;

    class HAKCDatabaseInformation {
    public:
        HAKCDatabaseInformation();

        StringRef GetServerURL() const;

        StringRef GetCompartmentEndpoint() const;

        StringRef GetDivisionEndpoint() const;

        std::chrono::milliseconds GetServerTimeout() const;

        void operator<<(HAKCYamlDatabaseConfig &DatabaseConfig);

    protected:
        std::string ServerURL;
        std::string CompartmentEndpoint;
        std::string DivisionEndpoint;
        std::chrono::milliseconds Timeout;
    };

    class HAKCSystemInformation {
    public:
        explicit HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis);

        bool OutputDebugInfo() const;

        bool OutputDebugInfo(GlobalValue *GV) const;

        bool OutputDebugInfo(StringRef SymbolName) const;

        Module &GetModule();

        const HAKCDatabaseInformation &GetDatabaseInformation() const;

        void operator<<(HAKCYamlConfig &YamlConfig);

        iterator_range<FunctionList::iterator> NoTransferFunctions();

        iterator_range<HAKCTransferList::iterator> CompartmentTransferFunctions();

        iterator_range<FunctionList::iterator> CompartmentalizationSupportFunctions();

        iterator_range<FunctionList::iterator> SafeTransitionFunctions();

        iterator_range<HAKCTypeSet::iterator> IgnoredTypes();

        iterator_range<HAKCGlobalVariableList::iterator> IgnoredGlobals();

        iterator_range<HAKCStringList::iterator> SeparateNamespacePaths();

        iterator_range<HAKCStringList::iterator> HAKCSourcePaths();

        iterator_range<HAKCCustomTransferList::iterator> HAKCCustomTransfers();

        iterator_range<HAKCCustomAllocationList::iterator> AllocationFunctions();

        iterator_range<HAKCStringList::iterator> IncludePaths();

        Function *CodeValidation() const;

        Function *DataValidation() const;

        Function *SignWithDivision() const;

        hakc_transfer_def_t CompartmentTransfer(bool PerCPU) const;

        HAKCTypeIdentifier &GetTypeIdentifier();

        hakc::HAKCPassModeTypeEnum GetPassMode() const;

        StringRef GetArch() const;

        StringRef GetPlatform() const;

        StringRef GetSourcePath() const;

        StringRef GetBuildPath() const;

        StringRef GetDagAnalysisRootPath() const;

    protected:
        CommonHAKCAnalysis &CommonAnalysis;
        HAKCTypeIdentifier TypeIdentifier;
        HAKCDatabaseInformation DatabaseInformation;
        bool DebugOutput;
        hakc::HAKCPassModeTypeEnum PassMode;
        std::string Arch;
        std::string Platform;
        std::string SourcePath;
        std::string BuildPath;
        std::string DagAnalysisRootPath;
        HAKCStringList IncludePathsList;
        FunctionList NoTransferFunctionList;
        HAKCTransferList CompartmentTransferFunctionList;
        Function *CodeValidationFunction;
        Function *DataValidationFunction;
        Function *SignWithDivisionFunction;
        hakc::hakc_transfer_def_t DefaultCompartmentTransfer;
        hakc::hakc_transfer_def_t PerCPUCompartmentTransfer;
        FunctionList CompartmentalizationSupportFunctionList;
        HAKCSymbolList SymbolsToOutputDebugInfo;
        HAKCStringList SeparateNamespacePathList;
        HAKCStringList HAKCSourcePathList;
        FunctionList SafeTransitionFunctionList;
        HAKCTypeSet IgnoredTypeSet;
        HAKCGlobalVariableList IgnoredGlobalList;
        HAKCCustomAllocationList AllocationFunctionList;
        HAKCCustomTransferList CustomTransferList;
    };
} // hakc

#endif //HAKC_HAKCSYSTEMINFORMATION_H
