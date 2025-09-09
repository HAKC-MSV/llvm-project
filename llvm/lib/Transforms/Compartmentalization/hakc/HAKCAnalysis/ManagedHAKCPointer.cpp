//
// Created by de29664 on 9/18/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"

namespace llvm::hakc {
ManagedHAKCPointerUse::ManagedHAKCPointerUse(ManagedHAKCPointer &P, User *User,
                                             unsigned OperandNo, unsigned ID)
    : ManagedPtr(P), UserP(User), OperandNo(OperandNo), ID(ID) {}

User *ManagedHAKCPointerUse::getUser() const { return UserP; }

unsigned ManagedHAKCPointerUse::getOperandNo() const { return OperandNo; }

Value *ManagedHAKCPointerUse::get() const {
  return UserP->getOperand(OperandNo);
}

void ManagedHAKCPointerUse::setUser(User *U) {
  if (!U) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to set a null user for " << UserP << "\n";
    throw std::exception();
  }
  UserP = U;
}

ManagedHAKCPointer &ManagedHAKCPointerUse::getManagedPtr() const {
  return ManagedPtr;
}

unsigned ManagedHAKCPointerUse::getID() const { return ID; }

void ManagedHAKCPointerUse::SortUses(
    SmallVectorImpl<ManagedHAKCPointerUseP> &ManagedUses) {
  llvm::sort(
      ManagedUses.begin(), ManagedUses.end(),
      [](const ManagedHAKCPointerUseP &LHS, const ManagedHAKCPointerUseP &RHS) {
        return LHS->getID() < RHS->getID();
      });
}

HAKCPointerBase::HAKCPointerBase(Value *BaseDefinition, unsigned ID)
    : BaseDefinition(BaseDefinition), AuthenticatedPointer(nullptr),
      HAKCTy(nullptr), ID(ID) {}

Value *HAKCPointerBase::GetBaseDefinition() const { return BaseDefinition; }

HAKCTypeP HAKCPointerBase::GetType() const { return HAKCTy; }

void HAKCPointerBase::SetType(const HAKCTypeP &NewHAKCTy) {
  HAKCTy = NewHAKCTy;
}

Value *HAKCPointerBase::GetAuthenticatedPointer() const {
  return AuthenticatedPointer;
}

bool ManagedHAKCPointer::IsDataPointer() const {
  return !HAKCTy->IsFunctionType() ||
         (HAKCTy->GetPointeeType() &&
          HAKCTy->GetPointeeType()->IsFunctionType());
}

void HAKCPointerBase::SetAuthenticatedPointer(Value *NewAuthenticatedPointer) {
  AuthenticatedPointer = NewAuthenticatedPointer;
}

unsigned HAKCPointerBase::GetID() const { return ID; }

ManagedHAKCPointer::ManagedHAKCPointer(Value *Pointer,
                                       HAKCPointerManager &Manager, unsigned ID)
    : HAKCPointerBase(Pointer, ID), ProtectedPointer(nullptr), Manager(Manager),
      BaseIsAuthenticated(false), ManuallyTransferred(false),
      PurposefullyIgnored(false), AuthenticatedIsCopyOfBase(false),
      AuthenticatedUses(), ProtectedUses(), CloneUses() {
  InitBaseDefinitionInfo();
}

void ManagedHAKCPointer::GetAllUses(
    SmallVectorImpl<ManagedHAKCPointerUseP> &Results) const {
  Results.append(AuthenticatedUses);
  Results.append(ProtectedUses);
  Results.append(CloneUses);
}

void ManagedHAKCPointer::InitBaseDefinitionInfo() {
  PurposefullyIgnored = Manager.GetFunctionAnalysis()
                            .GetModuleAnalysis()
                            .GetCommonAnalysis()
                            .IsIgnoredGlobal(BaseDefinition);

  if (PurposefullyIgnored) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << BaseDefinition << " is purposefully ignored\n";
  }
}

void ManagedHAKCPointer::CheckPointerReplacement(Value *Old, Value *New,
                                                 StringRef TypeName) const {
  CommonHAKCAnalysis::getLogger(Verbose) << "Setting " << TypeName << " Pointer of "
                                  << *this << " to be " << New << "\n";

  if (Old && Old != New) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Tried to replace " << TypeName << " Pointer " << Old << " with "
        << New << " in function "
        << Manager.GetFunctionAnalysis().GetFunction().getName() << "\n";

    if (!PurposefullyIgnored) {
      CommonHAKCAnalysis::getLogger(Fatal) << "!PurposefullyIgnored\n";
      throw std::exception();
    }
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Allowing change since " << *this << " is purposefully ignored\n";
  }
}

void ManagedHAKCPointer::SetProtectedPointer(Value *NewProtectedPointer) {
  CheckPointerReplacement(this->ProtectedPointer, NewProtectedPointer,
                          "Protected");
  this->ProtectedPointer = NewProtectedPointer;
  if (!PointerSetsCanBeEqual()) {
    if (this->ProtectedPointer &&
        this->ProtectedPointer == this->AuthenticatedPointer) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Authenticated and Protected pointers are the same for " << *this
          << " in function "
          << Manager.GetFunctionAnalysis().GetFunction().getName() << "\n";
      throw std::exception();
    }
  }
}

void ManagedHAKCPointer::SetAuthenticatedPointer(
    Value *NewAuthenticatedPointer) {
  CheckPointerReplacement(this->AuthenticatedPointer, NewAuthenticatedPointer,
                          "Authenticated");
  this->AuthenticatedPointer = NewAuthenticatedPointer;
  if (!PointerSetsCanBeEqual()) {
    if (this->AuthenticatedPointer &&
        this->ProtectedPointer == this->AuthenticatedPointer) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Authenticated and Protected pointers are the same for " << *this
          << " in function\n"
          << Manager.GetFunctionAnalysis().GetFunction() << "\n";
      throw std::exception();
    }
  }
}

void ManagedHAKCPointer::RegisterManualHAKCTransfer(CallBase *CallI) {
  if (!Manager.GetFunctionAnalysis()
           .GetModuleAnalysis()
           .GetCommonAnalysis()
           .IsHAKCTransferFunction(CallI->getCalledFunction())) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << CallI << " is not a HAKC Transfer function!\n";
    throw std::exception();
  }
  auto *TransferTypeCast = CommonHAKCAnalysis::GetTargetTypeCast(
      dyn_cast<CallInst>(CallI), BaseDefinition->getType());
  if (ProtectedPointer) {
    if (TransferTypeCast != ProtectedPointer) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Pointer already has a protected pointer: " << ProtectedPointer
          << "\n";
      throw std::exception();
    }
    CommonHAKCAnalysis::getLogger(Verbose)
        << CallI << " is already registered as the protected pointer of "
        << *this << "\n";
    return;
  }

  if (TransferTypeCast) {
    SetProtectedPointer(TransferTypeCast);
  } else {
    SetProtectedPointer(CallI);
  }
  ManuallyTransferred = true;
}

void ManagedHAKCPointer::AddAuthenticatedUse(ManagedHAKCPointerUseP &UPtr) {
  CommonHAKCAnalysis::getLogger(Verbose)
      << *this << " adding Authenticated Use " << *UPtr << "\n";
  AuthenticatedUses.push_back(UPtr);
}

void ManagedHAKCPointer::AddProtectedUse(ManagedHAKCPointerUseP &UPtr) {
  if (!Manager.FunctionIsCompartmentalized()) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << *this << " is not managing proctected uses since "
        << Manager.GetFunctionAnalysis().GetFunction().getName()
        << " is not compartmentalized\n";
    return;
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << *this << " adding Protected Use " << *UPtr << "\n";
  ProtectedUses.push_back(UPtr);
}

void ManagedHAKCPointer::AddCloneUse(ManagedHAKCPointerUseP &UPtr) {
  CommonHAKCAnalysis::getLogger(Verbose)
      << *this << " adding Clone Use " << *UPtr << "\n";
  CloneUses.push_back(UPtr);
}

Value *ManagedHAKCPointer::GetProtectedPointer() const {
  return ProtectedPointer;
}

bool ManagedHAKCPointer::BaseIsAuthenticatedPointer() const {
  return BaseIsAuthenticated;
}

bool ManagedHAKCPointer::IsAuthenticatedIsCopyOfBase() const {
  return AuthenticatedIsCopyOfBase;
}

bool ManagedHAKCPointer::DetermineIfBasePointerIsAuthenticated() {
  BaseIsAuthenticated = ComputeBasePointerAuthenticated();
  return BaseIsAuthenticated;
}

bool ManagedHAKCPointer::PointerSetsCanBeEqual() const {
  bool Result = PurposefullyIgnored;
  return Result;
}

bool ManagedHAKCPointer::PointerSetsShouldBeEqual() const {
  return PointerSetsCanBeEqual();
}

void ManagedHAKCPointer::GetAllIncomingValues(
    SmallVectorImpl<Value *> &Results) const {
  std::set<Value *> ValuesToCheck;
  if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
    for (auto &IncomingValue : PHI->incoming_values()) {
      ValuesToCheck.insert(IncomingValue.get());
    }
  } else if (auto *SelectI = dyn_cast<SelectInst>(BaseDefinition)) {
    ValuesToCheck.insert(SelectI->getTrueValue());
    ValuesToCheck.insert(SelectI->getFalseValue());
  } else if (auto *BinOp = dyn_cast<BinaryOperator>(BaseDefinition)) {
    ValuesToCheck.insert(BinOp->getOperand(0));
    ValuesToCheck.insert(BinOp->getOperand(1));
  } else {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unexpected MultiSSA User: " << BaseDefinition << "\n";
    throw std::exception();
  }

  Results.append(ValuesToCheck.begin(), ValuesToCheck.end());
}

bool ManagedHAKCPointer::AllIncomingValuesAreAuthenticated() {
  bool AllValuesAuthenticated = false;
  if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Checking incoming values of " << *this
                                    << " for authenticated values\n";
    AllValuesAuthenticated = true;
    SmallVector<Value *> ValuesToCheck;
    GetAllIncomingValues(ValuesToCheck);
    for (auto *ValueToCheck : ValuesToCheck) {
      if (isa<GlobalValue>(ValueToCheck)) {
        continue;
      }

      auto ManagedPtr = Manager.GetManagedPointer(ValueToCheck);
      if (ManagedPtr) {
        if (*ManagedPtr == *this || ManagedPtr->BaseIsAuthenticatedPointer()) {
          continue;
        }
      }
      AllValuesAuthenticated = false;
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Incoming value " << ValueToCheck << " of " << BaseDefinition
          << " is not authenticated\n";
      break;
    }
  }
  return AllValuesAuthenticated;
}

bool ManagedHAKCPointer::AllIncomingValuesWillBeAuthenticated() const {
  bool AllValuesAuthenticated = false;
  if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Checking incoming values of " << *this
                                    << " for authenticated values\n";
    AllValuesAuthenticated = true;
    SmallVector<Value *> ValuesToCheck;
    GetAllIncomingValues(ValuesToCheck);
    for (auto *ValueToCheck : ValuesToCheck) {
      if (!Manager.ValueWillBeAuthenticated(ValueToCheck)) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Incoming Value " << *ValueToCheck << " of " << *BaseDefinition
            << " is not authenticated\n";
        AllValuesAuthenticated = false;
        break;
      }
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Incoming Value " << *ValueToCheck << " will be authenticated\n";
    }
  }
  return AllValuesAuthenticated;
}

bool ManagedHAKCPointer::ComputeBasePointerAuthenticated() {
  if (PurposefullyIgnored) {
    return true;
  }

  // stack pointers are the "authenticated" pointer
  bool AlreadyAuthenticated =
      isa<AllocaInst>(BaseDefinition) || isa<GlobalObject>(BaseDefinition);
  if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
    if (Call->getCalledFunction()) {
      auto *Callee = Call->getCalledFunction();
      bool PointerIsTransferred = Manager.GetFunctionAnalysis()
                                      .GetModuleAnalysis()
                                      .GetCommonAnalysis()
                                      .IsHAKCTransferFunction(Callee);

      CommonHAKCAnalysis::getLogger(Verbose) << "Base Definition is ";
      if (!PointerIsTransferred) {
        CommonHAKCAnalysis::getLogger(Verbose) << "not ";
      }
      CommonHAKCAnalysis::getLogger(Verbose) << "a HAKC Transferred function\n";

      if (PointerIsTransferred) {
        AlreadyAuthenticated = PointerIsTransferred;
      } else {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "IsIntrinsicNeedingCloning(" << *Call << ") = "
            << Manager.GetFunctionAnalysis().IsIntrinsicNeedingCloning(Call)
            << "\n";
        AlreadyAuthenticated =
            Manager.GetFunctionAnalysis().IsIntrinsicNeedingCloning(Call);
      }
    } else if (Call->isInlineAsm()) {
      AlreadyAuthenticated = true;
    }
  } else if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
    AlreadyAuthenticated = AllIncomingValuesAreAuthenticated();
  }

  return AlreadyAuthenticated;
}

void ManagedHAKCPointer::SetPointerSetsToBeEqual() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << *this << " setting pointer sets to be equal\n";

  SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(),
                                                 AuthenticatedUses.end());
  SortedUses.append(CloneUses.begin(), CloneUses.end());
  SortedUses.append(ProtectedUses.begin(), ProtectedUses.end());
  ManagedHAKCPointerUse::SortUses(SortedUses);

  for (auto &UPtr : SortedUses) {
    Manager.AddAuthenticatedPointer(UPtr, UPtr->get());
    Manager.AddProtectedPointer(UPtr, UPtr->get());
  }
  SetProtectedPointer(BaseDefinition);
  SetAuthenticatedPointer(BaseDefinition);
}

void ManagedHAKCPointer::MaybeCreateProtectedPointer() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << __FUNCTION__ << " called for " << *this << "\n";

  if (PurposefullyIgnored) {
    CommonHAKCAnalysis::getLogger(Verbose) << *this << " is purposefully ignored\n";
    return;
  } else if (PointerSetsShouldBeEqual()) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << *this << " pointer sets will be made equal\n";
    return;
  }

  if (GetProtectedUserCount() == 0) {
    CommonHAKCAnalysis::getLogger(Verbose) << "No protected pointer use of " << *this
                                    << ", so transfer creation is not needed\n";
    return;
  }

  auto BaseShouldBeTransferred = BaseDefinitionShouldBeTransferred();

  CommonHAKCAnalysis::getLogger(Verbose)
      << "The Base Definition of " << *this << " is ";
  if (BaseIsAuthenticatedPointer()) {
    CommonHAKCAnalysis::getLogger(Verbose) << "authenticated ";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose) << "protected ";
  }
  if (BaseShouldBeTransferred) {
    CommonHAKCAnalysis::getLogger(Verbose) << "and should be transferred";
  }
  CommonHAKCAnalysis::getLogger(Verbose) << "\n";

  Value *ProtectedValue = nullptr;
  if (!BaseIsAuthenticatedPointer() && !BaseShouldBeTransferred &&
      !ManuallyTransferred) {
    ProtectedValue = BaseDefinition;
  } else if (ManuallyTransferred) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Transfer not needed for " << *this
        << " because ProtectedPointer is already set to be "
        << *ProtectedPointer << "\n";
    ProtectedValue = ProtectedPointer;
  } else if (BaseShouldBeTransferred) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Creating Transfer of BaseDefinition of " << *this << "\n";

    if (auto *GV = dyn_cast<GlobalValue>(BaseDefinition)) {
      ProtectedValue =
          Manager.GetFunctionAnalysis().SignGlobalPointerWithColor(GV);
    } else {
      auto *BaseDefI = dyn_cast<Instruction>(BaseDefinition);
      if (!BaseDefI) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Unexpected BaseDefinition for " << *this << " in function "
            << Manager.GetFunctionAnalysis().GetFunction().getName() << "\n";
      }
      ProtectedValue =
          Manager.GetFunctionAnalysis().CreateMissingTransfer(BaseDefI);
    }
  }

  if (!ProtectedValue && GetProtectedUserCount() > 0) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "The protected pointer of " << *this << " is " << ProtectedPointer
        << " but is not manually transferred\n";
    throw std::exception();
  }

  if (ProtectedValue) {
    SetProtectedPointer(ProtectedValue);
    SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(),
                                                   ProtectedUses.end());
    bool ReplaceCloneUses =
        !BaseIsAuthenticatedPointer() || ManuallyTransferred;
    if (ReplaceCloneUses) {
      SortedUses.append(CloneUses.begin(), CloneUses.end());
    }
    ManagedHAKCPointerUse::SortUses(SortedUses);
    for (auto &ProtectedUse : SortedUses) {
      if (ProtectedUse->get() == BaseDefinition) {
        Manager.AddProtectedPointer(ProtectedUse, ProtectedValue);
      } else if (ReplaceCloneUses) {
        Manager.AddProtectedPointer(ProtectedUse, ProtectedUse->get());
      }
    }
  }
}

void ManagedHAKCPointer::MaybeCreateBaseCopyPointer() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << __FUNCTION__ << " called for " << *this << "\n";

  /* Note these checks come from CreateBaseAuthenticatedPointer */
  if (PointerSetsShouldBeEqual() || GetAuthenticatedUserCount() == 0 ||
      BaseIsAuthenticatedPointer()) {
    return;
  }

  bool AllIncomingHaveAuthenticatedVersions =
      CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition) &&
      AllIncomingValuesWillBeAuthenticated();
  if (AllIncomingHaveAuthenticatedVersions) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "All incoming values will have authenticated versions\n";

    auto *BaseCopy =
        Manager.CloneInstruction(dyn_cast<Instruction>(BaseDefinition));
    SetAuthenticatedPointer(BaseCopy);
    AuthenticatedIsCopyOfBase = true;
  }
}

void ManagedHAKCPointer::CreateBaseAuthenticatedPointer() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << __FUNCTION__ << " called for " << *this << "\n";

  if (PointerSetsShouldBeEqual()) {
    SetPointerSetsToBeEqual();
    if (PurposefullyIgnored) {
      CommonHAKCAnalysis::getLogger(Verbose) << *this << " is purposefully ignored\n";
    }
    return;
  }

  if (GetAuthenticatedUserCount() == 0) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "No authenticated pointer uses of " << *this
        << ", so authenticated pointer creation is not needed\n";
    return;
  }

  if (BaseIsAuthenticatedPointer()) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "The Base Definition is authenticated, so setting uses to be "
           "authenticated\n";

    SetAuthenticatedPointer(BaseDefinition);
    SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(),
                                                   AuthenticatedUses.end());
    SortedUses.append(CloneUses.begin(), CloneUses.end());
    ManagedHAKCPointerUse::SortUses(SortedUses);
    for (auto &UPtr : SortedUses) {
      Manager.AddAuthenticatedPointer(UPtr, UPtr->get());
    }
    return;
  }

  if (AuthenticatedPointer) {
    CommonHAKCAnalysis::getLogger(Verbose) << "AuthenticatedPointer already created\n";
    return;
  }

  SmallVector<ManagedHAKCPointerUseP> Users;
  GetAllUses(Users);
  if (Users.empty()) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "User count of " << *this
        << " is 0. No Authenticated Pointer needed\n";
    return;
  }

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Creating Base Authenticated Pointer of " << *this << " with Users\n";
  for (auto &User : Users) {
    CommonHAKCAnalysis::getLogger(Verbose) << *User << "\n";
  }

  std::set<Instruction *> UserI;

  for (auto *User : BaseDefinition->users()) {
    if (auto *I = dyn_cast<Instruction>(User)) {
      if (I->getFunction() == &Manager.GetFunctionAnalysis().GetFunction()) {
        UserI.insert(I);
      }
    }
  }
  auto *AuthenticationInsertPoint =
      Manager.GetFunctionAnalysis().FindUseInsertionPoint(BaseDefinition,
                                                          UserI);

  if (HAKCTy && HAKCTy->IsIgnoredType()) {
    auto *I = Manager.CreateSafePointerAtLocation(BaseDefinition,
                                                  AuthenticationInsertPoint);
    if (I) {
      SetAuthenticatedPointer(I);
    }
  } else {
    Value *I = nullptr;
    if (!Manager.GetFunctionAnalysis().isCompartmentalizedFunction()) {
      I = Manager.CreateSafePointerAtLocation(BaseDefinition,
                                              AuthenticationInsertPoint);
    }

    if (!I) {
      I = Manager.CreateAuthenticationAtLocation(BaseDefinition,
                                                 AuthenticationInsertPoint);
    }
    if (I) {
      SetAuthenticatedPointer(I);
    }
  }

  if (!AuthenticatedPointer) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Failed to create authenticated pointer for " << *this
        << " in Function\n"
        << Manager.GetFunctionAnalysis().GetFunction() << "\n";
    throw std::exception();
  }
}

void ManagedHAKCPointer::CreatePointerReplacements() {
  bool CreateAuthenticatedCopies = GetAuthenticatedUserCount() > 0;
  bool CreateProtectedCopies = GetProtectedUserCount() > 0;

  if (!CreateAuthenticatedCopies) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "No Authenticated Users of " << *this
        << " so no authenticated clones will be created\n";
  }

  if (!CreateProtectedCopies) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "No Protected Users of " << *this
        << " so no protected clones will be created\n";
  }

  if (!CreateAuthenticatedCopies && !CreateProtectedCopies) {
    return;
  }

  SmallVector<ManagedHAKCPointerUseP> SortedUses;

  CommonHAKCAnalysis::getLogger(Verbose)
      << "\n\nCreating Clones of uses of " << *this << ":\n";
  GetAllUses(SortedUses);
  ManagedHAKCPointerUse::SortUses(SortedUses);
  for (auto &Use : SortedUses) {
    CommonHAKCAnalysis::getLogger(Verbose) << "\t" << *Use << "\n";
  }
  SortedUses.clear();

  if (CreateAuthenticatedCopies) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Creating Authenticated Uses Copies\n";
    SortedUses.append(AuthenticatedUses.begin(), AuthenticatedUses.end());
    ManagedHAKCPointerUse::SortUses(SortedUses);

    for (auto &SortedUse : SortedUses) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Creating authenticated clone of " << *SortedUse << "\n";
      auto *AuthenticatedUseClone = CreateAuthenticatedValue(*SortedUse);
      CommonHAKCAnalysis::getLogger(Verbose)
          << "AuthenticatedUseClone: " << *AuthenticatedUseClone << "\n";
    }

    SortedUses.clear();
  }

  if (CreateProtectedCopies) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Creating Protected Uses Copies\n";
    SortedUses.append(ProtectedUses.begin(), ProtectedUses.end());
    ManagedHAKCPointerUse::SortUses(SortedUses);

    for (auto &SortedUse : SortedUses) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Creating protected clone of " << *SortedUse << "\n";
      auto *ProtectedUseClone = CreateProtectedValue(*SortedUse);
      CommonHAKCAnalysis::getLogger(Verbose)
          << "ProtectedUseClone: " << *ProtectedUseClone << "\n";
    }

    SortedUses.clear();
  }

  SortedUses.append(CloneUses.begin(), CloneUses.end());
  ManagedHAKCPointerUse::SortUses(SortedUses);

  for (auto &SortedUse : SortedUses) {
    if (BaseIsAuthenticatedPointer()) {
      if (CreateProtectedCopies) {
        auto *ProtectedClone = CreateProtectedValue(*SortedUse);
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Created Protected clone: " << ProtectedClone << "\n";
      }
      if (CreateAuthenticatedCopies) {
        auto *AuthenticatedValue = CreateAuthenticatedValue(*SortedUse);
        if (!AuthenticatedValue) {
          CommonHAKCAnalysis::getLogger(Fatal)
              << "Could not find Authenticated Value for " << *SortedUse
              << "\n";
          throw std::exception();
        }
      }
    } else {
      if (CreateAuthenticatedCopies) {
        auto *AuthenticatedClone = CreateAuthenticatedValue(*SortedUse);
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Created Authenticated clone: " << AuthenticatedClone << "\n";
      }
      if (CreateProtectedCopies) {
        auto *ProtectedValue = CreateProtectedValue(*SortedUse);
        if (!ProtectedValue) {
          CommonHAKCAnalysis::getLogger(Fatal)
              << "Could not find Protected Value for " << *SortedUse << "\n";
          throw std::exception();
        }
      }
    }
  }
}

void ManagedHAKCPointer::CreatePointerUseClones() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << "\n\n"
      << __FUNCTION__ << " called for " << *this << "\n";

  CreatePointerReplacements();

  CommonHAKCAnalysis::getLogger(Verbose)
      << "\n"
      << Manager.GetFunctionAnalysis().GetFunction();
  for (auto &UPtr : AuthenticatedUses) {
    auto *Replacement = Manager.FindAuthenticatedValue(UPtr->get());
    CommonHAKCAnalysis::getLogger(Verbose) << *UPtr << ": ";
    if (Replacement) {
      CommonHAKCAnalysis::getLogger(Verbose) << Replacement;
    } else {
      CommonHAKCAnalysis::getLogger(Verbose) << "nullptr";
    }
    CommonHAKCAnalysis::getLogger(Verbose) << "\n";
  }
  CommonHAKCAnalysis::getLogger(Verbose) << "\n\nProtectedUses:\n";
  for (auto &UPtr : ProtectedUses) {
    auto *Replacement = Manager.FindAuthenticatedValue(UPtr->get());
    CommonHAKCAnalysis::getLogger(Verbose) << *UPtr << ": ";
    if (Replacement) {
      CommonHAKCAnalysis::getLogger(Verbose) << Replacement;
    } else {
      CommonHAKCAnalysis::getLogger(Verbose) << "nullptr";
    }
    CommonHAKCAnalysis::getLogger(Verbose) << "\n";
  }
}

bool ManagedHAKCPointer::BaseDefinitionShouldBeTransferred() {
  if (!CommonHAKCAnalysis::IsCompartmentalizedFunction(
          &Manager.GetFunctionAnalysis().GetFunction(), Manager.GetClient()) ||
      ManuallyTransferred || PurposefullyIgnored) {
    return false;
  }

  if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
    if (!Call->getCalledFunction()) {
      return true;
    }
    auto *Callee = Call->getCalledFunction();
    return Manager.GetFunctionAnalysis()
               .GetModuleAnalysis()
               .GetCommonAnalysis()
               .IsAllocation(BaseDefinition) ||
           !CommonHAKCAnalysis::FunctionsAreInSameCompartment(
               &Manager.GetFunctionAnalysis().GetFunction(), Callee,
               Manager.GetClient());
  } else if (BaseIsAuthenticatedPointer()) {
    return GetProtectedUserCount() > 0;
  }

  return false;
}

void ManagedHAKCPointer::TransformUses() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << __FUNCTION__ << " called for " << *this << "\n";

  TransformClones();

  if (GetAuthenticatedUserCount() > 0) {
    TransformUseSet(AuthenticatedUses);
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Not transforming Authenticated Pointer Replacements since user count "
         "of "
      << *this << " is 0\n";
  if (GetProtectedUserCount() > 0) {
    TransformUseSet(ProtectedUses);
  } else {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Not transforming Protected Pointer Replacements since user "
           "count is 0\n";
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Function after pointer transformation:\n"
      << Manager.GetFunctionAnalysis().GetFunction() << "\n";
}

void ManagedHAKCPointer::SetUseOperand(User *U, Value *Replacement,
                                       const ManagedHAKCPointerUse &PointerUse,
                                       bool IsAuthenticatedUse) {
  if (PurposefullyIgnored) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to set operand of purposefully ignored pointer " << *this
        << "\n";
    throw std::exception();
  }

  CommonHAKCAnalysis::getLogger(Debug)
      << "Setting Operand " << std::to_string(PointerUse.getOperandNo())
      << " of ";
  if (IsAuthenticatedUse) {
    CommonHAKCAnalysis::getLogger(Debug) << "Authenticated";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose) << "Protected";
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << " User " << U << " to be " << Replacement << " in function "
      << Manager.GetFunctionAnalysis().GetFunction().getName() << " for "
      << *this << "\n";

  if (PointerUse.getUser()->getValueID() != U->getValueID()) {
    if (CommonHAKCAnalysis::IsMultiSSAUser(PointerUse.getUser())) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Not changing operand of MultiSSA User\n";
      return;
    }
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Invalid PointerUse " << PointerUse << " for User " << *U << " of "
        << *this << " in function\n"
        << Manager.GetFunctionAnalysis().GetFunction() << "\n";
    throw std::exception();
  }

  U->setOperand(PointerUse.getOperandNo(), Replacement);
}

bool ManagedHAKCPointer::ValueIsManagedAndHasUsers(
    Value *V, bool CountAuthenticatedUsers) {
  bool Result = false;

  auto ManagedPointer = Manager.GetManagedPointer(V);
  if (ManagedPointer && *ManagedPointer != *this) {
    if (CountAuthenticatedUsers) {
      Result = ManagedPointer->GetAuthenticatedUserCount() > 0;
    } else {
      Result = ManagedPointer->GetProtectedUserCount() > 0;
    }
  }

  return Result;
}

bool ManagedHAKCPointer::UseIsManagedAndHasUsers(
    const ManagedHAKCPointerUse &PointerUse, bool CountAuthenticatedUsers) {
  return ValueIsManagedAndHasUsers(PointerUse.get(), CountAuthenticatedUsers);
}

void ManagedHAKCPointer::UpdateUserCounts() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << *this << " updating user counts of "
      << std::to_string(CloneUses.size()) << " uses\n";
  for (auto &CloneUse : CloneUses) {
    auto *U = CloneUse->getUser();
    if (ValueIsManagedAndHasUsers(U, true)) {
      AddAuthenticatedUse(CloneUse);
    }
    if (ValueIsManagedAndHasUsers(U, false)) {
      AddProtectedUse(CloneUse);
    }
  }
}

void ManagedHAKCPointer::TransformClones() {
  if (PurposefullyIgnored) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Not transforming clones since " << *this
                                    << " is purposefully ignored\n";
    return;
  }

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Transforming clones created for " << *this << "\n";
  for (auto &CloneUse : CloneUses) {
    CommonHAKCAnalysis::getLogger(Verbose) << "\t" << *CloneUse << "\n";
  }

  SmallVector<ManagedHAKCPointerUseP> SortedUses(CloneUses.begin(),
                                                 CloneUses.end());
  ManagedHAKCPointerUse::SortUses(SortedUses);
  for (const auto &CloneUse : SortedUses) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Handling Clone " << *CloneUse->getUser() << "\n";
    if (GetAuthenticatedUserCount() > 0) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Handling Authenticated Use of " << *CloneUse << "\n";
      auto *AuthenticatedVersion =
          Manager.FindAuthenticatedValue(CloneUse->getUser());
      if (!AuthenticatedVersion) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "No authenticated value created for " << *CloneUse << "\n";
      } else {
        auto *AuthenticatedUser = dyn_cast<User>(AuthenticatedVersion);
        auto *Replacement = Manager.FindAuthenticatedValue(*CloneUse);
        if (!Replacement) {
          if (Manager.GetManagedPointer(CloneUse->get()) == nullptr ||
              UseIsManagedAndHasUsers(*CloneUse, true)) {
            CommonHAKCAnalysis::getLogger(Fatal)
                << "Unable to find Authenticated replacement of " << *CloneUse
                << "\n";
            Manager.PrintAuthenticatedValues();
            CommonHAKCAnalysis::getLogger(Fatal)
                << "\n"
                << Manager.GetFunctionAnalysis().GetFunction() << "\n";
            throw std::exception();
          }
          CommonHAKCAnalysis::getLogger(Verbose)
              << *CloneUse << " does not need authenticated operand replaced\n";
          continue;
        }
        if (!AuthenticatedUser) {
          CommonHAKCAnalysis::getLogger(Fatal)
              << "AuthenticatedVersion is not a User: " << AuthenticatedVersion
              << "\n"
              << Manager.GetFunctionAnalysis().GetFunction() << "\n";
          throw std::exception();
        }

        SetUseOperand(AuthenticatedUser, Replacement, *CloneUse, true);
      }
    }
    if (GetProtectedUserCount() > 0) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Handling Protected Use of " << *CloneUse << "\n";
      auto *ProtectedVersion = Manager.FindProtectedValue(CloneUse->getUser());
      if (!ProtectedVersion) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "No protected value created for " << *CloneUse << "\n";
        continue;
      }
      auto *ProtectedUser = dyn_cast<User>(ProtectedVersion);
      auto *Replacement = Manager.FindProtectedValue(*CloneUse);
      if (!Replacement) {
        if (Manager.GetManagedPointer(CloneUse->get()) == nullptr ||
            UseIsManagedAndHasUsers(*CloneUse, false)) {
          CommonHAKCAnalysis::getLogger(Fatal)
              << "Unable to find Protected replacement of " << *CloneUse
              << "\n";
          Manager.PrintProtectedValues();
          throw std::exception();
        }
        CommonHAKCAnalysis::getLogger(Verbose)
            << *CloneUse << " does not need protected operand replaced\n";
        continue;
      }
      if (!ProtectedUser) {
        CommonHAKCAnalysis::getLogger(Fatal)
            << "ProtectedVersion is not a User: " << ProtectedVersion << "\n"
            << Manager.GetFunctionAnalysis().GetFunction() << "\n";
        throw std::exception();
      }

      SetUseOperand(ProtectedUser, Replacement, *CloneUse, false);
    }
  }
}

void ManagedHAKCPointer::TransformUseSet(
    SmallVectorImpl<ManagedHAKCPointerUseP> &UseSet) {
  if (PurposefullyIgnored) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Not transforming uses since " << *this
                                    << " is purposefully ignored\n";
    return;
  }

  bool UseAuthenticatedValue = (&UseSet == &AuthenticatedUses);
  StringRef ReplacementSource =
      UseAuthenticatedValue ? "Authenticated" : "Protected";

  ManagedHAKCPointerUse::SortUses(UseSet);

  CommonHAKCAnalysis::getLogger(Verbose) << "Replacing the following operands with "
                                  << ReplacementSource << " values\n";
  for (auto &Use : UseSet) {
    CommonHAKCAnalysis::getLogger(Verbose) << "\t" << *Use << "\n";
  }

  for (const auto &SortedUse : UseSet) {
    Value *Replacement, *ReplacementUser;
    if (UseAuthenticatedValue) {
      Replacement = Manager.FindAuthenticatedValue(*SortedUse);
      if (!CommonHAKCAnalysis::IsMultiSSAUser(SortedUse->getUser()) &&
          ValueIsManagedAndHasUsers(SortedUse->getUser(), true)) {
        ReplacementUser = SortedUse->getUser();
      } else {
        ReplacementUser = Manager.FindAuthenticatedValue(SortedUse->getUser());
      }
    } else {
      Replacement = Manager.FindProtectedValue(*SortedUse);
      if (!CommonHAKCAnalysis::IsMultiSSAUser(SortedUse->getUser()) &&
          ValueIsManagedAndHasUsers(SortedUse->getUser(), false)) {
        ReplacementUser = SortedUse->getUser();
      } else {
        ReplacementUser = Manager.FindProtectedValue(SortedUse->getUser());
      }
    }

    if (!Replacement) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Unable to find " << ReplacementSource << " replacement of "
          << *SortedUse << "\n"
          << Manager.GetFunctionAnalysis().GetFunction() << "\n";
      if (UseAuthenticatedValue) {
        Manager.PrintAuthenticatedValues();
      } else {
        Manager.PrintProtectedValues();
      }
      throw std::exception();
    }

    if (!ReplacementUser) {
      ReplacementUser = SortedUse->getUser();
    }

    if (!isa<User>(ReplacementUser)) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Invalid ReplacementUser: " << *ReplacementUser << "\n";
      throw std::exception();
    }

    SetUseOperand(dyn_cast<User>(ReplacementUser), Replacement, *SortedUse,
                  UseAuthenticatedValue);
  }
}

Value *
ManagedHAKCPointer::CreateAuthenticatedValue(ManagedHAKCPointerUse &HAKCUse) {
  if (PurposefullyIgnored) {
    return HAKCUse.get();
  }

  auto *Authenticated = Manager.CreateAuthenticatedValue(HAKCUse);
  if (!Authenticated) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "CreateAuthenticatedValue returned null for " << HAKCUse << "\n";
  } else {
    auto *Pointer = HAKCUse.get();
    SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(),
                                                   AuthenticatedUses.end());
    SortedUses.append(CloneUses.begin(), CloneUses.end());
    ManagedHAKCPointerUse::SortUses(SortedUses);

    for (auto &PointerUse : SortedUses) {
      if (PointerUse->get() == Pointer) {
        Manager.AddAuthenticatedPointer(PointerUse, Authenticated);
      }
    }
  }

  return Authenticated;
}

Value *
ManagedHAKCPointer::CreateProtectedValue(ManagedHAKCPointerUse &HAKCUse) {
  if (PurposefullyIgnored) {
    return HAKCUse.get();
  }

  auto Protected = Manager.CreateProtectedValue(HAKCUse);
  if (!Protected) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "CreateProtectedValue returned null for " << HAKCUse << "\n";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose) << "Found Protected " << Protected << "\n";
    auto *Pointer = HAKCUse.get();
    SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(),
                                                   ProtectedUses.end());
    SortedUses.append(CloneUses.begin(), CloneUses.end());
    ManagedHAKCPointerUse::SortUses(SortedUses);
    for (auto &PointerUse : SortedUses) {
      if (PointerUse->get() == Pointer) {
        Manager.AddProtectedPointer(PointerUse, Protected);
      }
    }
  }

  return Protected;
}

unsigned ManagedHAKCPointer::GetAuthenticatedUserCount() const {
  return AuthenticatedUses.size();
}

unsigned ManagedHAKCPointer::GetProtectedUserCount() const {
  return ProtectedUses.size();
}
} // namespace llvm::hakc
