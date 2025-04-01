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
    HAKCCompartmentalizationPolicy &Policy)
    : ModuleAnalysis(ModuleAnalysis), Policy(Policy),
      PointerManager(
          *this, Policy,
          ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().OutputDebugInfo(
              F)),
      DebugActive(
          ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().OutputDebugInfo(
              F)),
      DTree(*F), CurrentFunction(F), SetupHasRun(false),
      CompartmentTransferCount(0) {}

void HAKCFunctionAnalysis::UpdateHAKCFunctionParameters() {
  if (CommonHAKCAnalysis::IsUncompartmentalizedSymbol(CurrentFunction,
                                                      Policy)) {
    return;
  }

  if (DebugActive) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Updating parameters for the following HAKC functions:\n";
    for (auto *CallI : HAKCFunctionCalls) {
      CommonHAKCAnalysis::getWriter(DebugActive) << CallI << "\n";
    }
  }

  auto *F = &getFunction();
  auto *TransferTarget = F;
  if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
    auto TransferTargetName =
        F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
    TransferTarget = F->getParent()->getFunction(TransferTargetName);
  }
  auto TargetCompartment =
      Policy.GetDivision(TransferTarget).GetHAKCCompartment();

  for (auto *CallI : HAKCFunctionCalls) {
    auto HAKCTransferFunction =
        GetModuleAnalysis().GetCommonAnalysis().GetHAKCTransferDefinition(
            CallI->getCalledFunction());
    if (HAKCTransferFunction) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Updating HAKC call parameters for " << CallI << "\n";
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Updating index " << HAKCTransferFunction->GetCompartmentIdIdx()
          << " ("
          << CallI->getArgOperand(
                 HAKCTransferFunction->GetCompartmentIdIdx()->getZExtValue())
          << ") to " << TargetCompartment.GetCompartmentIDValue() << "\n";

      UpdateHAKCFunctionParameters(CallI, TargetCompartment,
                                   HAKCTransferFunction);
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "After update call is " << CallI << "\n";
    } else {
      CommonHAKCAnalysis::getWriter(DebugActive)
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
    CommonHAKCAnalysis::getWriter(true)
        << "Compartment transfer target " << *Operand
        << " is not a pointer but of type " << *Operand->getType()
        << " in function\n"
        << getFunction() << "\n";
    throw std::exception();
  }
  auto HAKCPointer = PointerManager.GetManagedPointer(Operand);
  if (!HAKCPointer) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find Managed Pointer for " << Operand << "\n";
    throw std::exception();
  }

  bool IsData =
      !hakc::CommonHAKCAnalysis::valueIsReadonlyPtr(getDef(Operand, false));
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "isData: " << std::to_string(IsData) << " for " << Operand << "\n";

  Instruction *TransferCall;
  if (Size == nullptr) {
    TransferCall = getTransformer().CreateCompartmentTransfer(
        *HAKCPointer, I, &getFunction(), IsData);
  } else {
    TransferCall = getTransformer().CreateSizedCompartmentTransfer(
        *HAKCPointer, I, &getFunction(), IsData, Size);
  }
  // TODO: fix tictac
  // if (Size == nullptr) {
  //   // Non-sized transition
  // } else {
  //   TransferCall = getTransformer().CreateSizedEpochTransition(operand, I, &getFunction(), isData, Size);
  // }
  TransferCall->setDebugLoc(DebugLoc);
  CompartmentTransferCount++;
  if (DebugActive) {
    CommonHAKCAnalysis::getWriter(DebugActive) << "Created transfer for ";
    if (!IsData) {
      CommonHAKCAnalysis::getWriter(DebugActive) << Operand->getName();
    } else {
      CommonHAKCAnalysis::getWriter(DebugActive) << Operand;
    }
    CommonHAKCAnalysis::getWriter(DebugActive) << ": " << TransferCall << "\n";
  }
  return TransferCall;
}

// TODO: fix tictac
// Value *HAKCFunctionAnalysis::AddEpochDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location) {
//   auto *bitcast = getTransformer().CreateEpochDataAuthentication(signed_ptr, location);
//   return bitcast;
// }
//
// Value *HAKCFunctionAnalysis::AddEpochCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location) {
//   auto *SafePointer = getTransformer().CreateEpochCodeAuthentication(SignedPtr, Location);
//   return SafePointer;
// }

// void HAKCFunctionAnalysis::AssignFunctionEpochs() {
//   if (!SetupHasRun) {
//     setup();
//   }
//   auto CurrentSymbol = getTransformer().getSystemInformation().findSymbol(CurrentFunction);
//   auto CurrentEpochs = getTransformer().getSystemInformation().getApplicableEpochs(CurrentSymbol);
//   for (auto pointer: PointerManager.GetManagedPointers()) {
//     for (auto epoch : CurrentEpochs) {
//       std::string typeString = epoch->getTypeString();
//       auto CandidateType = pointer->GetTypeBitcastUse(typeString);
//       if (debug_output) {
//         CommonHAKCAnalysis::getWriter() << "Attempting to assign ";
//         CommonHAKCAnalysis::getWriter() << typeString << " epoch ";
//         CommonHAKCAnalysis::getWriter() << "to " << CurrentFunction->getName().str() << "\n";
//       }
//       if (CandidateType != nullptr) {
//         epoch->assignType(CandidateType);
//         function_epochs.insert({CandidateType, epoch});
//         if(debug_output) {
//           CommonHAKCAnalysis::getWriter() << "Assigned type: ";
//           CandidateType->print(CommonHAKCAnalysis::getWriter());
//           CommonHAKCAnalysis::getWriter() << "\n";
//           CommonHAKCAnalysis::getWriter() << "to epoch " << *epoch << "\n";
//         }
//       }
//     }
//   }
// }
//
// // Value is the one we're transfering/etc
// tictac_epoch_id_t HAKCFunctionAnalysis::GetEpoch(Value *V) {
//   Type *TypeKey = CommonHAKCAnalysis::GetStrippedTypeFromValue(V);
//   auto found = function_epochs.find(TypeKey);
//   if (found == function_epochs.end()) {
//     return 0;
//   } else {
//     return found->second->GetEpochID();
//   }
// }


/**
 * @brief Checks if a user is in the current function
 * @param user
 * @return True if the user is in the current function
 */
bool HAKCFunctionAnalysis::userInFunction(Value *User) {
  Function &F = getFunction();
  if (auto *I = dyn_cast<Instruction>(User)) {
    return &F == I->getFunction();
  }

  CommonHAKCAnalysis::getWriter(true) << "Unexpected user: " << User << "\n";
  throw std::exception();
}

/**
 * @brief Finds the dominating BasicBlock among users and ptr
 * @param ptr
 * @param users
 * @return
 */
BasicBlock *
HAKCFunctionAnalysis::findDominatorUseBlock(Value *Ptr,
                                            std::set<Instruction *> &Users) {
  Function &F = getFunction();
  BasicBlock *Dominator = nullptr;
  if (auto *I = dyn_cast<Instruction>(Ptr)) {
    if (!isa<AllocaInst>(Ptr)) {
      Dominator = I->getParent();
    }
  }

  std::set<BasicBlock *> BasicBlocks;

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
    Dominator = &F.getEntryBlock();
  }

  return Dominator;
}

/**
 * @brief Finds an insertion point for new instructions.
 * @param v The Value for which we want to insert a new Instruction
 * @param users The users of v
 * @return The location at which to insert a new Instruction
 */
Instruction *
HAKCFunctionAnalysis::FindUseInsertionPoint(Value *V,
                                            std::set<Instruction *> &users) {
  if (auto phi = dyn_cast<PHINode>(V)) {
    return phi->getParent()->getFirstNonPHIOrDbgOrLifetime();
  }
  if (DebugActive) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Finding insertion point for ";
    if (V->getName().empty()) {
      CommonHAKCAnalysis::getWriter(DebugActive) << V;
    } else {
      CommonHAKCAnalysis::getWriter(DebugActive) << V->getName();
    }
    CommonHAKCAnalysis::getWriter(DebugActive) << "\n";
  }

  BasicBlock *DominatorBlock = findDominatorUseBlock(V, users);
  if (!DominatorBlock) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find block for " << V << "\n"
        << getFunction();
    throw std::exception();
  }

  for (Instruction &I : *DominatorBlock) {
    if (&I == V) {
      return I.getNextNonDebugInstruction();
    } else if (!isa<PHINode>(&I) && users.find(&I) != users.end()) {
      return &I;
    }
  }

  return DominatorBlock->getTerminator();
}

/**
 * @brief Returns the current Function
 * @return
 */
Function &HAKCFunctionAnalysis::getFunction() { return *CurrentFunction; }

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
    CommonHAKCAnalysis::getWriter(true)
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
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find Managed Pointer for " << SignedPtr << "\n";
    throw std::exception();
  }
  auto *SafePointer =
      getTransformer().CreateCodeAuthentication(*HAKCPointer, Location);
  return SafePointer;
}

bool HAKCFunctionAnalysis::AddManagedPointer(Use &PointerUse) {
  if (!CommonHAKCAnalysis::IsPointerLikeType(PointerUse->getType())) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to add an invalid ManagedHAKCPointer: " << PointerUse << "\n"
        << getFunction() << "\n";
    throw std::exception();
  }
  auto Result = PointerManager.ManagePointer(PointerUse);
  if (Result) {
    auto ManagedPointer = PointerManager.GetManagedPointer(PointerUse.get());
    if (auto *PHII = dyn_cast<PHINode>(ManagedPointer->GetBaseDefinition())) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Definition is a PHI Node. Adding all non-null incoming "
             "members\n";
      for (auto &Incoming : PHII->incoming_values()) {
        CommonHAKCAnalysis::getWriter(DebugActive)
            << "Adding Incoming member " << Incoming << "\n";
        AddManagedPointer(Incoming);
      }
    }
  }
  return Result;
}

/**
 * @brief Creates all authenticated pointers, and clones any intermediate
 * pointer arithmetic between authentication and dereference
 */
void HAKCFunctionAnalysis::createAllAuthenticatedPointers() {
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Function prior to making authenticated copies:\n"
      << getFunction() << "\n";
  PointerManager.CreateAuthenticatedPointersAndAllClones();
}

/**
 * @brief Replace signed pointer dereferences with authenticated dereferences
 */
void HAKCFunctionAnalysis::transformPointerDereferences() {
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Function prior to transforming pointer dereferences\n"
      << getFunction() << "\n";
  PointerManager.TransformPointers();
}

Value *
HAKCFunctionAnalysis::AddSafePointerCreationAtLocation(Value *SignedPtr,
                                                       Instruction *Location) {
  auto HAKCPointer = PointerManager.GetManagedPointer(SignedPtr);
  if (!HAKCPointer) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find Managed Pointer for " << SignedPtr << "\n";
    throw std::exception();
  }
  auto *SafePtr = getTransformer().CreateSafePointer(*HAKCPointer, Location);
  CommonHAKCAnalysis::getWriter(DebugActive)
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
      CommonHAKCAnalysis::getWriter(DebugActive)
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
        } /*else if (arg.getOperandNo() == 0) {
            return true;
        }*/
      }
    } else if (call->getCalledFunction()) {
      CommonHAKCAnalysis::getWriter(DebugActive)
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

bool HAKCFunctionAnalysis::IsCallInIntrinsicSet(CallBase *Call,
                                                ArrayRef<Intrinsic::ID> IDs) {
  bool result = false;
  if (auto *intrinsic = dyn_cast<IntrinsicInst>(Call)) {
    auto IDToFind = intrinsic->getIntrinsicID();
    auto Search = [IDToFind](Intrinsic::ID ID) { return IDToFind == ID; };

    result = llvm::any_of(IDs, Search);
    if (DebugActive) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Intrinsic (" << IDToFind << ") from "
          << Call->getFunction()->getName() << " " << intrinsic;
      if (result) {
        CommonHAKCAnalysis::getWriter(DebugActive) << " is in { ";
      } else {
        CommonHAKCAnalysis::getWriter(DebugActive) << " is not in { ";
      }
      for (auto id : IDs) {
        CommonHAKCAnalysis::getWriter(DebugActive) << id << " ";
      }
      CommonHAKCAnalysis::getWriter(DebugActive) << "}\n";
    }
  }
  return result;
}

bool HAKCFunctionAnalysis::IsIntrinsicNeedingAuthentication(CallBase *Call) {
  Intrinsic::ID IntrinsicsNeedingAuth[] = {
      Intrinsic::IndependentIntrinsics::memcpy,
      Intrinsic::IndependentIntrinsics::memmove,
      Intrinsic::IndependentIntrinsics::memset};

  return IsCallInIntrinsicSet(Call, IntrinsicsNeedingAuth);
}

bool HAKCFunctionAnalysis::IsIntrinsicNeedingCloning(CallBase *Call) {
  Intrinsic::ID IntrinsicsNeedingCloning[] = {
      Intrinsic::IndependentIntrinsics::lifetime_start,
      Intrinsic::IndependentIntrinsics::lifetime_end,
  };
  return IsCallInIntrinsicSet(Call, IntrinsicsNeedingCloning);
}

bool HAKCFunctionAnalysis::IsIntrinsicToSkip(CallBase *Call) {
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
  for (auto &Val : PhiNode->incoming_values()) {
    Value *def = getDef(Val.get(), true);
    if (Val.get() == target || def == target) {
      return true;
    } else if (auto *phi = dyn_cast<PHINode>(def)) {
      if (visited.find(phi) != visited.end()) {
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
  if (auto *call = dyn_cast<CallInst>(I)) {
    handleCall(call);
  } else if (auto *load = dyn_cast<LoadInst>(I)) {
    handleLoad(load);
  } else if (auto *store = dyn_cast<StoreInst>(I)) {
    handleStore(store);
  } else if (auto *compare = dyn_cast<CmpInst>(I)) {
    handleComparison(compare);
  } else if (auto *binOp = dyn_cast<BinaryOperator>(I)) {
    handleBinaryOperator(binOp);
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
  } else if (isa<BitCastOperator>(user) || isa<GEPOperator>(user)) {
    return getUserInst(*user->user_begin());
  } else {
    CommonHAKCAnalysis::getWriter(true) << "Unexpected user: " << user << "\n";
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
    if (nodes.find(phiNode) != nodes.end()) {
      return true;
    }
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Examining PHI Node " << phiNode << " for Globals (" << nodes.size()
        << ")\n";
    nodes.insert(phiNode);
    for (auto &val : phiNode->incoming_values()) {
      Value *def = getDef(val.get(), false);
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "\tPHI Node value: " << val << "\n\t\tDef: " << def << "\n";
      if (!isa<GlobalValue>(def)) {
        if (isa<PHINode>(def)) {
          if (isPHIofGlobalsOnly(def, nodes)) {
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

Instruction *HAKCFunctionAnalysis::GetFinalAllocaDef(AllocaInst *Alloca) {
  return Alloca;
}

Value *HAKCFunctionAnalysis::getDef(Value *V, bool followLoad) {
  auto *def = GetModuleAnalysis().GetCommonAnalysis().getDef(V, followLoad);
  if (!def) {
    CommonHAKCAnalysis::getWriter(true)
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
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Handling " << *load->getOperandUse(LoadInst::getPointerOperandIndex())
      << " from Load " << *load << "\n";
  AddManagedPointer(load->getOperandUse(LoadInst::getPointerOperandIndex()));
}

/**
 * @brief Process a StoreInst for analysis
 * @param store
 */
void HAKCFunctionAnalysis::handleStore(StoreInst *store) {
  AddManagedPointer(store->getOperandUse(StoreInst::getPointerOperandIndex()));

  if (auto *globalValue = dyn_cast<GlobalValue>(store->getValueOperand())) {
    if (globalShouldBeTransferred(store->getOperandUse(0))) {
      GlobalArgumentUses[globalValue].insert(store);
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
            .ValueShouldBeReplacedWithTransfer(func, Policy)) {
      CommonHAKCAnalysis::getWriter(DebugActive)
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
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Checking comparison " << *compare << "\n";

  MaybeAddCompareToDirectUsers(compare);

  if (isa<ConstantPointerNull>(compare->getOperand(0)) ||
      isa<ConstantPointerNull>(compare->getOperand(1))) {
    CommonHAKCAnalysis::getWriter(DebugActive)
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
      CommonHAKCAnalysis::getWriter(DebugActive)
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
        CommonHAKCAnalysis::getWriter(DebugActive) << "Argument 0 needs auth\n";
      } else {
        CommonHAKCAnalysis::getWriter(DebugActive)
            << "Argument 0 does not need auth\n";
      }
      if (arg1NeedsAuth) {
        CommonHAKCAnalysis::getWriter(DebugActive) << "Argument 1 needs auth\n";
      } else {
        CommonHAKCAnalysis::getWriter(DebugActive)
            << "Argument 1 does not need auth\n";
      }
    }
    if (arg0NeedsAuth && arg1NeedsAuth) {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Both operands should be checked\n";
      AddManagedPointer(compare->getOperandUse(0));
      AddManagedPointer(compare->getOperandUse(1));
    } else {
      if (arg0NeedsAuth) {
        CommonHAKCAnalysis::getWriter(DebugActive)
            << "Registering argument 0\n";
        AddManagedPointer(compare->getOperandUse(0));
      } else {
        CommonHAKCAnalysis::getWriter(DebugActive)
            << "Argument 1 (" << compare->getOperand(1)
            << " ) already authenticated\n";
      }
      if (arg1NeedsAuth) {
        CommonHAKCAnalysis::getWriter(DebugActive)
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
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Checking binary op " << binOp << "\n";
  /* Both operators need to be pointers to skip operations like
   * ptr | 0xFFFF
   */
  if (argNeedsAuthentication(binOp->getOperandUse(0)) &&
      argNeedsAuthentication(binOp->getOperandUse(1))) {
    CommonHAKCAnalysis::getWriter(DebugActive) << "Registering both operands\n";
    AddManagedPointer(binOp->getOperandUse(0));
    AddManagedPointer(binOp->getOperandUse(1));
  }
}

/**
 * @brief Returns true if a GlobalValue should be transferred
 * @param globalValue
 * @return
 */
bool HAKCFunctionAnalysis::globalShouldBeTransferred(Use &globalValueArg) {
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

    if (auto *call = dyn_cast<CallInst>(globalValueArg.getUser())) {
      if (!GetModuleAnalysis().GetCommonAnalysis().FunctionIsAnalysisCandidate(
              call->getCalledFunction())) {
        return false;
      }
      return true;
    }

    return globalValue->getValueType()->isPointerTy();
  }

  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Arg " << globalValueArg.getOperandNo() << " (" << globalValueArg
      << " ) is not a GlobalValue\n";
  return false;
}

bool HAKCFunctionAnalysis::isCompartmentalizedFunction() {
  return CommonHAKCAnalysis::IsCompartmentalizedFunction(CurrentFunction,
                                                         Policy);
}

/**
 * @brief Processes a function call for analysis
 * @param call
 */
void HAKCFunctionAnalysis::handleCall(CallInst *call) {
  if (call->getCalledFunction() && IsIntrinsicToSkip(call)) {
    return;
  }

  if (GetModuleAnalysis().GetCommonAnalysis().IsHAKCFunction(
          call->getCalledFunction())) {
    HAKCFunctionCalls.insert(call);
  }

  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Handling call " << *call << "\n";

  if (GetModuleAnalysis().GetCommonAnalysis().ValueIsUsedAsPointer(call)) {
    for (auto &U : call->uses()) {
      if (AddManagedPointer(U)) {
        break;
      }
    }
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
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *call << " needs authenticated args\n";
    } else {
      CommonHAKCAnalysis::getWriter(DebugActive)
          << *call << " does not need authenticated args\n";
    }
  }

  if (call->isIndirectCall()) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Indirect call: " << *call << "\n";
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
      CommonHAKCAnalysis::getWriter(DebugActive)
          << "Argument " << *arg << " for " << *call
          << " does not need authentication\n";
    }
  } else if (needsAuthenticatedArgs) {
    for (auto &arg : call->args()) {
      if (argNeedsAuthentication(arg)) {
        AddManagedPointer(arg);
      }
      CommonHAKCAnalysis::getWriter(DebugActive)
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
    for (auto &arg : call->args()) {
      Value *def = getDef(arg.get(), false);
      if (auto *glob = dyn_cast<GlobalValue>(def)) {
        if (globalShouldBeTransferred(arg)) {
          CommonHAKCAnalysis::getWriter(DebugActive)
              << "Global " << glob->getName() << " used by " << *call << "\n";
          GlobalArgumentUses[glob].insert(call);
          AddManagedPointer(arg);
        }
        CommonHAKCAnalysis::getWriter(DebugActive)
            << "Global " << glob->getName() << " should not be transferred to "
            << *call << "\n";
      } else if (auto *phiNode = dyn_cast<PHINode>(def)) {
        for (auto &val : phiNode->incoming_values()) {
          Value *valDef = getDef(val.get(), false);
          if (auto *globVal = dyn_cast<GlobalValue>(valDef)) {
            if (globalShouldBeTransferred(val)) {
              CommonHAKCAnalysis::getWriter(DebugActive)
                  << "Global " << globVal->getName() << " used by " << *call
                  << "\n";
              GlobalArgumentUses[globVal].insert(call);
            }
            CommonHAKCAnalysis::getWriter(DebugActive)
                << "Global " << globVal->getName()
                << " should not be transferred to " << *call << "\n";
          }
        }
      } else if (isa<AllocaInst>(def)) {
        if (!GetModuleAnalysis()
                 .GetCommonAnalysis()
                 .FunctionIsAnalysisCandidate(call->getCalledFunction())) {
          CommonHAKCAnalysis::getWriter(DebugActive)
              << "Function called by " << *call
              << " is not an analysis candidate\n";
          continue;
        }
      }
    }
    if (call->getCalledFunction()) {
      auto TargetCompartment =
          Policy.GetDivision(call->getCalledFunction()).GetHAKCCompartment();
      if (!CommonHAKCAnalysis::IsUncompartmentalizedSymbol(
              call->getCalledFunction(), Policy)) {
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
    getFunction().setSection(getHAKCFunctionSectionName());
  }
}

std::string HAKCFunctionAnalysis::getHAKCFunctionSectionName() {
  std::string sectionName = HAKC_SECTION_PREFIX.str();
  auto Compartment = Policy.GetDivision(&getFunction()).GetHAKCCompartment();
  sectionName += std::to_string(Compartment.GetCompartmentIDValue());
  if (getFunction().getSection().empty()) {
    sectionName += ".text";
  } else {
    sectionName += getFunction().getSection().str();
  }
  return sectionName;
}

void HAKCFunctionAnalysis::setup() {
  if (!SetupHasRun) {
    auto Compartment = Policy.GetDivision(CurrentFunction).GetHAKCCompartment();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Running setup for " << getFunction().getName() << "\n"
        << getFunction() << "\nCompartmentID = "
        << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
    PointerManager.SetFunctionIsCompartmentalized(
        !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(CurrentFunction,
                                                         Policy));
    for (auto it = inst_begin(CurrentFunction); it != inst_end(CurrentFunction);
         ++it) {
      Instruction *inst = &*it;
      HandleInstruction(inst);
    }
    SetupHasRun = true;
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "setup has run for " << getFunction().getName() << "\n";
}

bool HAKCFunctionAnalysis::modifiedFunction() const {
  return !(PointerManager.empty() && GlobalArgumentUses.empty() &&
           NonKernelDirectFunctionCallSet.empty() &&
           PointerManager.GetTotalAdditions() == 0 &&
           CompartmentTransferCount == 0);
}

void HAKCFunctionAnalysis::
    CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls() {
  auto CurrentDivision = Policy.GetDivision(&getFunction());
  // now, get valid target from backing store
  Policy.GetValidTargets(CurrentDivision.GetHAKCCompartment());
  for (auto *call : NonKernelDirectFunctionCallSet) {
    auto TargetCompartment =
        Policy.GetDivision(call->getCalledFunction()).GetHAKCCompartment();
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

      for (auto *Target :
           CurrentDivision.GetHAKCCompartment().GetValidTargets()) {
        CommonHAKCAnalysis::getWriter(true)
            << "Testing Target Compartment "
            << (unsigned int)Target->getSExtValue() << " == "
            << (unsigned int)TargetCompartment.GetCompartmentID()
                   ->getSExtValue()
            << " -> "
            << (Target->getSExtValue() ==
                TargetCompartment.GetCompartmentID()->getSExtValue())
            << "\n";
        // comparing i32 1 and i64 1 returns false (LLVM constant ints), so cast
        // to int64_t
        if (Target->getSExtValue() ==
            TargetCompartment.GetCompartmentID()->getSExtValue()) {
          ValidTransition = true;
          break;
        }
      }

      if (!ValidTransition) {
        CommonHAKCAnalysis::getWriter(true)
            << "A direct Compartment transition from "
            << std::to_string(
                   CurrentDivision.GetHAKCCompartment().GetCompartmentIDValue())
            << " to "
            << std::to_string(TargetCompartment.GetCompartmentIDValue())
            << " is statically possible but not allowed in the"
            << " Compartmentalization Policy\n"
            << "A call from " << call->getFunction()->getName() << " to "
            << call->getCalledFunction()->getName() << " is not allowed\n";
        throw std::exception();
      }

      if (call->getCalledFunction()->isVarArg()) {
        auto *VariadicTransfer =
            getTransformer().CreateTransferToVariadic(call);
        call->setCalledFunction(VariadicTransfer);
      }
    }
  }
}

HAKCTransformer &HAKCFunctionAnalysis::getTransformer() {
  return GetModuleAnalysis().GetTransformer();
}

HAKCModuleAnalysis &HAKCFunctionAnalysis::GetModuleAnalysis() {
  return ModuleAnalysis;
}

void HAKCFunctionAnalysis::AddInstrumentation(bool RelocateSection) {
  if (CommonHAKCAnalysis::IsOutsideTransferFunc(&getFunction())) {
    throw std::exception();
  }

  if (!SetupHasRun) {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << __FUNCTION__ << " calling setup for " << getFunction().getName()
        << "\n";
    setup();
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "setup() has run for " << getFunction().getName() << "\n";

  CommonHAKCAnalysis::getWriter(DebugActive) << "Managed Pointers:\n";

  for (auto &HAKCPointer : PointerManager.ManagedPointers()) {
    CommonHAKCAnalysis::getWriter(DebugActive) << *HAKCPointer << "\n+++\n";
    HAKCPointer->DetermineIfBasePointerIsAuthenticated();
  }

  if (modifiedFunction()) {
    if (RelocateSection) {
      relocateFunctionSection();
    }
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "---- createMissingTransfers ----\n";
    createMissingTransfers();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "----- UpdateHAKCFunctionParameters ------\n";
    UpdateHAKCFunctionParameters();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "---- createAllAuthenticatedPointers ----\n";
    createAllAuthenticatedPointers();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "----- transformPointerDereferences ------\n";
    transformPointerDereferences();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "- ReplaceDirectFunctionUsesWithTransfers -\n";
    ReplaceDirectFunctionUsesWithTransfers();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "------ "
           "CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls "
           "-----\n";
    CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";

    CommonHAKCAnalysis::getWriter(DebugActive) << getFunction() << "\n";

    CommonHAKCAnalysis::VerifyFunction(&getFunction());
  } else {
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Function " << getFunction().getName() << " unmodified\n";
  }
}

Instruction *HAKCFunctionAnalysis::CreateMissingTransfer(
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
  for (auto *U : GlobalVar->users()) {
    if (auto *I = dyn_cast<Instruction>(U)) {
      if (I->getFunction() == &getFunction()) {
        UserInstructions.insert(I);
      }
    }
  }

  auto HAKCPointer = PointerManager.GetManagedPointer(GlobalVar);
  if (!HAKCPointer) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find Managed Pointer for " << GlobalVar << "\n";
    throw std::exception();
  }
  auto *InsertionPoint = FindUseInsertionPoint(GlobalVar, UserInstructions);
  return getTransformer().CreateSignWithDivision(
      *HAKCPointer, InsertionPoint, &getFunction(), !isa<Function>(GlobalVar));
}

void HAKCFunctionAnalysis::createMissingTransfers() {
  if (CommonHAKCAnalysis::IsUncompartmentalizedSymbol(CurrentFunction,
                                                      Policy)) {
    return;
  }
  CommonHAKCAnalysis::getWriter(DebugActive)
      << "Function prior to making transfers:\n"
      << getFunction() << "\n";
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
      CommonHAKCAnalysis::getWriter(true)
          << "Could not find Managed Pointer for " << NewValue << "\n";
      throw std::exception();
    }
    Replacement =
        getTransformer().CreateBitCast(*HAKCPointer, Oper->getDestTy(), I);
  } else if (V == OldValue) {
    Replacement = NewValue;
  } else {
    CommonHAKCAnalysis::getWriter(true) << "Could not find ";
    if (auto *F = dyn_cast<Function>(OldValue)) {
      CommonHAKCAnalysis::getWriter(true) << F->getName();
    } else {
      CommonHAKCAnalysis::getWriter(true) << OldValue << "\n";
    }
    CommonHAKCAnalysis::getWriter(true) << " in " << *I << "\n";
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
    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Changing operand " << std::to_string(ArgNo) << " to " << name
        << " for\n\t" << *I << "\n";
    transfer->setLinkage(Func->getLinkage());
    transfer->copyAttributesFrom(Func);
    ReplaceInstructionOperand(I, ArgNo, V, transfer);
  }
}

void HAKCFunctionAnalysis::ReplaceDirectFunctionUsesWithTransfers() {
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

void HAKCFunctionAnalysis::InstrumentCode() {
  AddInstrumentation(
      !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(&getFunction(), Policy));
}

void HAKCFunctionAnalysis::UpdateHAKCFunctionParameters(
    CallInst *CallI, const HAKCCompartment &TargetCompartment,
    const hakc::function_def_t &HAKCTransferFunction) {
  CommonHAKCAnalysis::getWriter(DebugActive)
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
      Division = Policy.GetDivision(TransferTarget);
    } else {
      Division = Policy.GetDivision(F);
    }

    CommonHAKCAnalysis::getWriter(DebugActive)
        << "Setting argument "
        << HAKCTransferFunction->GetDivisionIdIdx()->getZExtValue() << " to be "
        << Division << "\n";
    CallI->setOperand(HAKCTransferFunction->GetDivisionIdIdx()->getZExtValue(),
                      Division.GetDivisionID());
  }
}
} // namespace llvm::hakc
