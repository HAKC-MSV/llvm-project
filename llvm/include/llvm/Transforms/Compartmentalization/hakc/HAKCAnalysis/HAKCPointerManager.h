//
// Created by de29664 on 11/14/23.
//

#ifndef HAKC_HAKCPOINTERMANAGER_H
#define HAKC_HAKCPOINTERMANAGER_H

#include "ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCLogger.h"
#include <memory>

namespace llvm::hakc {
using namespace llvm;

class HAKCFunctionAnalysis;

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
  explicit HAKCPointerManager(HAKCFunctionAnalysis &Analysis,
                              HAKCCompartmentalizationPolicy &Policy,
                              bool DebugActive);

  bool ManagePointer(Use &U);

  bool PointerIsEligibleForManagement(Use &U);

  iterator_range<ManagedHAKCPointerListType::iterator> ManagedPointers();

  HAKCFunctionAnalysis &GetFunctionAnalysis() const;

  /**
   * Returns the ManagedHAKCPointer that corresponds to the definition V
   * @param V
   * @return
   */
  ManagedHAKCPointerP GetManagedPointer(Value *V);
  bool empty() const;

  Value *GetDef(Value *V) const;

  /**
   * Return the Authenticated version of Pointer
   * @param Pointer
   * @param Debug
   * @return
   */
  Value *CreateAuthenticatedValue(ManagedHAKCPointerUse &PointerUse);

  Value *CreateProtectedValue(ManagedHAKCPointerUse &PointerUse);

  Value *FindAuthenticatedValue(Value *V);

  Value *FindProtectedValue(Value *V);

  Value *FindAuthenticatedValue(ManagedHAKCPointerUse &PointerUse);

  Value *FindProtectedValue(const ManagedHAKCPointerUse &PointerUse);

  static Value *
  FindManagedValue(const std::map<ManagedHAKCPointerUseP, Value *> &Storage,
                   const ManagedHAKCPointerUse &PointerUse);

  /**
   * Create authenticated versions of the ManagedHAKCPointer set
   * @param Debug
   */
  void CreateAuthenticatedPointersAndAllClones();

  void CreateAllTransfers();

  void TransformPointers();

  void AddAuthenticatedPointer(ManagedHAKCPointerUseP &PointerUse,
                               Value *Replacement);

  void AddProtectedPointer(ManagedHAKCPointerUseP &PointerUse,
                           Value *Replacement);

  bool ValueWillBeAuthenticated(Value *V);

  unsigned GetDataAuthenticationsAdded() const;

  unsigned GetCodeAuthenticationsAdded() const;

  unsigned GetSafePointersAdded() const;

  unsigned GetClonesAdded() const;

  unsigned GetTotalAdditions() const;

  void PrintProtectedValues() const;

  void PrintAuthenticatedValues() const;

  bool FunctionIsCompartmentalized() const;

  void SetFunctionIsCompartmentalized(bool FunctionIsCompartmentalized);

  HAKCCompartmentalizationPolicy &GetPolicy() const;

  Instruction *CloneInstruction(Instruction *I);

  Value *CreateSafePointerAtLocation(Value *Pointer,
                                     Instruction *InsertLocation);

  Value *CreateAuthenticationAtLocation(Value *Pointer,
                                        Instruction *InsertLocation);

  bool DebugIsActive() const;

protected:
  /**
   * The set of pointers under management
   */
  ManagedHAKCPointerListType ManagedPointersList;

  std::map<ManagedHAKCPointerUseP, Value *> AuthenticatedValues;
  std::map<ManagedHAKCPointerUseP, Value *> ProtectedValues;
  std::map<Instruction *, Instruction *> Clones;

  ManagedHAKCPointerUseListType AnalyzedUses;

  HAKCFunctionAnalysis &HAKCAnalysis;
  HAKCCompartmentalizationPolicy &Policy;

  unsigned DataAuthenticationsAdded;
  unsigned CodeAuthenticationsAdded;
  unsigned SafePointersAdded;

  bool IsCompartmentalized;
  bool DebugActive;

  void AddHAKCPointerReplacement(ManagedHAKCPointerUseP &PtrUse,
                                 Value *Replacement,
                                 bool AddingAuthenticatedReplacements);

  Value *FindManagedValue(std::map<ManagedHAKCPointerUseP, Value *> &Storage,
                          Value *Target);

  bool ManageNewPointer(Use &U);

  void ClassifyAllUsesOfDefinition(Value *Definition,
                                   ManagedHAKCPointer &ManagedPointer);

  bool UseIsAnalyzed(ManagedHAKCPointerUse &MangedPtrUse);

  bool UseShouldBeIgnored(Use &U);

  static bool UseShouldBeCloned(Use &U);

  bool UseShouldUtilizeAuthenticatedPointer(Use &U) const;

  bool UseShouldUtilizeSignedBasePointer(Use &U) const;

  bool IsClonedUseNeedingAdditionalClassification(Use &U);

  static void
  PrintManagedValues(const std::map<ManagedHAKCPointerUseP, Value *> &Storage);

  Value *FindManagedPointerReplacement(Value *Target,
                                       bool ReturnAuthenticatedPointer);

  ManagedHAKCPointerUseP
  CreateManagedPointerUse(ManagedHAKCPointer &ManagedPointer, User *U,
                          unsigned OperandNo);

  bool IsConstantExprUsedInKernelCall(User *U) const;

  HAKCLogger &GetLogger(HAKCLogLevel log_level, bool suppress_output) const;

private:
  unsigned CurrentPointerID;
  unsigned CurrentPointerUseID;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCPOINTERMANAGER_H
