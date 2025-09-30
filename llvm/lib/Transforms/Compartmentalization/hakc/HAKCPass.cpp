/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.cpp
 */

#include "llvm/Transforms/Compartmentalization/hakc/HAKCPass.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"

#include <llvm/Support/ScopedPrinter.h>

// critical reference guide for cl:
// https://llvm.org/docs/CommandLine.html#internal-vs-external-storage
std::string HAKC_CONFIG_PATH;

static cl::opt<std::string, true>
    HAKC_CONFIG_CL("hakc-config", cl::desc("Path to HAKC Configuration File"),
                   cl::location(HAKC_CONFIG_PATH), cl::Optional);
using namespace llvm::hakc;

namespace llvm {
namespace hakc {

bool skip_current_file(CommonHAKCAnalysis &HAKCAnalysis) {
  StringRef CurrentSourceName(HAKCAnalysis.GetModule().getSourceFileName());
  for (auto &path : HAKCAnalysis.GetSystemInfo().HAKCSourcePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning)
          << "Skipping hakc source " << CurrentSourceName << "\n";
      return true;
    }
  }

  for (auto &path : HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning)
          << "Skipping separate namespace source " << CurrentSourceName << "\n";
      return true;
    }
  }
  return false;
}

bool ShouldSendSymbolsToServer(HAKCTypeIdentifier& TypeIdentifier) {
  auto FunctionCount = TypeIdentifier.GetFunctions().size() +
                       TypeIdentifier.GetUnmappedFunctions().size();
  auto GlobalCount = TypeIdentifier.GetGlobals().size() +
                     TypeIdentifier.GetUnmappedGlobals().size();
  return FunctionCount + GlobalCount > 0;
}

static bool runEnforcement(CommonHAKCAnalysis &HAKCAnalysis) {
  if (skip_current_file(HAKCAnalysis)) {
    return false;
  }
  HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
  HAKCServerClient Client(ModuleAnalysis);
  HAKCTransformer Transformer(ModuleAnalysis, Client);
  switch (HAKCAnalysis.GetSystemInfo().GetPassMode()) {
  case Spatial:
    CommonHAKCAnalysis::getLogger(Info) << "Running Spatial Enforcement Pass Mode!\n";
    Transformer.performTransformations();
    break;
  default:
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid HAKC Pass mode\n";
    throw std::exception();
  }
  return true;
}

static bool runAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
  if (skip_current_file(HAKCAnalysis)) {
    return false;
  }
  HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
  switch (HAKCAnalysis.GetSystemInfo().GetPassMode()) {
  case Spatial:
    CommonHAKCAnalysis::getLogger(Info) << "Running Spatial Analysis Pass Mode!\n";
    break;
  default:
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid HAKC Pass mode\n";
    throw std::exception();
  }

  if (ShouldSendSymbolsToServer(ModuleAnalysis.GetTypeIdentifier())) {
    // Only ever connect to server if symbols need to be sent
    HAKCServerClient Client(ModuleAnalysis);
    Client.CloseConnection();
  } else {
    CommonHAKCAnalysis::getLogger(Info)
        << "Skipping file " << HAKCAnalysis.GetModule().getSourceFileName()
        << "with 0 symbols\n";
  }
  return false;
}

static bool RunHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM) {
  if (HAKC_CONFIG_PATH.empty()) {
    CommonHAKCAnalysis::getLogger(Fatal) << "no hakc-config pass specified\n";
    throw std::exception();
  }

  CommonHAKCAnalysis HAKCAnalysis(M, MAM, HAKC_CONFIG_PATH);
  switch (HAKCAnalysis.GetSystemInfo().GetBuildMode()) {
  case Analysis:
    return runAnalysis(HAKCAnalysis);
  case Enforcement:
    return runEnforcement(HAKCAnalysis);
  case RunConfigAndExit:
    return false;
  default:
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid HAKC build mode!\n";
    throw std::exception();
  }
}
} // namespace hakc

PreservedAnalyses HAKCPass::run(Module &M, ModuleAnalysisManager &MAM) {
  return RunHAKCAnalysis(M, MAM) ? PreservedAnalyses::none()
                                 : PreservedAnalyses::all();
}
} // namespace llvm
