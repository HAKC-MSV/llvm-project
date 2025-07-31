//
// Created by de29664 on 11/14/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCPointerManager.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

namespace llvm::hakc {
HAKCPointerManager::HAKCPointerManager(HAKCFunctionAnalysis &Analysis,
                                       HAKCCompartmentalizationPolicy &Policy,
                                       bool DebugActive)
    : ManagedPointersList(), AuthenticatedValues(), ProtectedValues(), Clones(),
      HAKCAnalysis(Analysis), Policy(Policy), DataAuthenticationsAdded(0),
      CodeAuthenticationsAdded(0), SafePointersAdded(0),
      IsCompartmentalized(false), DebugActive(DebugActive), CurrentPointerID(0),
      CurrentPointerUseID(0) {}

bool HAKCPointerManager::PointerIsEligibleForManagement(Use &U) const {
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Starting Pointer Management checks for " << U.get() << " from "
      << U.getUser() << "\n";

  /* The HAKCPointerManager::GetDef method performs some analysis to find a
   * definition that could be different from the "true" definition. Use the true
   * definition to check if we are managing constant strings.
   */
  auto *Pointer = U.get();
  auto *Definition = GetFunctionAnalysis().getDef(Pointer, false);
  auto *PointerTy = Pointer->getType();
  if (auto *AllocaI = dyn_cast<AllocaInst>(Definition)) {
    PointerTy = AllocaI->getAllocatedType();
  }
  if (isa<ConstantPointerNull>(Definition)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Pointer Manager ignores null pointers\n";
    return false;
  } else if (isa<ConstantInt>(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Pointer Manager ignores Constant Ints\n";
    return false;
  } else if (!CommonHAKCAnalysis::IsPointerLikeType(PointerTy)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Pointer Manager ignores non-pointers\n";
    return false;
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << *Pointer << " Type " << PointerTy << " is a pointer like type\n";

  if (auto *GV = dyn_cast<GlobalVariable>(Definition)) {
    if (CommonHAKCAnalysis::IsStringType(GV->getValueType())) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Pointer Manager is ignoring constant string " << Definition
          << "\n";
      return false;
    }
  }

  if (auto *call = dyn_cast<CallInst>(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Value " << *Pointer << " is a CallInst\n";

    bool IsInline = call->isInlineAsm();
    if (IsInline) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Call is Inline Assembly\n";
      /* These are usually the result of reading a register value */
      return GetFunctionAnalysis()
          .GetModuleAnalysis()
          .GetCommonAnalysis()
          .ValueIsUsedAsPointer(call);
    } else if (call->getCalledFunction() &&
               call->getCalledFunction()->isIntrinsic() &&
               call->getCalledFunction()->getIntrinsicID() ==
                   Intrinsic::IndependentIntrinsics::read_register) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Call is a read register intrinsic\n";
      return false;
    } else if (call->getType()->isIntegerTy(32)) {
      /* Sometimes functions that return i32 are cast to a pointer for a check
       * against IS_ERR(). No need to check this.
       * See find_mm_struct in mm/migrate.c.
       */
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Call returns 32-bit integer\n";
      return false;
    }
  } else if (auto *ConstExpr = dyn_cast<ConstantExpr>(Pointer)) {
    if (ConstExpr->isCast()) {
      auto *Operand =
          GetFunctionAnalysis().getDef(ConstExpr->getOperand(0), false);
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *ConstExpr << " operand def is " << *Operand << "\n";
      if (isa<ConstantInt>(Operand)) {
        HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
            << "ConstExpr is from ConstantInt\n";
        return false;
      }
    }
  } else if (isa<Constant>(Pointer) && Pointer->getType()->isIntegerTy()) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is a constant int\n";
    return false;
  } else if (!CommonHAKCAnalysis::IsPointerLikeType(Pointer->getType()) &&
             !Pointer->getType()->isArrayTy() && !isa<PtrToIntInst>(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is not a pointer, array, or pointer to int cast\n";
    return false;
  } else if (isa<ConstantPointerNull>(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is a constant null pointer\n";
    return false;
  } else if (GetFunctionAnalysis().IsPHIOfGlobalsOnly(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is a PHINode of Globals\n";
    return false;
  } else if (CommonHAKCAnalysis::IsKernelUserPointer(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is a Kernel pointer from user space\n";
    return false;
  } else if (auto *LoadI = dyn_cast<LoadInst>(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is used in a LoadInst\n";
    return !HAKCAnalysis.GetModuleAnalysis()
                .GetCommonAnalysis()
                .IsIgnoredGlobal(LoadI->getPointerOperand()) &&
           PointerIsEligibleForManagement(
               LoadI->getOperandUse(LoadInst::getPointerOperandIndex()));
  } else if (auto *StoreI = dyn_cast<StoreInst>(U.getUser())) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is used in a StoreInst\n";
    for (auto &Op : StoreI->operands()) {
      if (HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().IsIgnoredGlobal(
              Op.get())) {
        return false;
      }
    }
    return U.getOperandNo() == StoreInst::getPointerOperandIndex() &&
           !CommonHAKCAnalysis::IsKernelUserPointer(Pointer);
  } else if (auto *AllocaI = dyn_cast<AllocaInst>(Pointer)) {
    for (auto &Use : AllocaI->uses()) {
      if (isa<StoreInst>(Use.getUser())) {
        for (auto &Op : Use.getUser()->operands()) {
          if (HAKCAnalysis.GetModuleAnalysis()
                  .GetCommonAnalysis()
                  .IsIgnoredGlobal(Op.get())) {
            return false;
          }
        }
      }
    }
  } else if (isa<UndefValue>(Pointer)) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " is an undef value\n";
    return false;
  } else if (auto *CallI = dyn_cast<CallInst>(U.getUser())) {
    if (CallI->isInlineAsm()) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *Pointer << " is used in inline assembly\n";
      return GetFunctionAnalysis()
          .GetModuleAnalysis()
          .GetCommonAnalysis()
          .ValueIsUsedAsPointer(U.get());
    }
  } else if (!Pointer->getType()->isPointerTy()) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " Type is not a pointer: " << *Pointer->getType()
        << "\n";
    return false;
  } else if (Pointer->getType()->isPointerTy()) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << *Pointer << " Type is a pointer: " << *Pointer->getType() << "\n";
    return !CommonHAKCAnalysis::IsKernelUserPointer(Pointer);
  }
  return Pointer->getType()->isPointerTy() &&
         !CommonHAKCAnalysis::IsKernelUserPointer(Pointer);
}

bool HAKCPointerManager::ManageNewPointer(Use &U) {
  auto *BaseDefinition = GetDef(U.get());
  if (!BaseDefinition) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find BaseDefinition for " << U << "\n";
    throw std::exception();
  }
  if (isa<IntToPtrInst>(U.get())) {
    bool is_percpu_ptr = CommonHAKCAnalysis::IsPerCPUPointer(U);

    if (is_percpu_ptr) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Detected per-cpu pointer: " << U << "\n";
      BaseDefinition = U.get();
    }
  }

  auto NextID = CurrentPointerID + 1;

  auto ManagedPointer =
      std::make_shared<ManagedHAKCPointer>(BaseDefinition, *this, NextID);
  if (NextID == 4 &&
      HAKCAnalysis.GetFunction().getName() == "pci_irq_mask_msix") {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Found " << *ManagedPointer << "\n";
  }
  HAKCAnalysis.GetModuleAnalysis().GetTypeIdentifier().FindType(
      *ManagedPointer);
  if (ManagedPointer->GetType() && ManagedPointer->GetType()->IsIgnoredType()) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Ignoring pointer " << ManagedPointer
        << " because its HAKCType is ignored\n";
    return false;
  }
  if (ManagedPointer->GetType() && ManagedPointer->GetType()->IsIntegerType() &&
      !ManagedPointer->GetType()->GetPointeeType()) {
    auto PointeeTy = HAKCAnalysis.GetModuleAnalysis()
                         .GetTypeIdentifier()
                         .GetVoidPointerPointeeType();
    ManagedPointer->GetType()->SetPointeeType(PointeeTy);
  }
  if (ManagedPointer->GetType() &&
      !ManagedPointer->GetType()->IsPointerType()) {
    CommonHAKCAnalysis::getLogger(Verbose, !DebugActive)
        << *ManagedPointer << " is not a pointer type\n";
  }
  CurrentPointerID++;
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Starting the management of pointer " << std::to_string(NextID)
      << " with BaseDefinition " << BaseDefinition << "\n";
  ManagedPointersList.push_back(ManagedPointer);
  AnalyzedUses.clear();
  ClassifyAllUsesOfDefinition(ManagedPointer->GetBaseDefinition(),
                              *ManagedPointer);
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Managing " << ManagedPointer;
  if (ManagedPointer->GetType()) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << " with HAKCType " << *ManagedPointer->GetType();
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "\n";
  return true;
}

bool HAKCPointerManager::UseIsAnalyzed(ManagedHAKCPointerUse &MangedPtrUse) {
  auto Search = [MangedPtrUse](const ManagedHAKCPointerUseP &UPtr) {
    return *UPtr == MangedPtrUse;
  };

  return llvm::any_of(AnalyzedUses, Search);
}

bool HAKCPointerManager::IsConstantExprUsedInKernelCall(User *U) const {
  bool Result = false;
  if (isa<ConstantExpr>(U)) {
    for (auto *ConstUser : U->users()) {
      if (auto *Call = dyn_cast<CallBase>(ConstUser)) {
        if (Call->getFunction() == &GetFunctionAnalysis().GetFunction() &&
            CommonHAKCAnalysis::IsUncompartmentalizedSymbol(
                Call->getCalledFunction(), Policy)) {
          Result = true;
          break;
        }
      }
    }
  }

  return Result;
}

bool HAKCPointerManager::UseShouldBeIgnored(Use &U) {
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
      if (I->getFunction() != &GetFunctionAnalysis().GetFunction()) {
        UseShouldBeIgnored = true;
      }
    } else {
      UseShouldBeIgnored = !isa<Argument>(Def);
    }
  }

  if (!UseShouldBeIgnored) {
    if (auto *I = dyn_cast<Instruction>(UserP)) {
      UseShouldBeIgnored =
          (I->getFunction() != &GetFunctionAnalysis().GetFunction());
    } else if (auto *A = dyn_cast<Argument>(UserP)) {
      UseShouldBeIgnored =
          (A->getParent() != &GetFunctionAnalysis().GetFunction());
    }
  }

  return UseShouldBeIgnored;
}

bool HAKCPointerManager::UseShouldBeCloned(Use &U) {
  auto *UserP = U.getUser();
  bool CloneUse = isa<BitCastInst>(UserP) || isa<PtrToIntInst>(UserP) ||
                  isa<SelectInst>(UserP) || isa<SExtInst>(UserP) ||
                  isa<IntToPtrInst>(UserP) || isa<PHINode>(UserP) ||
                  isa<FreezeInst>(UserP) || isa<BinaryOperator>(UserP) ||
                  isa<TruncInst>(UserP) || isa<ZExtInst>(UserP);

  if (isa<SubOperator>(UserP)) {
    CloneUse = false;
  } else if (isa<GetElementPtrInst>(UserP)) {
    if (U.getOperandNo() == GetElementPtrInst::getPointerOperandIndex()) {
      CloneUse = true;
    }
  }

  return CloneUse;
}

bool HAKCPointerManager::UseShouldUtilizeAuthenticatedPointer(Use &U) const {
  auto *UserP = U.getUser();
  bool UseAuthenticatedPointer =
      isa<CmpInst>(UserP) || isa<LoadInst>(UserP) || isa<SubOperator>(UserP);
  if (auto *Call = dyn_cast<CallBase>(UserP)) {
    if (GetFunctionAnalysis()
            .GetModuleAnalysis()
            .GetCommonAnalysis()
            .IsHAKCTransferFunction(Call->getCalledFunction())) {
      UseAuthenticatedPointer = false;
    } else if (GetFunctionAnalysis()
                   .GetModuleAnalysis()
                   .GetCommonAnalysis()
                   .IsSafeTransitionCall(Call) ||
               Call->isInlineAsm() ||
               Call->getCalledOperandUse().getOperandNo() == U.getOperandNo() ||
               GetFunctionAnalysis().IsIntrinsicNeedingCloning(Call) ||
               GetFunctionAnalysis().IsIntrinsicNeedingAuthentication(Call)) {
      UseAuthenticatedPointer = true;
    }
  } else if (isa<StoreInst>(UserP)) {
    if (U.getOperandNo() == StoreInst::getPointerOperandIndex()) {
      UseAuthenticatedPointer = true;
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
  } else if (auto *ConstExpr = dyn_cast<ConstantExpr>(UserP)) {
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

bool HAKCPointerManager::UseShouldUtilizeSignedBasePointer(Use &U) const {
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
  } else if (auto *Call = dyn_cast<CallInst>(UserP)) {
    if (!GetFunctionAnalysis()
             .GetModuleAnalysis()
             .GetCommonAnalysis()
             .IsSafeTransitionCall(Call) ||
        Call->getCalledFunction() != nullptr ||
        GetFunctionAnalysis()
            .GetModuleAnalysis()
            .GetCommonAnalysis()
            .IsHAKCTransferFunction(Call->getCalledFunction()) ||
        CommonHAKCAnalysis::IsUncompartmentalizedSymbol(
            Call->getCalledFunction(), Policy)) {
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

bool HAKCPointerManager::IsClonedUseNeedingAdditionalClassification(Use &U) {
  bool NeedsAdditionalClassification = !isa<PHINode>(U.getUser());
  auto ManagedPointer = GetManagedPointer(U.getUser());
  if (ManagedPointer && U.getUser() == ManagedPointer->GetBaseDefinition()) {
    NeedsAdditionalClassification = false;
  }

  return NeedsAdditionalClassification;
}

ManagedHAKCPointerUseP
HAKCPointerManager::CreateManagedPointerUse(ManagedHAKCPointer &ManagedPointer,
                                            User *U, unsigned int OperandNo) {
  return std::make_shared<ManagedHAKCPointerUse>(ManagedPointer, U, OperandNo,
                                                 CurrentPointerUseID++);
}

void HAKCPointerManager::ClassifyAllUsesOfDefinition(
    Value *Definition, ManagedHAKCPointer &ManagedPointer) {
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Classifying " << std::to_string(Definition->getNumUses())
      << " uses of " << Definition << "\n";
  for (auto &U : Definition->uses()) {
    auto *User = U.getUser();
    if (auto *I = dyn_cast<Instruction>(User)) {
      if (I->getFunction() != &GetFunctionAnalysis().GetFunction()) {
        continue;
      }
    }

    auto UPtr = CreateManagedPointerUse(ManagedPointer, User, U.getOperandNo());
    if (UseIsAnalyzed(*UPtr)) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *UPtr << " is already analyzed\n";
      continue;
    }
    AnalyzedUses.push_back(UPtr);
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Classifying " << *UPtr << "\n";
    if (UseShouldBeIgnored(U)) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *UPtr << " is being ignored\n";
      continue;
    }
    if (UseShouldBeCloned(U)) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *User << " should be cloned\n";
      if (IsClonedUseNeedingAdditionalClassification(U)) {
        ClassifyAllUsesOfDefinition(User, ManagedPointer);
      }
      ManagedPointer.AddCloneUse(UPtr);
    } else if (UseShouldUtilizeAuthenticatedPointer(U)) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *UPtr << " should use authenticated Base Definition\n";
      ManagedPointer.AddAuthenticatedUse(UPtr);
    } else if (UseShouldUtilizeSignedBasePointer(U)) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << *UPtr << " should use signed Base Definition\n";
      if (auto *Call = dyn_cast<CallBase>(User)) {
        if (GetFunctionAnalysis()
                .GetModuleAnalysis()
                .GetCommonAnalysis()
                .IsHAKCTransferFunction(Call->getCalledFunction())) {
          ManagedPointer.RegisterManualHAKCTransfer(Call);
          HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(
              Verbose)
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
      if (!isa<Argument>(UPtr->getUser()) &&
          !isa<Instruction>(UPtr->getUser())) {
        CommonHAKCAnalysis::getLogger(Error)
            << "here0 " << GetFunctionAnalysis().GetFunction().getParent();
      } else {
        CommonHAKCAnalysis::getLogger(Error)
            << "here1 " << GetFunctionAnalysis().GetFunction();
      }
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
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Use " << U << " is not eligible for management\n";
  } else {
    auto ManagedPointer = GetManagedPointer(U.get());
    if (!ManagedPointer) {
      Result = ManageNewPointer(U);
    } else {
      Result = false;
    }
  }
  return Result;
}

HAKCFunctionAnalysis &HAKCPointerManager::GetFunctionAnalysis() const {
  return HAKCAnalysis;
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
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Finding Managed Pointer for " << V << "\n";
  auto *Def = GetDef(V);
  for (auto &ManagedPointer : ManagedPointers()) {
    if (*ManagedPointer == Def) {
      return ManagedPointer;
    }
  }

  return nullptr;
}

bool HAKCPointerManager::empty() const { return ManagedPointersList.empty(); }

Value *HAKCPointerManager::GetDef(Value *V) const {
  auto *BaseDefinition = GetFunctionAnalysis().getDef(V, false);

  if (isa<GlobalVariable>(BaseDefinition) &&
      !CommonHAKCAnalysis::IsStringType(BaseDefinition->getType())) {
    Value *NewBaseDefinition = nullptr;
    SmallVector<Value *> DefChain;
    GetFunctionAnalysis().GetModuleAnalysis().GetCommonAnalysis().findDefChain(
        V, false, DefChain);
    for (auto *Link : DefChain) {
      if (isa<CallInst>(Link)) {
        NewBaseDefinition = Link;
        break;
      }
    }

    if (NewBaseDefinition) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Changing BaseDefinition from " << *BaseDefinition << " to "
          << *NewBaseDefinition << "\n";
      BaseDefinition = NewBaseDefinition;
    }
  }

  return BaseDefinition;
}

Instruction *HAKCPointerManager::CloneInstruction(Instruction *I) {
  Instruction *Clone;
  if (Clones.find(I) == Clones.end()) {
    Clone = I->clone();
    Clone->insertBefore(I);
    Clones[I] = Clone;
  } else {
    Clone = Clones[I];
  }
  return Clone;
}

Value *
HAKCPointerManager::CreateProtectedValue(ManagedHAKCPointerUse &PointerUse) {
  auto *Pointer = PointerUse.get();

  auto *ProtectedValue = FindProtectedValue(PointerUse);
  if (ProtectedValue) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Returning Protected Version " << *ProtectedValue << " for "
        << PointerUse << "\n";
    return ProtectedValue;
  }
  auto ManagedPtr = GetManagedPointer(Pointer);
  if (ManagedPtr && ManagedPtr->GetBaseDefinition() == Pointer) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Returning ProtectedPointer\n";
    return ManagedPtr->GetProtectedPointer();
  }

  if (auto *I = dyn_cast<Instruction>(Pointer)) {
    auto Clone = CloneInstruction(I);
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Created Protected Version of " << *I << ": " << Clone << "\n";
    return Clone;
  }
  return nullptr;
}

Value *HAKCPointerManager::CreateAuthenticatedValue(
    ManagedHAKCPointerUse &PointerUse) {
  auto *Pointer = PointerUse.get();

  auto *AuthenticatedCopy = FindAuthenticatedValue(PointerUse);
  if (AuthenticatedCopy) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Returning Authenticated Copy " << AuthenticatedCopy << " for "
        << PointerUse << "\n";
    return AuthenticatedCopy;
  }

  if (auto *I = dyn_cast<Instruction>(Pointer)) {
    auto *Clone = CloneInstruction(I);
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Created Authenticated Copy of " << *I << ": " << Clone << "\n";
    return Clone;
  }
  return nullptr;
}

void HAKCPointerManager::CreateAllTransfers() {
  SmallVector<ManagedHAKCPointerP> SortedPointers;
  for (auto &P : ManagedPointers()) {
    SortedPointers.push_back(P);
  }
  bool PointersUpdated = true;
  while (PointersUpdated) {
    PointersUpdated = false;

    for (auto &ManagedPtr : SortedPointers) {
      auto CurrentAuthUserCount = ManagedPtr->GetAuthenticatedUserCount();
      auto CurrentProtUserCount = ManagedPtr->GetProtectedUserCount();

      ManagedPtr->UpdateUserCounts();
      if (CurrentAuthUserCount != ManagedPtr->GetAuthenticatedUserCount() ||
          CurrentProtUserCount != ManagedPtr->GetProtectedUserCount()) {
        HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
            << *ManagedPtr << " changed user count\n";
        PointersUpdated = true;
      }
    }
  }
  PointersUpdated = true;
  while (PointersUpdated) {
    PointersUpdated = false;

    for (auto &ManagedPtr : SortedPointers) {
      auto OrigBaseIsAuthenticated = ManagedPtr->BaseIsAuthenticatedPointer();
      auto BaseAuthenticatedResult =
          ManagedPtr->DetermineIfBasePointerIsAuthenticated();
      if (OrigBaseIsAuthenticated != BaseAuthenticatedResult) {
        HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
            << *ManagedPtr << " changed base authentication flag from "
            << std::to_string(OrigBaseIsAuthenticated) << " to "
            << std::to_string(BaseAuthenticatedResult) << "\n";
        PointersUpdated = true;
      }
    }
  }

  /* At this point, all uses should be classified, and we should know if
   * authenticated and protected pointers need to be created */

  for (auto &HAKCPointer : SortedPointers) {
    HAKCPointer->MaybeCreateProtectedPointer();
  }
}

void HAKCPointerManager::CreateAuthenticatedPointersAndAllClones() {
  SmallVector<ManagedHAKCPointerP> SortedPointers;
  for (auto &P : ManagedPointers()) {
    SortedPointers.push_back(P);
  }

  for (auto &ManagedPtr : SortedPointers) {
    /* Guarantee that auth and protected pointers get placed correctly */
    ManagedPtr->MaybeCreateBaseCopyPointer();
  }

  for (auto &ManagedPtr : SortedPointers) {
    ManagedPtr->CreateBaseAuthenticatedPointer();
    if (ManagedPtr->GetAuthenticatedPointer()) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Authenticated Pointer for " << *ManagedPtr << ": "
          << ManagedPtr->GetAuthenticatedPointer() << "\n";
    }
  }
  for (auto &ManagedPtr : SortedPointers) {
    ManagedPtr->CreatePointerUseClones();
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Created Authenticated and Protected Copies for " << *ManagedPtr
        << "\n";
  }
}

Value *HAKCPointerManager::FindManagedPointerReplacement(
    Value *Target, bool ReturnAuthenticatedPointer) {
  Value *Result = nullptr;
  for (auto &ManagedPtr : ManagedPointers()) {
    if (ManagedPtr->GetBaseDefinition() == Target ||
        ManagedPtr->GetAuthenticatedPointer() == Target ||
        ManagedPtr->GetProtectedPointer() == Target) {
        HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
            << "Returning ";
        if (ReturnAuthenticatedPointer) {
          HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(
              Verbose, !DebugActive)
              << "authenticated";
        } else {
          HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(
              Verbose, !DebugActive)
              << "protected";
        }
        HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
            << " pointer for " << *Target << "\n";

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

Value *
HAKCPointerManager::FindAuthenticatedValue(ManagedHAKCPointerUse &PointerUse) {
  auto *AuthValue = FindManagedValue(AuthenticatedValues, PointerUse);
  if (!AuthValue) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Unable to find Authenticated Managed Value for PointerUse "
        << PointerUse << "\n";
    AuthValue = FindManagedPointerReplacement(PointerUse.get(), true);
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Found authenticated managed pointer " << AuthValue << " for "
      << PointerUse << "\n";
  return AuthValue;
}

Value *HAKCPointerManager::FindProtectedValue(
    const ManagedHAKCPointerUse &PointerUse) {
  auto *ProtValue = FindManagedValue(ProtectedValues, PointerUse);
  if (!ProtValue) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Unable to find Protected Managed Value for " << PointerUse << "\n";
    ProtValue = FindManagedPointerReplacement(PointerUse.get(), false);
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Found protected managed pointer " << ProtValue << " for "
      << PointerUse << "\n";
  return ProtValue;
}

Value *HAKCPointerManager::FindManagedValue(
    const std::map<ManagedHAKCPointerUseP, Value *> &Storage,
    const ManagedHAKCPointerUse &PointerUse) {
  for (auto &it : Storage) {
    if (PointerUse == *it.first) {
      return it.second;
    }
  }

  return nullptr;
}

Value *HAKCPointerManager::FindManagedValue(
    std::map<ManagedHAKCPointerUseP, Value *> &Storage, Value *Target) {
  for (auto &it : Storage) {
    if (it.first->get() == Target) {
      return it.second;
    }
  }

  return nullptr;
}

Value *HAKCPointerManager::FindAuthenticatedValue(Value *V) {
  auto *AuthValue = FindManagedValue(AuthenticatedValues, V);
  if (!AuthValue) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Unable to find Authenticated Managed Value for " << V << "\n";
    AuthValue = FindManagedPointerReplacement(V, true);
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Found authenticated managed pointer " << AuthValue << " for " << V
      << "\n";
  return AuthValue;
}

Value *HAKCPointerManager::FindProtectedValue(Value *V) {
  auto *ProtValue = FindManagedValue(ProtectedValues, V);
  if (!ProtValue) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Unable to find Protected Managed Value for " << V << "\n";
    ProtValue = FindManagedPointerReplacement(V, false);
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Found protected managed pointer " << ProtValue << " for " << V
      << "\n";
  return ProtValue;
}

void HAKCPointerManager::AddHAKCPointerReplacement(
    ManagedHAKCPointerUseP &PtrUse, Value *Replacement,
    bool AddingAuthenticatedReplacements) {
  StringRef StorageName =
      AddingAuthenticatedReplacements ? "Authenticated" : "Protected";
  std::map<ManagedHAKCPointerUseP, Value *> &StorageToUse =
      (AddingAuthenticatedReplacements ? AuthenticatedValues : ProtectedValues);
  std::map<ManagedHAKCPointerUseP, Value *> &OtherStorage =
      (AddingAuthenticatedReplacements ? ProtectedValues : AuthenticatedValues);

  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Adding " << StorageName << " Pointer Replacement: " << *PtrUse
      << " -> " << Replacement << "\n";

  if (!PtrUse->getManagedPtr().PointerSetsCanBeEqual()) {
    auto *OtherStorageReplacement = FindManagedValue(OtherStorage, *PtrUse);
    if (OtherStorageReplacement) {
      if (OtherStorageReplacement == Replacement) {
        StringRef OtherStorageName =
            AddingAuthenticatedReplacements ? "Protected" : "Authenticated";
        CommonHAKCAnalysis::getLogger(Fatal)
            << StorageName << " replacement " << Replacement << " for "
            << *PtrUse << " matches " << OtherStorageName
            << " replacement in function\n"
            << GetFunctionAnalysis().GetFunction() << "\n";
        throw std::exception();
      }
    }
  }

  auto *ExistingPointer = FindManagedValue(StorageToUse, *PtrUse);
  if (!ExistingPointer) {
    HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
        << "Adding New " << StorageName << " Pointer Replacement\n";
    StorageToUse[PtrUse] = Replacement;
  } else {
    if (Replacement && ExistingPointer != Replacement) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Trying to replace existing " << StorageName << " Replacement "
          << ExistingPointer << " with " << Replacement << " for " << *PtrUse
          << "\n"
          << GetFunctionAnalysis().GetFunction() << "\n";
      throw std::exception();
    }
    if (Replacement) {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Setting Existing " << StorageName << " Pointer Replacement\n";
      StorageToUse[PtrUse] = Replacement;
    } else {
      HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
          << "Tried to add null to " << StorageName
          << "Pointer Replacement for " << *PtrUse << "\n";
    }
  }
}

void HAKCPointerManager::AddAuthenticatedPointer(
    ManagedHAKCPointerUseP &PointerUse, Value *Replacement) {
  AddHAKCPointerReplacement(PointerUse, Replacement, true);
}

void HAKCPointerManager::AddProtectedPointer(ManagedHAKCPointerUseP &PointerUse,
                                             Value *Replacement) {
  AddHAKCPointerReplacement(PointerUse, Replacement, false);
}

bool HAKCPointerManager::FunctionIsCompartmentalized() const {
  return IsCompartmentalized;
}

void HAKCPointerManager::SetFunctionIsCompartmentalized(
    bool FunctionIsCompartmentalized) {
  IsCompartmentalized = FunctionIsCompartmentalized;
}

void HAKCPointerManager::PrintManagedValues(
    const std::map<ManagedHAKCPointerUseP, Value *> &Storage) {
  for (auto &it : Storage) {
    CommonHAKCAnalysis::getLogger(Verbose) << *it.first << " -> ";
    if (it.second) {
      CommonHAKCAnalysis::getLogger(Verbose) << it.second;
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

void HAKCPointerManager::TransformPointers() {
  for (auto &ManagedPointer : ManagedPointers()) {
    ManagedPointer->TransformUses();
  }
}

bool HAKCPointerManager::ValueWillBeAuthenticated(Value *V) {
  if (!FunctionIsCompartmentalized() || isa<Constant>(V)) {
    return true;
  }
  auto ManagedPointer = GetManagedPointer(V);
  if (!ManagedPointer) {
    return false;
  }

  return ManagedPointer->BaseIsAuthenticatedPointer() ||
         ManagedPointer->GetAuthenticatedUserCount() > 0;
}

Value *
HAKCPointerManager::CreateSafePointerAtLocation(Value *Pointer,
                                                Instruction *InsertLocation) {
  auto *Managed = FindAuthenticatedValue(Pointer);
  if (Managed) {
    return Managed;
  }

  SafePointersAdded++;
  return GetFunctionAnalysis().AddSafePointerCreationAtLocation(Pointer,
                                                                InsertLocation);
}

Value *HAKCPointerManager::CreateAuthenticationAtLocation(
    Value *Pointer, Instruction *InsertLocation) {
  auto *Managed = FindAuthenticatedValue(Pointer);
  if (Managed) {
    return Managed;
  }
  auto ManagedPointer = GetManagedPointer(Pointer);
  if (!ManagedPointer) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not find Managed Pointer for " << *Pointer << "\n";
    throw std::exception();
  } else if (!ManagedPointer->GetType()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Managed Pointer " << *ManagedPointer << " found for Value "
        << *Pointer << " does not have a HAKCType\n";
    throw std::exception();
  }
  HAKCAnalysis.GetModuleAnalysis().GetCommonAnalysis().getLogger(Verbose, !DebugActive)
      << "Adding Authenticated Pointer for " << *ManagedPointer
      << " with HAKCType " << *ManagedPointer->GetType() << "\n"
      << " at " << *InsertLocation << "\n";
  if (CommonHAKCAnalysis::PointerShouldBeConsideredCode(*ManagedPointer)) {
    CodeAuthenticationsAdded++;
    return GetFunctionAnalysis().AddCodeAuthCheckAtLocation(Pointer,
                                                            InsertLocation);
  } else {
    DataAuthenticationsAdded++;
    return GetFunctionAnalysis().AddDataAuthCheckAtLocation(Pointer,
                                                            InsertLocation);
  }
}

HAKCCompartmentalizationPolicy &HAKCPointerManager::GetPolicy() const {
  return Policy;
}

bool HAKCPointerManager::DebugIsActive() const { return DebugActive; }
} // namespace llvm::hakc
