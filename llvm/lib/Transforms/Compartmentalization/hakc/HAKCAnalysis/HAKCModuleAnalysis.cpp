//
// Created by derrick on 8/20/21.
//
#include "llvm/IR/DIBuilder.h"
#include "llvm/Support/Regex.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

// remove policy and transformer, make it just module analysis

// then, make new subclass that does what the current module analysis does, and
// call it module transformation
namespace llvm::hakc {
HAKCModuleAnalysis::HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis)
    : CommonAnalysis(CommonAnalysis),
      TypeIdentifier(CommonAnalysis.GetSystemInfo().GetTypeIdentifier()) {
  runAnalysis();
}

void HAKCModuleAnalysis::runAnalysis() {
  // create managed pointers (essentially what is being done at the beginning of
  // the compartmentalization code)
  for (auto *F : AnalysisFunctions) {
    HAKCFunctionAnalysis FunctionAnalysis(F, *this);
    // perform type use analysis after all types are known
    FunctionAnalysis.TypeUseAnalysis();
  }
}

void HAKCModuleAnalysis::runEnforcement(bool UseSimulatedClient, bool NecOnly) {
  auto Client = ConstructClient(UseSimulatedClient, NecOnly);
  HAKCTransformer Transformer(*this, *Client);
  Transformer.runEnforcement();
}

std::unique_ptr<HAKCServerClientBase>
HAKCModuleAnalysis::ConstructClient(bool UseSimulatedClient, bool NecOnly) {
  std::unique_ptr<HAKCServerClientBase> Client;
  if (UseSimulatedClient) {
    Client =
        std::make_unique<FakeServerClient>(GetModule().getContext(), NecOnly);
  } else {
    Client = std::make_unique<HAKCServerClient>(*this);
  }
  return Client;
}

Module &HAKCModuleAnalysis::GetModule() const {
  return CommonAnalysis.GetSystemInfo().GetModule();
}

HAKCSystemInformation &HAKCModuleAnalysis::GetSystemInformation() const {
  return CommonAnalysis.GetSystemInfo();
}

HAKCTypeIdentifier &HAKCModuleAnalysis::GetTypeIdentifier() const {
  return TypeIdentifier;
}

bool HAKCModuleAnalysis::FunctionNeedsAnalysis(Function *F) const {
  bool needsAnalysis = !F->isIntrinsic() && !F->isDeclaration() &&
                       F->getSubprogram() != nullptr &&
                       !CommonHAKCAnalysis::IsOutsideTransferFunc(F) &&
                       !CommonAnalysis.IsHAKCFunction(F);
  const bool SuppressOutput =
      !CommonAnalysis.GetSystemInfo().OutputDebugInfo(F);
  if (!needsAnalysis) {
    goto out;
  }
  for (auto *user : F->users()) {
    if (!isa<CallInst>(user)) {
      /* Function is passed into a global variable */
      needsAnalysis = true;
    }
  }

out:

  CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput) << F->getName();
  if (!needsAnalysis) {
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput) << " does not need ";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput) << " needs ";
  }
  CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput) << "analysis\n";

  return needsAnalysis;
}

Function *HAKCModuleAnalysis::GetFunctionByName(const StringRef Name,
                                                FunctionType *FuncTy) const {
  auto Callee = GetModule().getOrInsertFunction(Name, FuncTy);
  return dyn_cast<Function>(Callee.getCallee());
}

bool HAKCModuleAnalysis::FunctionDefinedInAssembly(Function *F) const {
  const StringRef ModuleAsm = GetModule().getModuleInlineAsm();
  std::string SearchTerm = "[[:space:];]+";
  SearchTerm += F->getName().str();
  SearchTerm += ":";
  const Regex NameRegex(SearchTerm);
  SmallVector<StringRef, 2> Matches;
  const bool SuppressOutput =
      !CommonAnalysis.GetSystemInfo().OutputDebugInfo(F);
  const auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
  if (NameInAssembly) {
    CommonHAKCAnalysis::getLogger(Debug, SuppressOutput)
        << F->getName()
        << " was found in the Module inline assembly: " << Matches[0] << "\n";
  } else {
    CommonHAKCAnalysis::getLogger(Error, SuppressOutput)
        << "Could not find " << SearchTerm << " in\n"
        << ModuleAsm << "\n";
  }
  return NameInAssembly;
}

static bool _useEscapes(const Use &U, std::set<Value *> &expected) {
  /* If F is used in a global variable */
  if (const auto *gv = dyn_cast<GlobalVariable>(U.getUser())) {
    return gv->getSection() != ".discard.addressable";
  }
  if (isa<ConstantStruct>(U.getUser()) || isa<SelectInst>(U.getUser())) {
    return true;
  }
  if (auto *call = dyn_cast<CallInst>(U.getUser())) {
    for (auto &arg : call->args()) {
      if (arg.get() == U.get()) {
        return true;
      }
    }
  } else if (auto *bc = dyn_cast<BitCastOperator>(U.getUser())) {
    for (Use &u : bc->uses()) {
      if (expected.contains(u.get())) {
        continue;
      }
      expected.insert(u.get());
      if (_useEscapes(u, expected)) {
        return true;
      }
    }
  } else if (isa<ICmpInst>(U.getUser())) {
    return false;
  } else if (auto *store = dyn_cast<StoreInst>(U.getUser())) {
    if (store->getValueOperand() == U.get()) {
      return true;
    }
  } else if (auto *phi = dyn_cast<PHINode>(U.getUser())) {
    for (Use &u : phi->uses()) {
      if (expected.contains(u.get())) {
        continue;
      }
      expected.insert(u.get());
      if (_useEscapes(u, expected)) {
        return true;
      }
    }
  }

  return false;
}

bool HAKCModuleAnalysis::useEscapes(const Use &U) {
  std::set<Value *> examined;
  const bool escapes = _useEscapes(U, examined);

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Use " << U.get() << " in " << U.getUser();
  if (escapes) {
    CommonHAKCAnalysis::getLogger(Verbose) << " escapes\n";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose) << " does not escape\n";
  }

  return escapes;
}

bool HAKCModuleAnalysis::functionEscapes(Function *F) const {
  if (F->isIntrinsic()) {
    return false;
  }
  for (auto &U : F->uses()) {
    if (useEscapes(U)) {
      return true;
    }

    CommonHAKCAnalysis::getLogger(
        Debug, !CommonAnalysis.GetSystemInfo().OutputDebugInfo(F))
        << "Use " << U.getUser() << " does not escape\n";
  }
  const Function *transfer =
      GetModule().getFunction(CommonAnalysis.GetOutsideTransferName(F));
  if (transfer) {
    /* A transfer function reference has been made, so it escapes */
    return true;
  }

  return !CommonHAKCAnalysis::FunctionIsStatic(F);
}

CommonHAKCAnalysis &HAKCModuleAnalysis::GetCommonAnalysis() const {
  return CommonAnalysis;
}

// Get the StructType representing a kernel (module) parameter
StructType *HAKCModuleAnalysis::GetKernelParamType() const {
  // linux
  return StructType::getTypeByName(GetModule().getContext(),
                                   StringRef("struct.kernel_param"));
}

bool HAKCModuleAnalysis::FunctionIsInAnalysisSet(Function *F) {
  return CommonHAKCAnalysis::IsFunctionInFunctionList(
      F, make_range(AnalysisFunctions.begin(), AnalysisFunctions.end()));
}

void HAKCModuleAnalysis::OutputYAML(raw_ostream &out) const {
  // move from type identifier to module analysis
  SmallString<256> RealPath;
  CommonHAKCAnalysis::GetModuleFullPath(GetModule(), RealPath);

  out << "---\n";

  const auto GlobalCount = TypeIdentifier.GetGlobals().size() +
                           TypeIdentifier.GetUnmappedGlobals().size();
  if (GlobalCount > 0) {
    out << "globals:\n";
    std::vector<std::shared_ptr<HAKCGlobalInfo>> SortedGlobals;
    SortedGlobals.reserve(GlobalCount);
    for (auto &[key, val] : TypeIdentifier.GetGlobals()) {
      SortedGlobals.push_back(val);
    }
    for (const auto &Unmapped : TypeIdentifier.GetUnmappedGlobals()) {
      SortedGlobals.push_back(Unmapped);
    }
    llvm::sort(SortedGlobals.begin(), SortedGlobals.end(),
               [](const std::shared_ptr<HAKCGlobalInfo> &LHS,
                  const std::shared_ptr<HAKCGlobalInfo> &RHS) {
                 return LHS->GetName() < RHS->GetName();
               });
    for (const auto &it : SortedGlobals) {
      out.indent(HAKCInfo::IndentSpaces())
          << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
    }
  }

  const auto FunctionCount = TypeIdentifier.GetFunctions().size() +
                             TypeIdentifier.GetUnmappedFunctions().size();
  if (FunctionCount > 0) {
    out << "functions:\n";
    std::vector<std::shared_ptr<HAKCFunctionInfo>> SortedFunctions;
    SortedFunctions.reserve(FunctionCount);
    for (auto &[key, val] : TypeIdentifier.GetFunctions()) {
      SortedFunctions.push_back(val);
    }
    for (const auto &Unmapped : TypeIdentifier.GetUnmappedFunctions()) {
      SortedFunctions.push_back(Unmapped);
    }
    llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
               [](const std::shared_ptr<HAKCFunctionInfo> &LHS,
                  const std::shared_ptr<HAKCFunctionInfo> &RHS) {
                 return LHS->GetName() < RHS->GetName();
               });
    for (const auto &it : SortedFunctions) {
      out.indent(HAKCInfo::IndentSpaces())
          << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
    }
  }
}
} // namespace llvm::hakc
