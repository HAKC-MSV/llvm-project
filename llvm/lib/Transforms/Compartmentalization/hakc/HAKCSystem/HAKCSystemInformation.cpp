//
// Created by de29664 on 11/7/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

#include "llvm/Support/Threading.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferAction.h"

namespace llvm::hakc {

HAKCSystemInformation::HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis)
    : CommonAnalysis(CommonAnalysis), ConsoleLogLevel(Verbose), FileLogLevel(Verbose),
      BuildMode(InvalidBuildModeType),
      TypeIdentifier(CommonAnalysis), DebugDatabase(),
      CodeValidationFunction(nullptr), DataValidationFunction(nullptr),
      DefaultCompartmentTransfer(nullptr), PerCPUCompartmentTransfer(nullptr),
      SignWithDivisionFunction(nullptr), Timeout(), DefaultAccessToken(), DefaultCompartmentID(),
      DefaultDivisionID(), DefaultEntryToken(), MaxConnectionRetries(),
      ServerCoreCount() {}

StringRef HAKCSystemInformation::GetRootPath() const { return RootPath; }

StringRef HAKCSystemInformation::GetSocketPath() const { return SocketPath; }

StringRef HAKCSystemInformation::GetLogPath() const { return LogPath; }

unsigned HAKCSystemInformation::GetDefaultDivisionID() const {
  return DefaultDivisionID;
}

unsigned HAKCSystemInformation::GetDefaultCompartmentID() const {
  return DefaultCompartmentID;
}

unsigned HAKCSystemInformation::GetDefaultEntryToken() const {
  return DefaultEntryToken;
}

unsigned HAKCSystemInformation::GetDefaultAccessToken() const {
  return DefaultAccessToken;
}

unsigned HAKCSystemInformation::GetServerCoreCount() const {
  return ServerCoreCount;
}

StringRef HAKCSystemInformation::GetCompartmentEndpoint() const {
  return CompartmentEndpoint;
}

StringRef HAKCSystemInformation::GetDivisionEndpoint() const {
  return DivisionEndpoint;
}

StringRef HAKCSystemInformation::GetSymbolDivisionEndpoint() const {
  return SymbolDivisionEndpoint;
}

StringRef HAKCSystemInformation::GetValidTargetsEndpoint() const {
  return ValidTargetsEndpoint;
}

StringRef HAKCSystemInformation::GetSetDagFilenameEndpoint() const {
  return SetDagFilenameEndpoint;
}

StringRef HAKCSystemInformation::GetAddSymbolsEndpoint() const {
  return AddSymbolsEndpoint;
}

StringRef HAKCSystemInformation::GetAddFunctionEndpoint() const {
  return AddFunctionEndpoint;
}

StringRef HAKCSystemInformation::GetAddGlobalVariableEndpoint() const {
  return AddGlobalVariableEndpoint;
}

StringRef HAKCSystemInformation::GetTerminateConnectionEndpoint() const {
  return TerminateConnectionEndpoint;
}

StringRef HAKCSystemInformation::GetBuildPath() const { return BuildPath; }

HAKCBuildModeTypeEnum HAKCSystemInformation::GetBuildMode() const {
  return BuildMode;
}

function_def_t HAKCSystemInformation::CreateHAKCFunction(
    HAKCYAMLFunctionDefinition &YAMLFunctionDef) const {
  auto *TransferFunc = YAMLFunctionDef.GetFunction(TypeIdentifier);
  if (!TransferFunc) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find function " << YAMLFunctionDef.SymbolName << "\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Debug)
      << "Found HAKCFunction " << TransferFunc << " with Type "
      << TransferFunc->getFunctionType() << "\n";

  SmallVector<HAKCFunctionArgumentDefinition> Args;
  PopulateHAKCFunctionArgs(Args, YAMLFunctionDef);
  return std::make_shared<HAKCFunctionDefinition>(TransferFunc, Args);
}

custom_transfer_def_t HAKCSystemInformation::CreateCustomTransferFunction(
    HAKCYAMLCustomTransferType &YAMLCustomTransfer, HAKCTypeP HAKCTy) const {
  auto *TransferFunc = YAMLCustomTransfer.GetFunction(TypeIdentifier);
  if (!TransferFunc) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find function " << YAMLCustomTransfer.SymbolName << "\n";
    throw std::exception();
  }
  SmallVector<HAKCFunctionArgumentDefinition> Args;
  PopulateHAKCFunctionArgs(Args, YAMLCustomTransfer);
  return std::make_shared<HAKCCustomTransfer>(TransferFunc, HAKCTy, Args);
}

std::chrono::milliseconds HAKCSystemInformation::GetServerTimeout() const {
  return Timeout;
}

unsigned HAKCSystemInformation::GetMaxRetries() const {
  return MaxConnectionRetries;
}

void HAKCSystemInformation::PopulateHAKCFunctionArgs(
    SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args,
    HAKCYAMLFunctionDefinition &YAMLFunctionDef) const {
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
    SmallVectorImpl<function_def_t> &Results) {
  Results.append({CodeValidationFunction, DataValidationFunction,
                  SignWithDivisionFunction, DefaultCompartmentTransfer,
                  PerCPUCompartmentTransfer});
  Results.append(CustomTransferList.begin(), CustomTransferList.end());
  Results.append(CompartmentalizationSupportFunctionList.begin(),
                 CompartmentalizationSupportFunctionList.end());
}



bool HAKCSystemInformation::ShouldSkipCurrentFile() {
  const StringRef CurrentSourceName(GetModule().getSourceFileName());
  for (auto &path : HAKCSourcePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning) << "Skipping hakc source " << CurrentSourceName << "\n";
      return true;
    }
  }

  for (auto &path : SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning) << "Skipping separate namespace source " << CurrentSourceName << "\n";
      return true;
    }
  }
  return false;
}

bool HAKCSystemInformation::GetSkipCurrentFile() const {
  return skip_current_file;
}

void HAKCSystemInformation::operator<<(HAKCYAMLConfig &Config) {
  auto Endpoints = Config.Endpoints;

  BuildPath = Config.BuildDir;
  BuildMode = Config.BuildMode;
  if (BuildMode == RunConfigAndExit) {
    return;
  }

  // parse config files first to check whether a file should be skipped or a log stream should be created
  for (auto &FileType : Config.ClientConfig.SeparateNamespacePaths) {
    FileType.AddAllFiles(SeparateNamespacePathList);
  }

  for (auto &FileType : Config.ClientConfig.HAKCSourcePaths) {
    FileType.AddAllFiles(HAKCSourcePathList);
  }

  if (ShouldSkipCurrentFile()) {
    skip_current_file = true;
    return;
  }

  // Creating fd log as soon as possible
  CommonAnalysis.getHAKCLoggerObject().SetConsoleConfiguredLogLevels(Config.ClientConfig.ConsoleLogLevel); // setting the errs() stream to
  // the configured log level
  CommonAnalysis.getHAKCLoggerObject().addStream(CommonAnalysis.createLogPath(Config.BuildDir, BuildMode), Config.ClientConfig.FileLogLevel); // setting the fd_ostream to configured log level

  SocketPath = Config.SocketDir + "/" +
               std::to_string(get_threadid() % Config.ServerCoreCount);
  LogPath = Config.LogDir;
  ServerCoreCount = Config.ServerCoreCount;
  CompartmentEndpoint = Endpoints.GetCompartmentEndpoint;
  DivisionEndpoint = Endpoints.GetDivisionEndpoint;
  SymbolDivisionEndpoint = Endpoints.GetSymbolDivisionEndpoint;
  ValidTargetsEndpoint = Endpoints.GetValidTargetsEndpoint;
  AddSymbolsEndpoint = Endpoints.AddSymbolsEndpoint;
  AddFunctionEndpoint = Endpoints.AddFunctionEndpoint;
  AddGlobalVariableEndpoint = Endpoints.AddGlobalVariableEndpoint;
  TerminateConnectionEndpoint = Endpoints.TerminateConnectionEndpoint;
  Arch = Config.ClientConfig.Arch;
  Platform = Config.ClientConfig.Platform;
  ConsoleLogLevel = Config.ClientConfig.ConsoleLogLevel;
  FileLogLevel = Config.ClientConfig.FileLogLevel;
  DefaultCompartmentID = Config.DefaultCompartmentID;
  DefaultDivisionID = Config.DefaultDivisionID;
  DefaultEntryToken = Config.DefaultEntryToken;
  DefaultAccessToken = Config.DefaultAccessToken;

  // ProcessDebugInfo must happen before creating custom transfers
  // dag analysis actually happens here!
  TypeIdentifier.ProcessDebugInfo();

  for (auto &NoTransferFunction : Config.ClientConfig.NoTransferFunctions) {
    if (auto *F = GetModule().getFunction(NoTransferFunction.SymbolName)) {
      NoTransferFunctionList.push_back(F);
    }
  }

  for (auto &SymbolName : Config.ClientConfig.PassDebugSymbols) {
    if (auto *F = GetModule().getFunction(SymbolName)) {
      SymbolsToOutputDebugInfo.push_back(F);
    } else if (auto *Global = GetModule().getGlobalVariable(SymbolName)) {
      SymbolsToOutputDebugInfo.push_back(Global);
    } else {
      errs() << "\t\t Could not find Symbol " << SymbolName << "\n";
      throw std::exception();
    }
  }

  CodeValidationFunction =
      CreateHAKCFunction(Config.ClientConfig.CodeValidationFunction);
  if (!CodeValidationFunction) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not get CodeValidationFunction "
        << Config.ClientConfig.CodeValidationFunction.SymbolName << "\n";
    throw std::exception();
  }
  DataValidationFunction =
      CreateHAKCFunction(Config.ClientConfig.DataValidationFunction);
  if (!DataValidationFunction) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not get DataValidationFunction "
        << Config.ClientConfig.DataValidationFunction.SymbolName << "\n";
    throw std::exception();
  }



  for (auto &SafeFunction : Config.ClientConfig.SafeTransitionFunctions) {
    if (auto *F = GetModule().getFunction(SafeFunction.SymbolName)) {
      SafeTransitionFunctionList.push_back(F);
    }
  }

  DefaultCompartmentTransfer =
      CreateHAKCFunction(Config.ClientConfig.DefaultCompartmentTransfer);
  if (Config.ClientConfig.PerCPUCompartmentTransfer.IsValid()) {
    PerCPUCompartmentTransfer =
        CreateHAKCFunction(Config.ClientConfig.PerCPUCompartmentTransfer);
  } else {
    PerCPUCompartmentTransfer = DefaultCompartmentTransfer;
  }

  for (auto &Global : Config.ClientConfig.IgnoredGlobals) {
    if (auto *GV = GetModule().getGlobalVariable(Global.SymbolName, true)) {
      IgnoredGlobalList.push_back(GV);
    }
  }

  for (const auto &AllocationDefinition :
       Config.ClientConfig.AllocationFunctions) {
    auto Allocation =
        HAKCAllocationSize::FromYaml(AllocationDefinition, GetModule());
    if (Allocation) {
      AllocationFunctionList.push_back(Allocation);
    }
  }

  SmallVector<HAKCTypeP> Types;
  TypeIdentifier.GetHAKCTypes(Types);
  for (auto &SupportFunctionDefinition :
       Config.ClientConfig.CompartmentalizationSupportFunctions) {
    auto SupportFunction = CreateHAKCFunction(SupportFunctionDefinition);
    CompartmentalizationSupportFunctionList.push_back(SupportFunction);
  }

  SignWithDivisionFunction =
      CreateHAKCFunction(Config.ClientConfig.SignWithDivision);
  for (auto &StructName : Config.ClientConfig.IgnoredTypes) {
    TypeIdentifier.AddIgnoredType(StructName);
  }

  for (auto &CustomTransferDefinition :
       Config.ClientConfig.CustomTransferFunctions) {
    for (auto &HAKCTy : Types) {
      if (CustomTransferDefinition.TransferObjectTypeName == *HAKCTy) {
        auto CustomTransfer =
            CreateCustomTransferFunction(CustomTransferDefinition, HAKCTy);
        CustomTransferList.push_back(CustomTransfer);
        break;
      }
    }
  }

  SmallVector<function_def_t> DefinedFunctions;
  GetAllDefinedHAKCFunctions(DefinedFunctions);
  for (auto &PreTransferActionDefinition : Config.ClientConfig.PreTargetActions) {
    for (auto &FuncDef : DefinedFunctions) {
      if (FuncDef->GetName() == PreTransferActionDefinition.SymbolName) {
        auto Action = std::make_shared<HAKCPreTransferAction>(
            *FuncDef, PreTransferActionDefinition.Label);
        PreTransferActionList.push_back(Action);
        break;
      }
    }
  }
  for (auto &PostTargetActionDefinition : Config.ClientConfig.PostTargetActions) {
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
  // Note: Timeout is in ms, so multiply by 1000
  Timeout = std::chrono::milliseconds(Config.ClientConfig.Timeout * 1000);
  MaxConnectionRetries = Config.ClientConfig.MaxConnectionRetries;
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
  auto Search = [GV](const GlobalValue *Symbol) { return Symbol == GV; };
  if (any_of(SymbolsToOutputDebugInfo, Search) ||
      SymbolsToOutputDebugInfo.empty()) {
    return true;
  }
  return false;
}

Module &HAKCSystemInformation::GetModule() const {
  return CommonAnalysis.GetModule();
}

StringRef HAKCSystemInformation::GetSingleSourceFile() {
  return SingleSourceFile;
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

function_def_t HAKCSystemInformation::CompartmentTransfer(const bool PerCPU) const {
  if (PerCPU) {
    return PerCPUCompartmentTransfer;
  }
  return DefaultCompartmentTransfer;
}

bool HAKCSystemInformation::OutputDebugInfo(StringRef SymbolName) const {
  auto Search = [SymbolName](const GlobalValue *Symbol) {
    errs() << "Checking " << Symbol->getName() << " =?= " << SymbolName << ": "
           << (Symbol->getName() == SymbolName) << "\n";
    return Symbol->getName() == SymbolName;
  };

  if (any_of(SymbolsToOutputDebugInfo, Search) ||
      SymbolsToOutputDebugInfo.empty()) {
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

} // namespace llvm::hakc
