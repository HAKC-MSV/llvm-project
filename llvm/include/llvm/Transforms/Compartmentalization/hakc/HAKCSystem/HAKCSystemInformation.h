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

typedef std::shared_ptr<llvm::hakc::HAKCAllocationSize> HAKCCustomAllocation;

typedef SmallVector<llvm::hakc::function_def_t> HAKCFunctionList;
typedef SmallVector<llvm::hakc::function_def_t> HAKCTransferList;
typedef SmallVector<llvm::hakc::HAKCTypeP> HAKCStructList;
typedef SmallVector<llvm::hakc::custom_transfer_def_t> HAKCCustomTransferList;
typedef SmallVector<GlobalVariable *> HAKCGlobalVariableList;
typedef SmallVector<GlobalValue *> HAKCSymbolList;
typedef SmallVector<Function *> FunctionList;
typedef SmallVector<std::string, 16> HAKCStringList;
typedef SmallVector<HAKCCustomAllocation> HAKCCustomAllocationList;
typedef SmallVector<llvm::hakc::arg_def_t> HAKCArgumentsList;
typedef SmallVector<llvm::hakc::transfer_action_def_t> HAKCTransferActionList;
typedef SmallVector<llvm::hakc::pre_transfer_action_def_t>
    HAKCPreTransferActionList;
typedef SmallVector<llvm::hakc::post_target_action_def_t>
    HAKCPostTargetActionList;

namespace llvm::hakc {
class CommonHAKCAnalysis;
class HAKCSystemInformation;

class HAKCDatabaseInformation {
public:
  HAKCDatabaseInformation();

  StringRef GetServerURL() const;

  StringRef GetCompartmentEndpoint() const;

  StringRef GetDivisionEndpoint() const;

  StringRef GetSymbolDivisionEndpoint() const;

  StringRef GetValidTargetsEndpoint() const;

  std::chrono::milliseconds GetServerTimeout() const;

  unsigned GetMaxRetries() const;

  void operator<<(const HAKCYamlDatabaseConfig &DatabaseConfig);

protected:
  std::string ServerURL;
  std::string CompartmentEndpoint;
  std::string DivisionEndpoint;
  std::string SymbolDivisionEndpoint;
  std::string ValidTargetsEndpoint;
  std::chrono::milliseconds Timeout;
  unsigned MaxConnectionRetries;
};

class HAKCSystemInformation {
public:
  explicit HAKCSystemInformation(CommonHAKCAnalysis &CommonAnalysis);

  bool OutputDebugInfo() const;

  bool OutputDebugInfo(GlobalValue *GV) const;

  bool OutputDebugInfo(StringRef SymbolName) const;

  Module &GetModule() const;

  const HAKCDatabaseInformation &GetDatabaseInformation() const;

  void operator<<(HAKCYamlConfig &YamlConfig);

  iterator_range<FunctionList::iterator> NoTransferFunctions();

  iterator_range<HAKCTransferList::iterator> CompartmentTransferFunctions();

  iterator_range<HAKCFunctionList::iterator>
  CompartmentalizationSupportFunctions();

  iterator_range<FunctionList::iterator> SafeTransitionFunctions();

  iterator_range<HAKCGlobalVariableList::iterator> IgnoredGlobals();

  iterator_range<HAKCStringList::iterator> SeparateNamespacePaths();

  iterator_range<HAKCStringList::iterator> HAKCSourcePaths();

  iterator_range<HAKCCustomTransferList::iterator> HAKCCustomTransfers();

  iterator_range<HAKCCustomAllocationList::iterator> AllocationFunctions();

  iterator_range<HAKCStringList::iterator> IncludePaths();

  llvm::hakc::function_def_t CodeValidation() const;

  llvm::hakc::function_def_t DataValidation() const;

  llvm::hakc::function_def_t SignWithDivision() const;

  llvm::hakc::function_def_t CompartmentTransfer(bool PerCPU) const;

  HAKCTypeIdentifier &GetTypeIdentifier();

  hakc::HAKCPassModeTypeEnum GetPassMode() const;

  StringRef GetArch() const;

  StringRef GetPlatform() const;

  StringRef GetDagAnalysisRootPath() const;
  HAKCStructList GetStructList() const;

  iterator_range<HAKCPreTransferActionList::iterator> PreTransferActions();

  iterator_range<HAKCPostTargetActionList::iterator> PostTargetActions();

  // epoch_vec_t getApplicableEpochs(std::shared_ptr<HAKCSymbol> sym);

  // symbol_epoch_map_t getEpochs();

  StringRef GetSingleSourceFile();

protected:
  CommonHAKCAnalysis &CommonAnalysis;
  HAKCTypeIdentifier TypeIdentifier;
  HAKCDatabaseInformation DatabaseInformation;
  bool DebugOutput;
  hakc::HAKCPassModeTypeEnum PassMode;
  std::string SingleSourceFile;
  std::string Arch;
  std::string Platform;
  std::string DagAnalysisRootPath;
  HAKCStringList IncludePathsList;
  FunctionList NoTransferFunctionList;
  HAKCTransferList CompartmentTransferFunctionList;
  hakc::function_def_t CodeValidationFunction;
  hakc::function_def_t DataValidationFunction;
  hakc::function_def_t SignWithDivisionFunction;
  hakc::function_def_t DefaultCompartmentTransfer;
  hakc::function_def_t PerCPUCompartmentTransfer;
  HAKCFunctionList CompartmentalizationSupportFunctionList;
  HAKCSymbolList SymbolsToOutputDebugInfo;
  HAKCStringList SeparateNamespacePathList;
  HAKCStringList HAKCSourcePathList;
  FunctionList SafeTransitionFunctionList;
  HAKCGlobalVariableList IgnoredGlobalList;
  HAKCCustomAllocationList AllocationFunctionList;
  HAKCCustomTransferList CustomTransferList;
  HAKCPreTransferActionList PreTransferActionList;
  HAKCPostTargetActionList PostTargetActionList;
  HAKCStructList StructList;

  void
  GetAllDefinedHAKCFunctions(SmallVectorImpl<hakc::function_def_t> &Results);

  hakc::function_def_t
  CreateHAKCFunction(HAKCYAMLFunctionDefinition &YAMLFunctionDef,
                     const HAKCTypeIdentifier &TypeIdentifier) const;

  static hakc::custom_transfer_def_t
  CreateCustomTransferFunction(HAKCYAMLCustomTransferType &YAMLCustomTransfer,
                               HAKCTypeP HAKCTy,
                               const HAKCTypeIdentifier &TypeIdentifier);

  static void PopulateHAKCFunctionArgs(
      SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args,
      HAKCYAMLFunctionDefinition &YAMLFunctionDef,
      const HAKCTypeIdentifier &TypeIdentifier);
};
} // namespace llvm::hakc

#endif // HAKC_HAKCSYSTEMINFORMATION_H
