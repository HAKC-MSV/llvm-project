/**
 * @brief HAKC FunctionAnalysis and Transformation pass
 * @file HAKCPass.cpp
 */

#include "llvm/Transforms/Compartmentalization/hakc/HAKCPass.h"
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
      CommonHAKCAnalysis::getWriter(Error)
          << "Skipping hakc source " << CurrentSourceName << "\n";
      PerformTransformations = false;
    }
  }

  for (auto &path : HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Skipping separate namespace source " << CurrentSourceName << "\n";
      PerformTransformations = false;
    }
  }

  if (PerformTransformations) {
    HAKCCompartmentalizationPolicy Policy(HAKCAnalysis.GetSystemInfo());
    HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
    HAKCTransformer Transformer(ModuleAnalysis, Policy);
    Transformer.performTransformations();
  }

  return true;
}

static bool runDataAccessGraphAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
  Module &M = HAKCAnalysis.GetModule();
  SmallString<256> Path;
  SmallString<256> ModulePath;
  CommonHAKCAnalysis::GetModuleFullPath(M, ModulePath);
  sys::path::append(Path,
                    HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath());
  sys::path::append(Path, ModulePath);
  sys::path::replace_extension(Path, ".dag.yml");
  sys::path::make_preferred(Path);


  StringRef CurrentSourceName(M.getSourceFileName());
  for (auto &path : HAKCAnalysis.GetSystemInfo().HAKCSourcePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Skipping hakc source " << CurrentSourceName << "\n";
      return false;
    }
  }

  for (auto &path : HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
    if (CurrentSourceName.contains(path)) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Skipping separate namespace source " << CurrentSourceName <<
          "\n";
      return false;
    }
  }

  std::error_code err;
  err = sys::fs::create_directories(sys::path::parent_path(Path));
  if (err) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Failed to create " << sys::path::parent_path(Path) << "\n";
    throw std::exception();
  }
  raw_fd_ostream out(Path, err);
  if (!err) {
    HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
    ModuleAnalysis.TemporalAnalysis();
    ModuleAnalysis.OutputYAML(out);
    out.close();
  } else {
    CommonHAKCAnalysis::getWriter(Error) << "Failed to open " << Path << "\n";
    throw std::exception();
  }
  return false;
}

static bool RunHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM) {
  HAKCWriter::SetLogPath(M.getSourceFileName() + ".log");
  HAKCWriter::CreateLog();

  if (HAKC_CONFIG_PATH.empty()) {
    CommonHAKCAnalysis::getWriter(Error) << "no hakc-config pass specified\n";
    throw std::exception();
  }

  CommonHAKCAnalysis HAKCAnalysis(M, MAM, HAKC_CONFIG_PATH);

  CommonHAKCAnalysis::getWriter() << "abc\n";

  switch (HAKCAnalysis.GetSystemInfo().GetPassMode()) {
  case RunDataAccessGraphAnalysis:
    return runDataAccessGraphAnalysis(HAKCAnalysis);
  case RunDataAccessGraphAnalysisSingleSourceFile:
    if(M.getSourceFileName() != HAKCAnalysis.GetSystemInfo().GetSingleSourceFile()) {
      errs () << "Source file " << M.getSourceFileName() << " is not the target source file: " << HAKCAnalysis.GetSystemInfo().GetSingleSourceFile();
      return false;
    }
    errs() << "Analyzing target source file: " << M.getSourceFileName() << "\n";
    return runDataAccessGraphAnalysis(HAKCAnalysis);
  case RunCompartmentalization:
    return runCompartmentalization(HAKCAnalysis);
  case RunConfigAndExit:
    return false;
  default:
    CommonHAKCAnalysis::getWriter(Error) << "Invalid HAKC pass mode\n";
    throw std::exception();
  }
}
} // namespace hakc

PreservedAnalyses HAKCPass::run(Module &M, ModuleAnalysisManager &MAM) {
  return RunHAKCAnalysis(M, MAM) ? PreservedAnalyses::none()
                                 : PreservedAnalyses::all();
}
} // namespace llvm
