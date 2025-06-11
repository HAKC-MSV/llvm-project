/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.cpp
 */

#include "llvm/Transforms/Compartmentalization/hakc/HAKCPass.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallPrinter.h"

// critical reference guide for cl:
// https://llvm.org/docs/CommandLine.html#internal-vs-external-storage
std::string HAKC_CONFIG_PATH;

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
      CommonHAKCAnalysis::getWriter(true)
          << "Skipping hakc source " << CurrentSourceName << "\n";
      PerformTransformations = false;
    }
  }

  for (auto &path : HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getWriter(true)
          << "Skipping separate namespace source " << CurrentSourceName << "\n";
      PerformTransformations = false;
    }
  }

  if (PerformTransformations) {
    HAKCCompartmentalizationPolicy Policy(HAKCAnalysis.GetSystemInfo());
    HAKCModuleAnalysis ModuleTransformation(HAKCAnalysis, Policy);
    ModuleTransformation.performTransformations();
  }

  return true;
}

static bool runDataAccessGraphAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
  Module &M = HAKCAnalysis.GetModule();
  SmallString<256> Path;
  SmallString<256> ModulePath;
  CommonHAKCAnalysis::GetModuleFullPath(M, ModulePath);
  llvm::sys::path::append(
      Path, HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath());
  llvm::sys::path::append(Path, ModulePath);
  llvm::sys::path::replace_extension(Path, ".dag.yml");
  llvm::sys::path::make_preferred(Path);

  std::error_code err;
  err = sys::fs::create_directories(sys::path::parent_path(Path));
  if (err) {
    CommonHAKCAnalysis::getWriter(true)
        << "Failed to create " << sys::path::parent_path(Path) << "\n";
    throw std::exception();
  }
  raw_fd_ostream out(Path, err);
  if (!err) {
    HAKCAnalysis.GetSystemInfo().GetTypeIdentifier().OutputYAML(out);
    out.close();
  } else {
    CommonHAKCAnalysis::getWriter(true) << "Failed to open " << Path << "\n";
    throw std::exception();
  }

  return false;
}

static bool runPostDominatorAnalysis(CommonHAKCAnalysis &HAKCAnalysis, ModuleAnalysisManager &MAM) {
  Module &M = HAKCAnalysis.GetModule();
  SmallString<256> Path;
  SmallString<256> ModulePath;
  CommonHAKCAnalysis::GetModuleFullPath(M, ModulePath);
  llvm::sys::path::append(
      Path, HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath());
  llvm::sys::path::append(Path, ModulePath);
  llvm::sys::path::replace_extension(Path, ".dag.yml");
  llvm::sys::path::make_preferred(Path);

  std::error_code err;
  err = sys::fs::create_directories(sys::path::parent_path(Path));
  if (err) {
    CommonHAKCAnalysis::getWriter(true)
        << "Failed to create " << sys::path::parent_path(Path) << "\n";
    throw std::exception();
  }
  raw_fd_ostream out(Path, err);
  if (!err) {
    HAKCAnalysis.GetSystemInfo().GetTypeIdentifier().OutputYAML(out);
    out.close();
  } else {
    CommonHAKCAnalysis::getWriter(true) << "Failed to open " << Path << "\n";
    throw std::exception();
  }

  return false;
}

static bool RunHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM) {
  if (HAKC_CONFIG_PATH.empty()) {
    CommonHAKCAnalysis::getWriter(true) << "no hakc-config pass specified\n";
    throw std::exception();
  }
  CommonHAKCAnalysis HAKCAnalysis(M, MAM, HAKC_CONFIG_PATH);

  // llvm::writeCallGraphDOT(M, MAM, std::string(HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath()));

  switch (HAKCAnalysis.GetSystemInfo().GetPassMode()) {
  case RunDataAccessGraphAnalysis:
    return runDataAccessGraphAnalysis(HAKCAnalysis);
  case RunPostDominatorAnalysis:
    errs() << "Running RunPostDominatorAnalysis!!!\n";
    return runPostDominatorAnalysis(HAKCAnalysis, MAM);
  case RunCompartmentalization:
    return runCompartmentalization(HAKCAnalysis);
  case RunConfigAndExit:
    return false;
  default:
    CommonHAKCAnalysis::getWriter(true) << "Invalid HAKC pass mode\n";
    throw std::exception();
  }
}
} // namespace hakc

PreservedAnalyses HAKCPass::run(Module &M, ModuleAnalysisManager &MAM) {
  return RunHAKCAnalysis(M, MAM) ? PreservedAnalyses::none()
                            : PreservedAnalyses::all();
}
} // namespace llvm
