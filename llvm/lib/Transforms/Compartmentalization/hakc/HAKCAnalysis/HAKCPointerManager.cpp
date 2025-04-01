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

bool HAKCPointerManager::PointerIsEligibleForManagement(Value *Pointer) {
  /* The HAKCPointerManager::GetDef method performs some analysis to find a
   * definition that could be different from the "true" definition. Use the true
   * definition to check if we are managing constant strings.
   */
  auto *Definition = GetFunctionAnalysis().getDef(Pointer, false);
  if (isa<ConstantPointerNull>(Definition)) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Pointer Manager ignores null pointers\n";
    return false;
  } else if (isa<ConstantInt>(Pointer)) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Pointer Manager ignores Constant Ints\n";
    return false;
  } else if (!CommonHAKCAnalysis::IsPointerLikeType(Pointer->getType())) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Pointer Manager ignores non-pointers\n";
    return false;
  }

  if (auto *GV = dyn_cast<GlobalVariable>(Definition)) {
    if (CommonHAKCAnalysis::IsStringType(GV->getValueType())) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Pointer Manager is ignoring constant string " << Definition
          << "\n";
      return false;
    }
  }

  return true;
}

void HAKCPointerManager::ManageNewPointer(Value *V) {
  auto *BaseDefinition = GetDef(V);
  if (!BaseDefinition) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find BaseDefinition for " << V << "\n";
    throw std::exception();
  }

  auto NextID = CurrentPointerID++;
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Starting the management of pointer " << std::to_string(NextID)
      << " with BaseDefinition " << BaseDefinition << "\n";

  auto ManagedPointer =
      std::make_shared<ManagedHAKCPointer>(BaseDefinition, *this, NextID);
  HAKCAnalysis.GetModuleAnalysis().GetTypeIdentifier().FindType(
      *ManagedPointer);
  ManagedPointersList.push_back(ManagedPointer);
  AnalyzedUses.clear();
  ClassifyAllUsesOfDefinition(ManagedPointer->GetBaseDefinition(),
                              *ManagedPointer);
  CommonHAKCAnalysis::getWriter(DebugActive) << "Managing " << ManagedPointer;
  if (ManagedPointer->GetType()) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << " with HAKCType " << *ManagedPointer->GetType();
  }
  CommonHAKCAnalysis::getWriter(DebugActive) << "\n";
}

bool HAKCPointerManager::UseIsAnalyzed(ManagedHAKCPointerUse &MangedPtrUse) {
  auto Search = [MangedPtrUse](const ManagedHAKCPointerUseP &UPtr) {
    return *UPtr == MangedPtrUse;
  };

  return llvm::any_of(AnalyzedUses, Search);
}

bool HAKCPointerManager::IsConstantExprUsedInKernelCall(User *U) {
  bool Result = false;
  if (isa<ConstantExpr>(U)) {
    for (auto *ConstUser : U->users()) {
      if (auto *Call = dyn_cast<CallBase>(ConstUser)) {
        if (Call->getFunction() == &GetFunctionAnalysis().getFunction() &&
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
      if (I->getFunction() != &GetFunctionAnalysis().getFunction()) {
        UseShouldBeIgnored = true;
      }
    } else {
      UseShouldBeIgnored = !isa<Argument>(Def);
    }
  }

  if (!UseShouldBeIgnored) {
    if (auto *I = dyn_cast<Instruction>(UserP)) {
      UseShouldBeIgnored =
          (I->getFunction() != &GetFunctionAnalysis().getFunction());
    } else if (auto *A = dyn_cast<Argument>(UserP)) {
      UseShouldBeIgnored =
          (A->getParent() != &GetFunctionAnalysis().getFunction());
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
                  isa<TruncInst>(UserP);

  if (isa<SubOperator>(UserP)) {
    CloneUse = false;
  } else if (isa<GetElementPtrInst>(UserP)) {
    if (U.getOperandNo() == GetElementPtrInst::getPointerOperandIndex()) {
      CloneUse = true;
    }
  }

  return CloneUse;
}

bool HAKCPointerManager::UseShouldUtilizeAuthenticatedPointer(Use &U) {
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
               Call->getCalledFunction() == nullptr ||
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

bool HAKCPointerManager::UseShouldUtilizeSignedBasePointer(Use &U) {
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
      CommonHAKCAnalysis::getWriter(DebugActive) << "Signed 2\n";
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
  if (U.getUser() == ManagedPointer->GetBaseDefinition()) {
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
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Classifying " << std::to_string(Definition->getNumUses())
      << " uses of " << Definition << "\n";
  for (auto &U : Definition->uses()) {
    auto *User = U.getUser();
    auto UPtr = CreateManagedPointerUse(ManagedPointer, User, U.getOperandNo());
    if (UseIsAnalyzed(*UPtr)) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *UPtr << " is already analyzed\n";
      continue;
    }
    AnalyzedUses.push_back(UPtr);
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Classifying " << *UPtr << "\n";
    if (UseShouldBeIgnored(U)) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *UPtr << " is being ignored\n";
      continue;
    }
    if (UseShouldBeCloned(U)) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *User << " should be cloned\n";
      if (IsClonedUseNeedingAdditionalClassification(U)) {
        ClassifyAllUsesOfDefinition(User, ManagedPointer);
      }
      ManagedPointer.AddCloneUse(UPtr);
    } else if (UseShouldUtilizeAuthenticatedPointer(U)) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *UPtr << " should use authenticated Base Definition\n";
      ManagedPointer.AddAuthenticatedUse(UPtr);
    } else if (UseShouldUtilizeSignedBasePointer(U)) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *UPtr << " should use signed Base Definition\n";
      if (auto *Call = dyn_cast<CallBase>(User)) {
        if (GetFunctionAnalysis()
                .GetModuleAnalysis()
                .GetCommonAnalysis()
                .IsHAKCTransferFunction(Call->getCalledFunction())) {
          ManagedPointer.RegisterManualHAKCTransfer(Call);
          CommonHAKCAnalysis::getWriter(DebugActive)
              << "Registered " << *Call << " as the protected pointer of "
              << ManagedPointer << ".  Classifying uses...\n";
          ClassifyAllUsesOfDefinition(Call, ManagedPointer);
          continue;
        }
      }
      ManagedPointer.AddProtectedUse(UPtr);
    } else {
      CommonHAKCAnalysis::getWriter(true)
          << "Unexpected use of " << *UPtr << " --- " << UPtr->get()
          << " --- with " << ManagedPointer << " in \n";
      if (!isa<Argument>(UPtr->getUser()) &&
          !isa<Instruction>(UPtr->getUser())) {
        CommonHAKCAnalysis::getWriter(true)
            << "here0 " << GetFunctionAnalysis().getFunction().getParent();
      } else {
        CommonHAKCAnalysis::getWriter(true)
            << "here1 " << GetFunctionAnalysis().getFunction();
      }
      CommonHAKCAnalysis::getWriter(true) << "\n";
      throw std::exception();
    }
  }
}

bool HAKCPointerManager::ManagePointer(Value *V) {
  bool result = false;
  if (!PointerIsEligibleForManagement(V)) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Value " << *V << " is not eligible for management\n";
    return result;
  }
  auto ManagedPointer = GetManagedPointer(V);
  if (!ManagedPointer) {
    ManageNewPointer(V);
    result = true;
  } else {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Pointer " << *V << " is already managed: " << *ManagedPointer
        << "\n";
  }
  return result;
}

HAKCFunctionAnalysis &HAKCPointerManager::GetFunctionAnalysis() {
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
  auto *Def = GetDef(V);
  for (auto &ManagedPointer : ManagedPointers()) {
    if (*ManagedPointer == Def) {
      return ManagedPointer;
    }
  }

  return nullptr;
}

bool HAKCPointerManager::empty() const { return ManagedPointersList.empty(); }

Value *HAKCPointerManager::GetDef(Value *V) {
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
      CommonHAKCAnalysis::getWriter(DebugActive)
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
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Returning Protected Version " << *ProtectedValue << " for "
        << PointerUse << "\n";
    return ProtectedValue;
  }
  auto ManagedPtr = GetManagedPointer(Pointer);
  if (ManagedPtr && ManagedPtr->GetBaseDefinition() == Pointer) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Returning ProtectedPointer\n";
    return ManagedPtr->GetProtectedPointer();
  }

  if (auto *I = dyn_cast<Instruction>(Pointer)) {
    auto Clone = CloneInstruction(I);
    CommonHAKCAnalysis::getWriter(DebugActive)
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
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Returning Authenticated Copy " << AuthenticatedCopy << " for "
        << PointerUse << "\n";
    return AuthenticatedCopy;
  }

  if (auto *I = dyn_cast<Instruction>(Pointer)) {
    auto *Clone = CloneInstruction(I);
    CommonHAKCAnalysis::getWriter(DebugActive)
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
        CommonHAKCAnalysis::getWriter(DebugActive)
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
        CommonHAKCAnalysis::getWriter(DebugActive)
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
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Authenticated Pointer for " << *ManagedPtr << ": "
          << ManagedPtr->GetAuthenticatedPointer() << "\n";
    }
  }
  for (auto &ManagedPtr : SortedPointers) {
    ManagedPtr->CreatePointerUseClones();
    CommonHAKCAnalysis::getWriter(DebugActive)
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
      if (DebugActive) {
        CommonHAKCAnalysis::getWriter(DebugActive) << "Returning ";
        if (ReturnAuthenticatedPointer) {
          CommonHAKCAnalysis::getWriter(DebugActive) << "authenticated";
        } else {
          CommonHAKCAnalysis::getWriter(DebugActive) << "protected";
        }
        CommonHAKCAnalysis::getWriter(DebugActive)
            << " pointer for " << *Target << "\n";
      }
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
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Unable to find Authenticated Managed Value for PointerUse "
        << PointerUse << "\n";
    AuthValue = FindManagedPointerReplacement(PointerUse.get(), true);
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Found authenticated managed pointer " << AuthValue << " for "
      << PointerUse << "\n";
  return AuthValue;
}

Value *HAKCPointerManager::FindProtectedValue(
    const ManagedHAKCPointerUse &PointerUse) {
  auto *ProtValue = FindManagedValue(ProtectedValues, PointerUse);
  if (!ProtValue) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Unable to find Protected Managed Value for " << PointerUse << "\n";
    ProtValue = FindManagedPointerReplacement(PointerUse.get(), false);
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
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
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Unable to find Authenticated Managed Value for " << V << "\n";
    AuthValue = FindManagedPointerReplacement(V, true);
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Found authenticated managed pointer " << AuthValue << " for " << V
      << "\n";
  return AuthValue;
}

Value *HAKCPointerManager::FindProtectedValue(Value *V) {
  auto *ProtValue = FindManagedValue(ProtectedValues, V);
  if (!ProtValue) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Unable to find Protected Managed Value for " << V << "\n";
    ProtValue = FindManagedPointerReplacement(V, false);
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
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

  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Adding " << StorageName << " Pointer Replacement: " << *PtrUse
      << " -> " << Replacement << "\n";

  if (!PtrUse->getManagedPtr().PointerSetsCanBeEqual()) {
    auto *OtherStorageReplacement = FindManagedValue(OtherStorage, *PtrUse);
    if (OtherStorageReplacement) {
      if (OtherStorageReplacement == Replacement) {
        StringRef OtherStorageName =
            AddingAuthenticatedReplacements ? "Protected" : "Authenticated";
        CommonHAKCAnalysis::getWriter(true)
            << StorageName << " replacement " << Replacement << " for "
            << *PtrUse << " matches " << OtherStorageName
            << " replacement in function\n"
            << GetFunctionAnalysis().getFunction() << "\n";
        throw std::exception();
      }
    }
  }

  auto *ExistingPointer = FindManagedValue(StorageToUse, *PtrUse);
  if (!ExistingPointer) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Adding New " << StorageName << " Pointer Replacement\n";
    StorageToUse[PtrUse] = Replacement;
  } else {
    if (Replacement && ExistingPointer != Replacement) {
      CommonHAKCAnalysis::getWriter(true)
          << "Trying to replace existing " << StorageName << " Replacement "
          << ExistingPointer << " with " << Replacement << " for " << *PtrUse
          << "\n"
          << GetFunctionAnalysis().getFunction() << "\n";
      throw std::exception();
    }
    if (Replacement) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Setting Existing " << StorageName << " Pointer Replacement\n";
      StorageToUse[PtrUse] = Replacement;
    } else {
      CommonHAKCAnalysis::getWriter(DebugActive)
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
    CommonHAKCAnalysis::getWriter(true) << *it.first << " -> ";
    if (it.second) {
      CommonHAKCAnalysis::getWriter(true) << it.second;
    } else {
      CommonHAKCAnalysis::getWriter(true) << "nullptr";
    }
    CommonHAKCAnalysis::getWriter(true) << "\n\n";
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
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find Managed Pointer for " << *Pointer << "\n";
    throw std::exception();
  } else if (!ManagedPointer->GetType()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Managed Pointer " << *ManagedPointer << " found for Value "
        << *Pointer << " does not have a HAKCType\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Adding Authenticated Pointer for " << *ManagedPointer
      << " with HAKCType " << *ManagedPointer->GetType() << "\n"
      << " at " << *InsertLocation << "\n";
  // TODO: insert tictac check here (maybe)
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
