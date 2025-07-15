//
// Created by derrick on 8/20/21.
//
#include "llvm/IR/DIBuilder.h"
#include "llvm/Support/Regex.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"


// remove policy and transformer, make it just module analysis

// then, make new subclass that does what the current module analysis does, and call it module transformation
namespace llvm::hakc {
HAKCModuleAnalysis::HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis)
    : UsedCompartments(), CommonAnalysis(CommonAnalysis), AnalysisFunctions(),
      TypeIdentifier(CommonAnalysis.GetSystemInfo().GetTypeIdentifier()){};
//
// void HAKCModuleAnalysis::InitAnalysis() {
//   for (auto &F : GetModule().functions()) {
//     if (FunctionNeedsAnalysis(&F)) {
//       auto &Division = Policy.GetDivision(&F);
//       auto Compartment = Division.GetHAKCCompartment();
//       RegisterUsedCompartment(Compartment);
//       AnalysisFunctions.push_back(&F);
//     }
//   }
//   CommonHAKCAnalysis::SortFunctionList(AnalysisFunctions);
// }

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
    if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(F)) {
      CommonHAKCAnalysis::getWriter(true) << F->getName();
      if (!needsAnalysis) {
        CommonHAKCAnalysis::getWriter(true) << " does not need ";
      } else {
        CommonHAKCAnalysis::getWriter(true) << " needs ";
      }
      CommonHAKCAnalysis::getWriter(true) << "analysis\n";
    }

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

  auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
  if (NameInAssembly) {
    CommonHAKCAnalysis::getWriter(
        CommonAnalysis.GetSystemInfo().OutputDebugInfo(F))
        << F->getName()
        << " was found in the Module inline assembly: " << Matches[0] << "\n";
  } else {
    CommonHAKCAnalysis::getWriter(
        CommonAnalysis.GetSystemInfo().OutputDebugInfo(F))
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

bool in_debug = false;
bool HAKCModuleAnalysis::useEscapes(Use &U) {
  std::set<Value *> examined;
  bool escapes = _useEscapes(U, examined);
  if (in_debug) {
    CommonHAKCAnalysis::getWriter(in_debug)
        << "Use " << U.get() << " in " << U.getUser();
    if (escapes) {
      CommonHAKCAnalysis::getWriter(in_debug) << " escapes\n";
    } else {
      CommonHAKCAnalysis::getWriter(in_debug) << " does not escape\n";
    }
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
    CommonHAKCAnalysis::getWriter(
        CommonAnalysis.GetSystemInfo().OutputDebugInfo(F))
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

CommonHAKCAnalysis &HAKCModuleAnalysis::GetCommonAnalysis() {
  return CommonAnalysis;
}

StringRef HAKCModuleAnalysis::GlobalInitTransferPrefix() {
  return "hakc_glob_init_xfer_";
}

StringRef HAKCModuleAnalysis::GlobalInitTransferSectionName() const {
  return ".hakc.glob_init.text";
}

StringRef HAKCModuleAnalysis::GlobalInitTransferPointerSectionName() const {
  return ".hakc.global_init.data";
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

  CommonHAKCAnalysis::getWriter(
      CommonAnalysis.GetSystemInfo().OutputDebugInfo(GV))
      << "processing kernel param\n"
      << *kernparam << "\n";

  return kernparam;
}

// we generate these for all kernel params, some may go unused by the actual
// module loader (non-pointer params are ignored by the loader when it comes to
// transferring)
//    void HAKCModuleAnalysis::transferModuleParams() {
//
//        if (!isModuleCompartmentalized()) {
//            return;
//        }
//
//        auto *KernelParamType = GetKernelParamType();
//        // type not found, just do nothing
//        if (!KernelParamType) {
//            return;
//        }
//
//        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
//            CommonHAKCAnalysis::getWriter() << "kernel param type is: " <<
//            *KernelParamType << "\n";
//        }
//
//        // inspect all globals
//        for (auto &Global: GetModule().globals()) {
//            // trying to find globals of type GetKernelParamType()
//            if (auto *StructTy = dyn_cast<StructType>(Global.getValueType()))
//            {
//                // if true, the type of Global matches GetKernelParamType
//                if (StructTy == KernelParamType) {
//                    if
//                    (CommonAnalysis.GetSystemInfo().OutputDebugInfo(&Global))
//                    {
//                        CommonHAKCAnalysis::getWriter() << "found kernel
//                        param: " << Global << "\n";
//                    }
//
//                    // generate a GetCtx function for the parameter and update
//                    // function pointer array
//                    generateModuleParamGetCtxFunction(&Global);
//                }
//            }
//        }
//    }

bool HAKCModuleAnalysis::FunctionIsInAnalysisSet(Function *F) {
  return CommonHAKCAnalysis::IsFunctionInFunctionList(
      F, make_range(AnalysisFunctions.begin(), AnalysisFunctions.end()));
}


void HAKCModuleAnalysis::OutputYAML(raw_ostream &out) const {
  // move from type identifier to module analysis
  SmallString<256> RealPath;
  CommonHAKCAnalysis::GetModuleFullPath(GetModule(), RealPath);

  out << "---\n";
  // out << "CU: ";
  // out << RealPath;
  // out << "\n";


  auto GlobalCount = TypeIdentifier.GetGlobals().size() + TypeIdentifier.GetUnmappedGlobals().size();
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

  auto FunctionCount = TypeIdentifier.GetFunctions().size() + TypeIdentifier.GetUnmappedFunctions().size();
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

// ________________________________________________________________________________
//
// HAKCModuleTransform::HAKCModuleTransform(CommonHAKCAnalysis &CommonAnalysis,
//                                        HAKCCompartmentalizationPolicy &Policy)
//     : HAKCModuleAnalysis(CommonAnalysis), Policy(Policy), Transformer(Policy, *this){InitAnalysis();};
//
// void HAKCModuleTransform::InitAnalysis() {
//   for (auto &F : GetModule().functions()) {
//     if (FunctionNeedsAnalysis(&F)) {
//       auto &Division = Policy.GetDivision(&F);
//       auto Compartment = Division.GetHAKCCompartment();
//       RegisterUsedCompartment(Compartment);
//       AnalysisFunctions.push_back(&F);
//     }
//   }
//   CommonHAKCAnalysis::SortFunctionList(AnalysisFunctions);
// }
//
// HAKCTransformer &HAKCModuleTransform::GetTransformer() { return Transformer; }
//
// void HAKCModuleTransform::RegisterUsedCompartment(HAKCCompartment &compartment) {
//   if (compartment != Policy.GetDefaultDivision().GetHAKCCompartment()) {
//     UsedCompartments.push_back(compartment);
//   }
// }
//
// /**
//  * @brief Moves all global values to the specified HAKC ELF section
//  */
// void HAKCModuleTransform::MoveGlobalsToHAKCSection() {
//   std::set<GlobalVariable *> globalsToChange;
//
//   for (auto *pGlobal : globalsToChange) {
//     auto finalName = getGlobalHAKCSectionName(pGlobal);
//     auto compartment = Policy.GetDivision(pGlobal).GetHAKCCompartment();
//     RegisterUsedCompartment(compartment);
//
//     if (finalName != pGlobal->getSection()) {
//       CommonHAKCAnalysis::getWriter(
//           CommonAnalysis.GetSystemInfo().OutputDebugInfo(pGlobal))
//           << "Changing section of global " << *pGlobal << " to section "
//           << finalName << " from " << pGlobal->getSection() << "\n";
//       pGlobal->setSection(finalName);
//     }
//   }
// }
//
// Function *HAKCModuleTransform::GetFunctionByName(StringRef Name,
//                                                 FunctionType *FuncTy) {
//   auto Callee = GetModule().getOrInsertFunction(Name, FuncTy);
//   return dyn_cast<Function>(Callee.getCallee());
// }
//
// bool HAKCModuleTransform::isModuleCompartmentalized() {
//   auto &DefaultCompartment = Policy.GetDefaultDivision().GetHAKCCompartment();
//   auto Search = [DefaultCompartment](HAKCCompartment &Compartment) {
//     return Compartment != DefaultCompartment;
//   };
//
//   return llvm::any_of(UsedCompartments, Search);
// }
//
// bool HAKCModuleTransform::AliasShouldBeCreated(Function *F) {
//   //         /* See note in
//   //         HAKCModuleTransformLinux::TransferFunctionShouldBeCreated */ if
//   //         (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
//   //             return false;
//   //         }
//   //         return HAKCModuleTransformLinux::AliasShouldBeCreated(F);
//   return TransferFunctionShouldBeCreated(F);
// }
//
// bool HAKCModuleTransform::FunctionDefinedInAssembly(Function *F) {
//   StringRef ModuleAsm = GetModule().getModuleInlineAsm();
//   std::string SearchTerm = "[[:space:];]+";
//   SearchTerm += F->getName().str();
//   SearchTerm += ":";
//   Regex NameRegex(SearchTerm);
//   SmallVector<StringRef, 2> Matches;
//
//   auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
//   if (NameInAssembly) {
//     CommonHAKCAnalysis::getWriter(
//         CommonAnalysis.GetSystemInfo().OutputDebugInfo(F))
//         << F->getName()
//         << " was found in the Module inline assembly: " << Matches[0] << "\n";
//   } else {
//     CommonHAKCAnalysis::getWriter(
//         CommonAnalysis.GetSystemInfo().OutputDebugInfo(F))
//         << "Could not find " << SearchTerm << " in\n"
//         << ModuleAsm << "\n";
//   }
//   return NameInAssembly;
// }
//
//
// void HAKCModuleTransform::TransformModule() {
//   MoveGlobalsToHAKCSection();
//   TransformFunctions();
//   AddCompartmentMetadata();
//
//   CreateInitGlobalMemberTransfers();
//   AddTransferFunctions();
// }
//
// void HAKCModuleTransform::TransformFunctions() {
//   for (auto *F : AnalysisFunctions) {
//     HAKCFunctionAnalysis FunctionTransformation(F, *this, Policy);
//     FunctionTransformation.InstrumentCode();
//   }
// }
//
// void HAKCModuleTransform::performTransformations() {
//   TransformModule();
//   CommonHAKCAnalysis::getWriter(
//       CommonAnalysis.GetSystemInfo().OutputDebugInfo())
//       << "Final Module After Transformations:\n"
//       << GetModule() << "\n";
// }
//
// bool HAKCModuleTransform::TransferFunctionShouldBeCreated(Function *F) {
//   if (F->isDeclaration()) {
//     return false;
//   }
//
//   return CommonHAKCAnalysis::FunctionHasPointerArg(F);
// }
//
//
// std::string
// HAKCModuleTransform::getGlobalHAKCSectionName(GlobalVariable *GV) const {
//   if (CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GV, Policy)) {
//     return GV->getSection().str();
//   }
//
//   auto Compartment = Policy.GetDivision(GV).GetHAKCCompartment();
//   std::string finalName = HAKC_SECTION_PREFIX.str();
//   finalName += std::to_string(Compartment.GetCompartmentID()->getSExtValue());
//
//   finalName += GV->getSection().str();
//   if (GV->getSection().empty()) {
//     if (GV->isConstant()) {
//       finalName += ".rodata";
//     } else {
//       finalName += ".data";
//     }
//   }
//   return finalName;
// }
//
// void HAKCModuleTransform::AddTransferFunctions() {
//   FunctionList FuncsNeedingTransfers;
//   for (auto &F : GetModule().functions()) {
//     if (!CommonHAKCAnalysis::IsUncompartmentalizedSymbol(&F, Policy) &&
//         CommonAnalysis.functionIsTransferCandidate(&F, Policy) &&
//         !CommonHAKCAnalysis::IsOutsideTransferFunc(&F) && functionEscapes(&F)) {
//       FuncsNeedingTransfers.push_back(&F);
//     }
//   }
//   CommonHAKCAnalysis::SortFunctionList(FuncsNeedingTransfers);
//   for (auto *Funcp : FuncsNeedingTransfers) {
//     Function &F = *Funcp;
//     auto debug_output = CommonAnalysis.GetSystemInfo().OutputDebugInfo(Funcp);
//     Function *transferFunc = nullptr;
//
//     if (CommonAnalysis.functionIsTransferCandidate(&F, Policy)) {
//       auto Compartment = Policy.GetDivision(&F).GetHAKCCompartment();
//       transferFunc = GetTransformer().CreateTransferFunction(&F);
//       if (!transferFunc) {
//         CommonHAKCAnalysis::getWriter(true)
//             << "Could not create transfer for " << F.getName() << "\n";
//         throw std::exception();
//       }
//       bool TransferAlreadyExisted =
//           (transferFunc->getInstructionCount() > 0 && !F.isDeclaration());
//       if (TransferAlreadyExisted) {
//         CommonHAKCAnalysis::getWriter(debug_output)
//             << "Retrieved transfer function " << transferFunc->getName();
//       } else {
//         CommonHAKCAnalysis::getWriter(debug_output)
//             << "Created transfer function " << transferFunc->getName();
//       }
//       CommonHAKCAnalysis::getWriter(debug_output)
//           << " in compartment "
//           << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
//       if (!TransferAlreadyExisted) {
//         CommonHAKCAnalysis::getWriter(debug_output) << *transferFunc << "\n";
//       }
//
//       if (F.isDeclaration()) {
//         CommonHAKCAnalysis::getWriter(debug_output)
//             << F.getName() << " is a declaration\n";
//       }
//     } else {
//       CommonHAKCAnalysis::getWriter(debug_output)
//           << "No transfer created for " << F.getName() << "\n";
//     }
//     if (!transferFunc) {
//       transferFunc = GetFunctionByName(
//           CommonAnalysis.GetOutsideTransferName(&F), F.getFunctionType());
//       /* Ensure that transfer functions not defined here are treated
//        * the same as the function we are replacing */
//       transferFunc->setLinkage(F.getLinkage());
//       transferFunc->copyAttributesFrom(&F);
//     }
//
//     if (CommonAnalysis.ValueShouldBeReplacedWithTransfer(&F, Policy)) {
//       if (!CommonAnalysis.IsNoTransferFunction(&F)) {
//         CommonHAKCAnalysis::getWriter(debug_output)
//             << "Replacing uses of " << F.getName() << " with "
//             << transferFunc->getName() << "\n";
//         in_debug = debug_output;
//         std::vector<std::pair<User *, unsigned>> UsesToReplace;
//         auto TargetDivision = Policy.GetDivision(&F);
//
//         for (auto &FUse : F.uses()) {
//           if (auto *I = dyn_cast<Instruction>(FUse.getUser())) {
//             if (CommonHAKCAnalysis::IsOutsideTransferFunc(I->getFunction())) {
//               continue;
//             }
//           }
//           if (auto *CallI = dyn_cast<CallInst>(FUse.getUser())) {
//             if (CallI->getCalledFunction() == &F) {
//               auto HeadDivision =
//                   Policy.GetDivision(CallI->getParent()->getParent());
//               if (HeadDivision.GetHAKCCompartment().GetCompartmentID() !=
//                   TargetDivision.GetHAKCCompartment().GetCompartmentID()) {
//                 UsesToReplace.push_back(
//                     std::make_pair(CallI, FUse.getOperandNo()));
//                 continue;
//               }
//             }
//           }
//           if (useEscapes(FUse)) {
//             UsesToReplace.push_back(
//                 std::make_pair(FUse.getUser(), FUse.getOperandNo()));
//           }
//         }
//         for (auto &U : UsesToReplace) {
//           U.first->setOperand(U.second, transferFunc);
//         }
//
//         CommonHAKCAnalysis::getWriter(debug_output) << "Done\n"
//                                                     << GetModule() << "\n";
//       }
//     }
//     if (AliasShouldBeCreated(&F)) {
//       auto OrigName = F.getName().str();
//       auto NewName = CommonHAKCAnalysis::getOriginalTransformedName(&F);
//       CommonHAKCAnalysis::getWriter(debug_output)
//           << "Changing name from " << F.getName() << " to " << NewName << "\n";
//       F.setName(NewName);
//
//       auto *alias = GlobalAlias::create(OrigName, transferFunc);
//       CommonHAKCAnalysis::getWriter(debug_output)
//           << "Final Transfer:\n"
//           << *transferFunc << "\nAlias: " << *alias << "\n";
//     }
//   }
// }
//
// bool HAKCModuleTransform::ConstantStructTransferIsNeeded(
//     ConstantStruct *ConstStruct) {
//   bool Result = false;
//   CommonHAKCAnalysis::getWriter(
//       CommonAnalysis.GetSystemInfo().OutputDebugInfo())
//       << "TransferIsNeeded Checking " << *ConstStruct << "\n";
//   for (auto &Member : ConstStruct->operands()) {
//     auto *Def = CommonAnalysis.getDef(Member.get(), false);
//     CommonHAKCAnalysis::getWriter(
//         CommonAnalysis.GetSystemInfo().OutputDebugInfo())
//         << "Checking struct member " << *Member << " with Def " << *Def << "\n";
//     if (isa<ConstantPointerNull>(Def)) {
//       continue;
//     }
//     if (auto *GlobalVal = dyn_cast<GlobalValue>(Def)) {
//       Result =
//           !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalVal, Policy);
//     } else if (auto *StructMember = dyn_cast<ConstantStruct>(Def)) {
//       Result = ConstantStructTransferIsNeeded(StructMember);
//     }
//
//     if (Result) {
//       break;
//     }
//   }
//
//   CommonHAKCAnalysis::getWriter(
//       CommonAnalysis.GetSystemInfo().OutputDebugInfo())
//       << __FUNCTION__ << " Result: " << std::to_string(Result) << "\n";
//   return Result;
// }
//
// bool HAKCModuleTransform::TransferIsNeeded(GlobalVariable *GlobalVar) {
//   bool IsKernelSym =
//       CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalVar, Policy);
//   bool Result = GlobalVar->hasInitializer() && !IsKernelSym;
//   if (Result) {
//     if (auto *ConstStruct =
//             dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
//       Result = ConstantStructTransferIsNeeded(ConstStruct);
//     } else {
//       if (isa<GlobalValue>(GlobalVar->getInitializer())) {
//         Result = !IsKernelSym;
//       } else if (isa<ConstantPointerNull>(GlobalVar->getInitializer())) {
//         Result = false;
//       } else {
//         Result = CommonHAKCAnalysis::IsPointerLikeType(
//             GlobalVar->getInitializer()->getType());
//       }
//     }
//   }
//
//   return Result;
// }
//
// void HAKCModuleTransform::CreateInitGlobalMemberTransfers() {
//   std::vector<GlobalVariable *> GlobalsToModifyDuringInit;
//   for (auto &GV : GetModule().globals()) {
//     if (TransferIsNeeded(&GV)) {
//       GlobalsToModifyDuringInit.push_back(&GV);
//     }
//   }
//   CommonHAKCAnalysis::SortGlobalList(GlobalsToModifyDuringInit);
//
//   if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
//     CommonHAKCAnalysis::getWriter(true)
//         << "Creating Init transfer functions for "
//         << std::to_string(GlobalsToModifyDuringInit.size()) << " globals:\n";
//     for (auto *GlobToTransfer : GlobalsToModifyDuringInit) {
//       CommonHAKCAnalysis::getWriter(true) << GlobToTransfer->getName() << "\n";
//     }
//   }
//
//   for (auto *GlobToTransfer : GlobalsToModifyDuringInit) {
//     auto *InitTransfer = CreateInitTransfer(GlobToTransfer);
//     CommonHAKCAnalysis::getWriter(
//         CommonAnalysis.GetSystemInfo().OutputDebugInfo(GlobToTransfer))
//         << "Created InitTransfer " << InitTransfer->getName() << "\n";
//   }
// }
//
// Function *HAKCModuleTransform::CreateInitTransfer(GlobalVariable *GlobalVar) {
//   if (!GlobalVar->hasInitializer()) {
//     CommonHAKCAnalysis::getWriter(true) << GlobalVar << " has no initializer\n";
//     throw std::exception();
//   }
//
//   SmallString<128> FunctionName;
//   FunctionName.append(GlobalInitTransferPrefix());
//
//   for (auto letter : GlobalVar->getName()) {
//     if (letter != '@') {
//       FunctionName.push_back(letter);
//     }
//   }
//
//   auto *GlobalTransferTy =
//       FunctionType::get(Type::getVoidTy(GetModule().getContext()), {}, false);
//   auto *GlobalInitFunc =
//       GetFunctionByName(FunctionName.str(), GlobalTransferTy);
//   if (!GlobalInitFunc) {
//     CommonHAKCAnalysis::getWriter(true)
//         << "Could not get Global Transfer function " << FunctionName << "\n";
//     throw std::exception();
//   }
//
//   if (GlobalInitFunc->empty()) {
//     PopulateGlobalInitTransferFunc(GlobalInitFunc, GlobalVar);
//     CommonHAKCAnalysis::getWriter(
//         CommonAnalysis.GetSystemInfo().OutputDebugInfo(GlobalVar))
//         << "Finished Populating Global Init Transfer\n"
//         << *GlobalInitFunc << "\n";
//   }
//
//   return GlobalInitFunc;
// }
//
// std::string
// HAKCModuleTransform::GlobalVariableROSectionName(GlobalVariable *GlobalVar) {
//   auto Compartment = Policy.GetDivision(GlobalVar).GetHAKCCompartment();
//   std::string SectionName = ".hakc.";
//   SectionName += std::to_string(Compartment.GetCompartmentIDValue());
//   SectionName += ".ro_data";
//
//   return SectionName;
// }
//
// void HAKCModuleTransform::PopulateGlobalInitTransferFunc(
//     Function *GlobTransfer, GlobalVariable *GlobalVar) {
//   auto debug_output = CommonAnalysis.GetSystemInfo().OutputDebugInfo(GlobalVar);
//   CommonHAKCAnalysis::getWriter(debug_output)
//       << "Populating Global Init Transfer Function " << GlobTransfer->getName()
//       << "\n";
//
//   GlobTransfer->setSection(GlobalInitTransferSectionName());
//   if (!GlobTransfer->empty()) {
//     return;
//   }
//
//   if (GlobalVar->isConstant()) {
//     GlobalVar->setConstant(false);
//     GlobalVar->setSection(GlobalVariableROSectionName(GlobalVar));
//   }
//
//   CommonHAKCAnalysis::getWriter(debug_output)
//       << "Starting Global Init Population\n";
//   GetTransformer().PopulateGlobalTransfer(GlobTransfer, GlobalVar,
//                                           debug_output);
//   CommonHAKCAnalysis::VerifyFunction(GlobTransfer);
//
//   auto GlobalTrackerName = GlobTransfer->getName() + "_loc";
//   auto *TransferPointer =
//       dyn_cast<GlobalVariable>(GetModule().getOrInsertGlobal(
//           GlobalTrackerName.str(), GlobTransfer->getType()));
//   TransferPointer->setConstant(true);
//   TransferPointer->setInitializer(GlobTransfer);
//   TransferPointer->setSection(GlobalInitTransferPointerSectionName());
// }
//
// void HAKCModuleTransform::AddCompartmentMetadata() {
//   auto &DefaultCompartment = Policy.GetDefaultDivision().GetHAKCCompartment();
//   for (auto Compartment : UsedCompartments) {
//     if (Compartment != DefaultCompartment) {
//       GetTransformer().AddCompartmentMetadataEntry(Compartment);
//     }
//   }
// }
//
// // Get the StructType representing a kernel (module) parameter
// StructType *HAKCModuleTransform::GetKernelParamType() {
//   // linux
//   return llvm::StructType::getTypeByName(
//       GetModule().getContext(), llvm::StringRef("struct.kernel_param"));
// }
//
// // we generate these for all kernel params, some may go unused by the actual
// // module loader (non-pointer params are ignored by the loader when it comes to
// // transferring)
// //    void HAKCModuleTransform::transferModuleParams() {
// //
// //        if (!isModuleCompartmentalized()) {
// //            return;
// //        }
// //
// //        auto *KernelParamType = GetKernelParamType();
// //        // type not found, just do nothing
// //        if (!KernelParamType) {
// //            return;
// //        }
// //
// //        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
// //            CommonHAKCAnalysis::getWriter() << "kernel param type is: " <<
// //            *KernelParamType << "\n";
// //        }
// //
// //        // inspect all globals
// //        for (auto &Global: GetModule().globals()) {
// //            // trying to find globals of type GetKernelParamType()
// //            if (auto *StructTy = dyn_cast<StructType>(Global.getValueType()))
// //            {
// //                // if true, the type of Global matches GetKernelParamType
// //                if (StructTy == KernelParamType) {
// //                    if
// //                    (CommonAnalysis.GetSystemInfo().OutputDebugInfo(&Global))
// //                    {
// //                        CommonHAKCAnalysis::getWriter() << "found kernel
// //                        param: " << Global << "\n";
// //                    }
// //
// //                    // generate a GetCtx function for the parameter and update
// //                    // function pointer array
// //                    generateModuleParamGetCtxFunction(&Global);
// //                }
// //            }
// //        }
// //    }
//
// void HAKCModuleTransform::emitModParamGetCtx(GlobalValue *kernparam) {
//   // linux
//   // type of void*
//   PointerType *PointerTy =
//       PointerType::get(IntegerType::get(GetModule().getContext(), 8), 0);
//
//   // two args
//   std::vector<Type *> FuncTy_args;
//   // first arg points to param
//   FuncTy_args.push_back(PointerTy);
//   // second arg is int64_t flag (0 to return param's access token, 1 to return
//   // param's color)
//   FuncTy_args.push_back(IntegerType::get(GetModule().getContext(), 64));
//
//   // type of function that returns int64_t, takes (void *, int64_t)
//   FunctionType *FuncTy = FunctionType::get(
//       IntegerType::get(GetModule().getContext(), 64), FuncTy_args, false);
//
//   // create a function named "hakc_modparam_getctx_paramname"
//   auto c = GetModule().getOrInsertFunction(
//       MODPARAM_GETCTX_PREFIX.str() + kernparam->getName().str(), FuncTy);
//
//   auto *constc = dyn_cast<Constant>(c.getCallee());
//   auto *getctx = cast<Function>(constc);
//   getctx->setCallingConv(CallingConv::C);
//
//   // put "hakc_modparam_getctx_paramname" in a special text section in the
//   // module
//   getctx->setSection(HAKC_MODPARAM_TEXT_SECTION);
//
//   Function::arg_iterator args = getctx->arg_begin();
//   // param pointer
//   Value *pointerArg = args++;
//   // 0 to return context, 1 to return color
//   Value *returnTypeArg = args++;
//
//   // create entry basic block in our new function
//   BasicBlock *block =
//       BasicBlock::Create(GetModule().getContext(), "entry", getctx);
//   //
//   IRBuilder<> builder(block);
//   // constant zero for compare/select
//   Value *czero =
//       ConstantInt::get(IntegerType::get(GetModule().getContext(), 64), 0);
//
//   // get HAKC symbol for the kernel parameter Value
//   auto &Division = Policy.GetDivision(kernparam);
//
//   CommonHAKCAnalysis::getWriter(
//       CommonAnalysis.GetSystemInfo().OutputDebugInfo(kernparam))
//       << kernparam << " " << Division << "\n";
//
//   // cast kernparam to a void*
//   Value *voidCast;
//   auto AddrSpace = hakc::HAKCTransformer::GetPointerAddrSpace(kernparam);
//
//   if (kernparam->getType()->isIntegerTy()) {
//     voidCast = builder.CreateIntToPtr(kernparam, builder.getPtrTy(AddrSpace));
//   } else {
//     voidCast = builder.CreateBitCast(kernparam, builder.getPtrTy(AddrSpace));
//   }
//
//   // if returnTypeArg == 0, next step will use access token for return value
//   // else, use color for return value
//   Value *tokEqZero = builder.CreateICmpEQ(returnTypeArg, czero);
//   Value *tokColSelect = builder.CreateSelect(
//       tokEqZero, Division.GetAccessToken(), Division.GetDivisionID());
//
//   // check if the address passed in matches address of kernparam
//   Value *pointerArgEq = builder.CreateICmpEQ(pointerArg, voidCast);
//   // if it does, return the previously selected token/color
//   // it it isn't a match, return zero
//   Value *ctxSelect = builder.CreateSelect(pointerArgEq, tokColSelect, czero);
//   // function is done
//   builder.CreateRet(ctxSelect);
//
//   CommonHAKCAnalysis::getWriter(
//       CommonAnalysis.GetSystemInfo().OutputDebugInfo(kernparam))
//       << *getctx << "\n";
//
//   CommonHAKCAnalysis::VerifyFunction(getctx);
//
//   // generate function pointer and place in modparam fp section
//   auto CtxFPName = getctx->getName() + "_fp";
//   auto *gcfp = dyn_cast<GlobalVariable>(GetModule().getOrInsertGlobal(
//       CtxFPName.getSingleStringRef(), getctx->getType()));
//   gcfp->setSection(HAKC_MODPARAM_FUNCP_SECTION);
//   gcfp->setLinkage(GlobalValue::ExternalLinkage);
//   gcfp->setConstant(true);
//   gcfp->setInitializer(getctx);
// }
//
// bool HAKCModuleTransform::FunctionIsInAnalysisSet(Function *F) {
//   return CommonHAKCAnalysis::IsFunctionInFunctionList(
//       F, make_range(AnalysisFunctions.begin(), AnalysisFunctions.end()));
// }

// TODO: Add this to config definition
// takes a KernelParam and generate a function to get the HAKC signing context
// for the actual backing global variable
// used to correctly transfer charp parameters
//    void HAKCModuleAnalysis::generateModuleParamGetCtxFunction(GlobalVariable
//    *GV) {
//        // linux
//
//        GlobalValue *kernparam = ExtractGlobalFromKernelParam(GV);
//
//        if (!kernparam) {
//            CommonHAKCAnalysis::getWriter() << "Could not extract global from
//            kernel param " << *GV << "\n"; throw std::exception();
//        }
//
//        emitModParamGetCtx(kernparam);
//    }

//    void HAKCModuleAnalysis::updateCallParameters(const std::map<Function *,
//    std::set<CallInst *>> &calls_map) {
//        // linux
//        for (auto &pair: calls_map) {
//            Function *F = pair.first;
//            auto debug_output =
//            CommonAnalysis.GetSystemInfo().OutputDebugInfo(F);
//
//            for (auto &call: pair.second) {
//                auto HAKCTransferFunction =
//                GetHAKCTransferDef(call->getCalledFunction()->getName()); if
//                (HAKCTransferFunction) {
//                    if (debug_output) {
//                        CommonHAKCAnalysis::getWriter() << "Updating HAKC call
//                        parameters for " << *call << "\n";
//                    }
//                    hakc_compartment_id_t id;
//                    StringRef transferTargetName = F->getName();
//                    if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
//                        transferTargetName =
//                        F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
//                        Function *TransferTarget =
//                        GetModule().getFunction(transferTargetName); id =
//                        GetTransformer().getFunctionCompartmentID(TransferTarget);
//                    } else {
//                        id = GetTransformer().getFunctionCompartmentID(F);
//                    }
//                    if (id < 0) {
//                        CommonHAKCAnalysis::getWriter() << "Could not find
//                        Compartment ID for function "
//                                                        << transferTargetName
//                                                        << "\n";
//                        throw std::exception();
//                    }
//                    if (debug_output) {
//                        CommonHAKCAnalysis::getWriter() << "Updating index "
//                        << std::to_string(
//                                HAKCTransferFunction->GetCompartmentIdIdx())
//                                << " (";
//                        call->getArgOperand(HAKCTransferFunction->GetCompartmentIdIdx())->print(
//                                CommonHAKCAnalysis::getWriter());
//                        CommonHAKCAnalysis::getWriter() << ") to " <<
//                        std::to_string(id) << "\n";
//                    }
//                    call->setArgOperand(HAKCTransferFunction->GetCompartmentIdIdx(),
//                                        GetTransformer().GetHAKCCompartmentValue(id));
//
//                    if (HAKCTransferFunction->HasColorIdx()) {
//                        ConstantInt *color;
//                        if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
//                            transferTargetName =
//                            F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
//                            auto *TransferTarget =
//                            GetModule().getFunction(transferTargetName); color
//                            = getSymbolColor(TransferTarget);
//                        } else {
//                            color = getFunctionColor(F);
//                        }
//
//                        if (!color) {
//                            CommonHAKCAnalysis::getWriter() << "Could not find
//                            Color for function " << F->getName()
//                                                            << "\n";
//                            throw std::exception();
//                        }
//                        call->setArgOperand(HAKCTransferFunction->GetColorIdx(),
//                        color);
//                    }
//                    if (debug_output) {
//                        CommonHAKCAnalysis::getWriter() << "After update call
//                        is " << *call << "\n";
//                    }
//                }
//            }
//        }
//    }

//    bool HAKCModuleAnalysis::valueIsReadonlyPtr(Value *value) {
//        // linux
//        bool result = CommonAnalysis.valueIsReadonlyPtr(value);
//        if (!result) {
//            if (auto *callInst = dyn_cast<CallInst>(value)) {
//                /* We may have done some global transfer beforehand, so check
//                for that */ if (callInst->getCalledFunction() &&
//                    callInst->getCalledFunction()->getName() ==
//                    "hakc_sign_pointer_with_color") { auto *isCode =
//                    dyn_cast<ConstantInt>(
//                            callInst->getArgOperand(callInst->arg_size() -
//                            1));
//                    result = isCode->isOne();
//                }
//            }
//        }
//
//        return result;
//    }
} // namespace llvm::hakc
