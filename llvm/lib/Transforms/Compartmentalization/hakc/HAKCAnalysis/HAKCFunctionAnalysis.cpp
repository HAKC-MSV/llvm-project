//
// Created by derrick on 8/20/21.
//
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCPointerManager.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace llvm::hakc {

HAKCFunctionAnalysis::HAKCFunctionAnalysis(Function *F,
                                           HAKCModuleAnalysis &ModuleAnalysis)
    : ModuleAnalysis(ModuleAnalysis), _PointerManager(F, ModuleAnalysis),
      // DebugActive(ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().OutputDebugInfo(F)),
      DebugActive(true), CurrentFunction(F), CompartmentTransferCount(0) {
  HAKCFunctionAnalysis::setup();
}

void HAKCFunctionAnalysis::setup() {
  getLogger(Debug) << "Running Function Analysis setup for "
                   << CurrentFunction->getName() << "\n"
                   << *CurrentFunction << "\n";
  // during analysis consider all functions to be uncompartmentalized so
  // everything is analyzed
  for (auto it = inst_begin(CurrentFunction); it != inst_end(CurrentFunction);
       ++it) {
    Instruction *inst = &*it;
    HandleInstruction(inst);
  }
  getLogger(Debug) << "Function Analysis setup has run for "
                   << CurrentFunction->getName() << "\n";
}

void HAKCFunctionEnforcement::setup() {
  getLogger(Verbose) << "Running Function Enforcement setup for "
                     << CurrentFunction->getName() << "\n";
  if (CommonHAKCAnalysis::IsOutsideTransferFunc(CurrentFunction)) {
    getLogger(Fatal) << "Exception - Function " << CurrentFunction->getName()
                     << " is outside transfer function!\n";
    throw std::exception();
  }
  PointerManager.SetFunctionIsCompartmentalized(!IsUncompartmentalizedSymbol());
  getLogger(Debug) << "Function now has FunctionIsCompartmentalized set to "
                   << PointerManager.FunctionIsCompartmentalized() << "\n";
  AddInstrumentation();
  getLogger(Verbose) << "Function Enforcement setup has run for "
                     << CurrentFunction->getName() << "\n";
}

/**
 * @brief Checks if a user is in the current function
 * @param User
 * @return True if the user is in the current function
 */
bool HAKCFunctionEnforcement::userInFunction(Value *User) const {
  if (auto *I = dyn_cast<Instruction>(User)) {
    return CurrentFunction == I->getFunction();
  }
  getLogger(Fatal) << "Unexpected user: " << User << "\n";
  throw std::exception();
}

/**
 * @brief Finds the dominating BasicBlock among users and ptr
 * @param Ptr
 * @param Users
 * @return
 */
BasicBlock *HAKCFunctionEnforcement::findDominatorUseBlock(
    Value *Ptr, const std::set<Instruction *> &Users) const {
  BasicBlock *Dominator = nullptr;
  std::set<BasicBlock *> BasicBlocks;

  if (auto *I = dyn_cast<Instruction>(Ptr)) {
    if (!isa<AllocaInst>(Ptr)) {
      Dominator = I->getParent();
    }
  }

  for (auto *User : Users) {
    if (!userInFunction(User)) {
      continue;
    }
    if (auto *PHI = dyn_cast<PHINode>(User)) {
      for (unsigned I = 0; I < PHI->getNumIncomingValues(); I++) {
        auto *IncomingValue = PHI->getIncomingValue(I);
        if (IncomingValue == Ptr) {
          BasicBlocks.insert(PHI->getIncomingBlock(I));
        }
      }
    } else {
      BasicBlocks.insert(User->getParent());
    }
  }

  for (auto *BB : BasicBlocks) {
    if (!Dominator) {
      Dominator = BB;
    } else {
      Dominator = DTree
                      .findNearestCommonDominator(
                          Dominator->getFirstNonPHIOrDbgOrLifetime(),
                          BB->getFirstNonPHIOrDbgOrLifetime())
                      ->getParent();
    }
  }
  if (!Dominator) {
    Dominator = &CurrentFunction->getEntryBlock();
  }

  return Dominator;
}

bool HAKCFunctionAnalysis::AddManagedPointer(Use &PointerUse) const {
  if (!CommonHAKCAnalysis::IsPointerLikeType(PointerUse->getType())) {
    getLogger(Fatal) << "Trying to add an invalid ManagedHAKCPointer: "
                     << PointerUse << "\n"
                     << *CurrentFunction << "\n";
    throw std::exception();
  }
  auto Result = PointerManager.ManagePointer(PointerUse);
  if (Result) {
    if (!PointerManager.GetManagedPointer(PointerUse.get())) {
      getLogger(Fatal) << "Could not find ManagedPointer for " << PointerUse
                       << "\n";
      throw std::exception();
    }
  }
  return Result;
}

/**
 * @brief Returns true if an argument should be authenticated
 * @param arg The function argument to check
 * @return
 */
bool HAKCFunctionAnalysis::argNeedsAuthentication(Use &arg) const {
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
        if (arg.getOperandNo() == 1) {
          return false;
        }
        /*else if (arg.getOperandNo() == 0) {
                   return true;
               }*/
      }
    } else if (call->getCalledFunction()) {
      GetLogger(Verbose, !DebugActive)
          << "arg.getOperandNo() = " << arg.getOperandNo() << "\n";
      return ((arg->getType()->isPointerTy() ||
               isa<PtrToIntInst>(arg.get()))) &&
             (ModuleAnalysis.GetCommonAnalysis().IsSafeTransitionFunction(
                  call->getCalledFunction()) ||
              PointerManager.IsIntrinsicNeedingAuthentication(call));
    }
  }
  return (!isa<Function>(arg) &&
          PointerManager.PointerIsEligibleForManagement(arg));
}

bool HAKCFunctionAnalysis::IsIntrinsicToSkip(CallBase *Call) const {
  constexpr Intrinsic::ID IntrinsicsToSkip[] = {
      Intrinsic::IndependentIntrinsics::dbg_declare,
      /*Intrinsic::IndependentIntrinsics::dbg_addr,*/
      Intrinsic::IndependentIntrinsics::dbg_label,
      Intrinsic::IndependentIntrinsics::dbg_value,
      Intrinsic::IndependentIntrinsics::read_register,
  };
  return PointerManager.IsCallInIntrinsicSet(Call, IntrinsicsToSkip);
}

/**
 * @brief Returns true if the PHINode uses the specified target
 * @param PhiNode
 * @param target
 * @param visited
 * @return
 */
bool HAKCFunctionAnalysis::phiNodeUsesValue(PHINode *PhiNode, Value *target,
                                            std::set<PHINode *> &visited) {
  visited.insert(PhiNode);
  for (auto &Val : PhiNode->incoming_values()) {
    Value *def = getDef(Val.get(), true);
    if (Val.get() == target || def == target) {
      return true;
    }
    if (auto *phi = dyn_cast<PHINode>(def)) {
      if (visited.contains(phi)) {
        continue;
      }
      if (phiNodeUsesValue(phi, target, visited)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * @brief Perform analysis of an Instruction
 * @param I
 */
void HAKCFunctionAnalysis::HandleInstruction(Instruction *I) {
  // TODO: put permissions analysis here?
  if (auto *call = dyn_cast<CallInst>(I)) {
    handleCall(call);
  } else if (auto *load = dyn_cast<LoadInst>(I)) {
    handleLoad(load);
  } else if (auto *store = dyn_cast<StoreInst>(I)) {
    handleStore(store);
  } else if (auto *binOp = dyn_cast<BinaryOperator>(I)) {
    handleBinaryOperator(binOp);
  }
}

void HAKCFunctionEnforcement::HandleInstruction(Instruction *I) {
  if (dyn_cast<CallInst>(I) || dyn_cast<LoadInst>(I) ||
      dyn_cast<StoreInst>(I) || dyn_cast<BinaryOperator>(I)) {
    HAKCFunctionAnalysis::HandleInstruction(I);
  } else if (auto *compare = dyn_cast<CmpInst>(I)) {
    handleComparison(compare);
  }
}

/**
 * @brief Retrieves the Instruction of a User
 * @param user
 * @return
 */
Instruction *HAKCFunctionAnalysis::getUserInst(User *user) {
  if (auto *inst = dyn_cast<Instruction>(user)) {
    return inst;
  }
  if (isa<BitCastOperator>(user) || isa<GEPOperator>(user)) {
    return getUserInst(*user->user_begin());
  }
  getLogger(Fatal) << "Unexpected user: " << user << "\n";
  throw std::exception();
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
void HAKCFunctionAnalysis::handleLoad(LoadInst *load) const {
  GetLogger(Verbose, !DebugActive)
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

/**
 * @brief BinaryOperators (like bitwise OR) should use authenticated values
 * @param binOp
 */
void HAKCFunctionAnalysis::handleBinaryOperator(BinaryOperator *binOp) const {
  GetLogger(Verbose, !DebugActive) << "Checking binary op " << binOp << "\n";
  /* Both operators need to be pointers to skip operations like
   * ptr | 0xFFFF
   */
  if (argNeedsAuthentication(binOp->getOperandUse(0)) &&
      argNeedsAuthentication(binOp->getOperandUse(1))) {
    GetLogger(Verbose, !DebugActive) << "Registering both operands\n";
    AddManagedPointer(binOp->getOperandUse(0));
    AddManagedPointer(binOp->getOperandUse(1));
  }
}

/**
 * @brief Ensures that authenticated pointers are used in comparisons for
 * correctness
 * @param compare
 */
void HAKCFunctionEnforcement::handleComparison(CmpInst *compare) {
  GetLogger(Verbose, !DebugActive)
      << "Checking comparison " << *compare << "\n";

  MaybeAddCompareToDirectUsers(compare);

  if (isa<ConstantPointerNull>(compare->getOperand(0)) ||
      isa<ConstantPointerNull>(compare->getOperand(1))) {
    GetLogger(Verbose, !DebugActive)
        << "\tComparisons with null do not need authentication\n";
    return;
  }
  if (isa<Operator>(compare->getOperand(0)) ||
      isa<Operator>(compare->getOperand(1))) {
    bool comparisonIsWithConstant = false;
    if (const auto *bitCastOperator0 =
            dyn_cast<Operator>(compare->getOperand(0))) {
      if (const auto *ci =
              dyn_cast<ConstantInt>(bitCastOperator0->getOperand(0))) {
        comparisonIsWithConstant = (ci->getZExtValue() < user_space_end);
      }
    }
    if (!comparisonIsWithConstant) {
      if (const auto *bitCastOperator1 =
              dyn_cast<Operator>(compare->getOperand(1))) {
        if (const auto *ci =
                dyn_cast<ConstantInt>(bitCastOperator1->getOperand(0))) {
          comparisonIsWithConstant = (ci->getZExtValue() < user_space_end);
        }
      }
    }
    if (comparisonIsWithConstant) {
      GetLogger(Verbose, !DebugActive)
          << "\tComparisons with constant integers do not need "
             "authentications\n";
      return;
    }
  }

  if (isCompartmentalizedFunction()) {
    const bool arg0NeedsAuth =
        argNeedsAuthentication(compare->getOperandUse(0)) &&
        !isa<GlobalValue>(getDef(compare->getOperand(0), false));
    const bool arg1NeedsAuth =
        argNeedsAuthentication(compare->getOperandUse(1)) &&
        !isa<GlobalValue>(getDef(compare->getOperand(1), false));

    GetLogger(Verbose, !DebugActive)
        << "Argument 0 "
        << (arg0NeedsAuth ? "needs auth" : "does not need auth") << "\n";
    GetLogger(Verbose, !DebugActive)
        << "Argument 1 "
        << (arg1NeedsAuth ? "needs auth" : "does not need auth") << "\n";

    if (arg0NeedsAuth && arg1NeedsAuth) {
      GetLogger(Verbose, !DebugActive) << "Both operands should be checked\n";
      AddManagedPointer(compare->getOperandUse(0));
      AddManagedPointer(compare->getOperandUse(1));
    } else {
      if (arg0NeedsAuth) {
        GetLogger(Verbose, !DebugActive) << "Registering argument 0\n";
        AddManagedPointer(compare->getOperandUse(0));
      } else {
        GetLogger(Verbose, !DebugActive)
            << "Argument 1 (" << compare->getOperand(1)
            << " ) already authenticated\n";
      }
      if (arg1NeedsAuth) {
        GetLogger(Verbose, !DebugActive) << "Registering argument 1\n";
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
 * @brief Returns true if a GlobalValue should be transferred
 * @param globalValueArg
 * @return
 */
bool HAKCFunctionAnalysis::globalShouldBeTransferred(
    Use &globalValueArg) const {
  /* Don't transfer to printk */
  if (auto *globalValue =
          dyn_cast<GlobalValue>(getDef(globalValueArg.get(), false))) {
    /* Don't transfer THIS_MODULE */
    if (globalValue->getName() == "__this_module") {
      return false;
    }

    /* Ignore constant string arrays */
    if (globalValue->getValueType()->isArrayTy() &&
        globalValue->getValueType()->getArrayElementType()->isIntegerTy(8)) {
      return false;
    }

    if (const auto *call = dyn_cast<CallInst>(globalValueArg.getUser())) {
      if (!ModuleAnalysis.GetCommonAnalysis().FunctionIsAnalysisCandidate(
              call->getCalledFunction())) {
        return false;
      }
      return true;
    }

    return globalValue->getValueType()->isPointerTy();
  }

  GetLogger(Verbose, !DebugActive)
      << "Arg " << globalValueArg.getOperandNo() << " (" << globalValueArg
      << " ) is not a GlobalValue\n";
  return false;
}

/**
 * @brief Processes a function call for analysis
 * @param call
 */
void HAKCFunctionAnalysis::handleCall(CallInst *call) {
  if (call->getCalledFunction() && IsIntrinsicToSkip(call)) {
    return;
  }

  if (ModuleAnalysis.GetCommonAnalysis().IsHAKCFunction(
          call->getCalledFunction())) {
    HAKCFunctionCalls.insert(call);
  }

  GetLogger(Verbose, !DebugActive) << "Handling call " << *call << "\n";

  if (ModuleAnalysis.GetCommonAnalysis().ValueIsUsedAsPointer(call)) {
    for (auto &U : call->uses()) {
      if (AddManagedPointer(U)) {
        break;
      }
    }
  }

  bool needsAuthenticatedArgs =
      call->isInlineAsm() ||
      (ModuleAnalysis.FunctionIsInAnalysisSet(call->getCalledFunction()) &&
       !CommonHAKCAnalysis::IsOutsideTransferFunc(call->getCalledFunction()));

  if (isa<IntrinsicInst>(call)) {
    needsAuthenticatedArgs =
        PointerManager.IsIntrinsicNeedingAuthentication(call);
  }

  GetLogger(Verbose, !DebugActive)
      << *call
      << (needsAuthenticatedArgs ? " needs authenticated args"
                                 : " does not need authenticated args")
      << "\n";

  if (call->isIndirectCall()) {
    GetLogger(Verbose, !DebugActive) << "Indirect call: " << *call << "\n";
    AddManagedPointer(call->getCalledOperandUse());
    /* Using checked pointers for indirect calls because the indirect call
     * can be an assembly function, which currently requires valid pointers.
     * This is safe for other functions, since the target will be a transfer
     * function, and will perform the protecting before entering
     * compartmentalized code, or again create a valid pointer for
     * uncompartmentalized code */
    for (auto &arg : call->args()) {
      if (argNeedsAuthentication(arg)) {
        AddManagedPointer(arg);
      }
      GetLogger(Verbose, !DebugActive)
          << "Argument " << *arg << " for " << *call
          << " does not need authentication\n";
    }
  } else if (needsAuthenticatedArgs) {
    for (auto &arg : call->args()) {
      if (argNeedsAuthentication(arg)) {
        AddManagedPointer(arg);
      }
      GetLogger(Verbose, !DebugActive)
          << "Argument " << *arg << " for " << *call
          << " does not need authentication\n";
    }
  } else if (!ModuleAnalysis.GetCommonAnalysis().IsSafeTransitionCall(call)) {
    if (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic()) {
      /* Intrinsics that don't need authenticated args are basically bitwise
       * shifts and other minor things, so ignore them
       */
      return;
    }
    for (auto &arg : call->args()) {
      Value *def = getDef(arg.get(), false);
      if (auto *glob = dyn_cast<GlobalValue>(def)) {
        if (globalShouldBeTransferred(arg)) {
          GetLogger(Verbose, !DebugActive)
              << "Global " << glob->getName() << " used by " << *call << "\n";
          GlobalArgumentUses[glob].insert(call);
          AddManagedPointer(arg);
        }
        GetLogger(Verbose, !DebugActive)
            << "Global " << glob->getName() << " should not be transferred to "
            << *call << "\n";
      } else if (auto *phiNode = dyn_cast<PHINode>(def)) {
        for (auto &val : phiNode->incoming_values()) {
          Value *valDef = getDef(val.get(), false);
          if (auto *globVal = dyn_cast<GlobalValue>(valDef)) {
            if (globalShouldBeTransferred(val)) {
              GetLogger(Verbose, !DebugActive)
                  << "Global " << globVal->getName() << " used by " << *call
                  << "\n";
              GlobalArgumentUses[globVal].insert(call);
            }
            GetLogger(Verbose, !DebugActive)
                << "Global " << globVal->getName()
                << " should not be transferred to " << *call << "\n";
          }
        }
      } else if (isa<AllocaInst>(def)) {
        if (!ModuleAnalysis.GetCommonAnalysis().FunctionIsAnalysisCandidate(
                call->getCalledFunction())) {
          GetLogger(Verbose, !DebugActive) << "Function called by " << *call
                                           << " is not an analysis candidate\n";
        }
      }
    }
    if (call->getCalledFunction()) {
      DirectFunctionCallSet.insert(call);
    }
  }
}

HAKCTypeIdentifier &HAKCFunctionAnalysis::GetTypeIdentifier() const {
  return ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().GetTypeIdentifier();
}

void HAKCFunctionAnalysis::TypeUseAnalysis() const {
  GetLogger(Debug, !DebugActive)
      << "!!!! Starting Function Temporal Analysis !!!!\n";

  // loop through all managed pointers and their uses
  for (auto ptr : PointerManager.ManagedPointers()) {
    SmallVector<ManagedHAKCPointerUseP> Uses;
    ptr->GetAllUses(Uses);
    GetLogger(Debug, !DebugActive) << *ptr << "\n";
    for (auto Use : Uses) {
      GetLogger(Debug, !DebugActive) << "\tAnalyzing Use" << *Use << "\n";
      auto *I = Use->getUser();

      if (dyn_cast<CallInst>(I)) {
        TypeUseHandleCall(Use);
      } else if (dyn_cast<LoadInst>(I)) {
        TypeUseHandleLoad(Use);
      } else if (dyn_cast<StoreInst>(I)) {
        TypeUseHandleStore(Use);
      } else {
        GetLogger(Debug, !DebugActive)
            << "\t\tSkipping Use because it is not a call, load, or store!\n";
      }
    }
  }
  GetLogger(Debug, !DebugActive)
      << "!!!! Ending Function Temporal Analysis !!!!\n";
}

void HAKCFunctionAnalysis::AddPermissionUse(
    const ManagedHAKCPointer &ManagedPointer, const TypePerms perm) const {
  // TODO: fix segfault on line below when GetType() is null?
  const auto HAKCTy = ManagedPointer.GetType();
  if (!HAKCTy) {
    // HAKCFunctionAnalysis::getLogger(Fatal) << "ManagedPointer.GetType() is
    // Null for " << ManagedPointer << "!\n"; throw std::exception();
    return;
  }
  const auto pointeeType = HAKCTy->GetPointeeType();
  GetTypeIdentifier().ModifyTypeUse(CurrentFunction, pointeeType, perm);
}

void HAKCFunctionAnalysis::TypeUseHandleCall(
    const ManagedHAKCPointerUseP &CallUse) const {
  if (auto *CallUser = dyn_cast<CallInst>(CallUse->getUser())) {
    // works for both direct and indirect calls
    if (CallUse->getOperandNo() ==
        CallUser->getCalledOperandUse().getOperandNo()) {
      AddPermissionUse(CallUse->getManagedPtr(), Execute);
    }
  }
}

void HAKCFunctionAnalysis::TypeUseHandleLoad(
    const ManagedHAKCPointerUseP &LoadUse) const {
  if (LoadUse->getOperandNo() == LoadInst::getPointerOperandIndex()) {
    AddPermissionUse(LoadUse->getManagedPtr(), Read);
  } else {
    getLogger(Debug) << "LoadUse was not operand 0! (Is this even possible?)"
                     << *LoadUse << "\n";
  }
}

void HAKCFunctionAnalysis::TypeUseHandleStore(
    const ManagedHAKCPointerUseP &StoreUse) const {

  // check that the use is the thing that is being written to
  if (StoreUse->getOperandNo() == StoreInst::getPointerOperandIndex()) {
    AddPermissionUse(StoreUse->getManagedPtr(), Write);
  } else {
    getLogger(Debug) << "StoreUse was not operand 1! (Is this even possible?)"
                     << *StoreUse << "\n";
  }
}

bool HAKCFunctionAnalysis::modifiedFunction() const {
  return !(PointerManager.empty() && GlobalArgumentUses.empty() &&
           DirectFunctionCallSet.empty() &&
           PointerManager.GetTotalAdditions() == 0 &&
           CompartmentTransferCount == 0);
}

HAKCModuleAnalysis &HAKCFunctionAnalysis::GetModuleAnalysis() const {
  return ModuleAnalysis;
}

HAKCFunctionEnforcement::HAKCFunctionEnforcement(
    Function *F, HAKCModuleAnalysis &ModuleAnalysis,
    HAKCTransformer &Transformer, HAKCServerClientBase &Client)
    : HAKCFunctionAnalysis(F, ModuleAnalysis), Transformer(Transformer),
      Client(Client), DTree(*F) {
  HAKCFunctionEnforcement::setup();
}

void HAKCFunctionEnforcement::UpdateHAKCFunctionParameters() const {
  if (IsUncompartmentalizedSymbol()) {
    return;
  }

  GetLogger(Verbose, !DebugActive)
      << "Updating parameters for the following HAKC functions:\n";
  for (auto *CallI : HAKCFunctionCalls) {
    GetLogger(Verbose, !DebugActive) << CallI << "\n";
  }

  auto *F = CurrentFunction;
  auto *TransferTarget = F;
  if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
    const auto TransferTargetName =
        F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
    TransferTarget = F->getParent()->getFunction(TransferTargetName);
  }

  const auto TargetCompartment =
      Client.GetDivision(TransferTarget).GetHAKCCompartment();
  for (auto *CallI : HAKCFunctionCalls) {
    if (auto HAKCTransferFunction =
            ModuleAnalysis.GetCommonAnalysis().GetHAKCTransferDefinition(
                CallI->getCalledFunction())) {
      GetLogger(Verbose, !DebugActive)
          << "Updating HAKC call parameters for " << CallI << "\n";
      GetLogger(Verbose, !DebugActive)
          << "Updating index " << HAKCTransferFunction->GetCompartmentIdIdx()
          << " ("
          << CallI->getArgOperand(
                 HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue())
          << ") to " << TargetCompartment.GetCompartmentIDValue() << "\n";
      UpdateHAKCFunctionParameters(CallI, TargetCompartment,
                                   HAKCTransferFunction);
      GetLogger(Verbose, !DebugActive)
          << "After update call is " << CallI << "\n";
    } else {
      GetLogger(Verbose, !DebugActive)
          << "No HAKC Transfer function found for "
          << CallI->getCalledFunction()->getName() << "\n";
    }
  }
}

void HAKCFunctionEnforcement::CheckCompareOperandForDirectFunctionUse(
    CmpInst *CmpI, const unsigned OpNo) {
  auto *Op = getDef(CmpI->getOperand(OpNo), false);
  if (auto *func = dyn_cast<Function>(Op)) {
    if (ModuleAnalysis.GetCommonAnalysis().ValueShouldBeReplacedWithTransfer(
            func, Client)) {
      GetLogger(Verbose, !DebugActive)
          << "Adding comparison to directFunctionUsers for argument "
          << std::to_string(OpNo) << "\n";
      directFunctionUsers.insert(CmpI);
    }
  }
}

std::string HAKCFunctionEnforcement::getHAKCFunctionSectionName() const {
  const auto Compartment =
      Client.GetDivision(CurrentFunction).GetHAKCCompartment();

  std::string sectionName = HAKC_SECTION_PREFIX.str() +
                            std::to_string(Compartment.GetCompartmentIDValue());
  sectionName += (CurrentFunction->getSection().empty())
                     ? ".text"
                     : CurrentFunction->getSection().str();

  return sectionName;
}

bool HAKCFunctionEnforcement::IsUncompartmentalizedSymbol() {
  return CommonHAKCAnalysis::IsNECSymbol(CurrentFunction, Client);
}

bool HAKCFunctionEnforcement::IsUncompartmentalizedSymbol() const {
  return CommonHAKCAnalysis::IsNECSymbol(CurrentFunction, Client);
}

bool HAKCFunctionEnforcement::IsUncompartmentalizedSymbol(GlobalValue *GV) {
  return CommonHAKCAnalysis::IsNECSymbol(GV, Client);
}

bool HAKCFunctionEnforcement::IsUncompartmentalizedSymbol(
    GlobalValue *GV) const {
  return CommonHAKCAnalysis::IsNECSymbol(GV, Client);
}

bool HAKCFunctionEnforcement::isCompartmentalizedFunction() {
  return CommonHAKCAnalysis::IsCompartmentalizedFunction(CurrentFunction,
                                                         Client);
}

bool HAKCFunctionEnforcement::isCompartmentalizedFunction() const {
  return CommonHAKCAnalysis::IsCompartmentalizedFunction(CurrentFunction,
                                                         Client);
}

void HAKCFunctionEnforcement::UpdateHAKCFunctionParameters(
    CallInst *CallI, const HAKCCompartment &TargetCompartment,
    const function_def_t &HAKCTransferFunction) const {
  GetLogger(Verbose, !DebugActive)
      << "Setting "
      << *CallI->getArgOperand(
             HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue())
      << " to be " << *TargetCompartment.GetCompartmentID() << "\n";
  CallI->setOperand(HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue(),
                    TargetCompartment.GetCompartmentID());

  if (HAKCTransferFunction->GetDivisionIdIdx() != nullptr) {
    auto *F = CallI->getFunction();
    HAKCCompartmentDivision Division =
        CommonHAKCAnalysis::IsOutsideTransferFunc(F)
            ? Client.GetDivision(
                  CommonHAKCAnalysis::GetOriginalFunctionFromTransferFunction(
                      F))
            : Client.GetDivision(F);

    GetLogger(Verbose, !DebugActive)
        << "Setting argument "
        << HAKCTransferFunction->GetDivisionIdIdx()->getZExtValue() << " to be "
        << Division << "\n";
    CallI->setOperand(HAKCTransferFunction->GetDivisionIdIdx()->getZExtValue(),
                      Division.GetDivisionID());
  }
}

void HAKCFunctionEnforcement::
    CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls() const {
  auto CurrentDivision = Client.GetDivision(CurrentFunction);
  Client.GetValidTargets(CurrentDivision.GetHAKCCompartment());
  for (auto *call : DirectFunctionCallSet) {
    // need to check that call is actually compartmentalized here since that
    // check was removed from handle call function in analysis if
    // (CommonHAKCAnalysis::IsNECSymbol(call->getCalledFunction(), Client)) {
    //   getLogger(Verbose) << "Call " << *call->getCalledFunction() << "is not
    //   compartmentalized, skipping\n"; continue;
    // }
    auto TargetCompartment =
        Client.GetDivision(call->getCalledFunction()).GetHAKCCompartment();
    if (CurrentDivision.GetHAKCCompartment().GetCompartmentID() ==
        TargetCompartment.GetCompartmentID()) {
      /* Aliases are being used for transfer functions, so if the
       * called function is in the same compartment use the transformed function
       * name. Otherwise, do not change the function name, because the
       * transfer function will be used through the alias.
       */
      if (ModuleAnalysis.GetCommonAnalysis().SymbolNeedsTransferFunction(
              call->getCalledFunction(), Client)) {
        auto TransformedName = CommonHAKCAnalysis::getOriginalTransformedName(
            call->getCalledFunction());
        auto *const TransformedFunction = ModuleAnalysis.GetFunctionByName(
            TransformedName, call->getCalledFunction()->getFunctionType());
        call->setCalledFunction(TransformedFunction);
      }
    } else {

      // Fixing https://github.mit.edu/inherently-secure/ARM-MTE/issues/40
      bool ValidTransition = false;
      Client.GetValidTargets(CurrentDivision.GetHAKCCompartment());
      for (const auto *Target :
           CurrentDivision.GetHAKCCompartment().GetValidTargets()) {
        GetLogger(Verbose, !DebugActive)
            << "Testing Target Compartment " << Target->getZExtValue()
            << " == " << TargetCompartment.GetCompartmentID()->getZExtValue()
            << " -> "
            << (Target->getZExtValue() ==
                TargetCompartment.GetCompartmentID()->getZExtValue())
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
        getLogger(Error)
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
            GetTransformer().CreateTransferToVariadic(call, &PointerManager);
        call->setCalledFunction(VariadicTransfer);
      }
    }
  }
}

void HAKCFunctionEnforcement::MaybeAddCompareToDirectUsers(CmpInst *CmpI) {
  CheckCompareOperandForDirectFunctionUse(CmpI, 0);
  CheckCompareOperandForDirectFunctionUse(CmpI, 1);
}

void HAKCFunctionEnforcement::ReplaceDirectFunctionUsesWithTransfers() const {
  for (auto *I : directFunctionUsers) {
    for (unsigned i = 0; i < I->getNumOperands(); i++) {
      if (isa<CallInst>(I)) {
        auto *call = dyn_cast<CallInst>(I);
        if (call->getCalledOperandUse().getOperandNo() == i)
          /* Don't change actual function call, only the arguments */
          continue;
      }
      auto *Op = getDef(I->getOperand(i), false);
      if (isa<Function>(Op)) {
        CheckAndReplaceArgument(Op, I, i);
      } else if (auto *selectInst = dyn_cast<SelectInst>(Op)) {
        auto *TrueValue = getDef(selectInst->getTrueValue(), false);
        CheckAndReplaceArgument(TrueValue, I, i);
        auto *FalseValue = getDef(selectInst->getFalseValue(), false);
        CheckAndReplaceArgument(FalseValue, I, i);
      }
    }
  }
}

/**
 * @brief Replace signed pointer dereferences with authenticated dereferences
 */
void HAKCFunctionEnforcement::transformPointerDereferences() const {
  GetLogger(Verbose, !DebugActive)
      << "Function prior to transforming pointer dereferences\n"
      << *CurrentFunction << "\n";
  TransformPointers();
}

void HAKCFunctionEnforcement::createMissingTransfers() {
  if (!IsUncompartmentalizedSymbol()) {
    GetLogger(Verbose, !DebugActive) << "Function prior to making transfers:\n"
                                     << *CurrentFunction << "\n";
    CreateAllTransfers();
  }
}

/**
 * @brief Sets the function section to the correct PMC ELF section
 */
void HAKCFunctionEnforcement::relocateFunctionSection() const {
  if (isCompartmentalizedFunction()) {
    CurrentFunction->setSection(getHAKCFunctionSectionName());
  }
}

/**
 * @brief Creates all authenticated pointers, and clones any intermediate
 * pointer arithmetic between authentication and dereference
 */
void HAKCFunctionEnforcement::createAllAuthenticatedPointers() {
  GetLogger(Verbose, !DebugActive)
      << "Function prior to making authenticated copies:\n"
      << *CurrentFunction << "\n";
  CreateAuthenticatedPointersAndAllClones();
}

void HAKCFunctionEnforcement::CreateAuthenticatedPointersAndAllClones() {
  SmallVector<ManagedHAKCPointerP> SortedPointers;
  for (auto &P : PointerManager.ManagedPointers()) {
    SortedPointers.push_back(P);
  }

  for (const auto &ManagedPtr : SortedPointers) {
    /* Guarantee that auth and protected pointers get placed correctly */
    MaybeCreateBaseCopyPointer(ManagedPtr);
  }

  for (auto &ManagedPtr : SortedPointers) {
    CreateBaseAuthenticatedPointer(ManagedPtr);
    if (ManagedPtr->GetAuthenticatedPointer()) {
      GetLogger(Verbose, !DebugActive)
          << "Authenticated Pointer for " << *ManagedPtr << ": "
          << ManagedPtr->GetAuthenticatedPointer() << "\n";
    }
  }
  for (auto &ManagedPtr : SortedPointers) {
    CreatePointerUseClones(ManagedPtr);
    GetLogger(Verbose, !DebugActive)
        << "Created Authenticated and Protected Copies for " << *ManagedPtr
        << "\n";
  }
}

void HAKCFunctionEnforcement::CreateAllTransfers() {
  SmallVector<ManagedHAKCPointerP> SortedPointers;
  for (auto &P : PointerManager.ManagedPointers()) {
    SortedPointers.push_back(P);
  }
  bool PointersUpdated = true;
  while (PointersUpdated) {
    PointersUpdated = false;

    for (auto &ManagedPtr : SortedPointers) {
      const auto CurrentAuthUserCount = ManagedPtr->GetAuthenticatedUserCount();
      const auto CurrentProtUserCount = ManagedPtr->GetProtectedUserCount();

      UpdateUserCounts(ManagedPtr);
      if (CurrentAuthUserCount != ManagedPtr->GetAuthenticatedUserCount() ||
          CurrentProtUserCount != ManagedPtr->GetProtectedUserCount()) {
        GetLogger(Verbose, !DebugActive)
            << *ManagedPtr << " changed user count\n";
        PointersUpdated = true;
      }
    }
  }
  PointersUpdated = true;
  while (PointersUpdated) {
    PointersUpdated = false;

    for (auto &ManagedPtr : SortedPointers) {
      const auto OrigBaseIsAuthenticated =
          ManagedPtr->BaseIsAuthenticatedPointer();
      const auto BaseAuthenticatedResult =
          ManagedPtr->DetermineIfBasePointerIsAuthenticated();
      if (OrigBaseIsAuthenticated != BaseAuthenticatedResult) {
        GetLogger(Verbose, !DebugActive)
            << *ManagedPtr << " changed base authentication flag from "
            << std::to_string(OrigBaseIsAuthenticated) << " to "
            << std::to_string(BaseAuthenticatedResult) << "\n";
        PointersUpdated = true;
      }
    }
  }

  /* At this point, all uses should be classified, and we should know if
   * authenticated and protected pointers need to be created */

  for (const auto &HAKCPointer : SortedPointers) {
    MaybeCreateProtectedPointer(HAKCPointer);
  }
}

Value *HAKCFunctionEnforcement::CreateSafePointerAtLocation(
    Value *Pointer, Instruction *InsertLocation) const {
  if (auto *Managed = PointerManager.FindAuthenticatedValue(Pointer)) {
    return Managed;
  }

  PointerManager.IncrementSafePointersAdded();
  return AddSafePointerCreationAtLocation(Pointer, InsertLocation);
}

Value *HAKCFunctionEnforcement::CreateAuthenticationAtLocation(
    Value *Pointer, Instruction *InsertLocation) const {
  if (auto *Managed = PointerManager.FindAuthenticatedValue(Pointer)) {
    return Managed;
  }
  const auto ManagedPointer = PointerManager.GetManagedPointer(Pointer);
  if (!ManagedPointer) {
    getLogger(Fatal) << "Could not find Managed Pointer for " << *Pointer
                     << "\n";
    throw std::exception();
  }
  if (!ManagedPointer->GetType()) {
    getLogger(Fatal) << "Managed Pointer " << *ManagedPointer
                     << " found for Value " << *Pointer
                     << " does not have a HAKCType\n";
    throw std::exception();
  }
  GetLogger(Verbose, !DebugActive)
      << "Adding Authenticated Pointer for " << *ManagedPointer
      << " with HAKCType " << *ManagedPointer->GetType() << "\n"
      << " at " << *InsertLocation << "\n";
  if (CommonHAKCAnalysis::PointerShouldBeConsideredCode(*ManagedPointer)) {
    PointerManager.IncrementCodeAuthenticationsAdded();
    return AddCodeAuthCheckAtLocation(Pointer, InsertLocation);
  }
  PointerManager.IncrementDataAuthenticationsAdded();
  return AddDataAuthCheckAtLocation(Pointer, InsertLocation);
}

/**
 * @brief Finds an insertion point for new instructions.
 * @param V The Value for which we want to insert a new Instruction
 * @param Users The users of v
 * @return The location at which to insert a new Instruction
 */
Instruction *HAKCFunctionEnforcement::FindUseInsertionPoint(
    Value *V, const std::set<Instruction *> &Users) const {
  if (const auto phi = dyn_cast<PHINode>(V)) {
    return phi->getParent()->getFirstNonPHIOrDbgOrLifetime();
  }

  GetLogger(Verbose, !DebugActive)
      << "Finding insertion point for " << V << "\n";

  BasicBlock *DominatorBlock = findDominatorUseBlock(V, Users);
  if (!DominatorBlock) {
    GetLogger(Fatal, !DebugActive) << "Could not find block for " << V << "\n"
                                   << *CurrentFunction;
    throw std::exception();
  }
  // Print out the block that we find
  GetLogger(Verbose, !DebugActive)
      << "findDominatorUseBlock found " << *DominatorBlock << "\n";

  for (Instruction &I : *DominatorBlock) {
    if (&I == V) {
      return I.getNextNonDebugInstruction();
    }
    if (!isa<PHINode>(&I) && Users.contains(&I)) {
      GetLogger(Verbose, !DebugActive)
          << "In FindUseInsertionPoint Instruction " << I << "\n";
      return &I;
    }
  }

  GetLogger(Verbose, !DebugActive)
      << "In FindUseInsertionPoint got DominatorBlock "
      << DominatorBlock->getTerminator() << "\n";
  return DominatorBlock->getTerminator();
}

bool HAKCFunctionEnforcement::FunctionsAreInSameCompartment(Function *F,
                                                            Function *G) const {
  const auto FCompartment = Client.GetDivision(F).GetHAKCCompartment();
  const auto GCompartment = Client.GetDivision(G).GetHAKCCompartment();
  return FCompartment == GCompartment;
}

/**
 * @brief Adds a check of a signed pointer which checks for valid data access
 * @param SignedPtr The pointer to check
 * @param location The location at which to place the check
 * @return The result of the transfer
 */
Value *HAKCFunctionEnforcement::AddDataAuthCheckAtLocation(
    Value *SignedPtr, Instruction *location) const {
  const auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
  if (!HAKCPointer) {
    GetLogger(Fatal, !DebugActive)
        << "Could not find Managed Pointer for " << SignedPtr << "\n";
    throw std::exception();
  }
  auto *bitcast =
      GetTransformer().CreateDataAuthentication(*HAKCPointer, location);
  return bitcast;
}

Value *HAKCFunctionEnforcement::AddCodeAuthCheckAtLocation(
    Value *SignedPtr, Instruction *Location) const {
  const auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
  if (!HAKCPointer) {
    GetLogger(Fatal, !DebugActive)
        << "Could not find Managed Pointer for " << SignedPtr << "\n";
    throw std::exception();
  }
  auto *SafePointer =
      GetTransformer().CreateCodeAuthentication(*HAKCPointer, Location);
  return SafePointer;
}

Value *HAKCFunctionEnforcement::AddSafePointerCreationAtLocation(
    Value *SignedPtr, Instruction *Location) const {
  const auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
  if (!HAKCPointer) {
    GetLogger(Fatal, !DebugActive)
        << "Could not find Managed Pointer for " << SignedPtr << "\n";
    throw std::exception();
  }
  auto *SafePtr = GetTransformer().CreateSafePointer(*HAKCPointer, Location);
  GetLogger(Debug, !DebugActive)
      << "Created Safe Pointer\n\t" << *SafePtr << "\nFor Signed Pointer\n\t"
      << *SignedPtr << "\nat\n"
      << *Location << "\n";
  return SafePtr;
}

void HAKCFunctionEnforcement::AddInstrumentation() {

  GetLogger(Verbose, !DebugActive) << "Running AddInstrumentation for "
                                   << CurrentFunction->getName() << "\n";

  GetLogger(Verbose, !DebugActive) << "Managed Pointers:\n";

  for (auto &HAKCPointer : PointerManager.ManagedPointers()) {
    GetLogger(Verbose, !DebugActive) << *HAKCPointer << "\n+++\n";
    HAKCPointer->DetermineIfBasePointerIsAuthenticated();
  }

  if (modifiedFunction()) {
    relocateFunctionSection();

    GetLogger(Verbose, !DebugActive) << "---- createMissingTransfers ----\n";
    createMissingTransfers();
    GetLogger(Verbose, !DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    GetLogger(Verbose, !DebugActive)
        << "----- UpdateHAKCFunctionParameters ------\n";
    UpdateHAKCFunctionParameters();
    GetLogger(Verbose, !DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    GetLogger(Verbose, !DebugActive)
        << "---- createAllAuthenticatedPointers ----\n";
    createAllAuthenticatedPointers();
    GetLogger(Verbose, !DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    GetLogger(Verbose, !DebugActive)
        << "----- transformPointerDereferences ------\n";
    transformPointerDereferences();
    GetLogger(Verbose, !DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    GetLogger(Verbose, !DebugActive)
        << "- ReplaceDirectFunctionUsesWithTransfers -\n";
    ReplaceDirectFunctionUsesWithTransfers();
    GetLogger(Verbose, !DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    GetLogger(Verbose, !DebugActive) << "------ "
                                        "CheckForValidCompartmentTransitionAndU"
                                        "pdateIntraCompartmentCalls -----\n";
    CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();
    GetLogger(Verbose, !DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";

    GetLogger(Verbose, !DebugActive) << *CurrentFunction << "\n";
    // WriteBuggyFunctionToFile();
    CommonHAKCAnalysis::VerifyFunction(CurrentFunction);
  } else {
    GetLogger(Verbose, !DebugActive)
        << "Function " << CurrentFunction->getName() << " unmodified\n";
  }
  GetLogger(Verbose, !DebugActive) << "Finished running AddInstrumentation for "
                                   << CurrentFunction->getName() << "\n";
}

void HAKCFunctionEnforcement::WriteBuggyFunctionToFile() const {

  SmallString<256> Path;
  SmallString<256> ModulePath;
  GetModuleAnalysis().GetCommonAnalysis().GetModuleFullPath(
      GetModuleAnalysis().GetModule(), ModulePath);
  sys::path::append(Path,
                    GetModuleAnalysis().GetSystemInformation().GetBuildPath());
  sys::path::append(Path, ModulePath);
  sys::path::replace_extension(Path, CurrentFunction->getName() + ".ll");
  sys::path::make_preferred(Path);

  auto log_path = std::string(Path);
  auto log_dir = sys::path::parent_path(log_path).str();
  // create directory if it does not exist
  if (!sys::fs::exists(log_dir)) {
    if (sys::fs::create_directories(log_dir)) {
      errs() << "Failed to create " << sys::path::parent_path(log_path) << "\n";
    }
  }
  auto writer = std::make_shared<HAKCWriter>(log_path, Fatal);
  *writer << CurrentFunction;
}

HAKCTransformer &HAKCFunctionEnforcement::GetTransformer() const {
  return Transformer;
}

/**
 * @brief Transfers a pointer argument back to its original color after an
 * indirect call returns
 * @param Operand Indirect call argument
 * @param DebugLoc
 * @param I
 * @param Size
 * @return The call to the kernel resigning operation
 */
Instruction *HAKCFunctionEnforcement::addCompartmentTransferCall(
    Value *Operand, const DebugLoc &DebugLoc, Instruction *I,
    ConstantInt *Size) {
  if (!Operand->getType()->isPointerTy() && !isa<PtrToIntInst>(Operand) &&
      !Operand->getType()->isIntegerTy(
          HAKCCompartment::CompartmentIDBitCount)) {
    getLogger(Fatal) << "Compartment transfer target " << *Operand
                     << " is not a pointer but of type " << *Operand->getType()
                     << " in function\n"
                     << CurrentFunction << "\n";
    throw std::exception();
  }
  const auto HAKCPointer = PointerManager.GetManagedPointer(Operand);
  if (!HAKCPointer) {
    getLogger(Fatal) << "Could not find Managed Pointer for " << Operand
                     << "\n";
    throw std::exception();
  }

  const bool IsData = HAKCPointer->IsDataPointer();
  GetLogger(Verbose, !DebugActive)
      << "isData: " << std::to_string(IsData) << " for " << Operand << "\n";

  Instruction *TransferCall =
      !Size ? TransferCall = GetTransformer().CreateCompartmentTransfer(
                  *HAKCPointer, I, CurrentFunction, IsData)
            : TransferCall = GetTransformer().CreateSizedCompartmentTransfer(
                  *HAKCPointer, I, CurrentFunction, IsData, Size);

  TransferCall->setDebugLoc(DebugLoc);
  CompartmentTransferCount++;

  GetLogger(Verbose, !DebugActive) << "Created transfer for ";
  if (!IsData) {
    GetLogger(Verbose, !DebugActive) << Operand->getName();
  } else {
    GetLogger(Verbose, !DebugActive) << Operand;
  }
  GetLogger(Verbose, !DebugActive) << ": " << TransferCall << "\n";

  return TransferCall;
}

Instruction *HAKCFunctionEnforcement::CreateMissingTransfer(
    Instruction *PointerNeedingTransfer) {
  std::set<Instruction *> UserInstructions;
  for (auto *U : PointerNeedingTransfer->users()) {
    if (auto *I = dyn_cast<Instruction>(U)) {
      UserInstructions.insert(I);
    }
  }
  auto *InsertionPoint =
      FindUseInsertionPoint(PointerNeedingTransfer, UserInstructions);

  ConstantInt *Size = nullptr;
  if (auto *Call = dyn_cast<CallInst>(PointerNeedingTransfer)) {
    if (ModuleAnalysis.GetCommonAnalysis().IsAllocationFunction(
            Call->getCalledFunction())) {
      const auto AllocationDef =
          ModuleAnalysis.GetCommonAnalysis().GetAllocationDefinition(
              Call->getCalledFunction());
      Size = AllocationDef->GetSize(Call);
    }
  }
  return addCompartmentTransferCall(PointerNeedingTransfer,
                                    PointerNeedingTransfer->getDebugLoc(),
                                    InsertionPoint, Size);
}

Instruction *HAKCFunctionEnforcement::SignGlobalPointerWithColor(
    GlobalValue *GlobalVar) const {
  std::set<Instruction *> UserInstructions;
  for (auto *U : GlobalVar->users()) {
    if (auto *I = dyn_cast<Instruction>(U)) {
      if (I->getFunction() == CurrentFunction) {
        UserInstructions.insert(I);
      }
    }
  }

  auto HAKCPointer = PointerManager.GetManagedPointer(GlobalVar);
  if (!HAKCPointer) {
    getLogger(Fatal) << "Could not find Managed Pointer for " << GlobalVar
                     << "\n";
    throw std::exception();
  }
  auto *InsertionPoint = FindUseInsertionPoint(GlobalVar, UserInstructions);
  return GetTransformer().CreateSignWithDivision(
      *HAKCPointer, InsertionPoint, CurrentFunction, !isa<Function>(GlobalVar));
}

void HAKCFunctionEnforcement::ReplaceInstructionOperand(Instruction *I,
                                                        const unsigned ArgNo,
                                                        Value *OldValue,
                                                        Value *NewValue) const {
  auto *V = I->getOperand(ArgNo);
  Value *Replacement;
  if (auto *Oper = dyn_cast<BitCastOperator>(V)) {
    auto HAKCPointer = PointerManager.GetManagedPointer(NewValue);
    if (!HAKCPointer) {
      getLogger(Fatal) << "Could not find Managed Pointer for " << NewValue
                       << "\n";
      throw std::exception();
    }
    Replacement =
        GetTransformer().CreateBitCast(*HAKCPointer, Oper->getDestTy(), I);
  } else if (V == OldValue) {
    Replacement = NewValue;
  } else {
    GetLogger(Verbose, !DebugActive) << "Could not find ";
    if (const auto *F = dyn_cast<Function>(OldValue)) {
      GetLogger(Verbose, !DebugActive) << F->getName();
    } else {
      GetLogger(Verbose, !DebugActive) << OldValue << "\n";
    }
    GetLogger(Verbose, !DebugActive) << " in " << *I << "\n";
    throw std::exception();
  }
  I->setOperand(ArgNo, Replacement);
}

void HAKCFunctionEnforcement::CheckAndReplaceArgument(
    Value *V, Instruction *I, const unsigned int ArgNo) const {
  if (auto *Func = dyn_cast<Function>(V)) {
    const auto name =
        ModuleAnalysis.GetCommonAnalysis().GetOutsideTransferName(Func);
    const auto transfer =
        ModuleAnalysis.GetFunctionByName(name, Func->getFunctionType());
    GetLogger(Verbose, !DebugActive)
        << "Changing operand " << std::to_string(ArgNo) << " to " << name
        << " for\n\t" << *I << "\n";
    transfer->setLinkage(Func->getLinkage());
    transfer->copyAttributesFrom(Func);
    ReplaceInstructionOperand(I, ArgNo, V, transfer);
  }
}

void HAKCFunctionEnforcement::CreateBaseAuthenticatedPointer(
    const ManagedHAKCPointerP &ManagedPtr) {
  GetLogger(Verbose, !DebugActive)
      << __FUNCTION__ << " called for " << *ManagedPtr << "\n";

  if (ManagedPtr->PointerSetsShouldBeEqual()) {
    SetPointerSetsToBeEqual(ManagedPtr);
    if (ManagedPtr->GetPurposefullyIgnored()) {
      GetLogger(Verbose, !DebugActive)
          << ManagedPtr << " is purposefully ignored\n";
    }
    return;
  }

  if (ManagedPtr->GetAuthenticatedUserCount() == 0) {
    GetLogger(Verbose, !DebugActive)
        << "No authenticated pointer uses of " << *ManagedPtr
        << ", so authenticated pointer creation is not needed\n";
    return;
  }
  // in hakc_foo instance, incorrectly entering if statement
  if (ManagedPtr->BaseIsAuthenticatedPointer()) {
    GetLogger(Verbose, !DebugActive) << "The Base Definition is authenticated, "
                                        "so setting uses to be authenticated\n";

    ManagedPtr->SetAuthenticatedPointer(ManagedPtr->GetBaseDefinition());
    SmallVector<ManagedHAKCPointerUseP> SortedUses(
        ManagedPtr->GetAuthenticatedUses().begin(),
        ManagedPtr->GetAuthenticatedUses().end());
    SortedUses.append(ManagedPtr->GetCloneUses().begin(),
                      ManagedPtr->GetCloneUses().end());
    ManagedHAKCPointerUse::SortUses(SortedUses);
    for (auto &UPtr : SortedUses) {
      PointerManager.AddAuthenticatedPointer(UPtr, UPtr->get());
    }
    return;
  }

  if (ManagedPtr->GetAuthenticatedPointer()) {
    GetLogger(Verbose, !DebugActive)
        << "AuthenticatedPointer already created\n";
    return;
  }

  SmallVector<ManagedHAKCPointerUseP> Users;
  ManagedPtr->GetAllUses(Users);
  if (Users.empty()) {
    GetLogger(Verbose, !DebugActive)
        << "User count of " << *ManagedPtr
        << " is 0. No Authenticated Pointer needed\n";
    return;
  }

  GetLogger(Verbose, !DebugActive) << "Creating Base Authenticated Pointer of "
                                   << *ManagedPtr << " with Users\n";
  for (auto &User : Users) {
    GetLogger(Verbose, !DebugActive) << *User << "\n";
  }
  std::set<Instruction *> UserI;

  for (auto *User : ManagedPtr->GetBaseDefinition()->users()) {
    if (auto *I = dyn_cast<Instruction>(User)) {
      if (I->getFunction() == CurrentFunction) {
        GetLogger(Verbose, !DebugActive) << "\t with userI " << *I << "\n";
        UserI.insert(I);
      }
    }
  }

  Value *PointerToAuthenticate = ManagedPtr->GetBaseDefinition();
  bool CreateSafePointerOfTransfer = false;
  auto *AuthenticationInsertPoint =
      FindUseInsertionPoint(ManagedPtr->GetBaseDefinition(), UserI);
  if (BaseDefinitionShouldBeTransferred(ManagedPtr) &&
      isa_and_nonnull<Instruction>(ManagedPtr->GetProtectedPointer())) {
    CreateSafePointerOfTransfer = true;
    AuthenticationInsertPoint =
        dyn_cast<Instruction>(ManagedPtr->GetProtectedPointer())
            ->getNextNonDebugInstruction();
  }

  if (ManagedPtr->GetType() && ManagedPtr->GetType()->IsIgnoredType()) {
    if (auto *I = CreateSafePointerAtLocation(PointerToAuthenticate,
                                              AuthenticationInsertPoint)) {
      ManagedPtr->SetAuthenticatedPointer(I);
    }
  } else {
    Value *I = nullptr;
    if (!isCompartmentalizedFunction() || CreateSafePointerOfTransfer) {
      I = CreateSafePointerAtLocation(PointerToAuthenticate,
                                      AuthenticationInsertPoint);
    }

    if (!I) {
      I = CreateAuthenticationAtLocation(PointerToAuthenticate,
                                         AuthenticationInsertPoint);
    }
    if (I) {
      ManagedPtr->SetAuthenticatedPointer(I);
    }
  }

  if (!ManagedPtr->GetAuthenticatedPointer()) {
    getLogger(Fatal) << "Failed to create authenticated pointer for "
                     << *ManagedPtr << " in Function\n"
                     << CurrentFunction << "\n";
    throw std::exception();
  }
}

void HAKCFunctionEnforcement::TransformPointers() const {
  for (const auto &ManagedPointer : PointerManager.ManagedPointers()) {
    TransformUses(ManagedPointer);
  }
}

void HAKCFunctionEnforcement::SetPointerSetsToBeEqual(
    const ManagedHAKCPointerP &ManagedPtr) const {
  GetLogger(Verbose, !DebugActive)
      << ManagedPtr << " setting pointer sets to be equal\n";

  SmallVector<ManagedHAKCPointerUseP> SortedUses(
      ManagedPtr->GetAuthenticatedUses().begin(),
      ManagedPtr->GetAuthenticatedUses().end());
  SortedUses.append(ManagedPtr->GetCloneUses().begin(),
                    ManagedPtr->GetCloneUses().end());
  SortedUses.append(ManagedPtr->GetProtectedUses().begin(),
                    ManagedPtr->GetProtectedUses().end());
  ManagedHAKCPointerUse::SortUses(SortedUses);

  for (auto &UPtr : SortedUses) {
    PointerManager.AddAuthenticatedPointer(UPtr, UPtr->get());
    PointerManager.AddProtectedPointer(UPtr, UPtr->get());
  }
  ManagedPtr->SetProtectedPointer(ManagedPtr->GetBaseDefinition());
  ManagedPtr->SetAuthenticatedPointer(ManagedPtr->GetBaseDefinition());
}

void HAKCFunctionEnforcement::MaybeCreateProtectedPointer(
    const ManagedHAKCPointerP &ManagedPtr) {
  GetLogger(Verbose, !DebugActive)
      << __FUNCTION__ << " called for " << ManagedPtr << "\n";

  if (ManagedPtr->GetPurposefullyIgnored()) {
    GetLogger(Verbose, !DebugActive)
        << *ManagedPtr << " is purposefully ignored\n";
    return;
  }

  if (ManagedPtr->PointerSetsShouldBeEqual()) {
    GetLogger(Verbose, !DebugActive)
        << *ManagedPtr << " pointer sets will be made equal\n";
    return;
  }

  if (ManagedPtr->GetProtectedUserCount() == 0) {
    GetLogger(Verbose, !DebugActive)
        << "No protected pointer use of " << *ManagedPtr
        << " transfer creation is not needed\n";
    return;
  }

  if (!ManagedPtr->ProtectedPointerShouldBeCreated(Client)) {
    GetLogger(Verbose, !DebugActive)
        << "Protected pointer of " << *ManagedPtr << " should not be created\n";
    return;
  }

  // this is returning false
  const auto BaseShouldBeTransferred =
      BaseDefinitionShouldBeTransferred(ManagedPtr);

  GetLogger(Verbose, !DebugActive)
      << "The Base Definition of " << *ManagedPtr << " is ";
  if (ManagedPtr->BaseIsAuthenticatedPointer()) {
    GetLogger(Verbose, !DebugActive) << "authenticated ";
  } else {
    GetLogger(Verbose, !DebugActive) << "protected ";
  }
  if (BaseShouldBeTransferred) {
    GetLogger(Verbose, !DebugActive) << "and should be transferred";
  }
  GetLogger(Verbose, !DebugActive) << "\n";

  Value *ProtectedValue = nullptr;
  if (!ManagedPtr->BaseIsAuthenticatedPointer() && !BaseShouldBeTransferred &&
      !ManagedPtr->GetManuallyTransferred()) {
    ProtectedValue = ManagedPtr->GetBaseDefinition();
  } else if (ManagedPtr->GetManuallyTransferred()) {
    GetLogger(Verbose, !DebugActive)
        << "Transfer not needed for " << *ManagedPtr
        << " because ProtectedPointer is already set to be "
        << *ManagedPtr->GetProtectedPointer() << "\n";
    ProtectedValue = ManagedPtr->GetProtectedPointer();
  } else if (BaseShouldBeTransferred) {
    GetLogger(Verbose, !DebugActive)
        << "Creating Transfer of BaseDefinition of " << *ManagedPtr << "\n";

    if (auto *GV = dyn_cast<GlobalValue>(ManagedPtr->GetBaseDefinition())) {
      ProtectedValue = SignGlobalPointerWithColor(GV);
    } else {
      auto *BaseDefI = dyn_cast<Instruction>(ManagedPtr->GetBaseDefinition());
      if (!BaseDefI) {
        GetLogger(Verbose, !DebugActive)
            << "Unexpected BaseDefinition for " << *ManagedPtr
            << " in function " << CurrentFunction->getName() << "\n";
      }
      ProtectedValue = CreateMissingTransfer(BaseDefI);
    }
  }

  if (!ProtectedValue && ManagedPtr->ProtectedPointerShouldBeCreated(Client)) {
    getLogger(Fatal) << "The protected pointer of " << *ManagedPtr << " is "
                     << ManagedPtr->GetProtectedPointer()
                     << " but is not manually transferred\n";
    throw std::exception();
  }

  if (ProtectedValue) {
    ManagedPtr->SetProtectedPointer(ProtectedValue);
    SmallVector<ManagedHAKCPointerUseP> SortedUses(
        ManagedPtr->GetProtectedUses().begin(),
        ManagedPtr->GetProtectedUses().end());
    const bool ReplaceCloneUses = !ManagedPtr->BaseIsAuthenticatedPointer() ||
                                  ManagedPtr->GetManuallyTransferred();
    if (ReplaceCloneUses) {
      SortedUses.append(ManagedPtr->GetCloneUses().begin(),
                        ManagedPtr->GetCloneUses().end());
    }
    ManagedHAKCPointerUse::SortUses(SortedUses);
    for (auto &ProtectedUse : SortedUses) {
      if (ProtectedUse->get() == ManagedPtr->GetBaseDefinition()) {
        PointerManager.AddProtectedPointer(ProtectedUse, ProtectedValue);
      } else if (ReplaceCloneUses) {
        PointerManager.AddProtectedPointer(ProtectedUse, ProtectedUse->get());
      }
    }
  }
}

void HAKCFunctionEnforcement::MaybeCreateBaseCopyPointer(
    const ManagedHAKCPointerP &ManagedPtr) const {
  GetLogger(Verbose, !DebugActive)
      << __FUNCTION__ << " called for " << *ManagedPtr << "\n";

  /* Note these checks come from CreateBaseAuthenticatedPointer */
  if (ManagedPtr->PointerSetsShouldBeEqual() ||
      ManagedPtr->GetAuthenticatedUserCount() == 0 ||
      ManagedPtr->BaseIsAuthenticatedPointer()) {
    return;
  }

  if (CommonHAKCAnalysis::IsMultiSSAUser(ManagedPtr->GetBaseDefinition()) &&
      ManagedPtr->AllIncomingValuesWillBeAuthenticated()) {
    GetLogger(Verbose, !DebugActive)
        << "All incoming values will have authenticated versions\n";

    auto *BaseCopy = CloneInstruction(
        dyn_cast<Instruction>(ManagedPtr->GetBaseDefinition()));
    ManagedPtr->SetAuthenticatedPointer(BaseCopy);
    ManagedPtr->SetAuthenticatedIsCopyOfBase(true);
  }
}

void HAKCFunctionEnforcement::CreatePointerReplacements(
    const ManagedHAKCPointerP &ManagedPtr) const {
  const bool CreateAuthenticatedCopies =
      ManagedPtr->GetAuthenticatedUserCount() > 0;
  const bool CreateProtectedCopies =
      ManagedPtr->ProtectedPointerShouldBeCreated(Client);

  if (!CreateAuthenticatedCopies) {
    GetLogger(Verbose, !DebugActive)
        << "No Authenticated Users of " << *ManagedPtr
        << " so no authenticated clones will be created\n";
  }

  if (!CreateProtectedCopies) {
    GetLogger(Verbose, !DebugActive)
        << "CreateProtectedCopies is false for " << *ManagedPtr
        << " so no protected clones will be created\n";
  }

  if (!CreateAuthenticatedCopies && !CreateProtectedCopies) {
    return;
  }

  SmallVector<ManagedHAKCPointerUseP> SortedUses;

  GetLogger(Verbose, !DebugActive)
      << "\n\nCreating Clones of uses of " << *ManagedPtr << ":\n";
  ManagedPtr->GetAllUses(SortedUses);
  ManagedHAKCPointerUse::SortUses(SortedUses);
  for (auto &Use : SortedUses) {
    GetLogger(Verbose, !DebugActive) << "\t" << *Use << "\n";
  }
  SortedUses.clear();

  if (CreateAuthenticatedCopies) {
    GetLogger(Verbose, !DebugActive) << "Creating Authenticated Uses Copies\n";
    SortedUses.append(ManagedPtr->GetAuthenticatedUses().begin(),
                      ManagedPtr->GetAuthenticatedUses().end());
    ManagedHAKCPointerUse::SortUses(SortedUses);

    for (auto &SortedUse : SortedUses) {
      GetLogger(Verbose, !DebugActive)
          << "Creating authenticated clone of " << *SortedUse << "\n";
      auto *AuthenticatedUseClone =
          CreateAuthenticatedValue(ManagedPtr, *SortedUse);
      GetLogger(Verbose, !DebugActive)
          << "AuthenticatedUseClone: " << *AuthenticatedUseClone << "\n";
    }

    SortedUses.clear();
  }

  if (CreateProtectedCopies) {
    GetLogger(Verbose, !DebugActive) << "Creating Protected Uses Copies\n";
    SortedUses.append(ManagedPtr->GetProtectedUses().begin(),
                      ManagedPtr->GetProtectedUses().end());
    ManagedHAKCPointerUse::SortUses(SortedUses);

    for (auto &SortedUse : SortedUses) {
      GetLogger(Verbose, !DebugActive)
          << "Creating protected clone of " << *SortedUse << "\n";
      auto *ProtectedUseClone = CreateProtectedValue(ManagedPtr, *SortedUse);
      GetLogger(Verbose, !DebugActive)
          << "ProtectedUseClone: " << *ProtectedUseClone << "\n";
    }

    SortedUses.clear();
  }

  SortedUses.append(ManagedPtr->GetCloneUses().begin(),
                    ManagedPtr->GetCloneUses().end());
  ManagedHAKCPointerUse::SortUses(SortedUses);

  for (auto &SortedUse : SortedUses) {
    if (ManagedPtr->BaseIsAuthenticatedPointer()) {
      if (CreateProtectedCopies) {
        auto *ProtectedClone = CreateProtectedValue(ManagedPtr, *SortedUse);
        GetLogger(Verbose, !DebugActive)
            << "Created Protected clone: " << ProtectedClone << "\n";
      }
      if (CreateAuthenticatedCopies) {
        if (!CreateAuthenticatedValue(ManagedPtr, *SortedUse)) {
          getLogger(Fatal) << "Could not find Authenticated Value for "
                           << *SortedUse << "\n";
          throw std::exception();
        }
      }
    } else {
      if (CreateAuthenticatedCopies) {
        auto *AuthenticatedClone =
            CreateAuthenticatedValue(ManagedPtr, *SortedUse);
        GetLogger(Verbose, !DebugActive)
            << "Created Authenticated clone: " << AuthenticatedClone << "\n";
      }
      if (CreateProtectedCopies) {
        if (!CreateProtectedValue(ManagedPtr, *SortedUse)) {
          getLogger(Fatal) << "Could not find Protected Value for "
                           << *SortedUse << "\n";
          throw std::exception();
        }
      }
    }
  }
}

void HAKCFunctionEnforcement::CreatePointerUseClones(
    const ManagedHAKCPointerP &ManagedPtr) const {
  // look here
  GetLogger(Verbose, !DebugActive)
      << "\n\n"
      << __FUNCTION__ << " called for " << *ManagedPtr << "\n";

  CreatePointerReplacements(ManagedPtr);

  GetLogger(Verbose, !DebugActive) << "\n" << CurrentFunction;
  for (auto &UPtr : ManagedPtr->GetAuthenticatedUses()) {
    GetLogger(Verbose, !DebugActive) << *UPtr << ": ";
    if (auto *Replacement =
            PointerManager.FindAuthenticatedValue(UPtr->get())) {
      GetLogger(Verbose, !DebugActive) << Replacement;
    } else {
      GetLogger(Verbose, !DebugActive) << "nullptr";
    }
    GetLogger(Verbose, !DebugActive) << "\n";
  }
  GetLogger(Verbose, !DebugActive) << "\n\nProtectedUses:\n";
  for (auto &UPtr : ManagedPtr->GetProtectedUses()) {
    GetLogger(Verbose, !DebugActive) << *UPtr << ": ";
    if (auto *Replacement =
            PointerManager.FindAuthenticatedValue(UPtr->get())) {
      GetLogger(Verbose, !DebugActive) << Replacement;
    } else {
      GetLogger(Verbose, !DebugActive) << "nullptr";
    }
    GetLogger(Verbose, !DebugActive) << "\n";
  }
}

void HAKCFunctionEnforcement::TransformUses(
    const ManagedHAKCPointerP &ManagedPtr) const {
  GetLogger(Verbose, !DebugActive)
      << __FUNCTION__ << " called for " << *ManagedPtr << "\n";

  TransformClones(ManagedPtr);

  if (ManagedPtr->GetAuthenticatedUserCount() > 0) {
    TransformUseSet(ManagedPtr, ManagedPtr->GetAuthenticatedUses());
  } else {
    GetLogger(Verbose, !DebugActive)
        << "Not transforming Authenticated Pointer Replacements since user "
           "count of "
        << *ManagedPtr << " is 0\n";
  }
  if (ManagedPtr->ProtectedPointerShouldBeCreated(Client)) {
    TransformUseSet(ManagedPtr, ManagedPtr->GetProtectedUses());
  } else {
    GetLogger(Verbose, !DebugActive) << "Not transforming Protected Pointer "
                                        "Replacements since user count is 0\n";
  }
  GetLogger(Verbose, !DebugActive) << "Function after pointer transformation:\n"
                                   << *CurrentFunction << "\n";
}

void HAKCFunctionEnforcement::SetUseOperand(
    const ManagedHAKCPointerP &ManagedPtr, User *U, Value *Replacement,
    const ManagedHAKCPointerUse &PointerUse,
    const bool IsAuthenticatedUse) const {
  if (ManagedPtr->GetPurposefullyIgnored()) {
    getLogger(Fatal) << "Trying to set operand of purposefully ignored pointer "
                     << *ManagedPtr << "\n";
    throw std::exception();
  }

  GetLogger(Verbose, !DebugActive)
      << "Setting Operand " << std::to_string(PointerUse.getOperandNo())
      << " of " << (IsAuthenticatedUse ? "Authenticated" : "Protected");
  GetLogger(Verbose, !DebugActive)
      << " User " << U << " to be " << Replacement << " in function "
      << CurrentFunction->getName() << " for " << *ManagedPtr << "\n";

  if (PointerUse.getUser()->getValueID() != U->getValueID()) {
    if (CommonHAKCAnalysis::IsMultiSSAUser(PointerUse.getUser())) {
      GetLogger(Verbose, !DebugActive)
          << "Not changing operand of MultiSSA User\n";
      return;
    }
    getLogger(Fatal) << "Invalid PointerUse " << PointerUse << " for User "
                     << *U << " of " << *ManagedPtr << " in function\n"
                     << *CurrentFunction << "\n";
    throw std::exception();
  }

  U->setOperand(PointerUse.getOperandNo(), Replacement);
}

void HAKCFunctionEnforcement::UpdateUserCounts(
    const ManagedHAKCPointerP &ManagedPtr) const {
  GetLogger(Verbose, !DebugActive)
      << *ManagedPtr << " updating user counts of "
      << std::to_string(ManagedPtr->GetCloneUses().size()) << " uses\n";
  for (auto &CloneUse : ManagedPtr->GetCloneUses()) {
    auto *U = CloneUse->getUser();
    if (ManagedPtr->ValueIsManagedAndHasUsers(U, true)) {
      ManagedPtr->AddAuthenticatedUse(CloneUse);
    }
    if (ManagedPtr->ValueIsManagedAndHasUsers(U, false)) {
      ManagedPtr->AddProtectedUse(CloneUse);
    }
  }
}

void HAKCFunctionEnforcement::TransformClones(
    const ManagedHAKCPointerP &ManagedPtr) const {
  if (ManagedPtr->GetPurposefullyIgnored()) {
    GetLogger(Verbose, !DebugActive)
        << "Not transforming clones since " << *ManagedPtr
        << " is purposefully ignored\n";
    return;
  }

  GetLogger(Verbose, !DebugActive)
      << "Transforming clones created for " << *ManagedPtr << "\n";
  for (auto &CloneUse : ManagedPtr->GetCloneUses()) {
    GetLogger(Verbose, !DebugActive) << "\t" << *CloneUse << "\n";
  }

  SmallVector<ManagedHAKCPointerUseP> SortedUses(
      ManagedPtr->GetCloneUses().begin(), ManagedPtr->GetCloneUses().end());
  ManagedHAKCPointerUse::SortUses(SortedUses);
  for (const auto &CloneUse : SortedUses) {
    GetLogger(Verbose, !DebugActive)
        << "Handling Clone " << *CloneUse->getUser() << "\n";
    if (ManagedPtr->GetAuthenticatedUserCount() > 0) {
      GetLogger(Verbose, !DebugActive)
          << "Handling Authenticated Use of " << *CloneUse << "\n";
      auto *AuthenticatedVersion =
          PointerManager.FindAuthenticatedValue(CloneUse->getUser());
      if (!AuthenticatedVersion) {
        GetLogger(Verbose, !DebugActive)
            << "No authenticated value created for " << *CloneUse << "\n";
      } else {
        auto *AuthenticatedUser = dyn_cast<User>(AuthenticatedVersion);
        auto *Replacement = PointerManager.FindAuthenticatedValue(*CloneUse);
        if (!Replacement) {
          if (PointerManager.GetManagedPointer(CloneUse->get()) == nullptr ||
              ManagedPtr->UseIsManagedAndHasUsers(*CloneUse, true)) {
            getLogger(Fatal) << "Unable to find Authenticated replacement of "
                             << *CloneUse << "\n";
            PointerManager.PrintAuthenticatedValues();
            getLogger(Fatal) << "\n" << *CurrentFunction << "\n";
            throw std::exception();
          }
          GetLogger(Verbose, !DebugActive)
              << *CloneUse << " does not need authenticated operand replaced\n";
          continue;
        }
        if (!AuthenticatedUser) {
          getLogger(Fatal) << "AuthenticatedVersion is not a User: "
                           << AuthenticatedVersion << "\n"
                           << *CurrentFunction << "\n";
          throw std::exception();
        }

        SetUseOperand(ManagedPtr, AuthenticatedUser, Replacement, *CloneUse,
                      true);
      }
    }
    if (ManagedPtr->ProtectedPointerShouldBeCreated(Client)) {
      GetLogger(Verbose, !DebugActive)
          << "Handling Protected Use of " << *CloneUse << "\n";
      auto *ProtectedVersion =
          PointerManager.FindProtectedValue(CloneUse->getUser());
      if (!ProtectedVersion) {
        GetLogger(Verbose, !DebugActive)
            << "No protected value created for " << *CloneUse << "\n";
        continue;
      }
      auto *ProtectedUser = dyn_cast<User>(ProtectedVersion);
      auto *Replacement = PointerManager.FindProtectedValue(*CloneUse);
      if (!Replacement) {
        if (PointerManager.GetManagedPointer(CloneUse->get()) == nullptr ||
            ManagedPtr->UseIsManagedAndHasUsers(*CloneUse, false)) {
          getLogger(Fatal) << "Unable to find Protected replacement of "
                           << *CloneUse << "\n"
                           << CommonHAKCAnalysis::IsNECSymbol(&GetFunction(),
                                                              Client)
                           << "\n";
          PointerManager.PrintProtectedValues();
          throw std::exception();
        }
        GetLogger(Verbose, !DebugActive)
            << *CloneUse << " does not need protected operand replaced\n";
        continue;
      }
      if (!ProtectedUser) {
        getLogger(Fatal) << "ProtectedVersion is not a User: "
                         << ProtectedVersion << "\n"
                         << *CurrentFunction << "\n";
        throw std::exception();
      }

      SetUseOperand(ManagedPtr, ProtectedUser, Replacement, *CloneUse, false);
    }
  }
}

void HAKCFunctionEnforcement::TransformUseSet(
    const ManagedHAKCPointerP &ManagedPtr,
    SmallVectorImpl<ManagedHAKCPointerUseP> &UseSet) const {
  if (ManagedPtr->GetPurposefullyIgnored()) {
    GetLogger(Verbose, !DebugActive)
        << "Not transforming uses since " << *ManagedPtr
        << " is purposefully ignored\n";
    return;
  }

  const bool UseAuthenticatedValue =
      (&UseSet == &ManagedPtr->GetAuthenticatedUses());
  const StringRef ReplacementSource =
      UseAuthenticatedValue ? "Authenticated" : "Protected";

  ManagedHAKCPointerUse::SortUses(UseSet);

  GetLogger(Verbose, !DebugActive) << "Replacing the following operands with "
                                   << ReplacementSource << " values\n";
  for (auto &Use : UseSet) {
    GetLogger(Verbose, !DebugActive) << "\t" << *Use << "\n";
  }

  for (const auto &SortedUse : UseSet) {
    Value *Replacement = nullptr;
    Value *ReplacementUser = nullptr;
    if (UseAuthenticatedValue) {
      Replacement = PointerManager.FindAuthenticatedValue(*SortedUse);
      if (!CommonHAKCAnalysis::IsMultiSSAUser(SortedUse->getUser()) &&
          ManagedPtr->ValueIsManagedAndHasUsers(SortedUse->getUser(), true)) {
        ReplacementUser = SortedUse->getUser();
      } else {
        ReplacementUser =
            PointerManager.FindAuthenticatedValue(SortedUse->getUser());
      }
    } else {
      Replacement = PointerManager.FindProtectedValue(*SortedUse);
      if (!CommonHAKCAnalysis::IsMultiSSAUser(SortedUse->getUser()) &&
          ManagedPtr->ValueIsManagedAndHasUsers(SortedUse->getUser(), false)) {
        ReplacementUser = SortedUse->getUser();
      } else {
        ReplacementUser =
            PointerManager.FindProtectedValue(SortedUse->getUser());
      }
    }

    if (!Replacement) {
      getLogger(Fatal) << "Unable to find " << ReplacementSource
                       << " replacement of " << *SortedUse << "\n"
                       << *CurrentFunction << "\n";
      UseAuthenticatedValue ? PointerManager.PrintAuthenticatedValues()
                            : PointerManager.PrintProtectedValues();
      throw std::exception();
    }

    if (!ReplacementUser) {
      ReplacementUser = SortedUse->getUser();
    }

    if (!isa<User>(ReplacementUser)) {
      getLogger(Fatal) << "Invalid ReplacementUser: " << *ReplacementUser
                       << "\n";
      throw std::exception();
    }

    SetUseOperand(ManagedPtr, dyn_cast<User>(ReplacementUser), Replacement,
                  *SortedUse, UseAuthenticatedValue);
  }
}

Value *HAKCFunctionEnforcement::CreateAuthenticatedValueHelper(
    ManagedHAKCPointerUse &use) const {
  auto *Pointer = use.get();

  if (auto *AuthenticatedCopy = PointerManager.FindAuthenticatedValue(use)) {
    GetLogger(Verbose, !DebugActive)
        << "Returning Authenticated Copy " << AuthenticatedCopy << " for "
        << use << "\n";
    return AuthenticatedCopy;
  }

  if (auto *I = dyn_cast<Instruction>(Pointer)) {
    auto *Clone = CloneInstruction(I);
    GetLogger(Verbose, !DebugActive)
        << "Created Authenticated Copy of " << *I << ": " << Clone << "\n";
    return Clone;
  }
  return nullptr;
}

Value *HAKCFunctionEnforcement::CreateAuthenticatedValue(
    const ManagedHAKCPointerP &ManagedPtr, ManagedHAKCPointerUse &use) const {
  if (ManagedPtr->GetPurposefullyIgnored()) {
    return use.get();
  }

  auto *Authenticated = CreateAuthenticatedValueHelper(use);

  if (!Authenticated) {
    GetLogger(Verbose, !DebugActive)
        << "CreateAuthenticatedValueHelper returned null for " << use << "\n";
  } else {
    const auto *Pointer = use.get();
    SmallVector<ManagedHAKCPointerUseP> SortedUses(
        ManagedPtr->GetAuthenticatedUses().begin(),
        ManagedPtr->GetAuthenticatedUses().end());
    SortedUses.append(ManagedPtr->GetCloneUses().begin(),
                      ManagedPtr->GetCloneUses().end());
    ManagedHAKCPointerUse::SortUses(SortedUses);

    for (auto &PointerUse : SortedUses) {
      if (PointerUse->get() == Pointer) {
        PointerManager.AddAuthenticatedPointer(PointerUse, Authenticated);
      }
    }
  }

  return Authenticated;
}

Value *HAKCFunctionEnforcement::CreateProtectedValueHelper(
    ManagedHAKCPointerUse &PointerUse) const {
  auto *Pointer = PointerUse.get();

  if (auto *ProtectedValue = PointerManager.FindProtectedValue(PointerUse)) {
    GetLogger(Verbose, !DebugActive)
        << "Returning Protected Version " << *ProtectedValue << " for "
        << PointerUse << "\n";
    return ProtectedValue;
  }
  const auto ManagedPtr = PointerManager.GetManagedPointer(Pointer);
  if (ManagedPtr && ManagedPtr->GetBaseDefinition() == Pointer) {
    GetLogger(Verbose, !DebugActive) << "Returning ProtectedPointer\n";
    return ManagedPtr->GetProtectedPointer();
  }

  if (auto *I = dyn_cast<Instruction>(Pointer)) {
    const auto Clone = CloneInstruction(I);
    GetLogger(Verbose, !DebugActive)
        << "Created Protected Version of " << *I << ": " << Clone << "\n";
    return Clone;
  }
  return nullptr;
}

Value *HAKCFunctionEnforcement::CreateProtectedValue(
    const ManagedHAKCPointerP &ManagedPtr,
    ManagedHAKCPointerUse &HAKCUse) const {
  if (ManagedPtr->GetPurposefullyIgnored()) {
    return HAKCUse.get();
  }

  const auto Protected = CreateProtectedValueHelper(HAKCUse);
  if (!Protected) {
    GetLogger(Verbose, !DebugActive)
        << "CreateProtectedValue returned null for " << HAKCUse << "\n";
  } else {
    GetLogger(Verbose, !DebugActive) << "Found Protected " << Protected << "\n";
    const auto *Pointer = HAKCUse.get();
    SmallVector<ManagedHAKCPointerUseP> SortedUses(
        ManagedPtr->GetProtectedUses().begin(),
        ManagedPtr->GetProtectedUses().end());
    SortedUses.append(ManagedPtr->GetCloneUses().begin(),
                      ManagedPtr->GetCloneUses().end());
    ManagedHAKCPointerUse::SortUses(SortedUses);
    for (auto &PointerUse : SortedUses) {
      if (PointerUse->get() == Pointer) {
        PointerManager.AddProtectedPointer(PointerUse, Protected);
      }
    }
  }

  return Protected;
}

Instruction *HAKCFunctionEnforcement::CloneInstruction(Instruction *I) const {
  Instruction *Clone;
  if (!PointerManager.GetClones().contains(I)) {
    Clone = I->clone();
    Clone->insertBefore(I);
    PointerManager.GetClones()[I] = Clone;
  } else {
    Clone = PointerManager.GetClones()[I];
  }
  return Clone;
}

bool HAKCFunctionEnforcement::BaseDefinitionShouldBeTransferred(
    const ManagedHAKCPointerP &ManagedPtr) const {

  // TODO: Look here, maybe negate isuncompartmentalizedsymbol
  GetLogger(Verbose, !DebugActive)
      << "Calling BaseDefinitionShouldBeTransferred\n";
  if (CommonHAKCAnalysis::IsNECSymbol(&GetFunction(), Client) ||
      ManagedPtr->GetManuallyTransferred() ||
      ManagedPtr->GetPurposefullyIgnored()) {
    return false;
  }

  if (const auto *Call = dyn_cast<CallInst>(ManagedPtr->GetBaseDefinition())) {
    if (Call->isInlineAsm()) {
      return false;
    }
    if (!Call->getCalledFunction()) {
      return true;
    }
    auto *Callee = Call->getCalledFunction();
    return ModuleAnalysis.GetCommonAnalysis().IsAllocation(
               ManagedPtr->GetBaseDefinition()) ||
           !FunctionsAreInSameCompartment(CurrentFunction, Callee);
  }
  if (ManagedPtr->BaseIsAuthenticatedPointer()) {
    return ManagedPtr->ProtectedPointerShouldBeCreated(Client);
  }

  return false;
}
} // namespace llvm::hakc
