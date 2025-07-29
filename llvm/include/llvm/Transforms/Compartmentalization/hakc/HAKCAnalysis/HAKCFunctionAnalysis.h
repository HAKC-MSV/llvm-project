//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCFUNCTIONANALYSIS_H
#define HAKC_HAKCFUNCTIONANALYSIS_H

#include "llvm/IR/Dominators.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCPointerManager.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

namespace llvm::hakc {

class HAKCModuleAnalysis;
class HAKCModuleTransform;

class CommonHAKCAnalysis;

class HAKCPointerManager;

/**
 * @brief This pass does the following:
 * 1. Find pointers that should be authenticated, add a call to authenticate,
 * and then transform all instructions that dereference the pointer.  Note, that
 * the *actual* dereference might be the result of arbitrary number of GEPs, in
 * which case all intermediate GEP computations are cloned using the
 * authenticated pointer.
 *
 * 2. Insert a validity check for all indirect calls, and add a transfer of all
 * pointer arguments to the target address before the indirect call. Immediately
 * after the indirect call, the pointer arguments are transferred back to their
 * original clique.
 *
 * 3. Sign global variable pointers passed to functions so that subsequent
 * authentications pass.
 *
 * The current policy is to pass along signed pointers to functions, which could
 * then authenticate pointers which the caller has already authenticated.  This
 * might be redundant, and a source of overhead.
 */
class HAKCFunctionAnalysis {
protected:
  HAKCModuleAnalysis &ModuleAnalysis;
  HAKCTransformer &Transformer;
  HAKCCompartmentalizationPolicy &Policy;
  HAKCPointerManager PointerManager;
  bool DebugActive;

  /**
   * @brief Global variables used as function arguments
   */
  std::map<GlobalValue *, std::set<Instruction *>> GlobalArgumentUses;

  /**
   * @brief Used for ideal placement of authentication checks and cloned
   * instructions
   */
  DominatorTree DTree;

  std::set<CallInst *> NonKernelDirectFunctionCallSet;

  std::set<CallInst *> HAKCFunctionCalls;

  Function *CurrentFunction;

  bool SetupHasRun;

  /* All functions used in comparisons and function call arguments should be
   * transfer functions, so replace direct uses with transfer functions */
  std::set<Instruction *> directFunctionUsers;

  unsigned CompartmentTransferCount;

  Instruction *addCompartmentTransferCall(Value *operand,
                                          const DebugLoc &debugLoc,
                                          Instruction *I, ConstantInt *Size);

  bool userInFunction(Value *user) const;

  BasicBlock *findDominatorUseBlock(Value *ptr, std::set<Instruction *> &users) const;

  void createAllAuthenticatedPointers();

  void createMissingTransfers();

  void transformPointerDereferences();

  bool argNeedsAuthentication(Use &arg) const;

  bool phiNodeUsesValue(PHINode *phiNode, Value *target,
                        std::set<PHINode *> &visited);

  void HandleInstruction(Instruction *I);

  Instruction *getUserInst(User *user);

  bool isPHIofGlobalsOnly(Value *ptr, std::set<PHINode *> &nodes);

  void handleLoad(LoadInst *load);

  void handleComparison(CmpInst *compare);

  void handleCall(CallInst *call);

  void handleStore(StoreInst *Store);

  void handleBinaryOperator(BinaryOperator *binOp);

  bool globalShouldBeTransferred(Use &globalValueArg) const;

  void relocateFunctionSection();

  virtual std::string getHAKCFunctionSectionName();

  void CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();

  HAKCTransformer &getTransformer() const;

  bool AddManagedPointer(Use &PointerUse);

  void ReplaceInstructionOperand(Instruction *I, unsigned ArgNo,
                                 Value *OldValue, Value *NewValue);

  void ReplaceDirectFunctionUsesWithTransfers();

  void CheckCompareOperandForDirectFunctionUse(CmpInst *CmpI, unsigned OpNo);

  void MaybeAddCompareToDirectUsers(CmpInst *CmpI);

  void UpdateHAKCFunctionParameters() const;

  void AddInstrumentation(bool RelocateSection);

  void CheckAndReplaceArgument(Value *V, Instruction *I, unsigned ArgNo);

  bool IsCallInIntrinsicSet(CallBase *Call, ArrayRef<Intrinsic::ID> IDs) const;

  void UpdateHAKCFunctionParameters(
      CallInst *CallI, const HAKCCompartment &TargetCompartment,
      const hakc::function_def_t &HAKCTransferFunction) const;

public:
  virtual ~HAKCFunctionAnalysis() = default;

  HAKCFunctionAnalysis(Function *F, HAKCModuleAnalysis &ModuleAnalysis, HAKCTransformer &Transformer,
                       HAKCCompartmentalizationPolicy &Policy);

  HAKCTypeIdentifier& GetTypeIdentifier() const;

  HAKCLogger &getLogger(HAKCLogLevel log_level) const;

  void TemporalAnalysis();

  void TemporalAnalysisHandleCall(ManagedHAKCPointerUseP Use);

  void TemporalAnalysisHandleLoad(ManagedHAKCPointerUseP Use);

  void TemporalAnalysisHandleStore(ManagedHAKCPointerUseP Use);

  bool modifiedFunction() const;

  void InstrumentCode();

  void setup();

  Value *getDef(Value *, bool) const;

  Instruction *FindUseInsertionPoint(Value *v, std::set<Instruction *> &users) const;

  Value *AddDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location);

  Value *AddCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location);

  Value *AddSafePointerCreationAtLocation(Value *SignedPtr,
                                          Instruction *Location);

  bool isCompartmentalizedFunction() const;

  Function &GetFunction() const;

  Instruction *CreateMissingTransfer(Instruction *PointerNeedingTransfer);

  virtual Instruction *SignGlobalPointerWithColor(GlobalValue *GlobalVar);

  Instruction *GetFinalAllocaDef(AllocaInst *Alloca);

  bool IsPHIOfGlobalsOnly(Value *V);

  HAKCModuleAnalysis &GetModuleAnalysis() const;

  bool IsIntrinsicNeedingAuthentication(CallBase *Call) const;

  bool IsIntrinsicNeedingCloning(CallBase *Call) const;

  void AddPermissionUse(const ManagedHAKCPointer &ManagedPointer, TypePerms perm) const;

  bool IsIntrinsicToSkip(CallBase *Call) const;
  // TicTac code
  void AssignFunctionEpochs();

  tictac_epoch_id_t GetEpoch(Value *V);
  // std::map<Type*, std::shared_ptr<TICTACEpoch>> function_epochs;
  Value *AddEpochDataAuthCheckAtLocation(Value *signed_ptr, Instruction *location);
  Value *AddEpochCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location);

  HAKCWriter &getWriter();

  HAKCWriter &getWriter(HAKCLogLevel log_level);

};
} // namespace llvm::hakc

#endif // HAKC_HAKCFUNCTIONANALYSIS_H
