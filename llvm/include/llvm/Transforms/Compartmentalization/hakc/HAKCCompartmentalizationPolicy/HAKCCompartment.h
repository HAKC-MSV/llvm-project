//
// Created by de29664 on 8/6/24.
//

#ifndef HAKC_HAKCCOMPARTMENT_H
#define HAKC_HAKCCOMPARTMENT_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"


namespace llvm::hakc {
    class HAKCCompartmentDivision;

    class HAKCCompartment {
    public:
        HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t EntryToken, class LLVMContext &Context);

        HAKCCompartment();

        HAKCCompartment(const HAKCCompartment &C) = default;

        HAKC_Compartment_ID GetCompartmentID() const;

        HAKC_Access_Token GetEntryToken() const;

        std::vector<HAKC_Compartment_ID> GetValidTargets() const;

        void AddTarget(HAKC_Compartment_ID CompartmentID);

        hakc_compartment_id_t GetCompartmentIDValue() const;

        bool IsUncompartmentalized() const;

        static HAKC_Compartment_ID CreateID(hakc_compartment_id_t ID, Module &M);

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
        std::vector<HAKC_Compartment_ID> Targets;
    };
} // hakc

#endif //HAKC_HAKCCOMPARTMENT_H
