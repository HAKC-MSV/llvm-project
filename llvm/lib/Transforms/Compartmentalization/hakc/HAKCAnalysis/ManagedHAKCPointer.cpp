//
// Created by de29664 on 9/18/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"

#include <utility>
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"

namespace llvm::hakc {
    ManagedHAKCPointerUse::ManagedHAKCPointerUse(ManagedHAKCPointerP P, User *User, unsigned OperandNo,
                                                 unsigned ID) : ManagedPtr(std::move(P)),
                                                                UserP(User),
                                                                OperandNo(OperandNo),
                                                                ID(ID) {
    }

    User *ManagedHAKCPointerUse::getUser() const {
        return UserP;
    }

    unsigned ManagedHAKCPointerUse::getOperandNo() const {
        return OperandNo;
    }

    Value *ManagedHAKCPointerUse::get() const {
        return UserP->getOperand(OperandNo);
    }

    void ManagedHAKCPointerUse::setUser(User *U) {
        if (!U) {
            errs() << "Trying to set a null user for " << UserP << "\n";
            throw std::exception();
        }
        UserP = U;
    }

    ManagedHAKCPointerP ManagedHAKCPointerUse::getManagedPtr() const {
        return ManagedPtr;
    }

    unsigned ManagedHAKCPointerUse::getID() const {
        return ID;
    }

    void ManagedHAKCPointerUse::SortUses(SmallVector<ManagedHAKCPointerUseP> &ManagedUses) {
        llvm::sort(ManagedUses.begin(), ManagedUses.end(),
                   [](const ManagedHAKCPointerUseP &LHS, const ManagedHAKCPointerUseP &RHS) {
                       return LHS->getID() < RHS->getID();
                   });
    }

    HAKCPointerBase::HAKCPointerBase(Value *BaseDefinition, unsigned ID) : BaseDefinition(BaseDefinition),
                                                                           AuthenticatedPointer(nullptr),
                                                                           HAKCTy(nullptr), ID(ID) {
    }

    Value *HAKCPointerBase::GetBaseDefinition() const {
        return BaseDefinition;
    }

    HAKCTypeP HAKCPointerBase::GetType() {
        return HAKCTy;
    }

    void HAKCPointerBase::SetType(HAKCTypeP NewHAKCTy) {
        HAKCTy = std::move(NewHAKCTy);
    }

    Value *HAKCPointerBase::GetAuthenticatedPointer() {
        return AuthenticatedPointer;
    }

    void HAKCPointerBase::SetAuthenticatedPointer(Value *NewAuthenticatedPointer) {
        AuthenticatedPointer = NewAuthenticatedPointer;
    }

    unsigned HAKCPointerBase::GetID() const {
        return ID;
    }


    ManagedHAKCPointer::ManagedHAKCPointer(Value *Pointer, HAKCPointerManager &Manager,
                                           unsigned ID) : HAKCPointerBase(Pointer, ID),
                                                          ProtectedPointer(nullptr),
                                                          DebugActive(Manager.DebugIsActive()),
                                                          Manager(Manager),
                                                          BaseIsAuthenticated(false),
                                                          ManuallyTransferred(false),
                                                          PurposefullyIgnored(false),
                                                          AuthenticatedIsCopyOfBase(false),
                                                          AuthenticatedUses(),
                                                          ProtectedUses(),
                                                          CloneUses() {
        InitBaseDefinitionInfo();
    }

    std::set<ManagedHAKCPointerUseP> ManagedHAKCPointer::GetAllUses() {
        std::set<ManagedHAKCPointerUseP> Result;
        for (auto &UPtr: AuthenticatedUses) {
            Result.insert(UPtr);
        }
        for (auto &UPtr: ProtectedUses) {
            Result.insert(UPtr);
        }
        for (auto &UPtr: CloneUses) {
            Result.insert(UPtr);
        }

        return Result;
    }

    void ManagedHAKCPointer::InitBaseDefinitionInfo() {
        PurposefullyIgnored =
                Manager.GetFunctionAnalysis().GetModuleAnalysis().GetCommonAnalysis().IsIgnoredGlobal(BaseDefinition);

        if (PurposefullyIgnored) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << BaseDefinition << " is purposefully ignored\n";
            }
        }
    }

    void ManagedHAKCPointer::CheckPointerReplacement(Value *Old, Value *New, StringRef TypeName) const {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Setting " << TypeName << " Pointer of " << *this << " to be " << New
                    << "\n";
        }

        if (Old && Old != New) {
            CommonHAKCAnalysis::getWriter() << "Tried to replace " << TypeName << " Pointer " << Old << " with " << New
                    << " in function "
                    << Manager.GetFunctionAnalysis().getFunction().getName() << "\n";

            if (!PurposefullyIgnored) {
                errs() << "!PurposefullyIgnored\n";
                throw std::exception();
            } else {
                CommonHAKCAnalysis::getWriter() << "Allowing change since " << *this << " is purposefully ignored\n";
            }
        }
    }

    void ManagedHAKCPointer::SetProtectedPointer(Value *NewProtectedPointer) {
        CheckPointerReplacement(this->ProtectedPointer, NewProtectedPointer, "Protected");
        this->ProtectedPointer = NewProtectedPointer;
        if (!PointerSetsCanBeEqual()) {
            if (this->ProtectedPointer && this->ProtectedPointer == this->AuthenticatedPointer) {
                errs() << "exception in SetProtectedPointer\n";
                CommonHAKCAnalysis::getWriter() << "Authenticated and Protected pointers are the same for " << *this
                        << " in function "
                        << Manager.GetFunctionAnalysis().getFunction().getName() << "\n";
                throw std::exception();
            }
        }
    }

    void ManagedHAKCPointer::SetAuthenticatedPointer(Value *NewAuthenticatedPointer) {
        CheckPointerReplacement(this->AuthenticatedPointer, NewAuthenticatedPointer, "Authenticated");
        this->AuthenticatedPointer = NewAuthenticatedPointer;
        if (!PointerSetsCanBeEqual()) {
            if (this->AuthenticatedPointer && this->ProtectedPointer == this->AuthenticatedPointer) {
                errs() << "exception in SetAuthenticatedPointer\n";
                CommonHAKCAnalysis::getWriter() << "Authenticated and Protected pointers are the same for " << *this
                        << " in function\n" << Manager.GetFunctionAnalysis().getFunction()
                        << "\n";
                throw std::exception();
            }
        }
    }

    void ManagedHAKCPointer::RegisterManualHAKCTransfer(CallBase *CallI) {
        if (!Manager.GetFunctionAnalysis().GetModuleAnalysis().GetCommonAnalysis().IsHAKCTransferFunction(
            CallI->getCalledFunction())) {
            errs() << CallI << " is not a HAKC Transfer function!\n";
            throw std::exception();
        }
        auto *TransferTypeCast = CommonHAKCAnalysis::GetTargetTypeCast(dyn_cast<CallInst>(CallI),
                                                                       BaseDefinition->getType());
        if (ProtectedPointer) {
            if (TransferTypeCast != ProtectedPointer) {
                errs() << "Pointer already has a protected pointer: " << ProtectedPointer
                        << "\n";
                throw std::exception();
            }
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << CallI << " is already registered as the protected pointer of "
                        << *this << "\n";
            }
            return;
        }

        if (TransferTypeCast) {
            SetProtectedPointer(TransferTypeCast);
        } else {
            SetProtectedPointer(CallI);
        }
        ManuallyTransferred = true;
    }

    void ManagedHAKCPointer::AddAuthenticatedUse(const ManagedHAKCPointerUseP &UPtr) {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " adding Authenticated Use " << *UPtr << "\n";
        }
        AuthenticatedUses.insert(UPtr);
    }

    void ManagedHAKCPointer::AddProtectedUse(const ManagedHAKCPointerUseP &UPtr) {
        if (!Manager.FunctionIsCompartmentalized()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << *this << " is not managing proctected uses since "
                        << Manager.GetFunctionAnalysis().getFunction().getName()
                        << " is not compartmentalized\n";
            }
            return;
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " adding Protected Use " << *UPtr << "\n";
        }
        ProtectedUses.insert(UPtr);
    }

    void ManagedHAKCPointer::AddCloneUse(const ManagedHAKCPointerUseP &UPtr) {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " adding Clone Use " << *UPtr << "\n";
        }
        CloneUses.insert(UPtr);
    }

    Value *ManagedHAKCPointer::GetProtectedPointer() {
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

    bool ManagedHAKCPointer::PointerSetsCanBeEqual() {
        bool Result = PurposefullyIgnored;
        //        if (!Manager.FunctionIsCompartmentalized()) {
        //            if (isa<AllocaInst>(BaseDefinition)) {
        //                Result = true;
        //            } else if (auto *CallB = dyn_cast<CallBase>(BaseDefinition)) {
        //                Result = CallB->isInlineAsm();
        //            }
        //        }
        return Result;
    }

    bool ManagedHAKCPointer::PointerSetsShouldBeEqual() {
        return PointerSetsCanBeEqual();
    }

    std::set<Value *> ManagedHAKCPointer::GetAllIncomingValues() {
        std::set<Value *> ValuesToCheck;
        if (auto *PHI = dyn_cast<PHINode>(BaseDefinition)) {
            for (auto &IncomingValue: PHI->incoming_values()) {
                ValuesToCheck.insert(IncomingValue.get());
            }
        } else if (auto *SelectI = dyn_cast<SelectInst>(BaseDefinition)) {
            ValuesToCheck.insert(SelectI->getTrueValue());
            ValuesToCheck.insert(SelectI->getFalseValue());
        } else if (auto *BinOp = dyn_cast<BinaryOperator>(BaseDefinition)) {
            ValuesToCheck.insert(BinOp->getOperand(0));
            ValuesToCheck.insert(BinOp->getOperand(1));
        } else {
            errs() << "Unexpected MultiSSA User: " << BaseDefinition << "\n";
            throw std::exception();
        }

        return ValuesToCheck;
    }

    bool ManagedHAKCPointer::AllIncomingValuesAreAuthenticated() {
        bool AllValuesAuthenticated = false;
        if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Checking incoming values of " << *this
                        << " for authenticated values\n";
            }
            AllValuesAuthenticated = true;
            auto ValuesToCheck = GetAllIncomingValues();
            for (auto *ValueToCheck: ValuesToCheck) {
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
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Incoming value " << ValueToCheck << " of " << BaseDefinition
                            << " is not authenticated\n";
                }
                break;
            }
        }
        return AllValuesAuthenticated;
    }

    bool ManagedHAKCPointer::AllIncomingValuesWillBeAuthenticated() {
        bool AllValuesAuthenticated = false;
        if (CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition)) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Checking incoming values of " << *this
                        << " for authenticated values\n";
            }
            AllValuesAuthenticated = true;
            auto ValuesToCheck = GetAllIncomingValues();
            for (auto *ValueToCheck: ValuesToCheck) {
                if (!Manager.ValueWillBeAuthenticated(ValueToCheck)) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Incoming Value " << *ValueToCheck << " of "
                                << *BaseDefinition << " is not authenticated\n";
                    }
                    AllValuesAuthenticated = false;
                    break;
                } else if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Incoming Value " << *ValueToCheck
                            << " will be authenticated\n";
                }
            }
        }
        return AllValuesAuthenticated;
    }

    bool ManagedHAKCPointer::ComputeBasePointerAuthenticated() {
        if (PurposefullyIgnored) {
            return true;
        }

        // stack pointers are the "authenticated" pointer
        bool AlreadyAuthenticated = isa<AllocaInst>(BaseDefinition) || isa<GlobalObject>(BaseDefinition);
        if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
            if (Call->getCalledFunction()) {
                auto *Callee = Call->getCalledFunction();
                bool PointerIsTransferred = Manager.GetFunctionAnalysis().GetModuleAnalysis().GetCommonAnalysis().
                        IsHAKCTransferFunction(
                            Callee);
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Base Definition is ";
                    if (!PointerIsTransferred) {
                        CommonHAKCAnalysis::getWriter() << "not ";
                    }
                    CommonHAKCAnalysis::getWriter() << "a HAKC Transferred function\n";
                }
                if (PointerIsTransferred) {
                    AlreadyAuthenticated = PointerIsTransferred;
                } else {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "IsIntrinsicNeedingCloning(" << *Call << ") = "
                                << Manager.GetFunctionAnalysis().IsIntrinsicNeedingCloning(Call)
                                << "\n";
                    }
                    AlreadyAuthenticated = Manager.GetFunctionAnalysis().IsIntrinsicNeedingCloning(Call);
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
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " setting pointer sets to be equal\n";
        }

        SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(), AuthenticatedUses.end());
        SortedUses.append(CloneUses.begin(), CloneUses.end());
        SortedUses.append(ProtectedUses.begin(), ProtectedUses.end());
        ManagedHAKCPointerUse::SortUses(SortedUses);

        for (const auto &UPtr: SortedUses) {
            Manager.AddAuthenticatedPointer(UPtr, UPtr->get());
            Manager.AddProtectedPointer(UPtr, UPtr->get());
        }
        SetProtectedPointer(BaseDefinition);
        SetAuthenticatedPointer(BaseDefinition);
    }

    void ManagedHAKCPointer::MaybeCreateProtectedPointer() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        if (PurposefullyIgnored) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << *this << " is purposefully ignored\n";
            }
            return;
        } else if (PointerSetsShouldBeEqual()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << *this << " pointer sets will be made equal\n";
            }
            return;
        }

        if (GetProtectedUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No protected pointer use of " << *this
                        << ", so transfer creation is not needed\n";
            }
            return;
        }

        auto BaseShouldBeTransferred = BaseDefinitionShouldBeTransferred();
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "The Base Definition of " << *this << " is ";
            if (BaseIsAuthenticatedPointer()) {
                CommonHAKCAnalysis::getWriter() << "authenticated ";
            } else {
                CommonHAKCAnalysis::getWriter() << "protected ";
            }
            if (BaseShouldBeTransferred) {
                CommonHAKCAnalysis::getWriter() << "and should be transferred";
            }
            CommonHAKCAnalysis::getWriter() << "\n";
        }

        Value *ProtectedValue = nullptr;
        if (!BaseIsAuthenticatedPointer() && !BaseShouldBeTransferred && !ManuallyTransferred) {
            ProtectedValue = BaseDefinition;
        } else if (ManuallyTransferred) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Transfer not needed for " << *this
                        << " because ProtectedPointer is already set to be "
                        << *ProtectedPointer << "\n";
            }
            ProtectedValue = ProtectedPointer;
        } else if (BaseShouldBeTransferred) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Creating Transfer of BaseDefinition of " << *this << "\n";
            }

            if (auto *GV = dyn_cast<GlobalValue>(BaseDefinition)) {
                ProtectedValue = Manager.GetFunctionAnalysis().SignGlobalPointerWithColor(GV);
            } else {
                auto *BaseDefI = dyn_cast<Instruction>(BaseDefinition);
                if (!BaseDefI) {
                    CommonHAKCAnalysis::getWriter() << "Unexpected BaseDefinition for " << *this << " in function "
                            << Manager.GetFunctionAnalysis().getFunction().getName() << "\n";
                }
                ProtectedValue = Manager.GetFunctionAnalysis().CreateMissingTransfer(BaseDefI);
            }
        }

        if (!ProtectedValue && GetProtectedUserCount() > 0) {
            errs() << "exception in MaybeCreateProtectedPointer\n";
            CommonHAKCAnalysis::getWriter() << "The protected pointer of " << *this << " is " << ProtectedPointer
                    << " but is not manually transferred\n";
            throw std::exception();
        }

        if (ProtectedValue) {
            SetProtectedPointer(ProtectedValue);
            SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(), ProtectedUses.end());
            bool ReplaceCloneUses = !BaseIsAuthenticatedPointer() || ManuallyTransferred;
            if (ReplaceCloneUses) {
                SortedUses.append(CloneUses.begin(), CloneUses.end());
            }
            ManagedHAKCPointerUse::SortUses(SortedUses);
            for (const auto &ProtectedUse: SortedUses) {
                if (ProtectedUse->get() == BaseDefinition) {
                    Manager.AddProtectedPointer(ProtectedUse, ProtectedValue);
                } else if (ReplaceCloneUses) {
                    Manager.AddProtectedPointer(ProtectedUse, ProtectedUse->get());
                }
            }
        }
    }

    void ManagedHAKCPointer::MaybeCreateBaseCopyPointer() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        /* Note these checks come from CreateBaseAuthenticatedPointer */
        if (PointerSetsShouldBeEqual() || GetAuthenticatedUserCount() == 0 || BaseIsAuthenticatedPointer()) {
            return;
        }

        bool AllIncomingHaveAuthenticatedVersions =
                CommonHAKCAnalysis::IsMultiSSAUser(BaseDefinition) && AllIncomingValuesWillBeAuthenticated();
        if (AllIncomingHaveAuthenticatedVersions) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "All incoming values will have authenticated versions\n";
            }

            auto BaseCopy = Manager.CloneInstruction(dyn_cast<Instruction>(BaseDefinition));
            SetAuthenticatedPointer(BaseCopy);
            AuthenticatedIsCopyOfBase = true;
            return;
        }
    }

    void ManagedHAKCPointer::CreateBaseAuthenticatedPointer() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        if (PointerSetsShouldBeEqual()) {
            SetPointerSetsToBeEqual();
            if (PurposefullyIgnored) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << *this << " is purposefully ignored\n";
                }
            }
            return;
        }

        if (GetAuthenticatedUserCount() == 0) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "No authenticated pointer uses of " << *this
                        << ", so authenticated pointer creation is not needed\n";
            }
            return;
        }

        if (BaseIsAuthenticatedPointer()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter()
                        << "The Base Definition is authenticated, so setting uses to be authenticated\n";
            }

            SetAuthenticatedPointer(BaseDefinition);
            SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(), AuthenticatedUses.end());
            SortedUses.append(CloneUses.begin(), CloneUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);
            for (const auto &UPtr: SortedUses) {
                Manager.AddAuthenticatedPointer(UPtr, UPtr->get());
            }
            return;
        }

        if (AuthenticatedPointer) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "AuthenticatedPointer already created\n";
            }
            return;
        }

        auto Users = GetAllUses();
        if (Users.empty()) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "User count of " << *this
                        << " is 0. No Authenticated Pointer needed\n";
            }
            return;
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Creating Base Authenticated Pointer of " << *this
                    << " with Users\n";
            for (auto &User: Users) {
                CommonHAKCAnalysis::getWriter() << *User << "\n";
            }
        }
        std::set<Instruction *> UserI;

        for (auto *User: BaseDefinition->users()) {
            if (auto *I = dyn_cast<Instruction>(User)) {
                if (I->getFunction() == &Manager.GetFunctionAnalysis().getFunction()) {
                    UserI.insert(I);
                }
            }
        }
        auto *AuthenticationInsertPoint = Manager.GetFunctionAnalysis().FindUseInsertionPoint(BaseDefinition,
            UserI);
        if (Manager.GetFunctionAnalysis().GetModuleAnalysis().GetCommonAnalysis().IsIgnoredType(
            BaseDefinition->getType())) {
            auto *I = Manager.CreateSafePointerAtLocation(BaseDefinition, AuthenticationInsertPoint);
            if (I) {
                SetAuthenticatedPointer(I);
            }
        } else {
            Value *I = nullptr;
            if (!Manager.GetFunctionAnalysis().isCompartmentalizedFunction() || isa<CallBase>(BaseDefinition)) {
                I = Manager.CreateSafePointerAtLocation(BaseDefinition, AuthenticationInsertPoint);
            }

            if (!I) {
                I = Manager.CreateAuthenticationAtLocation(BaseDefinition, AuthenticationInsertPoint);
            }
            if (I) {
                SetAuthenticatedPointer(I);
            }
        }

        if (!AuthenticatedPointer) {
            errs() << "exception in CreateBaseAuthenticatedPointer";
            CommonHAKCAnalysis::getWriter() << "Failed to create authenticated pointer for " << *this
                    << " in Function\n" << Manager.GetFunctionAnalysis().getFunction()
                    << "\n";
            throw std::exception();
        }
    }

    void ManagedHAKCPointer::CreatePointerReplacements() {
        bool CreateAuthenticatedCopies = GetAuthenticatedUserCount() > 0;
        bool CreateProtectedCopies = GetProtectedUserCount() > 0;

        if (DebugActive && !CreateAuthenticatedCopies) {
            CommonHAKCAnalysis::getWriter() << "No Authenticated Users of " << *this
                    << " so no authenticated clones will be created\n";
        }

        if (DebugActive && !CreateProtectedCopies) {
            CommonHAKCAnalysis::getWriter() << "No Protected Users of " << *this
                    << " so no protected clones will be created\n";
        }

        if (!CreateAuthenticatedCopies && !CreateProtectedCopies) {
            return;
        }

        SmallVector<ManagedHAKCPointerUseP> SortedUses;
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "\n\nCreating Clones of uses of " << *this << ":\n";
            auto AllUses = GetAllUses();
            SortedUses.append(AllUses.begin(), AllUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);
            for (auto &Use: SortedUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << *Use << "\n";
            }
            SortedUses.clear();
        }

        if (CreateAuthenticatedCopies) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Creating Authenticated Uses Copies\n";
            }
            SortedUses.append(AuthenticatedUses.begin(), AuthenticatedUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);

            for (auto &SortedUse: SortedUses) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating authenticated clone of " << *SortedUse << "\n";
                }
                auto *AuthenticatedUseClone = CreateAuthenticatedValue(SortedUse);
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "AuthenticatedUseClone: " << *AuthenticatedUseClone << "\n";
                }
            }

            SortedUses.clear();
        }

        if (CreateProtectedCopies) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Creating Protected Uses Copies\n";
            }
            SortedUses.append(ProtectedUses.begin(), ProtectedUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);

            for (auto &SortedUse: SortedUses) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Creating protected clone of " << *SortedUse << "\n";
                }
                auto *ProtectedUseClone = CreateProtectedValue(SortedUse);
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "ProtectedUseClone: " << *ProtectedUseClone << "\n";
                }
            }

            SortedUses.clear();
        }

        SortedUses.append(CloneUses.begin(), CloneUses.end());
        ManagedHAKCPointerUse::SortUses(SortedUses);

        for (auto &SortedUse: SortedUses) {
            if (BaseIsAuthenticatedPointer()) {
                if (CreateProtectedCopies) {
                    auto *ProtectedClone = CreateProtectedValue(SortedUse);
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Created Protected clone: " << ProtectedClone << "\n";
                    }
                }
                if (CreateAuthenticatedCopies) {
                    auto *AuthenticatedValue = CreateAuthenticatedValue(SortedUse);
                    if (!AuthenticatedValue) {
                        errs() << "exception in CreatePointerReplacements\n";
                        CommonHAKCAnalysis::getWriter() << "Could not find Authenticated Value for " << *SortedUse
                                << "\n";
                        throw std::exception();
                    }
                }
            } else {
                if (CreateAuthenticatedCopies) {
                    auto *AuthenticatedClone = CreateAuthenticatedValue(SortedUse);
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "Created Authenticated clone: " << AuthenticatedClone
                                << "\n";
                    }
                }
                if (CreateProtectedCopies) {
                    auto *ProtectedValue = CreateProtectedValue(SortedUse);
                    if (!ProtectedValue) {
                        errs() << "exception in CreatePointerReplacements\n";
                        CommonHAKCAnalysis::getWriter() << "Could not find Protected Value for " << *SortedUse << "\n";
                        throw std::exception();
                    }
                }
            }
        }
    }

    void ManagedHAKCPointer::CreatePointerUseClones() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "\n\n" << __FUNCTION__ << " called for " << *this << "\n";
        }

        CreatePointerReplacements();

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "\n" << Manager.GetFunctionAnalysis().getFunction()
                    << "\nAuthenticatedUses:\n";
            for (auto &UPtr: AuthenticatedUses) {
                auto *Replacement = Manager.FindAuthenticatedValue(UPtr->get());
                CommonHAKCAnalysis::getWriter() << *UPtr << ": ";
                if (Replacement) {
                    CommonHAKCAnalysis::getWriter() << Replacement;
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
            CommonHAKCAnalysis::getWriter() << "\n\nProtectedUses:\n";
            for (auto &UPtr: ProtectedUses) {
                auto *Replacement = Manager.FindAuthenticatedValue(UPtr->get());
                CommonHAKCAnalysis::getWriter() << *UPtr << ": ";
                if (Replacement) {
                    CommonHAKCAnalysis::getWriter() << Replacement;
                } else {
                    CommonHAKCAnalysis::getWriter() << "nullptr";
                }
                CommonHAKCAnalysis::getWriter() << "\n";
            }
        }
    }

    bool ManagedHAKCPointer::BaseDefinitionShouldBeTransferred() {
        if (!CommonHAKCAnalysis::IsCompartmentalizedFunction(&Manager.GetFunctionAnalysis().getFunction(),
                                                             Manager.GetPolicy())
            || ManuallyTransferred || PurposefullyIgnored) {
            return false;
        }

        if (auto *Call = dyn_cast<CallInst>(BaseDefinition)) {
            if (!Call->getCalledFunction()) {
                return true;
            }
            auto *Callee = Call->getCalledFunction();
            return Manager.GetFunctionAnalysis().GetModuleAnalysis().GetCommonAnalysis().IsAllocation(
                       BaseDefinition) ||
                   !CommonHAKCAnalysis::FunctionsAreInSameCompartment(&Manager.GetFunctionAnalysis().getFunction(),
                                                                      Callee, Manager.GetPolicy());
        } else if (BaseIsAuthenticatedPointer()) {
            return GetProtectedUserCount() > 0;
        }

        return false;
    }

    void ManagedHAKCPointer::TransformUses() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " called for " << *this << "\n";
        }

        TransformClones();

        if (GetAuthenticatedUserCount() > 0) {
            TransformUseSet(AuthenticatedUses);
        } else if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Not transforming Authenticated Pointer Replacements since user count "
                    "of " << *this << " is 0\n";
        }
        if (GetProtectedUserCount() > 0) {
            TransformUseSet(ProtectedUses);
        } else if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Not transforming Protected Pointer Replacements since user "
                    "count is 0\n";
        }
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Function after pointer transformation:\n"
                    << Manager.GetFunctionAnalysis().getFunction() << "\n";
        }
    }

    void ManagedHAKCPointer::SetUseOperand(User *U, Value *Replacement, const ManagedHAKCPointerUseP &PointerUse,
                                           bool IsAuthenticatedUse) {
        if (PurposefullyIgnored) {
            errs() << "exception in SetUseOperand\n";
            CommonHAKCAnalysis::getWriter() << "Trying to set operand of purposefully ignored pointer " << *this
                    << "\n";
            throw std::exception();
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Setting Operand " <<
                    std::to_string(PointerUse->getOperandNo()) << " of ";
            if (IsAuthenticatedUse) {
                CommonHAKCAnalysis::getWriter() << "Authenticated";
            } else {
                CommonHAKCAnalysis::getWriter() << "Protected";
            }
            CommonHAKCAnalysis::getWriter() << " User " << U << " to be " << Replacement << " in function "
                    << Manager.GetFunctionAnalysis().getFunction().getName() << " for "
                    << *this << "\n";
        }

        if (PointerUse->getUser()->getValueID() != U->getValueID()) {
            if (CommonHAKCAnalysis::IsMultiSSAUser(PointerUse->getUser())) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Not changing operand of MultiSSA User\n";
                }
                return;
            }
            errs() << "exception in SetUseOperand\n";
            CommonHAKCAnalysis::getWriter() << "Invalid PointerUse " << *PointerUse << " for User "
                    << *U << " of " << *this << " in function\n"
                    << Manager.GetFunctionAnalysis().getFunction() << "\n";
            throw std::exception();
        }

        U->setOperand(PointerUse->getOperandNo(), Replacement);
    }

    bool ManagedHAKCPointer::ValueIsManagedAndHasUsers(Value *V, bool CountAuthenticatedUsers) {
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

    bool ManagedHAKCPointer::UseIsManagedAndHasUsers(const ManagedHAKCPointerUseP &PointerUse,
                                                     bool CountAuthenticatedUsers) {
        return ValueIsManagedAndHasUsers(PointerUse->get(), CountAuthenticatedUsers);
    }

    void ManagedHAKCPointer::UpdateUserCounts() {
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << *this << " updating user counts of " << std::to_string(CloneUses.size())
                    << " uses\n";
        }
        for (auto &CloneUse: CloneUses) {
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
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Not transforming clones since " << *this
                        << " is purposefully ignored\n";
            }
            return;
        }

        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Transforming clones created for " << *this << "\n";
            for (auto &CloneUse: CloneUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << *CloneUse << "\n";
            }
        }

        SmallVector<ManagedHAKCPointerUseP> SortedUses(CloneUses.begin(), CloneUses.end());
        ManagedHAKCPointerUse::SortUses(SortedUses);
        for (const auto &CloneUse: SortedUses) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Handling Clone " << *CloneUse->getUser() << "\n";
            }
            if (GetAuthenticatedUserCount() > 0) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Handling Authenticated Use of " << *CloneUse << "\n";
                }
                auto *AuthenticatedVersion = Manager.FindAuthenticatedValue(CloneUse->getUser());
                if (!AuthenticatedVersion) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "No authenticated value created for " << *CloneUse << "\n";
                    }
                } else {
                    auto *AuthenticatedUser = dyn_cast<User>(AuthenticatedVersion);
                    auto *Replacement = Manager.FindAuthenticatedValue(CloneUse);
                    if (!Replacement) {
                        if (Manager.GetManagedPointer(CloneUse->get()) == nullptr ||
                            UseIsManagedAndHasUsers(CloneUse, true)) {
                            CommonHAKCAnalysis::getWriter() << "Unable to find Authenticated replacement of "
                                    << *CloneUse << "\n";
                            Manager.PrintAuthenticatedValues();
                            errs() << "\n" << Manager.GetFunctionAnalysis().getFunction()
                                    << "\n";
                            throw std::exception();
                        } else {
                            if (DebugActive) {
                                CommonHAKCAnalysis::getWriter() << *CloneUse
                                        << " does not need authenticated operand replaced\n";
                            }
                            continue;
                        }
                    }
                    if (!AuthenticatedUser) {
                        errs() << "AuthenticatedVersion is not a User: "
                                << AuthenticatedVersion << "\n"
                                << Manager.GetFunctionAnalysis().getFunction() << "\n";
                        throw std::exception();
                    }

                    SetUseOperand(AuthenticatedUser, Replacement, CloneUse, true);
                }
            }
            if (GetProtectedUserCount() > 0) {
                if (DebugActive) {
                    CommonHAKCAnalysis::getWriter() << "Handling Protected Use of " << *CloneUse << "\n";
                }
                auto *ProtectedVersion = Manager.FindProtectedValue(CloneUse->getUser());
                if (!ProtectedVersion) {
                    if (DebugActive) {
                        CommonHAKCAnalysis::getWriter() << "No protected value created for " << *CloneUse << "\n";
                    }
                    continue;
                }
                auto *ProtectedUser = dyn_cast<User>(ProtectedVersion);
                auto *Replacement = Manager.FindProtectedValue(CloneUse);
                if (!Replacement) {
                    if (Manager.GetManagedPointer(CloneUse->get()) == nullptr ||
                        UseIsManagedAndHasUsers(CloneUse, false)) {
                        errs() << "exception in TransformClones\n";
                        CommonHAKCAnalysis::getWriter() << "Unable to find Protected replacement of "
                                << *CloneUse << "\n";
                        Manager.PrintProtectedValues();
                        errs() << "\n" << Manager.GetFunctionAnalysis().getFunction() << "\n";
                        throw std::exception();
                    } else {
                        if (DebugActive) {
                            CommonHAKCAnalysis::getWriter() << *CloneUse
                                    << " does not need protected operand replaced\n";
                        }
                        continue;
                    }
                }
                if (!ProtectedUser) {
                    errs() << "ProtectedVersion is not a User: " << ProtectedVersion << "\n"
                            << Manager.GetFunctionAnalysis().getFunction() << "\n";
                    throw std::exception();
                }

                SetUseOperand(ProtectedUser, Replacement, CloneUse, false);
            }
        }
    }

    void
    ManagedHAKCPointer::TransformUseSet(std::set<ManagedHAKCPointerUseP> &UseSet) {
        if (PurposefullyIgnored) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Not transforming uses since " << *this
                        << " is purposefully ignored\n";
            }
            return;
        }

        bool UseAuthenticatedValue = (&UseSet == &AuthenticatedUses);
        StringRef ReplacementSource = UseAuthenticatedValue ? "Authenticated" : "Protected";

        SmallVector<ManagedHAKCPointerUseP> SortedUses(UseSet.begin(), UseSet.end());
        ManagedHAKCPointerUse::SortUses(SortedUses);
        if (DebugActive) {
            CommonHAKCAnalysis::getWriter() << "Replacing the following operands with "
                    << ReplacementSource << " values\n";
            for (auto &Use: SortedUses) {
                CommonHAKCAnalysis::getWriter() << "\t" << *Use << "\n";
            }
        }

        for (const auto &SortedUse: SortedUses) {
            Value *Replacement, *ReplacementUser;
            if (UseAuthenticatedValue) {
                Replacement = Manager.FindAuthenticatedValue(SortedUse);
                if (!CommonHAKCAnalysis::IsMultiSSAUser(SortedUse->getUser()) &&
                    ValueIsManagedAndHasUsers(SortedUse->getUser(), true)) {
                    ReplacementUser = SortedUse->getUser();
                } else {
                    ReplacementUser = Manager.FindAuthenticatedValue(SortedUse->getUser());
                }
            } else {
                Replacement = Manager.FindProtectedValue(SortedUse);
                if (!CommonHAKCAnalysis::IsMultiSSAUser(SortedUse->getUser()) &&
                    ValueIsManagedAndHasUsers(SortedUse->getUser(), false)) {
                    ReplacementUser = SortedUse->getUser();
                } else {
                    ReplacementUser = Manager.FindProtectedValue(SortedUse->getUser());
                }
            }

            if (!Replacement) {
                errs() << "exception in TransformUseSet\n";
                CommonHAKCAnalysis::getWriter() << "Unable to find " << ReplacementSource << " replacement of "
                        << *SortedUse << "\n" << Manager.GetFunctionAnalysis().getFunction()
                        << "\n";
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
                errs() << "Invalid ReplacementUser: " << *ReplacementUser << "\n";
                throw std::exception();
            }

            SetUseOperand(dyn_cast<User>(ReplacementUser), Replacement, SortedUse, UseAuthenticatedValue);
        }
    }

    Value *ManagedHAKCPointer::CreateAuthenticatedValue(const ManagedHAKCPointerUseP &HAKCUse) {
        if (PurposefullyIgnored) {
            return HAKCUse->get();
        }

        auto Authenticated = Manager.CreateAuthenticatedValue(HAKCUse);
        if (!Authenticated) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateAuthenticatedValue returned null for " << *HAKCUse << "\n";
            }
        } else {
            auto *Pointer = HAKCUse->get();
            SmallVector<ManagedHAKCPointerUseP> SortedUses(AuthenticatedUses.begin(), AuthenticatedUses.end());
            SortedUses.append(CloneUses.begin(), CloneUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);

            for (const auto &PointerUse: SortedUses) {
                if (PointerUse->get() == Pointer) {
                    Manager.AddAuthenticatedPointer(PointerUse, Authenticated);
                }
            }
        }

        return Authenticated;
    }

    Value *ManagedHAKCPointer::CreateProtectedValue(const ManagedHAKCPointerUseP &HAKCUse) {
        if (PurposefullyIgnored) {
            return HAKCUse->get();
        }

        auto Protected = Manager.CreateProtectedValue(HAKCUse);
        if (!Protected) {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "CreateProtectedValue returned null for " << *HAKCUse << "\n";
            }
        } else {
            if (DebugActive) {
                CommonHAKCAnalysis::getWriter() << "Found Protected " << Protected << "\n";
            }
            auto *Pointer = HAKCUse->get();
            SmallVector<ManagedHAKCPointerUseP> SortedUses(ProtectedUses.begin(), ProtectedUses.end());
            SortedUses.append(CloneUses.begin(), CloneUses.end());
            ManagedHAKCPointerUse::SortUses(SortedUses);
            for (const auto &PointerUse: SortedUses) {
                if (PointerUse->get() == Pointer) {
                    Manager.AddProtectedPointer(PointerUse, Protected);
                }
            }
        }

        return Protected;
    }

    unsigned ManagedHAKCPointer::GetAuthenticatedUserCount() {
        return AuthenticatedUses.size();
    }

    unsigned ManagedHAKCPointer::GetProtectedUserCount() {
        return ProtectedUses.size();
    }
} // hakc
