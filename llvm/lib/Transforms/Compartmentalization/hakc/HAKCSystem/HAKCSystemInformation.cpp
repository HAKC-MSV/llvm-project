//
// Created by de29664 on 11/7/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

namespace llvm::hakc {
HAKCDatabaseInformation::HAKCDatabaseInformation()
    : ServerURL(), CompartmentEndpoint(), DivisionEndpoint(),
      SymbolDivisionEndpoint(), Timeout() {}

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

void HAKCDatabaseInformation::operator<<(
    HAKCYamlDatabaseConfig &DatabaseConfig) {
  ServerURL = DatabaseConfig.ServerURL;
  CompartmentEndpoint = DatabaseConfig.GetCompartmentEndpoint;
  DivisionEndpoint = DatabaseConfig.GetDivisionEndpoint;
  SymbolDivisionEndpoint = DatabaseConfig.GetSymbolDivisionEndpoint;
  ValidTargetsEndpoint = DatabaseConfig.GetValidTargetsEndpoint;
  Timeout = std::chrono::milliseconds(DatabaseConfig.ServerTimeout);
}

HAKCSystemInformation::HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis)
    : CommonAnalysis(CommonAnalysis), TypeIdentifier(CommonAnalysis),
      DatabaseInformation(), DebugOutput(false), PassMode(InvalidPassModeType),
      Arch(), Platform(), SourcePath(), BuildPath(), DagAnalysisRootPath(),
      IncludePathsList(), NoTransferFunctionList(),
      CompartmentTransferFunctionList(), CodeValidationFunction(nullptr),
      DataValidationFunction(nullptr), SignWithDivisionFunction(nullptr),
      DefaultCompartmentTransfer(nullptr), PerCPUCompartmentTransfer(nullptr),
      CompartmentalizationSupportFunctionList(), SymbolsToOutputDebugInfo(),
      SeparateNamespacePathList(), HAKCSourcePathList(),
      SafeTransitionFunctionList(), IgnoredTypeSet(), IgnoredGlobalList(),
      AllocationFunctionList(), CustomTransferList(), TransferState() {}

void HAKCSystemInformation::operator<<(HAKCYamlConfig &YamlConfig) {
  Arch = YamlConfig.Arch;
  Platform = YamlConfig.Platform;
  SourcePath = YamlConfig.SourcePath;
  BuildPath = YamlConfig.BuildPath;
  DagAnalysisRootPath = YamlConfig.DagAnalysisRootPath;
  PassMode = YamlConfig.PassMode;
  DebugOutput = YamlConfig.OutputAllDebugInfo;
  DatabaseInformation << YamlConfig.DatabaseConfig;

  // ProcessDebugInfo must happen before creating custom transfers
  TypeIdentifier.ProcessDebugInfo();

  for (auto &FunctionName : YamlConfig.NoTransferFunctions) {
    auto *F = GetModule().getFunction(FunctionName);
    if (F) {
      NoTransferFunctionList.push_back(F);
    }
  }

  for (auto &SymbolName : YamlConfig.PassDebugSymbols) {
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

  CodeValidationFunction =
      YamlConfig.CodeValidationFunction.GetFunction(TypeIdentifier);
  if (!CodeValidationFunction) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not get CodeValidationFunction "
        << YamlConfig.CodeValidationFunction.Name << "\n";
    throw std::exception();
  }
  DataValidationFunction =
      YamlConfig.DataValidationFunction.GetFunction(TypeIdentifier);
  if (!DataValidationFunction) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not get DataValidationFunction "
        << YamlConfig.DataValidationFunction.Name << "\n";
    throw std::exception();
  }

  IncludePathsList.append(YamlConfig.IncludePathsList.begin(),
                          YamlConfig.IncludePathsList.end());

  for (auto &FileType : YamlConfig.SeparateNamespacePaths) {
    auto PathRoot = FileType.PathRoot;
    for (auto &FileName : FileType.Files) {
      auto File = PathRoot + FileName;
      YamlConfig.SeparateNamespacePathsList.push_back(File);
    }
  }
  SeparateNamespacePathList.append(
      YamlConfig.SeparateNamespacePathsList.begin(),
      YamlConfig.SeparateNamespacePathsList.end());

  for (auto &FileType : YamlConfig.HAKCSourcePaths) {
    auto PathRoot = FileType.PathRoot;
    for (auto &FileName : FileType.Files) {
      auto File = PathRoot + FileName;
      YamlConfig.HAKCSourcePathsList.push_back(File);
    }
  }
  HAKCSourcePathList.append(YamlConfig.HAKCSourcePathsList.begin(),
                            YamlConfig.HAKCSourcePathsList.end());

  for (auto &FunctionName : YamlConfig.SafeTransitionFunctions) {
    auto *F = GetModule().getFunction(FunctionName);
    if (F) {
      SafeTransitionFunctionList.push_back(F);
    }
  }

  auto *DefaultTransferFunc =
      YamlConfig.DefaultCompartmentTransfer.GetFunction(TypeIdentifier);
  if (!DefaultTransferFunc) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find DefaultCompartmentTransfer "
        << YamlConfig.DefaultCompartmentTransfer.Name << "\n";
    throw std::exception();
  }
  DefaultCompartmentTransfer = std::make_shared<HAKCTransferFunction>(
      DefaultTransferFunc,
      YamlConfig.DefaultCompartmentTransfer.ParameterNameToIndex["address"],
      YamlConfig.DefaultCompartmentTransfer.ParameterNameToIndex["size"],
      YamlConfig.DefaultCompartmentTransfer.ParameterNameToIndex["compartment"],
      YamlConfig.DefaultCompartmentTransfer.ParameterNameToIndex["division"],
      YamlConfig.DefaultCompartmentTransfer.ParameterNameToIndex["is_code"]);
  if (YamlConfig.PerCPUCompartmentTransfer.IsValid()) {
    auto *PerCPUTransferFunc =
        YamlConfig.PerCPUCompartmentTransfer.GetFunction(TypeIdentifier);
    if (!PerCPUTransferFunc) {
      CommonHAKCAnalysis::getWriter(true)
          << "Could not find PerCPUCompartmentTransfer "
          << YamlConfig.PerCPUCompartmentTransfer.Name << "\n";
      throw std::exception();
    }
    PerCPUCompartmentTransfer = std::make_shared<HAKCTransferFunction>(
        PerCPUTransferFunc,
        YamlConfig.PerCPUCompartmentTransfer.ParameterNameToIndex["address"],
        YamlConfig.PerCPUCompartmentTransfer.ParameterNameToIndex["size"],
        YamlConfig.PerCPUCompartmentTransfer
            .ParameterNameToIndex["compartment"],
        YamlConfig.PerCPUCompartmentTransfer.ParameterNameToIndex["division"]);
  } else {
    PerCPUCompartmentTransfer = DefaultCompartmentTransfer;
  }

  for (auto &GlobalName : YamlConfig.IgnoredGlobals) {
    auto *GV = GetModule().getGlobalVariable(GlobalName, true);
    if (GV) {
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
    auto *F = SupportFunctionDefinition.GetFunction(TypeIdentifier);
    if (F) {
      CompartmentalizationSupportFunctionList.push_back(F);
    }
  }

  SignWithDivisionFunction =
      YamlConfig.SignWithDivision.GetFunction(TypeIdentifier);
  for (auto &StructName : YamlConfig.IgnoredTypes) {
    auto *Ty = StructType::getTypeByName(GetModule().getContext(), StructName);
    if (Ty) {
      IgnoredTypeSet.insert(Ty);
    }
  }

  for (auto &CustomTransferDefinition : YamlConfig.CustomTransferFunctions) {
    for (auto &HAKCTy : Types) {
      if (CustomTransferDefinition.TypeName == HAKCTy) {
        auto *F = CustomTransferDefinition.GetFunction(TypeIdentifier);
        auto CustomTransfer = std::make_shared<HAKCCustomTransfer>(
            F, HAKCTy, CustomTransferDefinition.PointerIdx,
            CustomTransferDefinition.CompartmentIdx,
            CustomTransferDefinition.DivisionIdx,
            CustomTransferDefinition.SizeIdx);
        CustomTransferList.push_back(CustomTransfer);
      }
    }
  }
  // Loop through the pre transfer actions and construct HAKCPreTransferAction
  // object
  for (auto &PreTransferActionDefinition : YamlConfig.PreTransferActions) {
    CommonAnalysis.getWriter(true)
        << "found pre transfer action: " << PreTransferActionDefinition.Name
        << "\n";
    auto *F = GetModule().getFunction(PreTransferActionDefinition.Name);
    if (F) {
      CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
          << "Found PreTransferAction " << PreTransferActionDefinition.Name
          << "\n";
      // construct a PreTransferAction object, and create state later
      std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel;
      for (auto Arg : PreTransferActionDefinition.Arguments) {
        hakc_label_ref_t label_ref =
            TransferState.AddActionArgumentLabel(Arg.label);
        ArgToLabel[Arg.idx] = label_ref;
      }
      auto PreTransferAction =
          std::make_shared<HAKCPreTransferAction>(F, ArgToLabel);
      TransferState.AddPreTransferAction(PreTransferAction);
    } else {
      CommonAnalysis.getWriter(true)
          << "Could not find PreTransferAction "
          << PreTransferActionDefinition.Name << "\n";
      throw std::exception();
    }
  }
  for (auto &PostTargetActionDefinition : YamlConfig.PostTargetActions) {
    // find the HAKC Function pointer that matches the function name
    auto *F = GetModule().getFunction(PostTargetActionDefinition.Name);
    if (F) {
      CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
          << "Found PostTargetAction " << PostTargetActionDefinition.Name
          << "\n";
      std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel;
      for (auto Arg : PostTargetActionDefinition.Arguments) {
        hakc_label_ref_t label_ref =
            TransferState.AddActionArgumentLabel(Arg.label);
        ArgToLabel[Arg.idx] = label_ref;
      }
      auto PostTargetAction =
          std::make_shared<HAKCPostTargetAction>(F, ArgToLabel);
      TransferState.AddPostTargetAction(PostTargetAction);
    } else {
      CommonAnalysis.getWriter(true) << "Could not find PostTargetAction "
                                     << PostTargetActionDefinition.Name << "\n";
      throw std::exception();
    }
  }

  CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
      << "CodeValidationFunction: " << *CodeValidationFunction << "\n";
  ;
  CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
      << "DataValidationFunction: " << *DataValidationFunction << "\n";
  ;

  CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
      << "CompartmentalizationSupportFunctions:\n";
  for (auto fn : CompartmentalizationSupportFunctionList) {
    CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
        << *fn << "\n";
  }

  // print out transfer state values
  CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
      << "TransferState Labels:\n";
  for (auto Label : TransferState.GetActionArgumentLabels()) {
    CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
        << Label << "\n";
  }
  CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
      << "TransferState Actions:\n";
  for (auto Action : TransferState.GetPreTransferActions()) {
    CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
        << *Action << "\n";
  }
  for (auto Action : TransferState.GetPostTargetActions()) {
    CommonAnalysis.getWriter(HAKCSystemInformation::OutputDebugInfo())
        << *Action << "\n";
  }
}

bool HAKCSystemInformation::OutputDebugInfo() const { return DebugOutput; }

bool HAKCSystemInformation::OutputDebugInfo(GlobalValue *GV) const {
  auto Search = [GV](GlobalValue *Symbol) { return Symbol == GV; };

  return OutputDebugInfo() || llvm::any_of(SymbolsToOutputDebugInfo, Search);
}

Module &HAKCSystemInformation::GetModule() {
  return CommonAnalysis.GetModule();
}

hakc::HAKCPassModeTypeEnum HAKCSystemInformation::GetPassMode() const {
  return PassMode;
}

StringRef HAKCSystemInformation::GetSourcePath() const { return SourcePath; }

StringRef HAKCSystemInformation::GetBuildPath() const { return BuildPath; }

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

hakc::hakc_transfer_def_t
HAKCSystemInformation::CompartmentTransfer(bool PerCPU) const {
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

iterator_range<FunctionList::iterator>
HAKCSystemInformation::CompartmentalizationSupportFunctions() {
  return make_range(CompartmentalizationSupportFunctionList.begin(),
                    CompartmentalizationSupportFunctionList.end());
}

iterator_range<FunctionList::iterator>
HAKCSystemInformation::SafeTransitionFunctions() {
  return make_range(SafeTransitionFunctionList.begin(),
                    SafeTransitionFunctionList.end());
}

iterator_range<HAKCTypeSet::iterator> HAKCSystemInformation::IgnoredTypes() {
  return make_range(IgnoredTypeSet.begin(), IgnoredTypeSet.end());
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

StringRef HAKCSystemInformation::GetArch() const { return Arch; }

StringRef HAKCSystemInformation::GetPlatform() const { return Platform; }

const HAKCDatabaseInformation &
HAKCSystemInformation::GetDatabaseInformation() const {
  return DatabaseInformation;
}

HAKCTransferState &HAKCSystemInformation::GetTransferState() {
  return TransferState;
}
} // namespace llvm::hakc
