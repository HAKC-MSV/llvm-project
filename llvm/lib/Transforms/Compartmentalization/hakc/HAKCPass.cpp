/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.cpp
 */

#include "llvm/Transforms/Compartmentalization/hakc/HAKCPass.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"

// critical reference guide for cl:
// https://llvm.org/docs/CommandLine.html#internal-vs-external-storage
std::string HAKC_CONFIG_PATH;
// TODO: add start up log level option
// std::string HAKC_CONFIG_PATH;

static cl::opt<std::string, true>
    HAKC_CONFIG_CL("hakc-config", cl::desc("Path to HAKC Configuration File"),
                   cl::location(HAKC_CONFIG_PATH), cl::Optional);
using namespace llvm::hakc;

namespace llvm {
namespace hakc {


static bool runCompartmentalization(CommonHAKCAnalysis &HAKCAnalysis) {
  bool PerformTransformations = true;
  Module &M = HAKCAnalysis.GetModule();
  StringRef CurrentSourceName(M.getSourceFileName());
  for (auto &path : HAKCAnalysis.GetSystemInfo().HAKCSourcePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning)
          << "Skipping hakc source " << CurrentSourceName << "\n";
      PerformTransformations = false;
    }
  }

  for (auto &path : HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning)
          << "Skipping separate namespace source " << CurrentSourceName << "\n";
      PerformTransformations = false;
    }
  }

  if (PerformTransformations) {
    HAKCServerClient Client(HAKCAnalysis.GetSystemInfo());
    HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
    HAKCTransformer Transformer(ModuleAnalysis, Client);
    if (HAKCAnalysis.GetSystemInfo().GetTemporalAnalysisEnabled()) {
      Transformer.performTemporalTransformations();
    }
    Transformer.performTransformations();
  }

  return true;
}

static bool runDataAccessGraphAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
  Module &M = HAKCAnalysis.GetModule();
  SmallString<256> Path{HAKCAnalysis.createDagYamlPath(HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath())};

  StringRef CurrentSourceName(M.getSourceFileName());
  for (auto &path : HAKCAnalysis.GetSystemInfo().HAKCSourcePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning)
          << "Skipping hakc source " << CurrentSourceName << "\n";
      return false;
    }
  }

  for (auto &path : HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getLogger(Warning)
          << "Skipping separate namespace source " << CurrentSourceName << "\n";
      return false;
    }
  }

  // std::error_code err;
  // err = sys::fs::create_directories(sys::path::parent_path(Path));
  // if (err) {
  //   CommonHAKCAnalysis::getLogger(Fatal)
  //       << "Failed to create " << sys::path::parent_path(Path) << "\n";
  //   throw std::exception();
  // }
  // Note: not going to create file if it will be empty
  // raw_fd_ostream out(Path, err);
  // if (!err) {
  HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
  if (HAKCAnalysis.GetSystemInfo().GetTemporalAnalysisEnabled()) {
    ModuleAnalysis.TemporalAnalysis();
  }
  auto TypeIdentifier = ModuleAnalysis.GetTypeIdentifier();
  auto FunctionCount = TypeIdentifier.GetFunctions().size() + TypeIdentifier.GetUnmappedFunctions().size();
  auto GlobalCount = TypeIdentifier.GetGlobals().size() + TypeIdentifier.GetUnmappedGlobals().size();
  // Only ever connect to server if symbols need to be sent
  if (FunctionCount + GlobalCount > 0) {
    HAKCServerClient Client(HAKCAnalysis.GetSystemInfo());
    Client.SendSymbolsToAnalysisServer(ModuleAnalysis, Path);
    Client.CloseConnection();
  }
  else {
    CommonHAKCAnalysis::getLogger(Info) << "Skipping file " << Path << "with 0 symbols\n";
  }
  // } else {
  //   CommonHAKCAnalysis::getLogger(Fatal) << "Failed to open " << Path << "\n";
  //   throw std::exception();
  // }
  return false;
}

static bool RunHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM) {
  if (HAKC_CONFIG_PATH.empty()) {
    CommonHAKCAnalysis::getLogger(Fatal) << "no hakc-config pass specified\n";
    throw std::exception();
  }

  CommonHAKCAnalysis HAKCAnalysis(M, MAM, HAKC_CONFIG_PATH);

  if (HAKCAnalysis.abort) {
    errs() << "ABORTING\n";
    return false;
  }

  // TODO remove below?
  std::shared_ptr<HAKCLogger> _Logger = HAKCAnalysis.get();
  switch (HAKCAnalysis.GetSystemInfo().GetPassMode()) {
  case RunDataAccessGraphAnalysis:
    return runDataAccessGraphAnalysis(HAKCAnalysis);
  case RunDataAccessGraphAnalysisSingleSourceFile:
    if (M.getSourceFileName() !=
        HAKCAnalysis.GetSystemInfo().GetSingleSourceFile()) {
      CommonHAKCAnalysis::getLogger(Fatal) << "Source file " << M.getSourceFileName()
             << " is not the target source file: "
             << HAKCAnalysis.GetSystemInfo().GetSingleSourceFile() << "\n";
      return false;
    }
    CommonHAKCAnalysis::getLogger(Info) << "Analyzing target source file: " << M.getSourceFileName() << "\n";
    return runDataAccessGraphAnalysis(HAKCAnalysis);
  case RunCompartmentalization:
    return runCompartmentalization(HAKCAnalysis);
  case RunConfigAndExit:
    return false;
  default:
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid HAKC pass mode\n";
    throw std::exception();
  }
}
} // namespace hakc

PreservedAnalyses HAKCPass::run(Module &M, ModuleAnalysisManager &MAM) {
  return RunHAKCAnalysis(M, MAM) ? PreservedAnalyses::none()
                                 : PreservedAnalyses::all();
}
} // namespace llvm
