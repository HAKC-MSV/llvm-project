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

// critical reference guide for cl: https://llvm.org/docs/CommandLine.html#internal-vs-external-storage
std::string HAKC_CONFIG_PATH;

static cl::opt<std::string, true> HAKC_CONFIG_CL("HAKC_CONFIG", cl::desc("Path to HAKC Configuration File"),
                                                 cl::location(HAKC_CONFIG_PATH), cl::Optional);
using namespace llvm::hakc;

namespace llvm {
    bool runCompartmentalization(CommonHAKCAnalysis &HAKCAnalysis) {
        bool PerformTransformations = true;
        Module &M = HAKCAnalysis.GetModule();
        StringRef CurrentSourceName(M.getSourceFileName());
        for (auto &path: HAKCAnalysis.GetSystemInfo().HAKCSourcePaths()) {
            if (CurrentSourceName.contains(path)) {
                CommonHAKCAnalysis::getWriter(true) << "Skipping hakc source " << CurrentSourceName << "\n";
                PerformTransformations = false;
            }
        }

        for (auto &path: HAKCAnalysis.GetSystemInfo().SeparateNamespacePaths()) {
            if (CurrentSourceName.contains(path)) {
                CommonHAKCAnalysis::getWriter(true) << "Skipping separate namespace source " << CurrentSourceName <<
                        "\n";
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

    bool runDataAccessGraphAnalysis(CommonHAKCAnalysis &HAKCAnalysis) {
        Module &M = HAKCAnalysis.GetModule();
        auto BasePath = CommonHAKCAnalysis::GetModuleFullPath(M);
        auto P = HAKCAnalysis.GetTransformedPath(BasePath);

        auto Prefix = HAKCAnalysis.GetSystemInfo().GetDagAnalysisRootPath().str();
        if (Prefix.back() != llvm::sys::path::get_separator().back()) {
            Prefix += llvm::sys::path::get_separator();
        }
        auto Path = Prefix;
        Path += P;
        Path += ".dag.yml";

        std::error_code err;
        err = sys::fs::create_directories(sys::path::parent_path(Path));
        if (err) {
            errs() << "Failed to create " << sys::path::parent_path(Path) << "\n";
            throw std::exception();
        }
        raw_fd_ostream out(Path, err);
        if (!err) {
            HAKCAnalysis.GetSystemInfo().GetTypeIdentifier().OutputYAML(out);
            out.close();
        } else {
            errs() << "Failed to open " << Path << "\n";
            throw std::exception();
        }

        return false;
    }

    bool RunHAKCAnalysis(Module &M) {
        if (HAKC_CONFIG_PATH.empty()) {
            errs() << "HAKC_CONFIG_PATH parameter '-mllvm -HAKC_CONFIG=somepath' not specifiecd\n";
            throw std::exception();
        }
        CommonHAKCAnalysis HAKCAnalysis(M, HAKC_CONFIG_PATH);

        switch (HAKCAnalysis.GetSystemInfo().GetPassMode()) {
            case RunDataAccessGraphAnalysis:
                return runDataAccessGraphAnalysis(HAKCAnalysis);
            case RunCompartmentalization:
                return runCompartmentalization(HAKCAnalysis);
            default:
                errs() << "Failed to get valid PassMode (this should never be called)\n";
                throw std::exception();
        }
    }

    PreservedAnalyses HAKCPass::run(Module &M, ModuleAnalysisManager &MAM) {
        return RunHAKCAnalysis(M) ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
}
