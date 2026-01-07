//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the function analysis subclass class of common analysis.
/// It contains specific functionality related to compartmentalization analysis
/// at the function level.
///
//===----------------------------------------------------------------------===//
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

public:
  virtual ~HAKCFunctionAnalysis() = default;

  HAKCFunctionAnalysis(Function *F, HAKCModuleAnalysis& ModuleAnalysis);

  virtual void setup();

  HAKCTypeIdentifier& GetTypeIdentifier() const;

  HAKCLogger &getLogger(HAKCLogLevel log_level) const {return CommonHAKCAnalysis::getLogger(log_level, !DebugActive);}

  HAKCLogger &GetLogger(HAKCLogLevel log_level, bool suppress_output) const {return CommonHAKCAnalysis::getLogger(log_level, suppress_output);}

  void TypeUseAnalysis();

  void TypeUseHandleCall(const ManagedHAKCPointerUseP &CallUse) const;

  void TypeUseHandleLoad(const ManagedHAKCPointerUseP &LoadUse) const;

  void TypeUseHandleStore(const ManagedHAKCPointerUseP &StoreUse) const;

  bool modifiedFunction() const;

  Value *getDef(Value *, bool) const;

  Function &GetFunction() const {return *CurrentFunction;}

  static Instruction *GetFinalAllocaDef(AllocaInst *Alloca);

  HAKCModuleAnalysis &GetModuleAnalysis() const;

  void AddPermissionUse(const ManagedHAKCPointer &ManagedPointer, TypePerms perm) const;

  bool IsIntrinsicToSkip(CallBase *Call) const;
protected:
  HAKCModuleAnalysis &ModuleAnalysis;
  HAKCPointerManager _PointerManager;
  HAKCPointerManager &PointerManager = _PointerManager;
  bool DebugActive;

  /**
   * @brief Global variables used as function arguments
   */
  std::map<GlobalValue *, std::set<Instruction *>> GlobalArgumentUses;

  std::set<CallInst *> DirectFunctionCallSet;

  std::set<CallInst *> HAKCFunctionCalls;

  Function *CurrentFunction;

  /* All functions used in comparisons and function call arguments should be
   * transfer functions, so replace direct uses with transfer functions */
  std::set<Instruction *> directFunctionUsers;

  unsigned CompartmentTransferCount;

  bool argNeedsAuthentication(Use &arg);

  bool phiNodeUsesValue(PHINode *phiNode, Value *target,
                        std::set<PHINode *> &visited);

  virtual void HandleInstruction(Instruction *I);

  Instruction *getUserInst(User *user);

  void handleLoad(LoadInst *load);

  void handleCall(CallInst *call);

  void handleStore(StoreInst *Store);

  void handleBinaryOperator(BinaryOperator *binOp);

  bool globalShouldBeTransferred(Use &globalValueArg) const;

  bool AddManagedPointer(Use &PointerUse);

};

class HAKCFunctionEnforcement : public HAKCFunctionAnalysis {
  public:

  explicit HAKCFunctionEnforcement(Function *F, HAKCModuleAnalysis& ModuleAnalysis, HAKCTransformer &Transformer, HAKCServerClientBase &Client);

  ~HAKCFunctionEnforcement() override = default;

  void setup() override;

  void HandleInstruction(Instruction *I) override;

  void handleComparison(CmpInst *compare);

  void UpdateHAKCFunctionParameters() const;

  void UpdateHAKCFunctionParameters(CallInst *CallI, const HAKCCompartment &TargetCompartment, const function_def_t &HAKCTransferFunction) const;

  void CheckCompareOperandForDirectFunctionUse(CmpInst *CmpI, unsigned OpNo);

  void CheckForValidCompartmentTransitionAndUpdateIntraCompartmentCalls();

  void MaybeAddCompareToDirectUsers(CmpInst *CmpI);

  Instruction *FindUseInsertionPoint(Value *V, const std::set<Instruction *> &Users) const;

  std::string getHAKCFunctionSectionName() const;

  Instruction *addCompartmentTransferCall(Value *Operand, const DebugLoc &DebugLoc, Instruction *I, ConstantInt *Size);

  void ReplaceDirectFunctionUsesWithTransfers();

  void transformPointerDereferences();

  void createMissingTransfers();

  void AddInstrumentation();

  void relocateFunctionSection() const;

  void createAllAuthenticatedPointers();

  HAKCTransformer &GetTransformer() const;

  Instruction *CreateMissingTransfer(Instruction *PointerNeedingTransfer);

  Instruction *SignGlobalPointerWithColor(GlobalValue *GlobalVar);

  void ReplaceInstructionOperand(Instruction *I, unsigned ArgNo, Value *OldValue, Value *NewValue);

  void CheckAndReplaceArgument(Value *V, Instruction *I, const unsigned int ArgNo);

protected:
  HAKCTransformer &Transformer;
  HAKCServerClientBase &Client;
  /**
   * @brief Used for ideal placement of authentication checks and cloned
   * instructions
   */
  DominatorTree DTree;

  bool IsUncompartmentalizedSymbol();

  bool IsUncompartmentalizedSymbol() const;

  bool IsUncompartmentalizedSymbol(GlobalValue *GV);

  bool IsUncompartmentalizedSymbol(GlobalValue *GV) const;

  bool isCompartmentalizedFunction();

  bool isCompartmentalizedFunction() const;

  void CreateBaseAuthenticatedPointer(const ManagedHAKCPointerP & ManagedPtr);

  void CreatePointerUseClones(const ManagedHAKCPointerP & ManagedPtr);

  void TransformPointers();

  void TransformUses(const ManagedHAKCPointerP &Use);

  void MaybeCreateProtectedPointer(const ManagedHAKCPointerP & ManagedPtr);

  void MaybeCreateBaseCopyPointer(const ManagedHAKCPointerP & ManagedPtr);

  void UpdateUserCounts(const ManagedHAKCPointerP &ManagedPtr);

  // void SetAuthenticatedPointer(Value *NewAuthenticatedPointer);

  /**
  * Return the Authenticated version of HAKCUse
  * @param use
  * @return
  */
  Value *CreateAuthenticatedValue(const ManagedHAKCPointerP &ManagedPtr, ManagedHAKCPointerUse &use);
  Value *CreateAuthenticatedValueHelper(ManagedHAKCPointerUse &use);

  /**
  * Return the Signed version of HAKCUse
  * @param use
  * @return
  */
  Value *CreateProtectedValue(const ManagedHAKCPointerP &ManagedPtr, ManagedHAKCPointerUse &use);
  Value *CreateProtectedValueHelper(ManagedHAKCPointerUse &HAKCUse);

  void TransformUseSet(const ManagedHAKCPointerP &ManagedPtr, SmallVectorImpl<ManagedHAKCPointerUseP> &UseSet);

  void TransformClones(const ManagedHAKCPointerP &ManagedPtr);

  void CreatePointerReplacements(const ManagedHAKCPointerP & ManagedPtr);

  void SetUseOperand(const ManagedHAKCPointerP &ManagedPtr,
                  User *U, Value *Replacement,
                    const ManagedHAKCPointerUse &PointerUse,
                    bool IsAuthenticatedUse);

  void SetPointerSetsToBeEqual(const ManagedHAKCPointerP & ManagedPtr);

  Value *AddDataAuthCheckAtLocation(Value *SignedPtr, Instruction *location);

  Value *AddCodeAuthCheckAtLocation(Value *SignedPtr, Instruction *Location);

  Value *AddSafePointerCreationAtLocation(Value *SignedPtr, Instruction *Location);

  BasicBlock *findDominatorUseBlock(Value *Ptr, const std::set<Instruction *> &Users) const;

  bool userInFunction(Value *User) const;

  void CreateAuthenticatedPointersAndAllClones();

  void CreateAllTransfers();

  Value *CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation);

  Value *CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation);

  bool FunctionsAreInSameCompartment(Function *F, Function *G) const;

  Instruction *CloneInstruction(Instruction *I);

  bool BaseDefinitionShouldBeTransferred(const ManagedHAKCPointerP &ManagedPtr) const;

  void WriteBuggyFunctionToFile();

};

} // namespace llvm::hakc

#endif // HAKC_HAKCFUNCTIONANALYSIS_H
