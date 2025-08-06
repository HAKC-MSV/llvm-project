//
// Created by de29664 on 11/7/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

namespace llvm::hakc {
HAKCDatabaseInformation::HAKCDatabaseInformation()
    : ServerURL(), CompartmentEndpoint(), DivisionEndpoint(),
      SymbolDivisionEndpoint(), Timeout(), MaxConnectionRetries(10) {}

StringRef HAKCDatabaseInformation::GetServerURL() const { return ServerURL; }

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

unsigned HAKCDatabaseInformation::GetMaxRetries() const {
  return MaxConnectionRetries;
}

void HAKCDatabaseInformation::operator<<(
    const HAKCYamlDatabaseConfig &DatabaseConfig) {
  ServerURL = DatabaseConfig.ServerURL;
  CompartmentEndpoint = DatabaseConfig.GetCompartmentEndpoint;
  DivisionEndpoint = DatabaseConfig.GetDivisionEndpoint;
  SymbolDivisionEndpoint = DatabaseConfig.GetSymbolDivisionEndpoint;
  ValidTargetsEndpoint = DatabaseConfig.GetValidTargetsEndpoint;
  // Note: Timeout is in ms, so multiply by 1000
  Timeout = std::chrono::milliseconds(DatabaseConfig.ServerTimeout * 1000);
  MaxConnectionRetries = DatabaseConfig.MaxConnectionRetries;
}

HAKCSystemInformation::HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis)
    : CommonAnalysis(CommonAnalysis), TypeIdentifier(CommonAnalysis),
      DatabaseInformation(), ConsoleLogLevel(Verbose), FileLogLevel(Verbose),
      DebugDatabase(), PassMode(InvalidPassModeType), Arch(), Platform(),
      DagAnalysisRootPath(), IncludePathsList(), NoTransferFunctionList(),
      CompartmentTransferFunctionList(), CodeValidationFunction(nullptr),
      DataValidationFunction(nullptr), SignWithDivisionFunction(nullptr),
      DefaultCompartmentTransfer(nullptr), PerCPUCompartmentTransfer(nullptr),
      CompartmentalizationSupportFunctionList(), SymbolsToOutputDebugInfo(),
      SeparateNamespacePathList(), HAKCSourcePathList(),
      SafeTransitionFunctionList(), IgnoredGlobalList(),
      AllocationFunctionList(), CustomTransferList(), PreTransferActionList(),
      PostTargetActionList(), StructList() {}

function_def_t HAKCSystemInformation::CreateHAKCFunction(
    HAKCYAMLFunctionDefinition &YAMLFunctionDef,
    const HAKCTypeIdentifier &TypeIdentifier) const {
  auto *TransferFunc = YAMLFunctionDef.GetFunction(TypeIdentifier);
  if (!TransferFunc) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find function " << YAMLFunctionDef.SymbolName << "\n";
    throw std::exception();
  } else {
    CommonHAKCAnalysis::getLogger(Debug)
        << "Found HAKCFunction " << TransferFunc << " with Type "
        << TransferFunc->getFunctionType() << "\n";
  }
  SmallVector<HAKCFunctionArgumentDefinition> Args;
  PopulateHAKCFunctionArgs(Args, YAMLFunctionDef, TypeIdentifier);
  return std::make_shared<HAKCFunctionDefinition>(TransferFunc, Args);
}

custom_transfer_def_t HAKCSystemInformation::CreateCustomTransferFunction(
    HAKCYAMLCustomTransferType &YAMLCustomTransfer, HAKCTypeP HAKCTy,
    const HAKCTypeIdentifier &TypeIdentifier) {
  auto *TransferFunc = YAMLCustomTransfer.GetFunction(TypeIdentifier);
  if (!TransferFunc) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find function " << YAMLCustomTransfer.SymbolName << "\n";
    throw std::exception();
  }
  SmallVector<HAKCFunctionArgumentDefinition> Args;
  PopulateHAKCFunctionArgs(Args, YAMLCustomTransfer, TypeIdentifier);
  return std::make_shared<HAKCCustomTransfer>(TransferFunc, HAKCTy, Args);
}

void HAKCSystemInformation::PopulateHAKCFunctionArgs(
    SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args,
    HAKCYAMLFunctionDefinition &YAMLFunctionDef,
    const HAKCTypeIdentifier &TypeIdentifier) {
  for (auto &YAMLArg : YAMLFunctionDef.Arguments) {
    auto *ArgTy = YAMLArg.GetType(TypeIdentifier);
    if (!ArgTy) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Could not determine type for argument " << YAMLArg.Idx
          << " in definition for " << YAMLFunctionDef.SymbolName << "\n";
      throw std::exception();
    }
    HAKCFunctionArgumentDefinition Arg(ArgTy, YAMLArg.Idx, YAMLArg.ArgUse);
    Args.push_back(Arg);
  }
}

void HAKCSystemInformation::GetAllDefinedHAKCFunctions(
    SmallVectorImpl<hakc::function_def_t> &Results) {
  Results.append({CodeValidationFunction, DataValidationFunction,
                  SignWithDivisionFunction, DefaultCompartmentTransfer,
                  PerCPUCompartmentTransfer});
  Results.append(CustomTransferList.begin(), CustomTransferList.end());
  Results.append(CompartmentalizationSupportFunctionList.begin(),
                 CompartmentalizationSupportFunctionList.end());
}

void HAKCSystemInformation::operator<<(HAKCYamlConfig &YamlConfig) {
  Arch = YamlConfig.Arch;
  Platform = YamlConfig.Platform;
  DagAnalysisRootPath = YamlConfig.DagAnalysisRootPath;
  PassMode = YamlConfig.PassMode;
  TemporalAnalysisEnabled = YamlConfig.TemporalAnalysisEnabled;
  if (PassMode == RunConfigAndExit) {
    return;
  }
  ConsoleLogLevel = YamlConfig.ConsoleLogLevel;
  FileLogLevel = YamlConfig.FileLogLevel;
  DatabaseInformation << YamlConfig.DatabaseConfig;
  if (PassMode == RunDataAccessGraphAnalysisSingleSourceFile) {
    SingleSourceFile = YamlConfig.SingleSourceFile;
  }

  // ProcessDebugInfo must happen before creating custom transfers
  // dag analysis actually happens here!
  if (PassMode == RunDataAccessGraphAnalysis) {
    TypeIdentifier.ProcessDebugInfo();
  } else {
    TypeIdentifier.ProcessDebugInfo();
  }

  for (auto &NoTransferFunction : YamlConfig.NoTransferFunctions) {
    if (auto *F = GetModule().getFunction(NoTransferFunction.SymbolName)) {
      NoTransferFunctionList.push_back(F);
    }
  }

  // errs() << "Constructing SymbolsToOutputDebugInfo from YamlConfig.PassDebugSymbols in file: " << GetModule().getSourceFileName() <<"\n";
  //
  // for (auto& F : GetModule().getFunctionList()) {
  //   errs() << "fn: " << F.getName() << "\n";
  // }

  for (auto &SymbolName : YamlConfig.PassDebugSymbols) {
    // errs() << "\t SymbolName: " << SymbolName << "\n";
    if (auto *F = GetModule().getFunction(SymbolName)) {
      // errs() << "\t\t found *F: " << *F << "\n";
      SymbolsToOutputDebugInfo.push_back(F);
    } else if (auto *Global = GetModule().getGlobalVariable(SymbolName)) {
      // errs() << "\t\t found *Global: " << *Global << "\n";
      SymbolsToOutputDebugInfo.push_back(Global);
    }
    else {
      errs() << "\t\t Could not find Symbol " << SymbolName << "\n";
      throw std::exception();
    }
  }

  CodeValidationFunction =
      CreateHAKCFunction(YamlConfig.CodeValidationFunction, TypeIdentifier);
  if (!CodeValidationFunction) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not get CodeValidationFunction "
        << YamlConfig.CodeValidationFunction.SymbolName << "\n";
    throw std::exception();
  }
  DataValidationFunction =
      CreateHAKCFunction(YamlConfig.DataValidationFunction, TypeIdentifier);
  if (!DataValidationFunction) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not get DataValidationFunction "
        << YamlConfig.DataValidationFunction.SymbolName << "\n";
    throw std::exception();
  }

  for (auto &FileType : YamlConfig.SeparateNamespacePaths) {
    FileType.AddAllFiles(SeparateNamespacePathList);
  }

  for (auto &FileType : YamlConfig.HAKCSourcePaths) {
    FileType.AddAllFiles(HAKCSourcePathList);
  }

  for (auto &SafeFunction : YamlConfig.SafeTransitionFunctions) {
    if (auto *F = GetModule().getFunction(SafeFunction.SymbolName)) {
      SafeTransitionFunctionList.push_back(F);
    }
  }

  DefaultCompartmentTransfer =
      CreateHAKCFunction(YamlConfig.DefaultCompartmentTransfer, TypeIdentifier);
  if (YamlConfig.PerCPUCompartmentTransfer.IsValid()) {
    PerCPUCompartmentTransfer = CreateHAKCFunction(
        YamlConfig.PerCPUCompartmentTransfer, TypeIdentifier);
  } else {
    PerCPUCompartmentTransfer = DefaultCompartmentTransfer;
  }

  for (auto &Global : YamlConfig.IgnoredGlobals) {
    if (auto *GV = GetModule().getGlobalVariable(Global.SymbolName, true)) {
      IgnoredGlobalList.push_back(GV);
    }
  }

  for (const auto &AllocationDefinition : YamlConfig.AllocationFunctions) {
    auto Allocation =
        HAKCAllocationSize::FromYaml(AllocationDefinition, GetModule());
    if (Allocation) {
      AllocationFunctionList.push_back(Allocation);
    }
  }

  SmallVector<HAKCTypeP> Types;
  TypeIdentifier.GetHAKCTypes(Types);
  for (auto &SupportFunctionDefinition :
       YamlConfig.CompartmentalizationSupportFunctions) {
    auto SupportFunction =
        CreateHAKCFunction(SupportFunctionDefinition, TypeIdentifier);
    CompartmentalizationSupportFunctionList.push_back(SupportFunction);
  }

  SignWithDivisionFunction =
      CreateHAKCFunction(YamlConfig.SignWithDivision, TypeIdentifier);
  for (auto &StructName : YamlConfig.IgnoredTypes) {
    TypeIdentifier.AddIgnoredType(StructName);
  }

  for (auto &CustomTransferDefinition : YamlConfig.CustomTransferFunctions) {
    for (auto &HAKCTy : Types) {
      if (CustomTransferDefinition.TransferObjectTypeName == *HAKCTy) {
        auto CustomTransfer = CreateCustomTransferFunction(
            CustomTransferDefinition, HAKCTy, TypeIdentifier);
        CustomTransferList.push_back(CustomTransfer);
        break;
      }
    }
  }

  SmallVector<hakc::function_def_t> DefinedFunctions;
  GetAllDefinedHAKCFunctions(DefinedFunctions);
  for (auto &PreTransferActionDefinition : YamlConfig.PreTargetActions) {
    for (auto &FuncDef : DefinedFunctions) {
      if (FuncDef->GetName() == PreTransferActionDefinition.SymbolName) {
        auto Action = std::make_shared<HAKCPreTransferAction>(
            *FuncDef, PreTransferActionDefinition.Label);
        PreTransferActionList.push_back(Action);
        break;
      }
    }
  }
  for (auto &PostTargetActionDefinition : YamlConfig.PostTargetActions) {
    for (auto &FuncDef : DefinedFunctions) {
      if (FuncDef->GetName() == PostTargetActionDefinition.SymbolName) {
        SmallVector<HAKCActionArgument> Arguments;
        for (auto Arg : PostTargetActionDefinition.Arguments) {
          HAKCActionArgument Argument(Arg.Idx, Arg.Label);
          Arguments.push_back(Argument);
        }
        auto Action = std::make_shared<HAKCPostTargetAction>(
            *FuncDef, PostTargetActionDefinition.Label, Arguments);
        PostTargetActionList.push_back(Action);
        break;
      }
    }
  }
}

HAKCLogLevel HAKCSystemInformation::GetConsoleLogLevel() const {
  return ConsoleLogLevel;
}

HAKCLogLevel HAKCSystemInformation::GetFileLogLevel() const {
  return FileLogLevel;
}

bool HAKCSystemInformation::GetDebugDatabase() const { return DebugDatabase; }

bool HAKCSystemInformation::OutputDebugInfo(GlobalValue *GV) const {
  // will always try to output debug info if there are no symbols specified
  auto Search = [GV](const GlobalValue *Symbol) {
    // errs() << "Checking " << Symbol->getName() << " =?= " << GV->getName() << ": " << (Symbol == GV) << "\n";
    return Symbol == GV;
  };
  if (llvm::any_of(SymbolsToOutputDebugInfo, Search) || SymbolsToOutputDebugInfo.empty()) {
    // errs() << "FOUND\n";
    return true;
  }
  return false;
}

Module &HAKCSystemInformation::GetModule() const {
  return CommonAnalysis.GetModule();
}

HAKCPassModeTypeEnum HAKCSystemInformation::GetPassMode() const {
  return PassMode;
}

bool HAKCSystemInformation::GetTemporalAnalysisEnabled() const {
  return TemporalAnalysisEnabled;
}

StringRef HAKCSystemInformation::GetSingleSourceFile() {
  return SingleSourceFile;
}

StringRef HAKCSystemInformation::GetDagAnalysisRootPath() const {
  return DagAnalysisRootPath;
}

HAKCStructList HAKCSystemInformation::GetStructList() const {
  return StructList;
}

function_def_t HAKCSystemInformation::CodeValidation() const {
  return CodeValidationFunction;
}

function_def_t HAKCSystemInformation::DataValidation() const {
  return DataValidationFunction;
}

function_def_t HAKCSystemInformation::SignWithDivision() const {
  return SignWithDivisionFunction;
}

HAKCTypeIdentifier &HAKCSystemInformation::GetTypeIdentifier() {
  return TypeIdentifier;
}

function_def_t HAKCSystemInformation::CompartmentTransfer(bool PerCPU) const {
  if (PerCPU) {
    return PerCPUCompartmentTransfer;
  } else {
    return DefaultCompartmentTransfer;
  }
}

bool HAKCSystemInformation::OutputDebugInfo(StringRef SymbolName) const {
  auto Search = [SymbolName](const GlobalValue *Symbol) {
    errs() << "Checking " << Symbol->getName() << " =?= " << SymbolName << ": " << (Symbol->getName() == SymbolName) << "\n";
    return Symbol->getName() == SymbolName;
  };

  if (llvm::any_of(SymbolsToOutputDebugInfo, Search) || SymbolsToOutputDebugInfo.empty()) {
    return true;
  }
  return false;
}

iterator_range<FunctionList::iterator>
HAKCSystemInformation::NoTransferFunctions() {
  return make_range(NoTransferFunctionList.begin(),
                    NoTransferFunctionList.end());
}

iterator_range<HAKCTransferList::iterator>
HAKCSystemInformation::CompartmentTransferFunctions() {
  return make_range(CompartmentTransferFunctionList.begin(),
                    CompartmentTransferFunctionList.end());
}

iterator_range<HAKCFunctionList::iterator>
HAKCSystemInformation::CompartmentalizationSupportFunctions() {
  return make_range(CompartmentalizationSupportFunctionList.begin(),
                    CompartmentalizationSupportFunctionList.end());
}

iterator_range<FunctionList::iterator>
HAKCSystemInformation::SafeTransitionFunctions() {
  return make_range(SafeTransitionFunctionList.begin(),
                    SafeTransitionFunctionList.end());
}

iterator_range<HAKCGlobalVariableList::iterator>
HAKCSystemInformation::IgnoredGlobals() {
  return make_range(IgnoredGlobalList.begin(), IgnoredGlobalList.end());
}

iterator_range<HAKCStringList::iterator>
HAKCSystemInformation::SeparateNamespacePaths() {
  return make_range(SeparateNamespacePathList.begin(),
                    SeparateNamespacePathList.end());
}

iterator_range<HAKCStringList::iterator>
HAKCSystemInformation::HAKCSourcePaths() {
  return make_range(HAKCSourcePathList.begin(), HAKCSourcePathList.end());
}

iterator_range<HAKCCustomTransferList::iterator>
HAKCSystemInformation::HAKCCustomTransfers() {
  return make_range(CustomTransferList.begin(), CustomTransferList.end());
}

iterator_range<HAKCCustomAllocationList::iterator>
HAKCSystemInformation::AllocationFunctions() {
  return make_range(AllocationFunctionList.begin(),
                    AllocationFunctionList.end());
}

iterator_range<HAKCStringList::iterator> HAKCSystemInformation::IncludePaths() {
  return make_range(IncludePathsList.begin(), IncludePathsList.end());
}

iterator_range<HAKCPreTransferActionList::iterator>
HAKCSystemInformation::PreTransferActions() {
  return make_range(PreTransferActionList.begin(), PreTransferActionList.end());
}

iterator_range<HAKCPostTargetActionList::iterator>
HAKCSystemInformation::PostTargetActions() {
  return make_range(PostTargetActionList.begin(), PostTargetActionList.end());
}

StringRef HAKCSystemInformation::GetArch() const { return Arch; }

StringRef HAKCSystemInformation::GetPlatform() const { return Platform; }

const HAKCDatabaseInformation &
HAKCSystemInformation::GetDatabaseInformation() const {
  return DatabaseInformation;
}
} // namespace llvm::hakc
