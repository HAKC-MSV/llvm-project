//
// Created by de29664 on 9/18/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"

namespace llvm::hakc {
ManagedHAKCPointerUse::ManagedHAKCPointerUse(ManagedHAKCPointer &P, User *User,
                                             const unsigned OperandNo,
                                             const unsigned ID)
  : ManagedPtr(P), UserP(User), OperandNo(OperandNo), ID(ID) {}

User *ManagedHAKCPointerUse::getUser() const { return UserP; }

unsigned ManagedHAKCPointerUse::getOperandNo() const { return OperandNo; }

Value *ManagedHAKCPointerUse::get() const {
  return UserP->getOperand(OperandNo);
}

void ManagedHAKCPointerUse::setUser(User *U) {
  if (!U) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Trying to set a null user for " << UserP << "\n";
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

HAKCPointerBase::HAKCPointerBase(Value *BaseDefinition, const unsigned ID)
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
                                       HAKCPointerManager &Manager,
                                       const unsigned ID)
  : HAKCPointerBase(Pointer, ID), Manager(Manager) {
  InitBaseDefinitionInfo();
}

void ManagedHAKCPointer::GetAllUses(
    SmallVectorImpl<ManagedHAKCPointerUseP> &Results) const {
  Results.append(AuthenticatedUses);
  Results.append(ProtectedUses);
  Results.append(CloneUses);
}

void ManagedHAKCPointer::InitBaseDefinitionInfo() {
  PurposefullyIgnored = Manager.GetModuleAnalysis()
      .GetCommonAnalysis()
      .IsIgnoredGlobal(BaseDefinition);

  if (PurposefullyIgnored) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << BaseDefinition << " is purposefully ignored\n";
  }
}

void ManagedHAKCPointer::CheckPointerReplacement(Value *Old, Value *New, const StringRef TypeName) const {
  CommonHAKCAnalysis::getLogger(Verbose) << "Setting " << TypeName <<
      " Pointer of "
      << *this << " to be " << New << "\n";

  if (Old && Old != New) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Tried to replace " << TypeName << " Pointer " << Old << " with "
        << New << " in function "
        << Manager.GetFunction().getName() << "\n";

    if (!PurposefullyIgnored) {
      CommonHAKCAnalysis::getLogger(Fatal) << "!PurposefullyIgnored\n";
      throw std::exception();
    }
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Allowing change since " << *this << " is purposefully ignored\n";
  }
}


void ManagedHAKCPointer::SetAuthenticatedPointer(Value *NewAuthenticatedPointer) {
  CheckPointerReplacement(this->AuthenticatedPointer, NewAuthenticatedPointer, "Authenticated");
  this->AuthenticatedPointer = NewAuthenticatedPointer;
  if (!PointerSetsCanBeEqual()) {
    if (this->AuthenticatedPointer &&
        this->ProtectedPointer == this->AuthenticatedPointer) {
      CommonHAKCAnalysis::getLogger(Fatal)
          << "Authenticated and Protected pointers are the same for " << *this
          << " in function\n"
          << Manager.GetFunction() << "\n";
      throw std::exception();
    }
  }
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
  const bool Result = PurposefullyIgnored;
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
  } else if (const auto *BinOp = dyn_cast<BinaryOperator>(BaseDefinition)) {
    ValuesToCheck.insert(BinOp->getOperand(0));
    ValuesToCheck.insert(BinOp->getOperand(1));
  } else {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unexpected MultiSSA User: " << BaseDefinition << "\n";
    throw std::exception();
  }

  Results.append(ValuesToCheck.begin(), ValuesToCheck.end());
}

bool ManagedHAKCPointer::AllIncomingValuesAreAuthenticated() const {
  bool AllValuesAuthenticated = false;
  if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Checking incoming values of " <<
        *this
        << " for authenticated values\n";
    AllValuesAuthenticated = true;
    SmallVector<Value *> ValuesToCheck;
    GetAllIncomingValues(ValuesToCheck);
    for (auto *ValueToCheck : ValuesToCheck) {
      if (isa<GlobalValue>(ValueToCheck)) { continue; }

      if (auto ManagedPtr = Manager.GetManagedPointer(ValueToCheck)) {
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
    CommonHAKCAnalysis::getLogger(Verbose) << "Checking incoming values of " <<
        *this
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

bool ManagedHAKCPointer::ComputeBasePointerAuthenticated() const {
  if (PurposefullyIgnored) { return true; }

  // stack pointers are the "authenticated" pointer
  bool AlreadyAuthenticated =
      isa<AllocaInst>(BaseDefinition) || isa<GlobalObject>(BaseDefinition);
  if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
    if (Call->getCalledFunction()) {
      auto *Callee = Call->getCalledFunction();
      const bool PointerIsTransferred = Manager.GetModuleAnalysis()
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
            << Manager.IsIntrinsicNeedingCloning(Call)
            << "\n";
        AlreadyAuthenticated =
            Manager.IsIntrinsicNeedingCloning(Call);
      }
    } else
      if (Call->isInlineAsm()) { AlreadyAuthenticated = true; }
  } else if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
    AlreadyAuthenticated = AllIncomingValuesAreAuthenticated();
  }

  return AlreadyAuthenticated;
}

bool ManagedHAKCPointer::ValueIsManagedAndHasUsers(
    Value *V, const bool CountAuthenticatedUsers) const {
  bool Result = false;

  if (const auto ManagedPointer = Manager.GetManagedPointer(V)) {
    if (ManagedPointer && *ManagedPointer != *this) {
      Result = CountAuthenticatedUsers
        ? ManagedPointer->GetAuthenticatedUserCount() > 0
        : ManagedPointer->GetProtectedUserCount() > 0;
    }
  }

  return Result;
}

bool ManagedHAKCPointer::UseIsManagedAndHasUsers(
    const ManagedHAKCPointerUse &PointerUse,
    const bool CountAuthenticatedUsers) const {
  return ValueIsManagedAndHasUsers(PointerUse.get(), CountAuthenticatedUsers);
}


unsigned ManagedHAKCPointer::GetAuthenticatedUserCount() const {
  return AuthenticatedUses.size();
}

unsigned ManagedHAKCPointer::GetProtectedUserCount() const {
  return ProtectedUses.size();
}

void ManagedHAKCPointer::AddCloneUse(const ManagedHAKCPointerUseP &UPtr) {
  CommonHAKCAnalysis::getLogger(Verbose) << *this << " adding Clone Use " << *UPtr << "\n";
  CloneUses.push_back(UPtr);
}

void ManagedHAKCPointer::AddAuthenticatedUse(
    const ManagedHAKCPointerUseP &UPtr) {
  CommonHAKCAnalysis::getLogger(Verbose) << *this << " adding Authenticated Use " << *UPtr << "\n";
  AuthenticatedUses.push_back(UPtr);
}


void ManagedHAKCPointer::RegisterManualHAKCTransfer(CallBase *CallI) {
  if (!Manager.GetModuleAnalysis()
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

  if (TransferTypeCast) { SetProtectedPointer(TransferTypeCast); } else {
    SetProtectedPointer(CallI);
  }
  ManuallyTransferred = true;
}

void ManagedHAKCPointer::SetProtectedPointer(Value *NewProtectedPointer) {
  CheckPointerReplacement(this->ProtectedPointer, NewProtectedPointer,
                          "Protected");
  this->ProtectedPointer = NewProtectedPointer;
  if (!PointerSetsCanBeEqual()) {
    if (this->ProtectedPointer && this->ProtectedPointer == this->AuthenticatedPointer) {
      CommonHAKCAnalysis::getLogger(Fatal) << "Authenticated and Protected pointers are the same for "
        << *this << " in function " << Manager.GetFunction().getName() << "\n";
      throw std::exception();
    }
  }
}

void ManagedHAKCPointer::AddProtectedUse(const ManagedHAKCPointerUseP &UPtr) {
  CommonHAKCAnalysis::getLogger(Debug) << "AddProtectedUse has FunctionIsCompartmentalized: " << Manager.FunctionIsCompartmentalized() << "\n";
  if (!Manager.FunctionIsCompartmentalized()) {
    CommonHAKCAnalysis::getLogger(Verbose) << *this << " is not managing protected uses since "
        << Manager.GetFunction().getName() << " is not compartmentalized\n";
    return;
  }
  CommonHAKCAnalysis::getLogger(Verbose) << *this << " adding Protected Use " << *UPtr << "\n";
  ProtectedUses.push_back(UPtr);
}


} // namespace llvm::hakc
