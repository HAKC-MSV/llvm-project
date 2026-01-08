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
    : UsedCompartments(), CommonAnalysis(CommonAnalysis), AnalysisFunctions(),
      TypeIdentifier(CommonAnalysis.GetSystemInfo().GetTypeIdentifier()) {}

Module &HAKCModuleAnalysis::GetModule() const {
  return CommonAnalysis.GetSystemInfo().GetModule();
}

HAKCTypeIdentifier &HAKCModuleAnalysis::GetTypeIdentifier() const {
  return TypeIdentifier;
}

bool HAKCModuleAnalysis::FunctionNeedsAnalysis(Function *F) const {
  bool needsAnalysis = !F->isIntrinsic() && !F->isDeclaration() &&
                       F->getSubprogram() != nullptr &&
                       !CommonHAKCAnalysis::IsOutsideTransferFunc(F) &&
                       !CommonAnalysis.IsHAKCFunction(F);
  bool SuppressOutput = !CommonAnalysis.GetSystemInfo().OutputDebugInfo(F);
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

Function *HAKCModuleAnalysis::GetFunctionByName(StringRef Name,
                                                FunctionType *FuncTy) {
  auto Callee = GetModule().getOrInsertFunction(Name, FuncTy);
  return dyn_cast<Function>(Callee.getCallee());
}

bool HAKCModuleAnalysis::FunctionDefinedInAssembly(Function *F) {
  StringRef ModuleAsm = GetModule().getModuleInlineAsm();
  std::string SearchTerm = "[[:space:];]+";
  SearchTerm += F->getName().str();
  SearchTerm += ":";
  Regex NameRegex(SearchTerm);
  SmallVector<StringRef, 2> Matches;
  bool SuppressOutput = !CommonAnalysis.GetSystemInfo().OutputDebugInfo(F);
  auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
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

bool _useEscapes(Use &U, std::set<Value *> &expected) {
  /* If F is used in a global variable */
  if (auto *gv = dyn_cast<GlobalVariable>(U.getUser())) {
    return gv->getSection() != ".discard.addressable";
  } else if (isa<ConstantStruct>(U.getUser()) || isa<SelectInst>(U.getUser())) {
    return true;
  } else if (auto *call = dyn_cast<CallInst>(U.getUser())) {
    for (auto &arg : call->args()) {
      if (arg.get() == U.get()) {
        return true;
      }
    }
  } else if (auto *bc = dyn_cast<BitCastOperator>(U.getUser())) {
    for (Use &u : bc->uses()) {
      if (expected.find(u.get()) != expected.end()) {
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
      if (expected.find(u.get()) != expected.end()) {
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

bool HAKCModuleAnalysis::useEscapes(Use &U) {
  std::set<Value *> examined;
  bool escapes = _useEscapes(U, examined);

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Use " << U.get() << " in " << U.getUser();
  if (escapes) {
    CommonHAKCAnalysis::getLogger(Verbose) << " escapes\n";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose) << " does not escape\n";
  }

  return escapes;
}

bool HAKCModuleAnalysis::functionEscapes(Function *F) {
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
  Function *transfer =
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
StructType *HAKCModuleAnalysis::GetKernelParamType() {
  // linux
  return llvm::StructType::getTypeByName(
      GetModule().getContext(), llvm::StringRef("struct.kernel_param"));
}

GlobalValue *
HAKCModuleAnalysis::ExtractGlobalFromKernelParam(GlobalVariable *GV) {
  // the result of walking through the kernel param struct
  // until we get to the actual global value backing the parameter
  GlobalValue *kernparam;

  auto *KernelParamType = GetKernelParamType();
  // type not found, just do nothing
  if (!KernelParamType) {
    return nullptr;
  }

  // trying to find globals of type GetKernelParamType()
  if (auto *StructTy = dyn_cast<StructType>(GV->getValueType())) {
    if (!(StructTy->getName() == KernelParamType->getName())) {
      return nullptr; // someone passed us a struct that wasn't a kernel param
      // struct
    }
  } else {
    return nullptr; // this is not good, don't give non-structs to this function
  }

  // we know it is a kernel param now, moving on

  // cast the value into a ConstantStruct so we can pick it apart
  auto *kp_struct = dyn_cast<ConstantStruct>(GV->getInitializer());

  // do we have struct kernel_param kp now
  if (kp_struct) {
    // the anonymous union that holds the Value we actually want
    // is the last element of the struct
    auto num_ops = kp_struct->getNumOperands();
    Constant *last_op = kp_struct->getOperand(num_ops - 1);

    // this holds kp->arg
    if (last_op) {
      // cast the union into a ConstantStruct so we can pick it apart
      auto *kparg_union = dyn_cast<ConstantStruct>(last_op);

      if (kparg_union) {
        // get the only thing in the struct, that's how unions work?
        // this constant is kp->arg, sort of
        Constant *kparg_val = kparg_union->getOperand(0);
        // check that the value in there is a BitCastOperator
        // it is bit-casting the global that backs the parameter
        if (auto *kparg_val_bco = dyn_cast<BitCastOperator>(kparg_val)) {
          // extract the pointer from the BitCastOperator
          Value *gv_from_bco = kparg_val_bco->getOperand(0);

          if (!(kernparam = dyn_cast<GlobalValue>(gv_from_bco))) {
            // if it isn't a global value, that's bad
            return nullptr;
          }

          // now we have kp->arg
        }
        // the thing in the union isn't a BitCastOperator, that's bad
        else {
          return nullptr;
        }
      }
      // we couldn't get the union out of the union struct, that's bad
      else {
        return nullptr;
      }
    }
    // we couldn't get the union struct at all out of the param struct, that's
    // bad
    else {
      return nullptr;
    }
  }
  // we couldn't even get the kernel param struct as a struct, that's bad
  else {
    return nullptr;
  }
  CommonHAKCAnalysis::getLogger(
      Debug, !CommonAnalysis.GetSystemInfo().OutputDebugInfo(GV))
      << "processing kernel param\n"
      << *kernparam << "\n";

  return kernparam;
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

  auto GlobalCount = TypeIdentifier.GetGlobals().size() +
                     TypeIdentifier.GetUnmappedGlobals().size();
  if (GlobalCount > 0) {
    out << "globals:\n";
    std::vector<std::shared_ptr<HAKCGlobalInfo>> SortedGlobals;
    SortedGlobals.reserve(GlobalCount);
    for (auto &it : TypeIdentifier.GetGlobals()) {
      SortedGlobals.push_back(it.second);
    }
    for (const auto &Unmapped : TypeIdentifier.GetUnmappedGlobals()) {
      SortedGlobals.push_back(Unmapped);
    }
    llvm::sort(SortedGlobals.begin(), SortedGlobals.end(),
               [](const std::shared_ptr<HAKCGlobalInfo> &LHS,
                  const std::shared_ptr<HAKCGlobalInfo> &RHS) {
                 return LHS->GetName() < RHS->GetName();
               });
    for (auto &it : SortedGlobals) {
      out.indent(HAKCInfo::IndentSpaces())
          << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
    }
  }

  auto FunctionCount = TypeIdentifier.GetFunctions().size() +
                       TypeIdentifier.GetUnmappedFunctions().size();
  if (FunctionCount > 0) {
    out << "functions:\n";
    std::vector<std::shared_ptr<HAKCFunctionInfo>> SortedFunctions;
    SortedFunctions.reserve(FunctionCount);
    for (auto &it : TypeIdentifier.GetFunctions()) {
      SortedFunctions.push_back(it.second);
    }
    for (const auto &Unmapped : TypeIdentifier.GetUnmappedFunctions()) {
      SortedFunctions.push_back(Unmapped);
    }
    llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
               [](const std::shared_ptr<HAKCFunctionInfo> &LHS,
                  const std::shared_ptr<HAKCFunctionInfo> &RHS) {
                 return LHS->GetName() < RHS->GetName();
               });
    for (auto &it : SortedFunctions) {
      out.indent(HAKCInfo::IndentSpaces())
          << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
    }
  }
}
} // namespace llvm::hakc
