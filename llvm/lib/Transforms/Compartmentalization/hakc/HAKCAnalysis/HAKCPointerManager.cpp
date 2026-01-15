//
// Created by de29664 on 11/14/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCPointerManager.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCLogger.h"

namespace llvm::hakc {
using namespace llvm;

HAKCPointerManager::HAKCPointerManager(Function *CurrentFunction,
                                       HAKCModuleAnalysis &ModuleAnalysis)
    : CurrentFunction(CurrentFunction), ModuleAnalysis(ModuleAnalysis),
      DebugActive(ModuleAnalysis.GetSystemInformation().OutputDebugInfo(
          CurrentFunction)) {}

HAKCLogger &HAKCPointerManager::GetLogger(const HAKCLogLevel log_level,
                                          const bool suppress_output) const {
  return ModuleAnalysis.GetCommonAnalysis().getLogger(log_level,
                                                      suppress_output);
}

bool HAKCPointerManager::IsPHIOfGlobalsOnly(Value *V) {
  std::set<PHINode *> nodes;
  return isPHIofGlobalsOnly(V, nodes);
}

HAKCModuleAnalysis &HAKCPointerManager::GetModuleAnalysis() const {
  return ModuleAnalysis;
}

Function &HAKCPointerManager::GetFunction() const { return *CurrentFunction; }

/**
 * @brief Returns true of ptr is a PHINode consisting only of global variables
 * @param ptr
 * @param nodes
 * @return
 */
bool HAKCPointerManager::isPHIofGlobalsOnly(Value *ptr,
                                            std::set<PHINode *> &nodes) {
  if (auto *phiNode = dyn_cast<PHINode>(ptr)) {
    if (nodes.contains(phiNode)) {
      return true;
    }
    ModuleAnalysis.GetCommonAnalysis().getLogger(Verbose)
        << "Examining PHI Node " << phiNode << " for Globals (" << nodes.size()
        << ")\n";
    nodes.insert(phiNode);
    for (auto &val : phiNode->incoming_values()) {
      Value *Definition = GetDef(val.get());
      ModuleAnalysis.GetCommonAnalysis().getLogger(Verbose)
          << "\tPHI Node value: " << val << "\n\t\tDef: " << Definition << "\n";
      if (!isa<GlobalValue>(Definition)) {
        if (isa<PHINode>(Definition)) {
          if (isPHIofGlobalsOnly(Definition, nodes)) {
            continue;
          }
        }
        return false;
      }
    }
    return true;
  }
  return false;
}

bool HAKCPointerManager::PointerIsEligibleForManagement(const Use &U) {

  GetLogger(Verbose, !DebugActive)
      << "Starting Pointer Management checks for " << U.get() << " from "
      << U.getUser() << "\n";

  /* The HAKCPointerManager::GetDef method performs some analysis to find a
   * definition that could be different from the "true" definition. Use the true
   * definition to check if we are managing constant strings.
   */
  auto *Pointer = U.get();
  // auto *Definition = GetFunctionAnalysis().getDef(Pointer, false);
  auto *Definition = ModuleAnalysis.GetCommonAnalysis().getDef(Pointer, false);
  auto *PointerTy = Pointer->getType();

  if (const auto *AllocaI = dyn_cast<AllocaInst>(Definition)) {
    PointerTy = AllocaI->getAllocatedType();
  }

  if (!ModuleAnalysis.GetCommonAnalysis().IsPointerLikeType(
          Definition->getType())) {
    GetLogger(Verbose, !DebugActive)
        << "Definition is not a pointer-like Type\n";
    return false;
  }

  if (isa<ConstantPointerNull>(Definition)) {
    GetLogger(Verbose, !DebugActive) << "Pointer Manager ignores null pointers\n";
    return false;
  }
  if (isa<ConstantInt>(Pointer)) {
    GetLogger(Verbose, !DebugActive) << "Pointer Manager ignores Constant Ints\n";
    return false;
  }
  if (!CommonHAKCAnalysis::IsPointerLikeType(PointerTy)) {
    GetLogger(Verbose, !DebugActive) << "Pointer Manager ignores non-pointers\n";
    return false;
  }
  GetLogger(Verbose, !DebugActive) << *Pointer << " Type " << PointerTy << " is a pointer like type\n";

  if (const auto *GV = dyn_cast<GlobalVariable>(Definition)) {
    if (CommonHAKCAnalysis::IsStringType(GV->getValueType())) {
      GetLogger(Verbose, !DebugActive) << "Pointer Manager is ignoring constant string " << Definition << "\n";
      return false;
    }
  }

  if (auto *call = dyn_cast<CallInst>(Pointer)) {
    GetLogger(Verbose, !DebugActive) << "Value " << *Pointer << " is a CallInst\n";

    if (bool IsInline = call->isInlineAsm()) {
      GetLogger(Verbose, !DebugActive) << "Call is Inline Assembly\n";
      /* These are usually the result of reading a register value */
      return ModuleAnalysis.GetCommonAnalysis().ValueIsUsedAsPointer(call);
    }
    if (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic() &&
        call->getCalledFunction()->getIntrinsicID() == Intrinsic::IndependentIntrinsics::read_register) {
      GetLogger(Verbose, !DebugActive) << "Call is a read register intrinsic\n";
      return false;
    }
    if (call->getType()->isIntegerTy(32)) {
      /* Sometimes functions that return i32 are cast to a pointer for a check
       * against IS_ERR(). No need to check this.
       * See find_mm_struct in mm/migrate.c.
       */
      GetLogger(Verbose, !DebugActive) << "Call returns 32-bit integer\n";
      return false;
    }
  } else if (auto *ConstExpr = dyn_cast<ConstantExpr>(Pointer)) {
    if (ConstExpr->isCast()) {
      // auto *Operand = GetFunctionAnalysis().getDef(ConstExpr->getOperand(0), false);
      auto *Operand = ModuleAnalysis.GetCommonAnalysis().getDef(ConstExpr->getOperand(0), false);
      GetLogger(Verbose, !DebugActive)
          << *ConstExpr << " operand def is " << *Operand << "\n";
      if (isa<ConstantInt>(Operand)) {
        GetLogger(Verbose, !DebugActive) << "ConstExpr is from ConstantInt\n";
        return false;
      }
    }
  } else if (isa<Constant>(Pointer) && Pointer->getType()->isIntegerTy()) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is a constant int\n";
    return false;
  } else if (!CommonHAKCAnalysis::IsPointerLikeType(Pointer->getType()) && !Pointer->getType()->isArrayTy() && !isa<PtrToIntInst>(Pointer)) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is not a pointer, array, or pointer to int cast\n";
    return false;
  } else if (isa<ConstantPointerNull>(Pointer)) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is a constant null pointer\n";
    return false;
  } else if (IsPHIOfGlobalsOnly(Pointer)) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is a PHINode of Globals\n";
    return false;
  } else if (ModuleAnalysis.GetCommonAnalysis().IsKernelUserPointer(Pointer)) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is a Kernel pointer from user space\n";
    return false;
  } else if (auto *LoadI = dyn_cast<LoadInst>(Pointer)) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is used in a LoadInst\n";
    return !ModuleAnalysis.GetCommonAnalysis().IsIgnoredGlobal(
               LoadI->getPointerOperand()) &&
           ModuleAnalysis.GetCommonAnalysis().ValueIsUsedAsPointer(Pointer);
  }
  else if (auto *StoreI = dyn_cast<StoreInst>(U.getUser())) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is used in a StoreInst\n";
    for (auto &Op : StoreI->operands()) {
      if (ModuleAnalysis.GetCommonAnalysis().IsIgnoredGlobal(Op.get())) {
        return false;
      }
    }
    return U.getOperandNo() == StoreInst::getPointerOperandIndex() &&
           !ModuleAnalysis.GetCommonAnalysis().IsKernelUserPointer(Pointer);
  } else if (auto *AllocaI = dyn_cast<AllocaInst>(Pointer)) {
    for (auto &Use : AllocaI->uses()) {
      if (isa<StoreInst>(Use.getUser())) {
        for (auto &Op : Use.getUser()->operands()) {
          if (ModuleAnalysis.GetCommonAnalysis().IsIgnoredGlobal(Op.get())) {
            return false;
          }
        }
      }
    }
  } else if (isa<UndefValue>(Pointer)) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " is an undef value\n";
    return false;
  } else if (const auto *CallI = dyn_cast<CallInst>(U.getUser())) {
    if (CallI->isInlineAsm()) {
      GetLogger(Verbose, !DebugActive) << *Pointer << " is used in inline assembly\n";
      return ModuleAnalysis.GetCommonAnalysis().ValueIsUsedAsPointer(U.get());
    }
  } else if (!Pointer->getType()->isPointerTy()) {
    GetLogger(Verbose, !DebugActive) << *Pointer << " Type is not a pointer: " << *Pointer->getType() << "\n";
    return false;
  } else if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
    // Check if use is a load (load -> GEP -> load pattern)
    if (!GetManagedPointer(GEP->getPointerOperand())) {
      if (isa<StoreInst>(U.getUser()) || isa<LoadInst>(U.getUser())) {
        GetLogger(Verbose, !DebugActive)
            << "Pointer of " << *Pointer
            << " is used in a load or store and must be managed\n";
        return true;
      }
      GetLogger(Verbose, !DebugActive)
          << "Pointer of " << *Pointer << " is not managed\n";
      return false;
    }
  }
  return Pointer->getType()->isPointerTy() &&
         !ModuleAnalysis.GetCommonAnalysis().IsKernelUserPointer(Pointer);
}

bool HAKCPointerManager::ManageNewPointer(Use &U) {
  auto *BaseDefinition = GetDef(U.get());
  if (!BaseDefinition) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find BaseDefinition for " << U << "\n";
    throw std::exception();
  }
  if (isa<IntToPtrInst>(U.get())) {
    bool is_percpu_ptr = ModuleAnalysis.GetCommonAnalysis().IsPerCPUPointer(U);

    if (is_percpu_ptr) {
      GetLogger(Verbose, !DebugActive) << "Detected per-cpu pointer: " << U << "\n";
      BaseDefinition = U.get();
    }
  }
  auto NextID = CurrentPointerID + 1;

  auto ManagedPointer = std::make_shared<ManagedHAKCPointer>(BaseDefinition, *this, NextID);
  ModuleAnalysis.GetTypeIdentifier().FindType(*ManagedPointer);
  if (ManagedPointer->GetType() && ManagedPointer->GetType()->IsIgnoredType()) {
    GetLogger(Verbose, !DebugActive) << "Ignoring pointer " << ManagedPointer << " because its HAKCType is ignored\n";
    return false;
  }

  if (ManagedPointer->GetType() &&
      ManagedPointer->GetType()->GetPointeeType() == nullptr &&
      (ManagedPointer->GetType()->IsStructType() ||
       ManagedPointer->GetType()->IsUnionType() ||
       ManagedPointer->GetType()->IsEnumType())) {
    GetLogger(Verbose, !DebugActive)
        << "Ignoring " << *ManagedPointer
        << " because it is an enum or a struct or union that is not equivalent to a pointer\n";
    return false;
  }
  if (ManagedPointer->GetType() && ManagedPointer->GetType()->IsIntegerType() &&
      !ManagedPointer->GetType()->GetPointeeType()) {
    const auto PointeeTy = ModuleAnalysis.GetTypeIdentifier().GetVoidPointerPointeeType();
    ManagedPointer->GetType()->SetPointeeType(PointeeTy);
  }
  if (ManagedPointer->GetType() && !ManagedPointer->GetType()->IsPointerType()) {
    CommonHAKCAnalysis::getLogger(Verbose, !DebugActive) << *ManagedPointer << " is not a pointer type\n";
  }
  CurrentPointerID++;
  GetLogger(Verbose, !DebugActive)
      << "Starting the management of pointer " << NextID
      << " with BaseDefinition " << BaseDefinition << "\n";
  ManagedPointersList.push_back(ManagedPointer);
  AnalyzedUses.clear();
  ClassifyAllUsesOfDefinition(ManagedPointer->GetBaseDefinition(),
                              *ManagedPointer);
  GetLogger(Verbose, !DebugActive) << "Managing " << ManagedPointer;
  if (ManagedPointer->GetType()) {
    GetLogger(Verbose, !DebugActive) << " with HAKCType " << *ManagedPointer->GetType();
  }
  GetLogger(Verbose, !DebugActive) << "\n";

  return true;
}

bool HAKCPointerManager::UseIsAnalyzed(ManagedHAKCPointerUse &MangedPtrUse) {
  auto Search = [MangedPtrUse](const ManagedHAKCPointerUseP &UPtr) {
    return *UPtr == MangedPtrUse;
  };

  return llvm::any_of(AnalyzedUses, Search);
}

bool HAKCPointerManager::IsConstantExprUsedInKernelCall(User *U) const {
  if (isa<ConstantExpr>(U)) {
    for (auto *ConstUser : U->users()) {
      if (auto *Call = dyn_cast<CallBase>(ConstUser)) {
        if (Call->getFunction() == CurrentFunction) {
          // && IsUncompartmentalizedSymbol(Call->getCalledFunction())
          return true;
        }
      }
    }
  }

  return false;
}

bool HAKCPointerManager::UseShouldBeIgnored(const Use &U) const {
  auto *UserP = U.getUser();
  bool UseShouldBeIgnored = false;
  if (auto *Cmp = dyn_cast<CmpInst>(UserP)) {
    for (auto &Op : Cmp->operands()) {
      if (isa<ConstantPointerNull>(Op.get())) {
        UseShouldBeIgnored = true;
        break;
      }
    }
  } else if (CommonHAKCAnalysis::IsConstantUsedInGlobal(UserP) ||
             isa<BlockAddress>(UserP) || isa<GlobalVariable>(UserP) ||
             isa<GlobalAlias>(UserP)) {
    UseShouldBeIgnored = true;
  } else if (auto *Op = dyn_cast<Operator>(UserP)) {
    auto *Def = GetDef(Op);
    if (auto *I = dyn_cast<Instruction>(Def)) {
      if (I->getFunction() != CurrentFunction) {
        UseShouldBeIgnored = true;
      }
    } else {
      UseShouldBeIgnored = !isa<Argument>(Def);
    }
  }

  if (!UseShouldBeIgnored) {
    if (auto *I = dyn_cast<Instruction>(UserP)) {
      UseShouldBeIgnored = (I->getFunction() != CurrentFunction);
    } else if (auto *A = dyn_cast<Argument>(UserP)) {
      UseShouldBeIgnored = (A->getParent() != CurrentFunction);
    }
  }

  return UseShouldBeIgnored;
}

bool HAKCPointerManager::UseShouldBeCloned(const Use &U) {
  auto *UserP = U.getUser();
  bool CloneUse = isa<BitCastInst>(UserP) || isa<PtrToIntInst>(UserP) ||
                  isa<SelectInst>(UserP) || isa<SExtInst>(UserP) ||
                  isa<IntToPtrInst>(UserP) || isa<PHINode>(UserP) ||
                  isa<BinaryOperator>(UserP) || isa<TruncInst>(UserP) ||
                  isa<ZExtInst>(UserP);

  if (isa<SubOperator>(UserP)) {
    CloneUse = false;
  } else if (isa<GetElementPtrInst>(UserP)) {
    if (U.getOperandNo() == GetElementPtrInst::getPointerOperandIndex()) {
      CloneUse = true;
    }
  }

  return CloneUse;
}

bool HAKCPointerManager::IsCallInIntrinsicSet(
    CallBase *Call, ArrayRef<Intrinsic::ID> IDs) const {
  bool result = false;
  if (auto *intrinsic = dyn_cast<IntrinsicInst>(Call)) {
    auto IDToFind = intrinsic->getIntrinsicID();
    auto Search = [IDToFind](const Intrinsic::ID ID) { return IDToFind == ID; };

    result = any_of(IDs, Search);
    GetLogger(Verbose, !DebugActive)
        << "Intrinsic (" << IDToFind << ") from "
        << Call->getFunction()->getName() << " " << intrinsic;
    if (result) {
      GetLogger(Verbose, !DebugActive) << " is in { ";
    } else {
      GetLogger(Verbose, !DebugActive) << " is not in { ";
    }
    for (const auto id : IDs) {
      GetLogger(Verbose, !DebugActive) << id << " ";
    }
    GetLogger(Verbose, !DebugActive) << "}\n";
  }
  return result;
}

bool HAKCPointerManager::IsIntrinsicNeedingCloning(CallBase *Call) const {
  constexpr Intrinsic::ID IntrinsicsNeedingCloning[] = {
      Intrinsic::IndependentIntrinsics::lifetime_start,
      Intrinsic::IndependentIntrinsics::lifetime_end,
  };
  return IsCallInIntrinsicSet(Call, IntrinsicsNeedingCloning);
}

bool HAKCPointerManager::IsIntrinsicNeedingAuthentication(
    CallBase *Call) const {
  constexpr Intrinsic::ID IntrinsicsNeedingAuth[] = {
      Intrinsic::IndependentIntrinsics::memcpy,
      Intrinsic::IndependentIntrinsics::memmove,
      Intrinsic::IndependentIntrinsics::memset};

  return IsCallInIntrinsicSet(Call, IntrinsicsNeedingAuth);
}

bool HAKCPointerManager::UseShouldUtilizeAuthenticatedPointer(
    const Use &U) const {
  auto *UserP = U.getUser();
  bool UseAuthenticatedPointer = isa<CmpInst>(UserP) || isa<LoadInst>(UserP) ||
                                 isa<SubOperator>(UserP) ||
                                 isa<FreezeInst>(UserP);
  if (auto *Call = dyn_cast<CallBase>(UserP)) {
    if (ModuleAnalysis.GetCommonAnalysis().IsHAKCTransferFunction(
            Call->getCalledFunction())) {
      UseAuthenticatedPointer = false;
    } else if (ModuleAnalysis.GetCommonAnalysis().IsSafeTransitionCall(Call) ||
               Call->isInlineAsm() ||
               Call->getCalledOperandUse().getOperandNo() == U.getOperandNo() ||
               IsIntrinsicNeedingCloning(Call) ||
               IsIntrinsicNeedingAuthentication(Call)) {
      UseAuthenticatedPointer = true;
    }
  } else if (isa<StoreInst>(UserP)) {
    // Q: Do we want the store instruction to use the authenticated pointer?
    if (U.getOperandNo() == StoreInst::getPointerOperandIndex()) {
      UseAuthenticatedPointer = true;
      // UseAuthenticatedPointer = false;
    }
  } else if (isa<AtomicCmpXchgInst>(UserP)) {
    if (U.getOperandNo() == AtomicCmpXchgInst::getPointerOperandIndex()) {
      UseAuthenticatedPointer = true;
    }
  } else if (isa<AtomicRMWInst>(UserP)) {
    if (U.getOperandNo() == AtomicRMWInst::getPointerOperandIndex()) {
      UseAuthenticatedPointer = true;
    }
  } else if (isa<ReturnInst>(UserP)) {
    /* Returning pointers should be authenticated, but otherwise not, because,
     * e.g., they might be the result of the subtraction of two pointers */
    if (CommonHAKCAnalysis::IsPointerLikeType(UserP->getType())) {
      UseAuthenticatedPointer = false;
    }
  } else if (const auto *ConstExpr = dyn_cast<ConstantExpr>(UserP)) {
    if (/*ConstExpr->isCompare() || */ ConstExpr->getOpcode() ==
        Instruction::GetElementPtr) {
      UseAuthenticatedPointer = true;
    }
  } else if (isa<GetElementPtrInst>(UserP)) {
    if (U.getOperandNo() != GetElementPtrInst::getPointerOperandIndex()) {
      UseAuthenticatedPointer = true;
    }
  }
  return UseAuthenticatedPointer;
}

bool HAKCPointerManager::UseShouldUtilizeSignedBasePointer(const Use &U) const {
  auto *UserP = U.getUser();
  bool UseSignedPointer =
      isa<AddrSpaceCastOperator>(UserP) || isa<BitCastOperator>(UserP) ||
      isa<GEPOperator>(UserP) || isa<PtrToIntOperator>(UserP) ||
      /*isa<ZExtOperator>(UserP) ||*/
      isa<ReturnInst>(UserP) || isa<SwitchInst>(UserP) ||
      isa<InsertValueInst>(UserP);
  if (isa<StoreInst>(UserP)) {
    if (U.getOperandNo() != StoreInst::getPointerOperandIndex()) {
      UseSignedPointer = true;
    }
  } else if (isa<AtomicCmpXchgInst>(UserP)) {
    if (U.getOperandNo() != AtomicCmpXchgInst::getPointerOperandIndex()) {
      UseSignedPointer = true;
    }
  } else if (const auto *Call = dyn_cast<CallInst>(UserP)) {
    if (!ModuleAnalysis.GetCommonAnalysis().IsSafeTransitionCall(Call) ||
        Call->getCalledFunction() != nullptr ||
        ModuleAnalysis.GetCommonAnalysis().IsHAKCTransferFunction(Call->getCalledFunction())) {
      // || IsUncompartmentalizedSymbol(Call->getCalledFunction())
      UseSignedPointer = true;
    } else if (Call->isInlineAsm()) {
      UseSignedPointer = false;
    }
  } else if (isa<AtomicRMWInst>(UserP)) {
    if (U.getOperandNo() != AtomicRMWInst::getPointerOperandIndex()) {
      UseSignedPointer = true;
    }
  } else if (isa<ReturnInst>(UserP)) {
    /* Returning pointers should be authenticated, but otherwise not, because,
     * e.g., they might be the result of the subtraction of two pointers */
    if (CommonHAKCAnalysis::IsPointerLikeType(UserP->getType())) {
      UseSignedPointer = true;
    }
  } else if (IsConstantExprUsedInKernelCall(UserP)) {
    UseSignedPointer = true;
  }

  return UseSignedPointer;
}

bool HAKCPointerManager::IsClonedUseNeedingAdditionalClassification(const Use &U) {
  bool NeedsAdditionalClassification = !isa<PHINode>(U.getUser());
  const auto ManagedPointer = GetManagedPointer(U.getUser());
  if (ManagedPointer && U.getUser() == ManagedPointer->GetBaseDefinition()) {
    NeedsAdditionalClassification = false;
  }

  return NeedsAdditionalClassification;
}

ManagedHAKCPointerUseP
HAKCPointerManager::CreateManagedPointerUse(ManagedHAKCPointer &ManagedPointer, User *U, unsigned int OperandNo) {
  return std::make_shared<ManagedHAKCPointerUse>(ManagedPointer, U, OperandNo, CurrentPointerUseID++);
}

void HAKCPointerManager::ClassifyAllUsesOfDefinition(Value *Definition, ManagedHAKCPointer &ManagedPointer) {
  GetLogger(Verbose, !DebugActive)<< "Classifying " << std::to_string(Definition->getNumUses())
      << " uses of " << Definition << "\n";

  for (auto &U : Definition->uses()) {
    auto *User = U.getUser();
    if (auto *I = dyn_cast<Instruction>(User)) {
      if (I->getFunction() != CurrentFunction) {
        continue;
      }
    }

    auto UPtr = CreateManagedPointerUse(ManagedPointer, User, U.getOperandNo());
    if (UseIsAnalyzed(*UPtr)) {
      GetLogger(Verbose, !DebugActive) << *UPtr << " is already analyzed\n";
      continue;
    }
    AnalyzedUses.push_back(UPtr);
    GetLogger(Verbose, !DebugActive) << "Classifying " << *UPtr << "\n";
    if (UseShouldBeIgnored(U)) {
      GetLogger(Verbose, !DebugActive) << *UPtr << " is being ignored\n";
      continue;
    }
    if (UseShouldBeCloned(U)) {
      GetLogger(Verbose, !DebugActive) << *User << " should be cloned\n";
      if (IsClonedUseNeedingAdditionalClassification(U)) {
        ClassifyAllUsesOfDefinition(User, ManagedPointer);
      }
      ManagedPointer.AddCloneUse(UPtr);
    } else if (UseShouldUtilizeAuthenticatedPointer(U)) {
      GetLogger(Verbose, !DebugActive)
          << *UPtr << " should use authenticated Base Definition\n";
      ManagedPointer.AddAuthenticatedUse(UPtr);
    } else if (UseShouldUtilizeSignedBasePointer(U)) {
      GetLogger(Verbose, !DebugActive)
          << *UPtr << " should use signed Base Definition\n";
      if (auto *Call = dyn_cast<CallBase>(User)) {
        if (ModuleAnalysis.GetCommonAnalysis().IsHAKCTransferFunction(
                Call->getCalledFunction())) {
          ManagedPointer.RegisterManualHAKCTransfer(Call);
          GetLogger(Verbose, !DebugActive)
              << "Registered " << *Call << " as the protected pointer of "
              << ManagedPointer << ".  Classifying uses...\n";
          ClassifyAllUsesOfDefinition(Call, ManagedPointer);
          continue;
        }
      }
      ManagedPointer.AddProtectedUse(UPtr);
    } else {
      CommonHAKCAnalysis::getLogger(Error)
          << "Unexpected use of " << *UPtr << " --- " << UPtr->get()
          << " --- with " << ManagedPointer << " in \n";
      CommonHAKCAnalysis::getLogger(Fatal) << "\n";
      throw std::exception();
    }
  }
}

/**
 *
 * @param U
 * @return True if a new pointer is being managed
 */
bool HAKCPointerManager::ManagePointer(Use &U) {
  bool Result = PointerIsEligibleForManagement(U);
  if (!Result) {
    GetLogger(Verbose, !DebugActive) << "Use " << U << " is not eligible for management\n";
  } else {
    if (!GetManagedPointer(U.get())) {
      Result = ManageNewPointer(U);
    } else {
      Result = false;
    }
  }
  return Result;
}

iterator_range<ManagedHAKCPointerListType::iterator>
HAKCPointerManager::ManagedPointers() {
  llvm::sort(
      ManagedPointersList.begin(), ManagedPointersList.end(),
      [](const ManagedHAKCPointerP &LHS, const ManagedHAKCPointerP &RHS) {
        return LHS->GetID() < RHS->GetID();
      });
  return make_range(ManagedPointersList.begin(), ManagedPointersList.end());
}

ManagedHAKCPointerP HAKCPointerManager::GetManagedPointer(Value *V) {
  GetLogger(Verbose, !DebugActive)
      << "Finding Managed Pointer for " << V << "\n";
  auto *Def = GetDef(V);
  for (auto &ManagedPointer : ManagedPointers()) {
    if (*ManagedPointer == Def ||
        (ManagedPointer->GetProtectedPointer() &&
         ManagedPointer->GetProtectedPointer() == Def)) {
      return ManagedPointer;
    }
  }

  return nullptr;
}

bool HAKCPointerManager::empty() const { return ManagedPointersList.empty(); }

Value *HAKCPointerManager::GetDef(Value *V) const {
  // auto *BaseDefinition = GetFunctionAnalysis().getDef(V, false);
  auto *BaseDefinition = ModuleAnalysis.GetCommonAnalysis().getDef(V, false);

  if (isa<GlobalVariable>(BaseDefinition) && !CommonHAKCAnalysis::IsStringType(BaseDefinition->getType())) {
    Value *NewBaseDefinition = nullptr;
    SmallVector<Value *> DefChain;
    ModuleAnalysis.GetCommonAnalysis().findDefChain(V, false, DefChain);
    for (auto *Link : DefChain) {
      if (isa<CallInst>(Link)) {
        NewBaseDefinition = Link;
        break;
      }
    }

    if (NewBaseDefinition) {
      GetLogger(Verbose, !DebugActive)
          << "Changing BaseDefinition from " << *BaseDefinition << " to "
          << *NewBaseDefinition << "\n";
      BaseDefinition = NewBaseDefinition;
    }
  }

  return BaseDefinition;
}

Value *HAKCPointerManager::FindManagedPointerReplacement(Value *Target, const bool ReturnAuthenticatedPointer) {
  Value *Result = nullptr;
  for (const auto &ManagedPtr : ManagedPointers()) {
    if (ManagedPtr->GetBaseDefinition() == Target ||
        ManagedPtr->GetAuthenticatedPointer() == Target ||
        ManagedPtr->GetProtectedPointer() == Target) {
      GetLogger(Verbose, !DebugActive) << "Returning ";
      if (ReturnAuthenticatedPointer) {
        GetLogger(Verbose, !DebugActive) << "authenticated";
      } else {
        GetLogger(Verbose, !DebugActive) << "protected";
      }
      GetLogger(Verbose, !DebugActive) << " pointer for " << *Target << "\n";

      if (ReturnAuthenticatedPointer) {
        Result = ManagedPtr->GetAuthenticatedPointer();
      } else {
        Result = ManagedPtr->GetProtectedPointer();
      }
      break;
    }
  }
  return Result;
}

Value *HAKCPointerManager::FindAuthenticatedValue(ManagedHAKCPointerUse &PointerUse) {
  auto *AuthValue = FindManagedValue(AuthenticatedValues, PointerUse);
  if (!AuthValue) {
    GetLogger(Verbose, !DebugActive)
        << "Unable to find Authenticated Managed Value for PointerUse "
        << PointerUse << "\n";
    AuthValue = FindManagedPointerReplacement(PointerUse.get(), true);
  }
  if (AuthValue) {
    GetLogger(Verbose, !DebugActive)
        << "Found authenticated managed pointer " << AuthValue << " for "
        << PointerUse << "\n";
  }
  return AuthValue;
}

Value *HAKCPointerManager::FindProtectedValue(
    const ManagedHAKCPointerUse &PointerUse) {
  auto *ProtValue = FindManagedValue(ProtectedValues, PointerUse);
  if (!ProtValue) {
    GetLogger(Verbose, !DebugActive) << "Unable to find Protected Managed Value for " << PointerUse << "\n";
    ProtValue = FindManagedPointerReplacement(PointerUse.get(), false);
  }
  if (ProtValue) {
    GetLogger(Verbose, !DebugActive)
        << "Found protected managed pointer " << ProtValue << " for "
        << PointerUse << "\n";
  }
  return ProtValue;
}

Value *HAKCPointerManager::FindManagedValue(const std::map<ManagedHAKCPointerUseP, Value *> &Storage,
  const ManagedHAKCPointerUse &PointerUse) {
  for (const auto &[key, val] : Storage) {
    if (PointerUse == *key) {
      return val;
    }
  }

  return nullptr;
}

Value *HAKCPointerManager::FindManagedValue(const std::map<ManagedHAKCPointerUseP, Value *> &Storage, const Value *Target) {
  for (const auto &[key, val] : Storage) {
    if (key->get() == Target) {
      return val;
    }
  }

  return nullptr;
}

Value *HAKCPointerManager::FindAuthenticatedValue(Value *V) {
  auto *AuthValue = FindManagedValue(AuthenticatedValues, V);
  if (!AuthValue) {
    GetLogger(Verbose, !DebugActive) << "Unable to find Authenticated Managed Value for " << V << "\n";
    AuthValue = FindManagedPointerReplacement(V, true);
  }
  if (AuthValue) {
    GetLogger(Verbose, !DebugActive) << "Found authenticated managed pointer " << AuthValue << " for " << V << "\n";
  }
  return AuthValue;
}

Value *HAKCPointerManager::FindProtectedValue(Value *V) {
  auto *ProtValue = FindManagedValue(ProtectedValues, V);
  if (!ProtValue) {
    GetLogger(Verbose, !DebugActive) << "Unable to find Protected Managed Value for " << V << "\n";
    ProtValue = FindManagedPointerReplacement(V, false);
  }
  if (ProtValue) {
    GetLogger(Verbose, !DebugActive) << "Found protected managed pointer " << ProtValue << " for " << V << "\n";
  }
  return ProtValue;
}

void HAKCPointerManager::AddHAKCPointerReplacement(
    const ManagedHAKCPointerUseP &PtrUse, Value *Replacement,
    const bool AddingAuthenticatedReplacements) {
  const StringRef StorageName = AddingAuthenticatedReplacements ? "Authenticated" : "Protected";
  std::map<ManagedHAKCPointerUseP, Value *> &StorageToUse = (AddingAuthenticatedReplacements ? AuthenticatedValues : ProtectedValues);
  const std::map<ManagedHAKCPointerUseP, Value *> &OtherStorage = (AddingAuthenticatedReplacements ? ProtectedValues : AuthenticatedValues);

  GetLogger(Verbose, !DebugActive) << "Adding " << StorageName << " Pointer Replacement: " << *PtrUse
      << " -> " << Replacement << "\n";

  if (!PtrUse->getManagedPtr().PointerSetsCanBeEqual()) {
    if (const auto *OtherStorageReplacement = FindManagedValue(OtherStorage, *PtrUse)) {
      if (OtherStorageReplacement == Replacement) {
        const StringRef OtherStorageName = AddingAuthenticatedReplacements ? "Protected" : "Authenticated";
        CommonHAKCAnalysis::getLogger(Fatal)
            << StorageName << " replacement " << Replacement << " for "
            << *PtrUse << " matches " << OtherStorageName
            << " replacement in function\n"
            << CurrentFunction << "\n";
        throw std::exception();
      }
    }
  }

  auto *ExistingPointer = FindManagedValue(StorageToUse, *PtrUse);
  if (!ExistingPointer) {
    GetLogger(Verbose, !DebugActive) << "Adding New " << StorageName << " Pointer Replacement\n";
    StorageToUse[PtrUse] = Replacement;
  } else {
    if (Replacement && ExistingPointer != Replacement) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Trying to replace existing " << StorageName << " Replacement "
          << ExistingPointer << " with " << Replacement << " for " << *PtrUse
          << "\n" << CurrentFunction << "\n";
      throw std::exception();
    }
    if (Replacement) {
      GetLogger(Verbose, !DebugActive) << "Setting Existing " << StorageName << " Pointer Replacement\n";
      StorageToUse[PtrUse] = Replacement;
    } else {
      GetLogger(Verbose, !DebugActive) << "Tried to add null to " << StorageName
          << "Pointer Replacement for " << *PtrUse << "\n";
    }
  }
}

void HAKCPointerManager::AddAuthenticatedPointer(const ManagedHAKCPointerUseP &PointerUse, Value *Replacement) {
  AddHAKCPointerReplacement(PointerUse, Replacement, true);
}

void HAKCPointerManager::AddProtectedPointer(const ManagedHAKCPointerUseP &PointerUse,
                                             Value *Replacement) {
  AddHAKCPointerReplacement(PointerUse, Replacement, false);
}

bool HAKCPointerManager::FunctionIsCompartmentalized() const {
  return IsCompartmentalized;
}

void HAKCPointerManager::SetFunctionIsCompartmentalized(const bool FunctionIsCompartmentalized) {
  IsCompartmentalized = FunctionIsCompartmentalized;
}

void HAKCPointerManager::PrintManagedValues(const std::map<ManagedHAKCPointerUseP, Value *> &Storage) {
  for (const auto &[fst, snd] : Storage) {
    CommonHAKCAnalysis::getLogger(Verbose) << *fst << " -> ";
    if (snd) {
      CommonHAKCAnalysis::getLogger(Verbose) << snd;
    } else {
      CommonHAKCAnalysis::getLogger(Verbose) << "nullptr";
    }
    CommonHAKCAnalysis::getLogger(Verbose) << "\n\n";
  }
}

void HAKCPointerManager::PrintProtectedValues() const {
  PrintManagedValues(ProtectedValues);
}

void HAKCPointerManager::PrintAuthenticatedValues() const {
  PrintManagedValues(AuthenticatedValues);
}

unsigned HAKCPointerManager::GetDataAuthenticationsAdded() const {
  return DataAuthenticationsAdded;
}

unsigned HAKCPointerManager::GetCodeAuthenticationsAdded() const {
  return CodeAuthenticationsAdded;
}

unsigned HAKCPointerManager::GetSafePointersAdded() const {
  return SafePointersAdded;
}

unsigned HAKCPointerManager::GetClonesAdded() const { return Clones.size(); }

unsigned HAKCPointerManager::GetTotalAdditions() const {
  return GetClonesAdded() + GetSafePointersAdded() +
         GetCodeAuthenticationsAdded() + GetDataAuthenticationsAdded();
}

bool HAKCPointerManager::ValueWillBeAuthenticated(Value *V) {
  if (!FunctionIsCompartmentalized() || isa<Constant>(V)) {
    return true;
  }
  const auto ManagedPointer = GetManagedPointer(V);
  if (!ManagedPointer) {
    return false;
  }

  return ManagedPointer->BaseIsAuthenticatedPointer() || ManagedPointer->GetAuthenticatedUserCount() > 0;
}

bool HAKCPointerManager::DebugIsActive() const { return DebugActive; }

} // namespace llvm::hakc
