//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the base class that tracks pointers that are created
/// during compartmentalization, e.g., a pointer that needs to be authenticated
/// before allowing data to cross compartment boundaries
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 11/14/23.
//

#ifndef HAKC_HAKCPOINTERMANAGER_H
#define HAKC_HAKCPOINTERMANAGER_H

#include "ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCLogger.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"
#include <memory>

namespace llvm::hakc {
using namespace llvm;

class HAKCFunctionAnalysis;
class HAKCFunctionEnforcement;

/**
 * In a function, there are two versions of each pointer that need to be
 * tracked, a pointer that can be dereferenced and a pointer that is protected.
 * Any dereference could be the result of arbitrary number of Instructions, and
 * we assume that pointer origins (e.g., function arguments or the result of a
 * call) return a protected pointer.  So Instructions that lead to a dereference
 * will dereference a protected pointer, so those Instructions need to be cloned
 * and modified to use the authenticated pointer.  This manager tracks those
 * clones, so exactly one is ever created.
 */
typedef std::shared_ptr<ManagedHAKCPointer> ManagedHAKCPointerP;
typedef std::vector<ManagedHAKCPointerP> ManagedHAKCPointerListType;
typedef std::vector<ManagedHAKCPointerUseP> ManagedHAKCPointerUseListType;

class HAKCPointerManager {
public:
  explicit HAKCPointerManager(Function* Function, HAKCModuleAnalysis &Analysis);

  virtual ~HAKCPointerManager() = default;

  bool ManagePointer(Use &U);

  bool PointerIsEligibleForManagement(const Use &U);

  iterator_range<ManagedHAKCPointerListType::iterator> ManagedPointers();

  HAKCModuleAnalysis &GetModuleAnalysis() const;

  Function &GetFunction() const;

  /**
   * Returns the ManagedHAKCPointer that corresponds to the definition V
   * @param V
   * @return
   */
  ManagedHAKCPointerP GetManagedPointer(Value *V);
  bool empty() const;

  Value *GetDef(Value *V) const;

  Value *FindAuthenticatedValue(Value *V);

  Value *FindProtectedValue(Value *V);

  Value *FindAuthenticatedValue(ManagedHAKCPointerUse &PointerUse);

  Value *FindProtectedValue(const ManagedHAKCPointerUse &PointerUse);

  static Value *FindManagedValue(const std::map<ManagedHAKCPointerUseP, Value *> &Storage, const ManagedHAKCPointerUse &PointerUse);

  void AddAuthenticatedPointer(const ManagedHAKCPointerUseP &PointerUse, Value *Replacement);

  void AddProtectedPointer(const ManagedHAKCPointerUseP &PointerUse, Value *Replacement);

  bool ValueWillBeAuthenticated(Value *V);

  unsigned GetDataAuthenticationsAdded() const;

  void IncrementDataAuthenticationsAdded() { DataAuthenticationsAdded++; }

  unsigned GetCodeAuthenticationsAdded() const;

  void IncrementCodeAuthenticationsAdded() { CodeAuthenticationsAdded++; }

  unsigned GetSafePointersAdded() const;

  void IncrementSafePointersAdded() { SafePointersAdded++; }

  unsigned GetClonesAdded() const;

  std::map<Instruction *, Instruction *>& GetClones() { return Clones; }

  unsigned GetTotalAdditions() const;

  void PrintProtectedValues() const;

  void PrintAuthenticatedValues() const;

  bool FunctionIsCompartmentalized() const;

  void SetFunctionIsCompartmentalized(bool FunctionIsCompartmentalized);

  bool DebugIsActive() const;

  bool IsIntrinsicNeedingCloning(CallBase *Call) const;

  bool IsCallInIntrinsicSet(CallBase *Call, ArrayRef<Intrinsic::ID> IDs) const;

  bool IsIntrinsicNeedingAuthentication(CallBase *Call) const;

  // Q: Should functions always be in the same compartment during analysis?
  virtual bool FunctionsAreInSameCompartment(Function *F, Function *G) {return true;}

protected:
  /**
   * The set of pointers under management
   */
  ManagedHAKCPointerListType ManagedPointersList;

  std::map<ManagedHAKCPointerUseP, Value *> AuthenticatedValues;
  std::map<ManagedHAKCPointerUseP, Value *> ProtectedValues;
  std::map<Instruction *, Instruction *> Clones;

  ManagedHAKCPointerUseListType AnalyzedUses;

  Function* CurrentFunction;
  HAKCModuleAnalysis &ModuleAnalysis;

  unsigned DataAuthenticationsAdded = 0;
  unsigned CodeAuthenticationsAdded = 0;
  unsigned SafePointersAdded = 0;

  bool IsCompartmentalized = false;
  bool DebugActive;

  void AddHAKCPointerReplacement(const ManagedHAKCPointerUseP &PtrUse,
                                 Value *Replacement,
                                 bool AddingAuthenticatedReplacements);

  static Value *FindManagedValue(const std::map<ManagedHAKCPointerUseP, Value *> &Storage, const Value *Target);

  bool ManageNewPointer(Use &U);

  void ClassifyAllUsesOfDefinition(Value *Definition, ManagedHAKCPointer &ManagedPointer);

  bool UseIsAnalyzed(ManagedHAKCPointerUse &MangedPtrUse);

  bool UseShouldBeIgnored(const Use &U) const;

  static bool UseShouldBeCloned(const Use &U);

  bool UseShouldUtilizeAuthenticatedPointer(const Use &U) const;

  bool UseShouldUtilizeSignedBasePointer(const Use &U) const;

  bool IsClonedUseNeedingAdditionalClassification(const Use &U);

  static void PrintManagedValues(const std::map<ManagedHAKCPointerUseP, Value *> &Storage);

  Value *FindManagedPointerReplacement(Value *Target, bool ReturnAuthenticatedPointer);

  ManagedHAKCPointerUseP CreateManagedPointerUse(ManagedHAKCPointer &ManagedPointer, User *U, unsigned OperandNo);

  bool IsConstantExprUsedInKernelCall(User *U) const;

  bool isPHIofGlobalsOnly(Value *ptr, std::set<PHINode *> &nodes);

  bool IsPHIOfGlobalsOnly(Value *V);

  HAKCLogger &GetLogger(HAKCLogLevel log_level, bool suppress_output) const;

private:
  unsigned CurrentPointerID = 0;
  unsigned CurrentPointerUseID = 0;
};

} // namespace llvm::hakc

#endif // HAKC_HAKCPOINTERMANAGER_H
