//
// Created by de29664 on 8/6/24.
//

#ifndef HAKC_HAKCCOMPARTMENT_H
#define HAKC_HAKCCOMPARTMENT_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <llvm/ADT/SmallSet.h>


namespace llvm::hakc {
    class HAKCCompartmentDivision;

    typedef std::set<HAKC_Compartment_ID> CompartmentIDSet;

    class HAKCCompartment {
    public:
        HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t EntryToken, class LLVMContext &Context);

        HAKCCompartment();

        HAKC_Compartment_ID GetCompartmentID() const;

        HAKC_Access_Token GetEntryToken() const;

        iterator_range<CompartmentIDSet::const_iterator> GetValidTargets() const;

        void AddTarget(HAKC_Compartment_ID CompartmentID);

        hakc_compartment_id_t GetCompartmentIDValue() const;

        bool IsUncompartmentalized() const;

        static HAKC_Compartment_ID CreateID(hakc_compartment_id_t ID, Module &M);

        static IntegerType *GetEntryTokenType(LLVMContext &Ctx);

        friend bool operator==(const HAKCCompartment &lhs, const HAKCCompartment &rhs) {
            return lhs.Compartment == rhs.Compartment;
        }

        friend bool operator!=(const HAKCCompartment &lhs, const HAKCCompartment &rhs) {
            return !(lhs == rhs);
        }

        static constexpr unsigned CompartmentIDBitCount = 32;

    protected:
        HAKC_Compartment_ID Compartment;
        HAKC_Access_Token EntryToken;
        CompartmentIDSet Targets;
    };
} // hakc

#endif //HAKC_HAKCCOMPARTMENT_H
