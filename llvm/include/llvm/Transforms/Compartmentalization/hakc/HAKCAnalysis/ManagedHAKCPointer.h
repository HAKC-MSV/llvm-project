//
// Created by de29664 on 9/18/23.
//

#ifndef HAKC_MANAGEDHAKCPOINTER_H
#define HAKC_MANAGEDHAKCPOINTER_H

#include "llvm/IR/Value.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

using namespace llvm;

namespace llvm::hakc {
class HAKCPointerBase {
protected:
  /**
   * The original source of a pointer
   */
  Value *BaseDefinition;

  /**
   * A pointer suitable for dereferencing
   */
  Value *AuthenticatedPointer;

  HAKCTypeP HAKCTy;

  unsigned ID;

public:
  HAKCPointerBase(Value *BaseDefinition, unsigned ID);

  virtual ~HAKCPointerBase() = default;

  Value *GetBaseDefinition() const;

  HAKCTypeP GetType() const;

  void SetType(const HAKCTypeP &NewHAKCTy);

  Value *GetAuthenticatedPointer() const;

  virtual void SetAuthenticatedPointer(Value *NewAuthenticatedPointer);

  unsigned GetID() const;

  friend bool operator==(const HAKCPointerBase &Lhs, Value *V) {
    return Lhs.GetBaseDefinition() == V;
  }

  friend bool operator!=(const HAKCPointerBase &Lhs, Value *V) {
    return !(Lhs == V);
  }

  friend bool operator==(Value *V, const HAKCPointerBase &Rhs) {
    return (Rhs == V);
  }

  friend bool operator!=(Value *V, const HAKCPointerBase &Rhs) {
    return !(V == Rhs);
  }

  friend bool operator==(const HAKCPointerBase &Lhs,
                         const HAKCPointerBase &Rhs) {
    return Lhs.GetBaseDefinition() == Rhs.GetBaseDefinition();
  }

  friend bool operator!=(const HAKCPointerBase &Lhs,
                         const HAKCPointerBase &Rhs) {
    return !(Lhs == Rhs);
  }
};
typedef std::shared_ptr<HAKCPointerBase> HAKCPointerBaseP;

class ManagedHAKCPointer;

/**
 * Stores Instruction and Operand to change
 */
class ManagedHAKCPointerUse {
public:
  ManagedHAKCPointerUse(ManagedHAKCPointer &P, User *User, unsigned OperandNo,
                        unsigned ID);

  User *getUser() const;

  void setUser(User *U);

  unsigned getOperandNo() const;

  Value *get() const;

  ManagedHAKCPointer &getManagedPtr() const;

  unsigned getID() const;

  static void SortUses(
      SmallVectorImpl<std::shared_ptr<ManagedHAKCPointerUse>> &ManagedUses);

protected:
  ManagedHAKCPointer &ManagedPtr;
  User *UserP;
  unsigned OperandNo;
  unsigned ID;

public:
  friend bool operator==(const ManagedHAKCPointerUse &Lhs, const Use &Rhs) {
    return Lhs.getUser() == Rhs.getUser() &&
           Lhs.getOperandNo() == Rhs.getOperandNo();
  }

  friend bool operator!=(const ManagedHAKCPointerUse &Lhs, const Use &Rhs) {
    return !(Lhs == Rhs);
  }

  friend bool operator==(const Use &Lhs, const ManagedHAKCPointerUse &Rhs) {
    return (Rhs == Lhs);
  }

  friend bool operator!=(const Use &Lhs, const ManagedHAKCPointerUse &Rhs) {
    return !(Lhs == Rhs);
  }

  friend bool operator==(const ManagedHAKCPointerUse &Lhs,
                         const ManagedHAKCPointerUse &Rhs) {
    return (Lhs.getUser() == Rhs.getUser()) &&
           (Lhs.getOperandNo() == Rhs.getOperandNo());
  }

  friend bool operator!=(const ManagedHAKCPointerUse &Lhs,
                         const ManagedHAKCPointerUse &Rhs) {
    return !(Lhs == Rhs);
  }
};

typedef std::shared_ptr<ManagedHAKCPointerUse> ManagedHAKCPointerUseP;
class HAKCPointerManager;

/**
 * A single managed Pointer.  Contains the original definition of the pointer,
 * an authenticated pointer suitable for dereferencing, and a protected pointer
 * to be used in function arguments.  The base definition and protected pointer
 * can be different if BaseDefinition is from an external function call.
 */
class ManagedHAKCPointer : public HAKCPointerBase {
protected:
  /**
   * A pointer belonging to the current function compartment
   */
  Value *ProtectedPointer;

  bool DebugActive;
  HAKCPointerManager &Manager;

  bool BaseIsAuthenticated;

  bool ManuallyTransferred;

  bool PurposefullyIgnored;

  bool AuthenticatedIsCopyOfBase;

  /**
   * Pointer uses and their replacements
   */
  SmallVector<ManagedHAKCPointerUseP> AuthenticatedUses;
  SmallVector<ManagedHAKCPointerUseP> ProtectedUses;
  SmallVector<ManagedHAKCPointerUseP> CloneUses;

  /**
   * Return the Authenticated version of HAKCUse
   * @param HAKCUse
   * @return
   */
  Value *CreateAuthenticatedValue(ManagedHAKCPointerUse &HAKCUse);

  /**
   * Return the Signed version of HAKCUse
   * @param HAKCUse
   * @return
   */
  Value *CreateProtectedValue(ManagedHAKCPointerUse &HAKCUse);

  void TransformUseSet(SmallVectorImpl<ManagedHAKCPointerUseP> &UseSet);

  void TransformClones();

  void CreatePointerReplacements();

  bool ComputeBasePointerAuthenticated();

  void GetAllUses(SmallVectorImpl<ManagedHAKCPointerUseP> &Result) const;

  void SetProtectedPointer(Value *NewProtectedPointer);

  void SetUseOperand(User *U, Value *Replacement,
                     const ManagedHAKCPointerUse &PointerUse,
                     bool IsAuthenticatedUse);

  bool AllIncomingValuesAreAuthenticated();

  bool AllIncomingValuesWillBeAuthenticated() const;

  void GetAllIncomingValues(SmallVectorImpl<Value *> &Result) const;

  bool PointerSetsShouldBeEqual() const;

  void SetPointerSetsToBeEqual();

  bool UseIsManagedAndHasUsers(const ManagedHAKCPointerUse &PointerUse,
                               bool CountAuthenticatedUsers);

  bool ValueIsManagedAndHasUsers(Value *V, bool CountAuthenticatedUsers);

public:
  ManagedHAKCPointer(Value *Pointer, HAKCPointerManager &Manager, unsigned ID);

  ~ManagedHAKCPointer() = default;

  Value *GetProtectedPointer() const;

  void CreateBaseAuthenticatedPointer();

  void CreatePointerUseClones();

  bool BaseDefinitionShouldBeTransferred();

  void TransformUses();

  void MaybeCreateProtectedPointer();

  void MaybeCreateBaseCopyPointer();

  void RegisterManualHAKCTransfer(CallBase *CallI);

  unsigned GetAuthenticatedUserCount() const;

  unsigned GetProtectedUserCount() const;

  bool BaseIsAuthenticatedPointer() const;

  bool IsAuthenticatedIsCopyOfBase() const;

  bool DetermineIfBasePointerIsAuthenticated();

  void AddAuthenticatedUse(ManagedHAKCPointerUseP &UPtr);

  void AddProtectedUse(ManagedHAKCPointerUseP &UPtr);

  void AddCloneUse(ManagedHAKCPointerUseP &UPtr);

  bool PointerSetsCanBeEqual() const;

  void UpdateUserCounts();

  void SetAuthenticatedPointer(Value *NewAuthenticatedPointer) override;

private:
  void InitBaseDefinitionInfo();

  void CheckPointerReplacement(Value *Old, Value *New,
                               StringRef TypeName) const;
};
} // namespace llvm::hakc

#endif // HAKC_MANAGEDHAKCPOINTER_H
