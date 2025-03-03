//
// Created by de29664 on 11/14/23.
//

#ifndef HAKC_HAKCPOINTERMANAGER_H
#define HAKC_HAKCPOINTERMANAGER_H

#include <set>
#include "llvm/IR/Instructions.h"
#include "ManagedHAKCPointer.h"

namespace llvm::hakc {
    using namespace llvm;

    class HAKCFunctionAnalysis;

    /**
     * In a function, there are two versions of each pointer that need to be tracked, a pointer that can be
     * dereferenced and a pointer that is protected.  Any dereference could be the result of arbitrary number of
     * Instructions, and we assume that pointer origins (e.g., function arguments or the result of a call) return a
     * protected pointer.  So Instructions that lead to a dereference will dereference a protected pointer, so those
     * Instructions need to be cloned and modified to use the authenticated pointer.  This manager tracks those
     * clones, so exactly one is ever created.
     */
    class HAKCPointerManager {
    public:
        explicit HAKCPointerManager(HAKCFunctionAnalysis &Analysis, HAKCCompartmentalizationPolicy &Policy,
                                    bool DebugActive);

        bool ManagePointer(Value *V);

        std::set<ManagedHAKCPointerP> GetManagedPointers();

        void GetSortedPointers(SmallVector<ManagedHAKCPointerP> &SortedPointers);

        HAKCFunctionAnalysis &GetFunctionAnalysis();

        /**
         * Returns the ManagedHAKCPointer that corresponds to the definition V
         * @param V
         * @return
         */
        ManagedHAKCPointerP GetManagedPointer(Value *V);

        bool empty() const;

        Value *GetDef(Value *V);

        /**
         * Return the Authenticated version of Pointer
         * @param Pointer
         * @param Debug
         * @return
         */
        Value *CreateAuthenticatedValue(const ManagedHAKCPointerUseP &PointerUse);

        Value *CreateProtectedValue(const ManagedHAKCPointerUseP &PointerUse);

        Value *FindAuthenticatedValue(Value *V);

        Value *FindProtectedValue(Value *V);

        Value *FindAuthenticatedValue(const ManagedHAKCPointerUseP &PointerUse);

        Value *FindProtectedValue(const ManagedHAKCPointerUseP &PointerUse);

        static Value *
        FindManagedValue(std::map<ManagedHAKCPointerUseP, Value *> &Storage, const ManagedHAKCPointerUseP &PointerUse);

        /**
         * Create authenticated versions of the ManagedHAKCPointer set
         * @param Debug
         */
        void CreateAuthenticatedPointersAndAllClones();

        void CreateAllTransfers();

        void TransformPointers();

        void AddAuthenticatedPointer(const ManagedHAKCPointerUseP &PointerUse, Value *Replacement);

        void AddProtectedPointer(const ManagedHAKCPointerUseP &PointerUse, Value *Replacement);

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

        Value *CreateSafePointerAtLocation(Value *Pointer, Instruction *InsertLocation);

        Value *CreateAuthenticationAtLocation(Value *Pointer, Instruction *InsertLocation);

        bool DebugIsActive() const;

    protected:
        /**
         * The set of pointers under management
         */
        std::set<ManagedHAKCPointerP> ManagedPointers;

        std::map<ManagedHAKCPointerUseP, Value *> AuthenticatedValues;
        std::map<ManagedHAKCPointerUseP, Value *> ProtectedValues;
        std::map<Instruction *, Instruction *> Clones;

        std::set<ManagedHAKCPointerUseP> AnalyzedUses;

        HAKCFunctionAnalysis &HAKCAnalysis;
        HAKCCompartmentalizationPolicy &Policy;

        unsigned DataAuthenticationsAdded;
        unsigned CodeAuthenticationsAdded;
        unsigned SafePointersAdded;

        bool IsCompartmentalized;
        bool DebugActive;

        bool PointerIsEligibleForManagement(Value *Pointer);

        void AddHAKCPointerReplacement(const ManagedHAKCPointerUseP &PtrUse, Value *Replacement,
                                       bool AddingAuthenticatedReplacements);

        Value *FindManagedValue(std::map<ManagedHAKCPointerUseP, Value *> &Storage, Value *Target);

        void ManageNewPointer(Value *V);

        void ClassifyAllUsesOfDefinition(Value *Definition, const ManagedHAKCPointerP &ManagedPointer);

        bool UseIsAnalyzed(const ManagedHAKCPointerUseP &UseP);

        bool UseShouldBeIgnored(Use &U);

        static bool UseShouldBeCloned(Use &U);

        bool UseShouldUtilizeAuthenticatedPointer(Use &U);

        bool UseShouldUtilizeSignedBasePointer(Use &U);

        bool IsClonedUseNeedingAdditionalClassification(Use &U);

        static void PrintManagedValues(const std::map<ManagedHAKCPointerUseP, Value *> &Storage);

        Value *FindManagedPointerReplacement(Value *Target, bool ReturnAuthenticatedPointer);

        ManagedHAKCPointerUseP CreateManagedPointerUse(const ManagedHAKCPointerP &ManagedPointer, User *U,
                                                       unsigned OperandNo);

        bool IsConstantExprUsedInKernelCall(User *U);

    private:
        unsigned CurrentPointerID;
        unsigned CurrentPointerUseID;
    };
} // hakc

#endif //HAKC_HAKCPOINTERMANAGER_H
