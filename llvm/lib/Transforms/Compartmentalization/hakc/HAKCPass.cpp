#include "llvm/Transforms/Compartmentalization/hakc/HAKCPass.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"

#include <llvm/Support/ScopedPrinter.h>

static cl::opt<std::string>
    HAKCConfigPath("hakc-config", cl::desc("Path to HAKC Configuration File"),
                   cl::value_desc("string"), cl::init(""));
static cl::opt<std::string>
    HAKCServerPath("hakc-server-path",
                   cl::desc("Path to the HAKC Server socket"),
                   cl::value_desc("string"), cl::init(""), cl::Optional);
static cl::opt<std::string>
    PassModeArg("pass-mode",
                cl::desc("Determines the behavior of the HAKC Pass"),
                cl::value_desc("string"), cl::init(""));
static cl::opt<bool> UseSimulatedClient("use-simulated-client",
                                        cl::desc("Use simulated server"),
                                        cl::init(false));
static cl::opt<bool> NecOnly("use-nec-only",
                             cl::desc("Use NEC for simulated server"),
                             cl::init(false));

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

bool ShouldSendSymbolsToServer(HAKCTypeIdentifier &TypeIdentifier) {
  auto FunctionCount = TypeIdentifier.GetFunctions().size() +
                       TypeIdentifier.GetUnmappedFunctions().size();
  auto GlobalCount = TypeIdentifier.GetGlobals().size() +
                     TypeIdentifier.GetUnmappedGlobals().size();
  return FunctionCount + GlobalCount > 0;
}

static bool runEnforcement(CommonHAKCAnalysis &HAKCAnalysis) {
  if (HAKCAnalysis.GetSystemInfo().GetSkipCurrentFile()) {
    return false;
  }
  CommonHAKCAnalysis::getLogger(Info) << "Running Enforcement Pass Mode!\n";
  HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);
  ModuleAnalysis.runEnforcement(UseSimulatedClient);
  return true;
}

static void runAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
  if (skip_current_file(HAKCAnalysis)) {
    return;
  }
  CommonHAKCAnalysis::getLogger(Info) << "Running Analysis Pass Mode!\n";
  HAKCModuleAnalysis ModuleAnalysis(HAKCAnalysis);

  if (ShouldSendSymbolsToServer(ModuleAnalysis.GetTypeIdentifier())) {
    // Only ever connect to server if symbols need to be sent
    auto Client = ModuleAnalysis.ConstructClient(UseSimulatedClient);
    // Symbols gathered are sent on connection termination
    Client->CloseConnection();
  } else {
    CommonHAKCAnalysis::getLogger(Debug)
        << "Skipping file " << HAKCAnalysis.GetModule().getSourceFileName()
        << "with 0 symbols\n";
  }
}

static bool RunHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM) {
  if (HAKCConfigPath.getValue().empty()) {
    CommonHAKCAnalysis::getLogger(Fatal) << "no hakc-config pass specified\n";
    throw std::exception();
  }

            const auto PassMode = CommonHAKCAnalysis::ParsePassMode(PassModeArg.getValue());
            CommonHAKCAnalysis HAKCAnalysis(M, MAM, HAKCConfigPath.getValue(),
                                            HAKCServerPath.getValue(), PassMode);
            switch (PassMode) {
                case Analysis:
                    runAnalysis(HAKCAnalysis);
                    return false;
                case Enforcement:
                    runEnforcement(HAKCAnalysis);
                    return true;
                case RunConfigAndExit:
                    return false;
                default:
                    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid HAKC build mode!\n";
                    throw std::exception();
            }
        }
    } // namespace hakc

PreservedAnalyses HAKCPass::run(Module &M, ModuleAnalysisManager &MAM) {
  return hakc::RunHAKCAnalysis(M, MAM) ? PreservedAnalyses::none()
                                       : PreservedAnalyses::all();
}
} // namespace llvm
