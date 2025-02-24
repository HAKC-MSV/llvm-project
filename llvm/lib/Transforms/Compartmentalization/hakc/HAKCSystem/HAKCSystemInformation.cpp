//
// Created by de29664 on 11/7/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

namespace llvm::hakc {
    HAKCDatabaseInformation::HAKCDatabaseInformation() : ServerURL(), CompartmentEndpoint(), DivisionEndpoint(),
                                                         SymbolDivisionEndpoint(), Timeout() {
    }

    StringRef HAKCDatabaseInformation::GetServerURL() const {
        return ServerURL;
    }

    StringRef HAKCDatabaseInformation::GetCompartmentEndpoint() const {
        return CompartmentEndpoint;
    }

    StringRef HAKCDatabaseInformation::GetDivisionEndpoint() const {
        return DivisionEndpoint;
    }

    StringRef HAKCDatabaseInformation::GetSymbolDivisionEndpoint() const {
        return SymbolDivisionEndpoint;
    }

    StringRef HAKCDatabaseInformation::GetValidTargetsEndpoint() const {
      return ValidTargetsEndpoint;
    }

    std::chrono::milliseconds HAKCDatabaseInformation::GetServerTimeout() const {
        return Timeout;
    }

    void HAKCDatabaseInformation::operator<<(HAKCYamlDatabaseConfig &DatabaseConfig) {
        ServerURL = DatabaseConfig.ServerURL;
        CompartmentEndpoint = DatabaseConfig.GetCompartmentEndpoint;
        DivisionEndpoint = DatabaseConfig.GetDivisionEndpoint;
        SymbolDivisionEndpoint = DatabaseConfig.GetSymbolDivisionEndpoint;
        ValidTargetsEndpoint = DatabaseConfig.GetValidTargetsEndpoint;
        Timeout = std::chrono::milliseconds(DatabaseConfig.ServerTimeout);
    }

    HAKCSystemInformation::HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis) : CommonAnalysis(CommonAnalysis),
        TypeIdentifier(CommonAnalysis), DatabaseInformation(), DebugOutput(false), PassMode(InvalidPassModeType),
        Arch(), Platform(),
        SourcePath(), BuildPath(), DagAnalysisRootPath(), IncludePathsList(),
        NoTransferFunctionList(), CompartmentTransferFunctionList(), CodeValidationFunction(nullptr),
        DataValidationFunction(nullptr), SignWithDivisionFunction(nullptr), DefaultCompartmentTransfer(nullptr),
        PerCPUCompartmentTransfer(nullptr), CompartmentalizationSupportFunctionList(), SymbolsToOutputDebugInfo(),
        SeparateNamespacePathList(), HAKCSourcePathList(), SafeTransitionFunctionList(), IgnoredTypeSet(),
        IgnoredGlobalList(), AllocationFunctionList(), CustomTransferList() {
    }

    void HAKCSystemInformation::operator<<(HAKCYamlConfig &YamlConfig) {
        Arch = YamlConfig.Arch;
        Platform = YamlConfig.Platform;
        SourcePath = YamlConfig.SourcePath;
        BuildPath = YamlConfig.BuildPath;
        DagAnalysisRootPath = YamlConfig.DagAnalysisRootPath;
        PassMode = YamlConfig.PassMode;
        DebugOutput = YamlConfig.OutputAllDebugInfo;

        DatabaseInformation << YamlConfig.DatabaseConfig;

        for (auto &FunctionName: YamlConfig.NoTransferFunctions) {
            auto *F = GetModule().getFunction(FunctionName);
            if (F) {
                NoTransferFunctionList.push_back(F);
            }
        }

        for (auto &SymbolName: YamlConfig.PassDebugSymbols) {
            auto *F = GetModule().getFunction(SymbolName);
            if (F) {
                SymbolsToOutputDebugInfo.push_back(F);
            } else {
                auto *Global = GetModule().getGlobalVariable(SymbolName);
                if (Global) {
                    SymbolsToOutputDebugInfo.push_back(Global);
                }
            }
        }

        auto CodeValidation = GetModule().getOrInsertFunction(YamlConfig.CodeValidationFunction,
                                                              CommonHAKCAnalysis::GetCodeAuthenticationFunctionType(
                                                                  GetModule()));
        CodeValidationFunction = dyn_cast<Function>(CodeValidation.getCallee());
        auto DataValidation = GetModule().getOrInsertFunction(YamlConfig.DataValidationFunction,
                                                              CommonHAKCAnalysis::GetDataAuthenticationFunctionType(
                                                                  GetModule()));
        DataValidationFunction = dyn_cast<Function>(DataValidation.getCallee());


        IncludePathsList.append(YamlConfig.IncludePathsList.begin(),
                                YamlConfig.IncludePathsList.end());

        for (auto &FileType: YamlConfig.SeparateNamespacePaths) {
            auto PathRoot = FileType.PathRoot;
            for (auto &FileName: FileType.Files) {
                auto File = PathRoot + FileName;
                YamlConfig.SeparateNamespacePathsList.push_back(File);
            }
        }
        SeparateNamespacePathList.append(YamlConfig.SeparateNamespacePathsList.begin(),
                                         YamlConfig.SeparateNamespacePathsList.end());

        for (auto &FileType: YamlConfig.HAKCSourcePaths) {
            auto PathRoot = FileType.PathRoot;
            for (auto &FileName: FileType.Files) {
                auto File = PathRoot + FileName;
                YamlConfig.HAKCSourcePathsList.push_back(File);
            }
        }
        HAKCSourcePathList.append(YamlConfig.HAKCSourcePathsList.begin(),
                                  YamlConfig.HAKCSourcePathsList.end());

        for (auto &FunctionName: YamlConfig.SafeTransitionFunctions) {
            auto *F = GetModule().getFunction(FunctionName);
            if (F) {
                SafeTransitionFunctionList.push_back(F);
            }
        }

        auto *DefaultTransferFunc = dyn_cast<Function>(
            GetModule().getOrInsertFunction(YamlConfig.DefaultCompartmentTransfer.FunctionName,
                                            CommonHAKCAnalysis::GetTransferFunctionType(
                                                GetModule())).getCallee());
        DefaultCompartmentTransfer = std::make_shared<HAKCTransferFunction>(DefaultTransferFunc,
                                                                            YamlConfig.DefaultCompartmentTransfer.
                                                                            PointerIdx,
                                                                            YamlConfig.DefaultCompartmentTransfer.
                                                                            CompartmentIdx,
                                                                            YamlConfig.DefaultCompartmentTransfer.
                                                                            DivisionIdx,
                                                                            YamlConfig.DefaultCompartmentTransfer.
                                                                            SizeIdx);
        if (YamlConfig.PerCPUCompartmentTransfer.IsValid()) {
            auto *PerCPUTransferFunc = dyn_cast<Function>(
                GetModule().getOrInsertFunction(YamlConfig.PerCPUCompartmentTransfer.FunctionName,
                                                CommonHAKCAnalysis::GetTransferFunctionType(
                                                    GetModule())).getCallee());
            PerCPUCompartmentTransfer = std::make_shared<HAKCTransferFunction>(PerCPUTransferFunc,
                                                                               YamlConfig.PerCPUCompartmentTransfer.
                                                                               PointerIdx,
                                                                               YamlConfig.PerCPUCompartmentTransfer.
                                                                               CompartmentIdx,
                                                                               YamlConfig.PerCPUCompartmentTransfer.
                                                                               DivisionIdx,
                                                                               YamlConfig.PerCPUCompartmentTransfer.
                                                                               SizeIdx);
        } else {
            PerCPUCompartmentTransfer = DefaultCompartmentTransfer;
        }

        for (auto &SupportFunctionDefinition: YamlConfig.CompartmentalizationSupportFunctions) {
            auto *F = SupportFunctionDefinition.GetFunction(GetModule());
            if (F) {
                CompartmentalizationSupportFunctionList.push_back(F);
            }
        }

        SignWithDivisionFunction = YamlConfig.SignWithDivision.GetFunction(GetModule());
        for (auto &StructName: YamlConfig.IgnoredTypes) {
          auto *Ty = StructType::getTypeByName(GetModule().getContext(), StructName);
          if (Ty) {
            IgnoredTypeSet.insert(Ty);
          }
        }


        for (auto &GlobalName: YamlConfig.IgnoredGlobals) {
            auto *GV = GetModule().getGlobalVariable(GlobalName, true);
            if (GV) {
                IgnoredGlobalList.push_back(GV);
            }
        }

        for (const auto &AllocationDefinition: YamlConfig.AllocationFunctions) {
            auto Allocation = HAKCAllocationSize::FromYaml(AllocationDefinition, GetModule());
            if (Allocation) {
                AllocationFunctionList.push_back(Allocation);
            }
        }

        SmallVector<HAKCTypeP> Types;
        // ProcessDebugInfo must happen before creating custom transfers
        TypeIdentifier.ProcessDebugInfo();
        TypeIdentifier.GetHAKCTypes(Types);
        for (auto &CustomTransferDefinition: YamlConfig.CustomTransferFunctions) {
            for (auto &HAKCTy: Types) {
                if (CustomTransferDefinition.TypeName == HAKCTy) {
                    auto *F = CustomTransferDefinition.GetFunction(GetModule());
                    auto CustomTransfer = std::make_shared<HAKCCustomTransfer>(F, HAKCTy,
                                                                               CustomTransferDefinition.PointerIdx,
                                                                               CustomTransferDefinition.CompartmentIdx,
                                                                               CustomTransferDefinition.DivisionIdx,
                                                                               CustomTransferDefinition.SizeIdx);
                    CustomTransferList.push_back(CustomTransfer);
                }
            }
        }
    }

    bool HAKCSystemInformation::OutputDebugInfo() const {
        return DebugOutput;
    }

    bool HAKCSystemInformation::OutputDebugInfo(GlobalValue *GV) const {
        auto Search = [GV](GlobalValue *Symbol) {
            return Symbol == GV;
        };

        return OutputDebugInfo() || llvm::any_of(SymbolsToOutputDebugInfo, Search);
    }

    Module &HAKCSystemInformation::GetModule() {
        return CommonAnalysis.GetModule();
    }

    hakc::HAKCPassModeTypeEnum HAKCSystemInformation::GetPassMode() const {
        return PassMode;
    }

    StringRef HAKCSystemInformation::GetSourcePath() const {
        return SourcePath;
    }

    StringRef HAKCSystemInformation::GetBuildPath() const {
        return BuildPath;
    }

    StringRef HAKCSystemInformation::GetDagAnalysisRootPath() const {
        return DagAnalysisRootPath;
    }

    Function *HAKCSystemInformation::CodeValidation() const {
        return CodeValidationFunction;
    }


    Function *HAKCSystemInformation::DataValidation() const {
        return DataValidationFunction;
    }

    Function *HAKCSystemInformation::SignWithDivision() const {
        return SignWithDivisionFunction;
    }

    HAKCTypeIdentifier &HAKCSystemInformation::GetTypeIdentifier() {
        return TypeIdentifier;
    }

    hakc::hakc_transfer_def_t HAKCSystemInformation::CompartmentTransfer(bool PerCPU) const {
        if (PerCPU) {
            return PerCPUCompartmentTransfer;
        } else {
            return DefaultCompartmentTransfer;
        }
    }

    bool HAKCSystemInformation::OutputDebugInfo(StringRef SymbolName) const {
        auto Search = [SymbolName](GlobalValue *Symbol) {
            return Symbol->getName() == SymbolName;
        };

        return OutputDebugInfo() || llvm::any_of(SymbolsToOutputDebugInfo, Search);
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::NoTransferFunctions() {
        return make_range(NoTransferFunctionList.begin(), NoTransferFunctionList.end());
    }

    iterator_range<HAKCTransferList::iterator> HAKCSystemInformation::CompartmentTransferFunctions() {
        return make_range(CompartmentTransferFunctionList.begin(), CompartmentTransferFunctionList.end());
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::CompartmentalizationSupportFunctions() {
        return make_range(CompartmentalizationSupportFunctionList.begin(),
                          CompartmentalizationSupportFunctionList.end());
    }

    iterator_range<FunctionList::iterator> HAKCSystemInformation::SafeTransitionFunctions() {
        return make_range(SafeTransitionFunctionList.begin(), SafeTransitionFunctionList.end());
    }

    iterator_range<HAKCTypeSet::iterator> HAKCSystemInformation::IgnoredTypes() {
        return make_range(IgnoredTypeSet.begin(), IgnoredTypeSet.end());
    }

    iterator_range<HAKCGlobalVariableList::iterator> HAKCSystemInformation::IgnoredGlobals() {
        return make_range(IgnoredGlobalList.begin(), IgnoredGlobalList.end());
    }

    iterator_range<HAKCStringList::iterator> HAKCSystemInformation::SeparateNamespacePaths() {
        return make_range(SeparateNamespacePathList.begin(), SeparateNamespacePathList.end());
    }

    iterator_range<HAKCStringList::iterator> HAKCSystemInformation::HAKCSourcePaths() {
        return make_range(HAKCSourcePathList.begin(), HAKCSourcePathList.end());
    }

    iterator_range<HAKCCustomTransferList::iterator> HAKCSystemInformation::HAKCCustomTransfers() {
        return make_range(CustomTransferList.begin(), CustomTransferList.end());
    }

    iterator_range<HAKCCustomAllocationList::iterator> HAKCSystemInformation::AllocationFunctions() {
        return make_range(AllocationFunctionList.begin(), AllocationFunctionList.end());
    }

    iterator_range<HAKCStringList::iterator> HAKCSystemInformation::IncludePaths() {
        return make_range(IncludePathsList.begin(), IncludePathsList.end());
    }

    StringRef HAKCSystemInformation::GetArch() const {
        return Arch;
    }

    StringRef HAKCSystemInformation::GetPlatform() const {
        return Platform;
    }

    const HAKCDatabaseInformation &HAKCSystemInformation::GetDatabaseInformation() const {
        return DatabaseInformation;
    }
} // hakc
