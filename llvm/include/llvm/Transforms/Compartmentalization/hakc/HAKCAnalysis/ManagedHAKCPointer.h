//
// Created by de29664 on 9/18/23.
//

#ifndef HAKC_MANAGEDHAKCPOINTER_H
#define HAKC_MANAGEDHAKCPOINTER_H

#include <set>
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

using namespace llvm;

namespace llvm::hakc {
    class ManagedHAKCPointerUse;

    using ManagedHAKCPointerUseP = std::shared_ptr<ManagedHAKCPointerUse>;

    class ManagedHAKCPointer;

    using ManagedHAKCPointerP = std::shared_ptr<ManagedHAKCPointer>;

    class HAKCPointerManager;

    class HAKCPointerBase;

    using HAKCPointerBaseP = std::shared_ptr<HAKCPointerBase>;

    /**
     * Stores Instruction and Operand to change
     */
    class ManagedHAKCPointerUse {
    public:
        ManagedHAKCPointerUse(ManagedHAKCPointerP P, User *User, unsigned OperandNo, unsigned ID);

        User *getUser() const;

        void setUser(User *U);

        unsigned getOperandNo() const;

        Value *get() const;

        ManagedHAKCPointerP getManagedPtr() const;

        unsigned getID() const;

        static void SortUses(SmallVector<ManagedHAKCPointerUseP> &ManagedUses);

    protected:
        ManagedHAKCPointerP ManagedPtr;
        User *UserP;
        unsigned OperandNo;
        unsigned ID;

    public:
        friend bool operator==(const ManagedHAKCPointerUse &lhs, const Use &rhs) {
            return lhs.getUser() == rhs.getUser() && lhs.getOperandNo() == rhs.getOperandNo();
        }

        friend bool operator!=(const ManagedHAKCPointerUse &lhs, const Use &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const Use &lhs, const ManagedHAKCPointerUse &rhs) {
            return (rhs == lhs);
        }

        friend bool operator!=(const Use &lhs, const ManagedHAKCPointerUse &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const ManagedHAKCPointerUse &lhs, const ManagedHAKCPointerUse &rhs) {
            return (lhs.getUser() == rhs.getUser()) && (lhs.getOperandNo() == rhs.getOperandNo());
        }

        friend bool operator!=(const ManagedHAKCPointerUse &lhs, const ManagedHAKCPointerUse &rhs) {
            return !(lhs == rhs);
        }
    };

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

        HAKCTypeP GetType();

        void SetType(HAKCTypeP NewHAKCTy);

        Value *GetAuthenticatedPointer();

        virtual void SetAuthenticatedPointer(Value *NewAuthenticatedPointer);

        unsigned GetID() const;

        friend bool operator==(const HAKCPointerBase &lhs, Value *V) {
            return lhs.GetBaseDefinition() == V;
        }

        friend bool operator!=(const HAKCPointerBase &lhs, Value *V) {
            return !(lhs == V);
        }

        friend bool operator==(Value *V, const HAKCPointerBase &rhs) {
            return (rhs == V);
        }

        friend bool operator!=(Value *V, const HAKCPointerBase &rhs) {
            return !(V == rhs);
        }

        friend bool operator==(const HAKCPointerBase &lhs, const HAKCPointerBase &rhs) {
            return lhs.GetBaseDefinition() == rhs.GetBaseDefinition();
        }

        friend bool operator!=(const HAKCPointerBase &lhs, const HAKCPointerBase &rhs) {
            return !(lhs == rhs);
        }
    };

    /**
     * A single managed Pointer.  Contains the original definition of the pointer, an authenticated pointer suitable
     * for dereferencing, and a protected pointer to be used in function arguments.  The base definition and
     * protected pointer can be different if BaseDefinition is from an external function call.
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
        std::set<ManagedHAKCPointerUseP> AuthenticatedUses;
        std::set<ManagedHAKCPointerUseP> ProtectedUses;
        std::set<ManagedHAKCPointerUseP> CloneUses;

        /**
         * Return the Authenticated version of HAKCUse
         * @param HAKCUse
         * @return
         */
        Value *CreateAuthenticatedValue(const ManagedHAKCPointerUseP &HAKCUse);

        /**
         * Return the Signed version of HAKCUse
         * @param HAKCUse
         * @return
         */
        Value *CreateProtectedValue(const ManagedHAKCPointerUseP &HAKCUse);

        void TransformUseSet(std::set<ManagedHAKCPointerUseP> &UseSet);

        void TransformClones();

        void CreatePointerReplacements();

        bool ComputeBasePointerAuthenticated();

        std::set<ManagedHAKCPointerUseP> GetAllUses();

        void SetProtectedPointer(Value *NewProtectedPointer);

        void
        SetUseOperand(User *U, Value *Replacement, const ManagedHAKCPointerUseP &PointerUse, bool IsAuthenticatedUse);

        bool AllIncomingValuesAreAuthenticated();

        bool AllIncomingValuesWillBeAuthenticated();

        std::set<Value *> GetAllIncomingValues();

        bool PointerSetsShouldBeEqual();

        void SetPointerSetsToBeEqual();

        bool UseIsManagedAndHasUsers(const ManagedHAKCPointerUseP &PointerUse, bool CountAuthenticatedUsers);

        bool ValueIsManagedAndHasUsers(Value *V, bool CountAuthenticatedUsers);

    public:
        ManagedHAKCPointer(Value *Pointer, HAKCPointerManager &Manager, unsigned ID);

        ~ManagedHAKCPointer() = default;

        Value *GetProtectedPointer();

        void CreateBaseAuthenticatedPointer();

        void CreatePointerUseClones();

        bool BaseDefinitionShouldBeTransferred();

        void TransformUses();

        void MaybeCreateProtectedPointer();

        void MaybeCreateBaseCopyPointer();

        void RegisterManualHAKCTransfer(CallBase *CallI);

        unsigned GetAuthenticatedUserCount();

        unsigned GetProtectedUserCount();

        bool BaseIsAuthenticatedPointer() const;

        bool IsAuthenticatedIsCopyOfBase() const;

        bool DetermineIfBasePointerIsAuthenticated();

        void AddAuthenticatedUse(const ManagedHAKCPointerUseP &UPtr);

        void AddProtectedUse(const ManagedHAKCPointerUseP &UPtr);

        void AddCloneUse(const ManagedHAKCPointerUseP &UPtr);

        bool PointerSetsCanBeEqual();

        void UpdateUserCounts();

        void SetAuthenticatedPointer(Value *NewAuthenticatedPointer) override;

    private:
        void InitBaseDefinitionInfo();

        void CheckPointerReplacement(Value *Old, Value *New, StringRef TypeName) const;
    };
} // hakc

#endif //HAKC_MANAGEDHAKCPOINTER_H
