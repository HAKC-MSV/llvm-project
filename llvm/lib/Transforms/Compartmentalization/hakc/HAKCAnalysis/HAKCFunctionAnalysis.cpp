//
// Created by derrick on 8/20/21.
//
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace llvm::hakc {
    HAKCFunctionAnalysis::HAKCFunctionAnalysis(
        Function *F, HAKCModuleAnalysis &ModuleAnalysis,
        HAKCTransformer &Transformer, HAKCServerClientBase &Client)
        : ModuleAnalysis(ModuleAnalysis), Transformer(Transformer), Client(Client),
          PointerManager(
              *this, Client,
              ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().OutputDebugInfo(
                  F)),
          DebugActive(
              ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().OutputDebugInfo(
                  F)),
          DTree(*F), CurrentFunction(F), SetupHasRun(false),
          CompartmentTransferCount(0) {
    }

    HAKCLogger &HAKCFunctionAnalysis::getLogger(HAKCLogLevel log_level) const {
        return CommonHAKCAnalysis::getLogger(log_level, !DebugActive);
    }

    void HAKCFunctionAnalysis::UpdateHAKCFunctionParameters() const {
        if (CommonHAKCAnalysis::IsUncompartmentalizedSymbol(CurrentFunction,
                                                            Client)) { return; }

        HAKCFunctionAnalysis::getLogger(Debug)
                << "Updating parameters for the following HAKC functions:\n";
        for (auto *CallI: HAKCFunctionCalls) {
            HAKCFunctionAnalysis::getLogger(Verbose) << CallI << "\n";
        }

        auto *F = &GetFunction();
        auto *TransferTarget = F;
        if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
            auto TransferTargetName =
                    F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
            TransferTarget = F->getParent()->getFunction(TransferTargetName);
        }
        auto TargetCompartment =
                Client.GetDivision(TransferTarget).GetHAKCCompartment();

        for (auto *CallI: HAKCFunctionCalls) {
            auto HAKCTransferFunction =
                    GetModuleAnalysis().GetCommonAnalysis().GetHAKCTransferDefinition(
                        CallI->getCalledFunction());
            if (HAKCTransferFunction) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Updating HAKC call parameters for " << CallI << "\n";
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Updating index " << HAKCTransferFunction->GetCompartmentIdIdx()
                        << " ("
                        << CallI->getArgOperand(
                            HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue())
                        << ") to " << TargetCompartment.GetCompartmentIDValue() << "\n";

                UpdateHAKCFunctionParameters(CallI, TargetCompartment,
                                             HAKCTransferFunction);
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "After update call is " << CallI << "\n";
            } else {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "No HAKC Transfer function found for "
                        << CallI->getCalledFunction()->getName() << "\n";
            }
        }
    }

    /**
     * @brief Transfers a pointer argument back to its original color after an
     * indirect call returns
     * @param operand Indirect call argument
     * @return The call to the kernel resigning operation
     */
    Instruction *HAKCFunctionAnalysis::addCompartmentTransferCall(
        Value *Operand, const DebugLoc &DebugLoc, Instruction *I,
        ConstantInt *Size) {
        if (!Operand->getType()->isPointerTy() && !isa<PtrToIntInst>(Operand) &&
            !Operand->getType()->isIntegerTy(
                HAKCCompartment::CompartmentIDBitCount)) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Compartment transfer target " << *Operand
                    << " is not a pointer but of type " << *Operand->getType()
                    << " in function\n"
                    << GetFunction() << "\n";
            throw std::exception();
        }
        auto HAKCPointer = PointerManager.GetManagedPointer(Operand);
        if (!HAKCPointer) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find Managed Pointer for " << Operand << "\n";
            throw std::exception();
        }

        bool IsData = HAKCPointer->IsDataPointer();
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "isData: " << std::to_string(IsData) << " for " << Operand << "\n";

        Instruction *TransferCall;
        if (Size == nullptr) {
            TransferCall = getTransformer().CreateCompartmentTransfer(
                *HAKCPointer, I, &GetFunction(), IsData);
        } else {
            TransferCall = getTransformer().CreateSizedCompartmentTransfer(
                *HAKCPointer, I, &GetFunction(), IsData, Size);
        }

        TransferCall->setDebugLoc(DebugLoc);
        CompartmentTransferCount++;

        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Created transfer for ";
        if (!IsData) {
            HAKCFunctionAnalysis::getLogger(Verbose) << Operand->getName();
        } else { HAKCFunctionAnalysis::getLogger(Verbose) << Operand; }
        HAKCFunctionAnalysis::getLogger(Verbose)
                << ": " << TransferCall << "\n";

        return TransferCall;
    }

    /**
     * @brief Checks if a user is in the current function
     * @param user
     * @return True if the user is in the current function
     */
    bool HAKCFunctionAnalysis::userInFunction(Value *User) const {
        Function &F = GetFunction();
        if (auto *I = dyn_cast<Instruction>(User)) { return &F == I->getFunction(); }

        HAKCFunctionAnalysis::getLogger(Fatal) << "Unexpected user: " << User << "\n";
        throw std::exception();
    }

    /**
     * @brief Finds the dominating BasicBlock among users and ptr
     * @param ptr
     * @param users
     * @return
     */
    BasicBlock *HAKCFunctionAnalysis::findDominatorUseBlock(
        Value *Ptr, std::set<Instruction *> &Users) const {
        Function &F = GetFunction();
        BasicBlock *Dominator = nullptr;
        if (auto *I = dyn_cast<Instruction>(Ptr)) {
            if (!isa<AllocaInst>(Ptr)) { Dominator = I->getParent(); }
        }

        std::set<BasicBlock *> BasicBlocks;

        for (auto *User: Users) {
            if (!userInFunction(User)) { continue; }
            if (auto *PHI = dyn_cast<PHINode>(User)) {
                for (unsigned I = 0; I < PHI->getNumIncomingValues(); I++) {
                    auto *IncomingValue = PHI->getIncomingValue(I);
                    if (IncomingValue == Ptr) {
                        BasicBlocks.insert(PHI->getIncomingBlock(I));
                    }
                }
            } else { BasicBlocks.insert(User->getParent()); }
        }

        for (auto *BB: BasicBlocks) {
            if (!Dominator) { Dominator = BB; } else {
                Dominator = DTree
                        .findNearestCommonDominator(
                            Dominator->getFirstNonPHIOrDbgOrLifetime(),
                            BB->getFirstNonPHIOrDbgOrLifetime())
                        ->getParent();
            }
        }

        if (!Dominator) { Dominator = &F.getEntryBlock(); }

        return Dominator;
    }

    /**
     * @brief Finds an insertion point for new instructions.
     * @param v The Value for which we want to insert a new Instruction
     * @param users The users of v
     * @return The location at which to insert a new Instruction
     */
    Instruction *HAKCFunctionAnalysis::FindUseInsertionPoint(
        Value *V, std::set<Instruction *> &users) const {
        if (auto phi = dyn_cast<PHINode>(V)) {
            return phi->getParent()->getFirstNonPHIOrDbgOrLifetime();
        }

        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Finding insertion point for " << V << "\n";

        BasicBlock *DominatorBlock = findDominatorUseBlock(V, users);
        if (!DominatorBlock) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find block for " << V << "\n"
                    << GetFunction();
            throw std::exception();
        }

        for (Instruction &I: *DominatorBlock) {
            if (&I == V) { return I.getNextNonDebugInstruction(); } else if (
                !isa<PHINode>(&I) && users.contains(&I)) { return &I; }
        }

        return DominatorBlock->getTerminator();
    }

    /**
     * @brief Returns the current Function
     * @return
     */
    Function &HAKCFunctionAnalysis::GetFunction() const { return *CurrentFunction; }

    /**
     * @brief Adds a check of a signed pointer which checks for valid data access
     * @param signed_ptr The pointer to check
     * @param location The location at which to place the check
     * @return The result of the transfer
     */
    Value *HAKCFunctionAnalysis::AddDataAuthCheckAtLocation(Value *SignedPtr,
                                                            Instruction *location) {
        auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
        if (!HAKCPointer) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find Managed Pointer for " << SignedPtr << "\n";
            throw std::exception();
        }
        auto *bitcast =
                getTransformer().CreateDataAuthentication(*HAKCPointer, location);
        return bitcast;
    }

    Value *HAKCFunctionAnalysis::AddCodeAuthCheckAtLocation(Value *SignedPtr,
                                                            Instruction *Location) {
        auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
        if (!HAKCPointer) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find Managed Pointer for " << SignedPtr << "\n";
            throw std::exception();
        }
        auto *SafePointer =
                getTransformer().CreateCodeAuthentication(*HAKCPointer, Location);
        return SafePointer;
    }

    bool HAKCFunctionAnalysis::AddManagedPointer(Use &PointerUse) {
        if (!CommonHAKCAnalysis::IsPointerLikeType(PointerUse->getType())) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Trying to add an invalid ManagedHAKCPointer: " << PointerUse << "\n"
                    << GetFunction() << "\n";
            throw std::exception();
        }
        auto Result = PointerManager.ManagePointer(PointerUse);
        if (Result) {
            auto ManagedPointer = PointerManager.GetManagedPointer(PointerUse.get());
            if (!ManagedPointer) {
                HAKCFunctionAnalysis::getLogger(Fatal)
                        << "Could not find ManagedPointer for " << PointerUse << "\n";
                throw std::exception();
            }
        }
        return Result;
    }

    /**
     * @brief Creates all authenticated pointers, and clones any intermediate
     * pointer arithmetic between authentication and dereference
     */
    void HAKCFunctionAnalysis::createAllAuthenticatedPointers() {
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Function prior to making authenticated copies:\n"
                << GetFunction() << "\n";
        PointerManager.CreateAuthenticatedPointersAndAllClones();
    }

    /**
     * @brief Replace signed pointer dereferences with authenticated dereferences
     */
    void HAKCFunctionAnalysis::transformPointerDereferences() {
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Function prior to transforming pointer dereferences\n"
                << GetFunction() << "\n";
        PointerManager.TransformPointers();
    }

    Value *
    HAKCFunctionAnalysis::AddSafePointerCreationAtLocation(Value *SignedPtr,
                                                           Instruction *Location) {
        auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
        if (!HAKCPointer) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find Managed Pointer for " << SignedPtr << "\n";
            throw std::exception();
        }
        auto *SafePtr = getTransformer().CreateSafePointer(*HAKCPointer, Location);
        HAKCFunctionAnalysis::getLogger(Debug)
                << "Created Safe Pointer\n\t" << *SafePtr << "\nFor Signed Pointer\n\t"
                << *SignedPtr << "\nat\n"
                << *Location << "\n";
        return SafePtr;
    }

    /**
     * @brief Returns true if an argument should be authenticated
     * @param arg The function argument to check
     * @return
     */
    bool HAKCFunctionAnalysis::argNeedsAuthentication(Use &arg) {
        if (auto *call = dyn_cast<CallInst>(arg.getUser())) {
            if (auto *inlineAsm = dyn_cast<InlineAsm>(call->getCalledOperand())) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Arg " << *arg << " of " << *call << " is argument "
                        << arg.getOperandNo() << "\n";
                /* The RCU protected double-link list generates this assembly, and we want
                 * to store authenticated pointers. So ensure that authenticated pointers
                 * are the values getting stored.  See __list_add_rcu for an example.
                 * Perhaps a better way to handle this is to use Capstone to analyze the
                 * inline assembly string, and figure out the stored value in an
                 * architectural independent way. But that's way down the road. */
                if (inlineAsm->getAsmString() == "stlr $1, $0") {
                    if (arg.getOperandNo() == 1) { return false; }
                    /*else if (arg.getOperandNo() == 0) {
                               return true;
                           }*/
                }
            } else if (call->getCalledFunction()) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "arg.getOperandNo() = " << arg.getOperandNo() << "\n";
                return ((arg->getType()->isPointerTy() ||
                         isa<PtrToIntInst>(arg.get()))) &&
                       (GetModuleAnalysis().GetCommonAnalysis().IsSafeTransitionFunction(
                            call->getCalledFunction()) ||
                        IsIntrinsicNeedingAuthentication(call));
            }
        }
        return (!isa<Function>(arg) &&
                PointerManager.PointerIsEligibleForManagement(arg));
    }

    bool HAKCFunctionAnalysis::IsCallInIntrinsicSet(
        CallBase *Call, ArrayRef<Intrinsic::ID> IDs) const {
        bool result = false;
        if (auto *intrinsic = dyn_cast<IntrinsicInst>(Call)) {
            auto IDToFind = intrinsic->getIntrinsicID();
            auto Search = [IDToFind](Intrinsic::ID ID) { return IDToFind == ID; };

            result = llvm::any_of(IDs, Search);
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Intrinsic (" << IDToFind << ") from "
                    << Call->getFunction()->getName() << " " << intrinsic;
            if (result) {
                HAKCFunctionAnalysis::getLogger(Verbose) << " is in { ";
            } else { HAKCFunctionAnalysis::getLogger(Verbose) << " is not in { "; }
            for (auto id: IDs) {
                HAKCFunctionAnalysis::getLogger(Verbose) << id << " ";
            }
            HAKCFunctionAnalysis::getLogger(Verbose) << "}\n";
        }
        return result;
    }

    bool HAKCFunctionAnalysis::IsIntrinsicNeedingAuthentication(
        CallBase *Call) const {
        Intrinsic::ID IntrinsicsNeedingAuth[] = {
            Intrinsic::IndependentIntrinsics::memcpy,
            Intrinsic::IndependentIntrinsics::memmove,
            Intrinsic::IndependentIntrinsics::memset
        };

        return IsCallInIntrinsicSet(Call, IntrinsicsNeedingAuth);
    }

    bool HAKCFunctionAnalysis::IsIntrinsicNeedingCloning(CallBase *Call) const {
        Intrinsic::ID IntrinsicsNeedingCloning[] = {
            Intrinsic::IndependentIntrinsics::lifetime_start,
            Intrinsic::IndependentIntrinsics::lifetime_end,
        };
        return IsCallInIntrinsicSet(Call, IntrinsicsNeedingCloning);
    }

    bool HAKCFunctionAnalysis::IsIntrinsicToSkip(CallBase *Call) const {
        Intrinsic::ID IntrinsicsToSkip[] = {
            Intrinsic::IndependentIntrinsics::dbg_declare,
            /*Intrinsic::IndependentIntrinsics::dbg_addr,*/
            Intrinsic::IndependentIntrinsics::dbg_label,
            Intrinsic::IndependentIntrinsics::dbg_value,
            Intrinsic::IndependentIntrinsics::read_register,
        };
        return IsCallInIntrinsicSet(Call, IntrinsicsToSkip);
    }

    /**
     * @brief Returns true if the PHINode uses the specified target
     * @param phiNode
     * @param target
     * @return
     */
    bool HAKCFunctionAnalysis::phiNodeUsesValue(PHINode *PhiNode, Value *target,
                                                std::set<PHINode *> &visited) {
        visited.insert(PhiNode);
        for (auto &Val: PhiNode->incoming_values()) {
            Value *def = getDef(Val.get(), true);
            if (Val.get() == target || def == target) { return true; } else if (auto *
                    phi = dyn_cast<PHINode>(def)) {
                if (visited.contains(phi)) { continue; }
                if (phiNodeUsesValue(phi, target, visited)) { return true; }
            }
        }
        return false;
    }

    /**
     * @brief Perform analysis of an Instruction
     * @param I
     */
    void HAKCFunctionAnalysis::HandleInstruction(Instruction *I) {
        // TODO: put permissions analysis here
        if (auto *call = dyn_cast<CallInst>(I)) { handleCall(call); } else if (auto *
                load = dyn_cast<LoadInst>(I)) { handleLoad(load); } else if (auto *store =
                dyn_cast<StoreInst>(I)) { handleStore(store); } else if (auto *compare =
                dyn_cast<CmpInst>(I)) { handleComparison(compare); } else if (auto *binOp
                = dyn_cast<BinaryOperator>(I)) { handleBinaryOperator(binOp); }
    }

    /**
     * @brief Retrieves the Instruction of a User
     * @param user
     * @return
     */
    Instruction *HAKCFunctionAnalysis::getUserInst(User *user) {
        if (auto *inst = dyn_cast<Instruction>(user)) { return inst; } else if (
            isa<BitCastOperator>(user) || isa<GEPOperator>(user)) {
            return getUserInst(*user->user_begin());
        } else {
            HAKCFunctionAnalysis::getLogger(Fatal) << "Unexpected user: " << user <<
                    "\n";
            throw std::exception();
        }
    }

    bool HAKCFunctionAnalysis::IsPHIOfGlobalsOnly(Value *V) {
        std::set<PHINode *> nodes;
        return isPHIofGlobalsOnly(V, nodes);
    }

    /**
     * @brief Returns true of ptr is a PHINode consisting only of global variables
     * @param ptr
     * @param nodes
     * @return
     */
    bool HAKCFunctionAnalysis::isPHIofGlobalsOnly(Value *ptr,
                                                  std::set<PHINode *> &nodes) {
        if (auto *phiNode = dyn_cast<PHINode>(ptr)) {
            if (nodes.contains(phiNode)) { return true; }
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Examining PHI Node " << phiNode << " for Globals (" << nodes.size()
                    << ")\n";
            nodes.insert(phiNode);
            for (auto &val: phiNode->incoming_values()) {
                Value *def = getDef(val.get(), false);
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "\tPHI Node value: " << val << "\n\t\tDef: " << def << "\n";
                if (!isa<GlobalValue>(def)) {
                    if (isa<PHINode>(def)) {
                        if (isPHIofGlobalsOnly(def, nodes)) { continue; }
                    }
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    Instruction *HAKCFunctionAnalysis::GetFinalAllocaDef(AllocaInst *Alloca) {
        return Alloca;
    }

    Value *HAKCFunctionAnalysis::getDef(Value *V, bool followLoad) const {
        auto *def = GetModuleAnalysis().GetCommonAnalysis().getDef(V, followLoad);
        if (!def) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find definition for " << V << "\n";
            throw std::exception();
        }
        return def;
    }

    /**
     * @brief Process a LoadInst for analysis
     * @param load
     */
    void HAKCFunctionAnalysis::handleLoad(LoadInst *load) {
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Handling " << *load->getOperandUse(LoadInst::getPointerOperandIndex())
                << " from Load " << *load << "\n";
        AddManagedPointer(load->getOperandUse(LoadInst::getPointerOperandIndex()));
    }

    /**
     * @brief Process a StoreInst for analysis
     * @param Store
     */
    void HAKCFunctionAnalysis::handleStore(StoreInst *Store) {
        AddManagedPointer(Store->getOperandUse(StoreInst::getPointerOperandIndex()));

        if (auto *GlobValue = dyn_cast<GlobalValue>(Store->getValueOperand())) {
            if (globalShouldBeTransferred(Store->getOperandUse(0))) {
                GlobalArgumentUses[GlobValue].insert(Store);
            }
        }
    }

    void HAKCFunctionAnalysis::MaybeAddCompareToDirectUsers(CmpInst *CmpI) {
        CheckCompareOperandForDirectFunctionUse(CmpI, 0);
        CheckCompareOperandForDirectFunctionUse(CmpI, 1);
    }

    void HAKCFunctionAnalysis::CheckCompareOperandForDirectFunctionUse(
        CmpInst *CmpI, unsigned OpNo) {
        auto *Op = getDef(CmpI->getOperand(OpNo), false);
        if (auto *func = dyn_cast<Function>(Op)) {
            if (GetModuleAnalysis()
                .GetCommonAnalysis()
                .ValueShouldBeReplacedWithTransfer(func, Client)) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Adding comparison to directFunctionUsers for argument "
                        << std::to_string(OpNo) << "\n";
                directFunctionUsers.insert(CmpI);
            }
        }
    }

    /**
     * @brief Ensures that authenticated pointers are used in comparisons for
     * correctness
     * @param compare
     */
    void HAKCFunctionAnalysis::handleComparison(CmpInst *compare) {
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Checking comparison " << *compare << "\n";

        MaybeAddCompareToDirectUsers(compare);

        if (isa<ConstantPointerNull>(compare->getOperand(0)) ||
            isa<ConstantPointerNull>(compare->getOperand(1))) {
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "\tComparisons with null do not need authentication\n";
            return;
        } else if (isa<Operator>(compare->getOperand(0)) ||
                   isa<Operator>(compare->getOperand(1))) {
            bool comparisonIsWithConstant = false;
            auto *bitCastOperator0 = dyn_cast<Operator>(compare->getOperand(0));
            if (bitCastOperator0) {
                if (auto *ci = dyn_cast<ConstantInt>(bitCastOperator0->getOperand(0))) {
                    comparisonIsWithConstant = (ci->getZExtValue() < user_space_end);
                }
            }
            if (!comparisonIsWithConstant) {
                auto *bitCastOperator1 = dyn_cast<Operator>(compare->getOperand(1));
                if (bitCastOperator1) {
                    if (auto *ci = dyn_cast<ConstantInt>(bitCastOperator1->getOperand(0))) {
                        comparisonIsWithConstant = (ci->getZExtValue() < user_space_end);
                    }
                }
            }

            if (comparisonIsWithConstant) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "\tComparisons with constant integers do not need "
                        "authentications\n";
                return;
            }
        }

        if (isCompartmentalizedFunction()) {
            bool arg0NeedsAuth =
                    argNeedsAuthentication(compare->getOperandUse(0)) &&
                    !isa<GlobalValue>(getDef(compare->getOperand(0), false));
            bool arg1NeedsAuth =
                    argNeedsAuthentication(compare->getOperandUse(1)) &&
                    !isa<GlobalValue>(getDef(compare->getOperand(1), false));
            if (DebugActive) {
                if (arg0NeedsAuth) {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Argument 0 needs auth\n";
                } else {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Argument 0 does not need auth\n";
                }
                if (arg1NeedsAuth) {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Argument 1 needs auth\n";
                } else {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Argument 1 does not need auth\n";
                }
            }
            if (arg0NeedsAuth && arg1NeedsAuth) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Both operands should be checked\n";
                AddManagedPointer(compare->getOperandUse(0));
                AddManagedPointer(compare->getOperandUse(1));
            } else {
                if (arg0NeedsAuth) {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Registering argument 0\n";
                    AddManagedPointer(compare->getOperandUse(0));
                } else {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Argument 1 (" << compare->getOperand(1)
                            << " ) already authenticated\n";
                }
                if (arg1NeedsAuth) {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Registering argument 1\n";
                    AddManagedPointer(compare->getOperandUse(1));
                }
            }
        } else {
            if (argNeedsAuthentication(compare->getOperandUse(0))) {
                AddManagedPointer(compare->getOperandUse(0));
            }
            if (argNeedsAuthentication(compare->getOperandUse(1))) {
                AddManagedPointer(compare->getOperandUse(1));
            }
        }
    }

    /**
     * @brief BinaryOperators (like bitwise OR) should use authenticated values
     * @param binOp
     */
    void HAKCFunctionAnalysis::handleBinaryOperator(BinaryOperator *binOp) {
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Checking binary op " << binOp << "\n";
        /* Both operators need to be pointers to skip operations like
         * ptr | 0xFFFF
         */
        if (argNeedsAuthentication(binOp->getOperandUse(0)) &&
            argNeedsAuthentication(binOp->getOperandUse(1))) {
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Registering both operands\n";
            AddManagedPointer(binOp->getOperandUse(0));
            AddManagedPointer(binOp->getOperandUse(1));
        }
    }

    /**
     * @brief Returns true if a GlobalValue should be transferred
     * @param globalValue
     * @return
     */
    bool HAKCFunctionAnalysis::globalShouldBeTransferred(
        Use &globalValueArg) const {
        /* Don't transfer to printk */
        if (auto *globalValue =
                dyn_cast<GlobalValue>(getDef(globalValueArg.get(), false))) {
            /* Don't transfer THIS_MODULE */
            if (globalValue->getName() == "__this_module") { return false; }

            /* Ignore constant string arrays */
            if (globalValue->getValueType()->isArrayTy() &&
                globalValue->getValueType()->getArrayElementType()->isIntegerTy(8)) {
                return false;
            }

            if (auto *call = dyn_cast<CallInst>(globalValueArg.getUser())) {
                if (!GetModuleAnalysis().GetCommonAnalysis().FunctionIsAnalysisCandidate(
                    call->getCalledFunction())) { return false; }
                return true;
            }

            return globalValue->getValueType()->isPointerTy();
        }

        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Arg " << globalValueArg.getOperandNo() << " (" << globalValueArg
                << " ) is not a GlobalValue\n";
        return false;
    }

    bool HAKCFunctionAnalysis::isCompartmentalizedFunction() const {
        return CommonHAKCAnalysis::IsCompartmentalizedFunction(CurrentFunction,
                                                               Client);
    }

    /**
     * @brief Processes a function call for analysis
     * @param call
     */
    void HAKCFunctionAnalysis::handleCall(CallInst *call) {
        if (call->getCalledFunction() && IsIntrinsicToSkip(call)) { return; }

        if (GetModuleAnalysis().GetCommonAnalysis().IsHAKCFunction(
            call->getCalledFunction())) { HAKCFunctionCalls.insert(call); }

        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Handling call " << *call << "\n";

        if (GetModuleAnalysis().GetCommonAnalysis().ValueIsUsedAsPointer(call)) {
            for (auto &U: call->uses()) { if (AddManagedPointer(U)) { break; } }
        }

        bool needsAuthenticatedArgs =
        (call->isInlineAsm() ||
         (GetModuleAnalysis().FunctionIsInAnalysisSet(
              call->getCalledFunction()) &&
          !CommonHAKCAnalysis::IsOutsideTransferFunc(call->getCalledFunction())));

        if (isa<IntrinsicInst>(call)) {
            needsAuthenticatedArgs = IsIntrinsicNeedingAuthentication(call);
        }

        if (DebugActive) {
            if (needsAuthenticatedArgs) {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << *call << " needs authenticated args\n";
            } else {
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << *call << " does not need authenticated args\n";
            }
        }

        if (call->isIndirectCall()) {
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Indirect call: " << *call << "\n";
            AddManagedPointer(call->getCalledOperandUse());
            /* Using checked pointers for indirect calls because the indirect call
             * can be an assembly function, which currently requires valid pointers.
             * This is safe for other functions, since the target will be a transfer
             * function, and will perform the protecting before entering
             * compartmentalized code, or again create a valid pointer for
             * uncompartmentalized code */
            for (auto &arg: call->args()) {
                if (argNeedsAuthentication(arg)) { AddManagedPointer(arg); }
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Argument " << *arg << " for " << *call
                        << " does not need authentication\n";
            }
        } else if (needsAuthenticatedArgs) {
            for (auto &arg: call->args()) {
                if (argNeedsAuthentication(arg)) { AddManagedPointer(arg); }
                HAKCFunctionAnalysis::getLogger(Verbose)
                        << "Argument " << *arg << " for " << *call
                        << " does not need authentication\n";
            }
        } else if (!GetModuleAnalysis().GetCommonAnalysis().IsSafeTransitionCall(
            call)) {
            if (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic()) {
                /* Intrinsics that don't need authenticated args are basically bitwise
                 * shifts and other minor things, so ignore them
                 */
                return;
            }
            for (auto &arg: call->args()) {
                Value *def = getDef(arg.get(), false);
                if (auto *glob = dyn_cast<GlobalValue>(def)) {
                    if (globalShouldBeTransferred(arg)) {
                        HAKCFunctionAnalysis::getLogger(Verbose)
                                << "Global " << glob->getName() << " used by " << *call << "\n";
                        GlobalArgumentUses[glob].insert(call);
                        AddManagedPointer(arg);
                    }
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Global " << glob->getName() << " should not be transferred to "
                            << *call << "\n";
                } else if (auto *phiNode = dyn_cast<PHINode>(def)) {
                    for (auto &val: phiNode->incoming_values()) {
                        Value *valDef = getDef(val.get(), false);
                        if (auto *globVal = dyn_cast<GlobalValue>(valDef)) {
                            if (globalShouldBeTransferred(val)) {
                                HAKCFunctionAnalysis::getLogger(Verbose)
                                        << "Global " << globVal->getName() << " used by " << *call
                                        << "\n";
                                GlobalArgumentUses[globVal].insert(call);
                            }
                            HAKCFunctionAnalysis::getLogger(Verbose)
                                    << "Global " << globVal->getName()
                                    << " should not be transferred to " << *call << "\n";
                        }
                    }
                } else if (isa<AllocaInst>(def)) {
                    if (!GetModuleAnalysis()
                        .GetCommonAnalysis()
                        .FunctionIsAnalysisCandidate(call->getCalledFunction())) {
                        HAKCFunctionAnalysis::getLogger(Verbose)
                                << "Function called by " << *call
                                << " is not an analysis candidate\n";
                        continue;
                    }
                }
            }
            if (call->getCalledFunction()) {
                auto TargetCompartment =
                        Client.GetDivision(call->getCalledFunction()).GetHAKCCompartment();
                if (!CommonHAKCAnalysis::IsUncompartmentalizedSymbol(
                    call->getCalledFunction(), Client)) {
                    NonKernelDirectFunctionCallSet.insert(call);
                }
            }
        }
    }

    /**
     * @brief Sets the function section to the correct PMC ELF section
     */
    void HAKCFunctionAnalysis::relocateFunctionSection() {
        if (isCompartmentalizedFunction()) {
            GetFunction().setSection(getHAKCFunctionSectionName());
        }
    }

    std::string HAKCFunctionAnalysis::getHAKCFunctionSectionName() {
        std::string sectionName = HAKC_SECTION_PREFIX.str();
        auto Compartment = Client.GetDivision(&GetFunction()).GetHAKCCompartment();
        sectionName += std::to_string(Compartment.GetCompartmentIDValue());
        if (GetFunction().getSection().empty()) { sectionName += ".text"; } else {
            sectionName += GetFunction().getSection().str();
        }
        return sectionName;
    }

    HAKCTypeIdentifier &HAKCFunctionAnalysis::GetTypeIdentifier() const {
        return ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().GetTypeIdentifier();
    }

    void HAKCFunctionAnalysis::setup() {
        if (!SetupHasRun) {
            auto Compartment = Client.GetDivision(CurrentFunction).GetHAKCCompartment();
            HAKCFunctionAnalysis::getLogger(Debug)
                    << "Running setup for " << GetFunction().getName() << "\n"
                    << GetFunction() << "\nCompartmentID = "
                    << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
            PointerManager.SetFunctionIsCompartmentalized(
                !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(CurrentFunction,
                                                                 Client));
            for (auto it = inst_begin(CurrentFunction); it != inst_end(CurrentFunction);
                 ++it) {
                Instruction *inst = &*it;
                HandleInstruction(inst);
            }
            SetupHasRun = true;
        }
        HAKCFunctionAnalysis::getLogger(Debug)
                << "setup has run for " << GetFunction().getName() << "\n";
    }

    bool HAKCFunctionAnalysis::modifiedFunction() const {
        return !(PointerManager.empty() && GlobalArgumentUses.empty() &&
                 NonKernelDirectFunctionCallSet.empty() &&
                 PointerManager.GetTotalAdditions() == 0 &&
                 CompartmentTransferCount == 0);
    }

    void HAKCFunctionAnalysis::
    CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls() {
        auto CurrentDivision = Client.GetDivision(&GetFunction());
        Client.GetValidTargets(CurrentDivision.GetHAKCCompartment());
        for (auto *call: NonKernelDirectFunctionCallSet) {
            auto TargetCompartment =
                    Client.GetDivision(call->getCalledFunction()).GetHAKCCompartment();
            if (CurrentDivision.GetHAKCCompartment().GetCompartmentID() ==
                TargetCompartment.GetCompartmentID()) {
                /* Aliases are being used for transfer functions, so if the
                 * called function is in the same compartment use the transformed function
                 * name. Otherwise do not change the function name, because the
                 * transfer function will be used through the alias.
                 */
                auto TransformedName = CommonHAKCAnalysis::getOriginalTransformedName(
                    call->getCalledFunction());
                auto TransformedFunction = GetModuleAnalysis().GetFunctionByName(
                    TransformedName, call->getCalledFunction()->getFunctionType());
                call->setCalledFunction(TransformedFunction);
            } else {
                // Fixing https://github.mit.edu/inherently-secure/ARM-MTE/issues/40
                bool ValidTransition = false;

                for (auto *Target:
                     CurrentDivision.GetHAKCCompartment().GetValidTargets()) {
                    HAKCFunctionAnalysis::getLogger(Verbose)
                            << "Testing Target Compartment " << Target->getZExtValue() << " == "
                            << TargetCompartment.GetCompartmentID()->getSExtValue()
                            << " -> "
                            << (Target->getSExtValue() ==
                                TargetCompartment.GetCompartmentID()->getSExtValue())
                            << "\n";
                    // comparing i32 1 and i64 1 returns false (LLVM constant ints), so cast
                    // to int64_t
                    if (Target->getZExtValue() ==
                        TargetCompartment.GetCompartmentID()->getZExtValue()) {
                        ValidTransition = true;
                        break;
                    }
                }

                if (!ValidTransition) {
                    HAKCFunctionAnalysis::getLogger(Error)
                            << "A direct Compartment transition from "
                            << std::to_string(
                                CurrentDivision.GetHAKCCompartment().GetCompartmentIDValue())
                            << " to "
                            << std::to_string(TargetCompartment.GetCompartmentIDValue())
                            << " is statically possible but not allowed in the"
                            << " Compartmentalization Client\n"
                            << "A call from " << call->getFunction()->getName() << " to "
                            << call->getCalledFunction()->getName() << " is not allowed\n";
                    throw std::exception();
                }

                if (call->getCalledFunction()->isVarArg()) {
                    auto *VariadicTransfer =
                            getTransformer().CreateTransferToVariadic(call, &PointerManager);
                    call->setCalledFunction(VariadicTransfer);
                }
            }
        }
    }

    HAKCTransformer &HAKCFunctionAnalysis::getTransformer() const {
        return Transformer;
    }

    HAKCModuleAnalysis &HAKCFunctionAnalysis::GetModuleAnalysis() const {
        return ModuleAnalysis;
    }

    void HAKCFunctionAnalysis::AddInstrumentation(bool RelocateSection) {
        if (CommonHAKCAnalysis::IsOutsideTransferFunc(&GetFunction())) {
            throw std::exception();
        }

        if (!SetupHasRun) {
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << __FUNCTION__ << " calling setup for " << GetFunction().getName()
                    << "\n";
            setup();
        }
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "setup() has run for " << GetFunction().getName() << "\n";

        HAKCFunctionAnalysis::getLogger(Verbose) << "Managed Pointers:\n";

        for (auto &HAKCPointer: PointerManager.ManagedPointers()) {
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << *HAKCPointer << "\n+++\n";
            HAKCPointer->DetermineIfBasePointerIsAuthenticated();
        }

        if (modifiedFunction()) {
            if (RelocateSection) { relocateFunctionSection(); }
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "---- createMissingTransfers ----\n";
            createMissingTransfers();
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "----- UpdateHAKCFunctionParameters ------\n";
            UpdateHAKCFunctionParameters();
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "---- createAllAuthenticatedPointers ----\n";
            createAllAuthenticatedPointers();
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "----- transformPointerDereferences ------\n";
            transformPointerDereferences();
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "- ReplaceDirectFunctionUsesWithTransfers -\n";
            ReplaceDirectFunctionUsesWithTransfers();
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "------ "
                    "CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls "
                    "-----\n";
            CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";

            HAKCFunctionAnalysis::getLogger(Verbose)
                    << GetFunction() << "\n";

            CommonHAKCAnalysis::VerifyFunction(&GetFunction());
        } else {
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Function " << GetFunction().getName() << " unmodified\n";
        }
    }

    Instruction *HAKCFunctionAnalysis::CreateMissingTransfer(
        Instruction *PointerNeedingTransfer) {
        std::set<Instruction *> UserInstructions;
        for (auto *U: PointerNeedingTransfer->users()) {
            if (auto *I = dyn_cast<Instruction>(U)) { UserInstructions.insert(I); }
        }
        auto *InsertionPoint =
                FindUseInsertionPoint(PointerNeedingTransfer, UserInstructions);

        ConstantInt *Size = nullptr;
        if (auto *Call = dyn_cast<CallInst>(PointerNeedingTransfer)) {
            if (GetModuleAnalysis().GetCommonAnalysis().IsAllocationFunction(
                Call->getCalledFunction())) {
                auto AllocationDef =
                        GetModuleAnalysis().GetCommonAnalysis().GetAllocationDefinition(
                            Call->getCalledFunction());
                Size = AllocationDef->GetSize(Call);
            }
        }
        return addCompartmentTransferCall(PointerNeedingTransfer,
                                          PointerNeedingTransfer->getDebugLoc(),
                                          InsertionPoint, Size);
    }

    Instruction *
    HAKCFunctionAnalysis::SignGlobalPointerWithColor(GlobalValue *GlobalVar) {
        std::set<Instruction *> UserInstructions;
        for (auto *U: GlobalVar->users()) {
            if (auto *I = dyn_cast<Instruction>(U)) {
                if (I->getFunction() == &GetFunction()) { UserInstructions.insert(I); }
            }
        }

        auto HAKCPointer = PointerManager.GetManagedPointer(GlobalVar);
        if (!HAKCPointer) {
            HAKCFunctionAnalysis::getLogger(Fatal)
                    << "Could not find Managed Pointer for " << GlobalVar << "\n";
            throw std::exception();
        }
        auto *InsertionPoint = FindUseInsertionPoint(GlobalVar, UserInstructions);
        return getTransformer().CreateSignWithDivision(
            *HAKCPointer, InsertionPoint, &GetFunction(), !isa<Function>(GlobalVar));
    }

    void HAKCFunctionAnalysis::createMissingTransfers() {
        if (CommonHAKCAnalysis::IsUncompartmentalizedSymbol(CurrentFunction,
                                                            Client)) { return; }
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Function prior to making transfers:\n"
                << GetFunction() << "\n";
        PointerManager.CreateAllTransfers();
    }

    void HAKCFunctionAnalysis::ReplaceInstructionOperand(Instruction *I,
                                                         unsigned ArgNo,
                                                         Value *OldValue,
                                                         Value *NewValue) {
        auto *V = I->getOperand(ArgNo);
        Value *Replacement;
        if (auto *Oper = dyn_cast<BitCastOperator>(V)) {
            auto HAKCPointer = PointerManager.GetManagedPointer(NewValue);
            if (!HAKCPointer) {
                HAKCFunctionAnalysis::getLogger(Fatal)
                        << "Could not find Managed Pointer for " << NewValue << "\n";
                throw std::exception();
            }
            Replacement =
                    getTransformer().CreateBitCast(*HAKCPointer, Oper->getDestTy(), I);
        } else if (V == OldValue) { Replacement = NewValue; } else {
            HAKCFunctionAnalysis::getLogger(Verbose) << "Could not find ";
            if (auto *F = dyn_cast<Function>(OldValue)) {
                HAKCFunctionAnalysis::getLogger(Verbose) << F->getName();
            } else { HAKCFunctionAnalysis::getLogger(Verbose) << OldValue << "\n"; }
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << " in " << *I << "\n";
            throw std::exception();
        }
        I->setOperand(ArgNo, Replacement);
    }

    void HAKCFunctionAnalysis::CheckAndReplaceArgument(Value *V, Instruction *I,
                                                       unsigned int ArgNo) {
        if (auto *Func = dyn_cast<Function>(V)) {
            auto name =
                    GetModuleAnalysis().GetCommonAnalysis().GetOutsideTransferName(Func);
            auto transfer =
                    GetModuleAnalysis().GetFunctionByName(name, Func->getFunctionType());
            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Changing operand " << std::to_string(ArgNo) << " to " << name
                    << " for\n\t" << *I << "\n";
            transfer->setLinkage(Func->getLinkage());
            transfer->copyAttributesFrom(Func);
            ReplaceInstructionOperand(I, ArgNo, V, transfer);
        }
    }

    void HAKCFunctionAnalysis::ReplaceDirectFunctionUsesWithTransfers() {
        for (auto *I: directFunctionUsers) {
            for (unsigned i = 0; i < I->getNumOperands(); i++) {
                if (isa<CallInst>(I)) {
                    auto *call = dyn_cast<CallInst>(I);
                    if (call->getCalledOperandUse().getOperandNo() == i)
                        /* Don't change actual function call, only the arguments */
                        continue;
                }
                auto *Op = getDef(I->getOperand(i), false);
                if (isa<Function>(Op)) { CheckAndReplaceArgument(Op, I, i); } else if (
                    auto *selectInst = dyn_cast<SelectInst>(Op)) {
                    auto *TrueValue = getDef(selectInst->getTrueValue(), false);
                    CheckAndReplaceArgument(TrueValue, I, i);
                    auto *FalseValue = getDef(selectInst->getFalseValue(), false);
                    CheckAndReplaceArgument(FalseValue, I, i);
                }
            }
        }
    }

    void HAKCFunctionAnalysis::InstrumentCode() {
        AddInstrumentation(
            !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(&GetFunction(), Client));
    }

    void HAKCFunctionAnalysis::UpdateHAKCFunctionParameters(
        CallInst *CallI, const HAKCCompartment &TargetCompartment,
        const hakc::function_def_t &HAKCTransferFunction) const {
        HAKCFunctionAnalysis::getLogger(Verbose)
                << "Setting "
                << *CallI->getArgOperand(
                    HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue())
                << " to be " << *TargetCompartment.GetCompartmentID() << "\n";
        CallI->setOperand(HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue(),
                          TargetCompartment.GetCompartmentID());

        if (HAKCTransferFunction->GetDivisionIdIdx() != nullptr) {
            auto *F = CallI->getFunction();
            HAKCCompartmentDivision Division;
            if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
                auto *TransferTarget =
                        CommonHAKCAnalysis::GetOriginalFunctionFromTransferFunction(F);
                Division = Client.GetDivision(TransferTarget);
            } else { Division = Client.GetDivision(F); }

            HAKCFunctionAnalysis::getLogger(Verbose)
                    << "Setting argument "
                    << HAKCTransferFunction->GetDivisionIdIdx()->getZExtValue() << " to be "
                    << Division << "\n";
            CallI->setOperand(HAKCTransferFunction->GetDivisionIdIdx()->getZExtValue(),
                              Division.GetDivisionID());
        }
    }
} // namespace llvm::hakc
