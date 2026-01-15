//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the compartment object which tracks the compartment id,
/// entry token, and valid targets that can be called from this compartment.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 8/6/24.
//

#ifndef HAKC_HAKCCOMPARTMENT_H
#define HAKC_HAKCCOMPARTMENT_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <set>

namespace llvm::hakc {
    class HAKCCompartmentDivision;

    typedef std::set<HAKC_Compartment_ID> CompartmentIDSet;

    class HAKCCompartment {
    public:
        HAKCCompartment(hakc_compartment_id_t Compartment,
                        hakc_access_token_t EntryToken, class LLVMContext &Context);

        HAKCCompartment();

        HAKC_Compartment_ID GetCompartmentID() const;

        HAKC_Access_Token GetEntryToken() const;

        iterator_range<CompartmentIDSet::const_iterator> GetValidTargets() const;

        unsigned GetValidTargetsSize() const;

        void AddTarget(HAKC_Compartment_ID CompartmentID);

        hakc_compartment_id_t GetCompartmentIDValue() const;

      static HAKC_Compartment_ID CreateID(hakc_compartment_id_t ID, const Module &M);

        static IntegerType *GetEntryTokenType(LLVMContext &Ctx);

        friend bool operator==(const HAKCCompartment &lhs,
                               const HAKCCompartment &rhs) {
            return lhs.Compartment == rhs.Compartment;
        }

        friend bool operator!=(const HAKCCompartment &lhs,
                               const HAKCCompartment &rhs) {
            return !(lhs == rhs);
        }

        static constexpr unsigned CompartmentIDBitCount = 32;

      protected:
        HAKC_Compartment_ID Compartment = nullptr;
        HAKC_Access_Token EntryToken = nullptr;
        CompartmentIDSet Targets;
      };
} // namespace llvm::hakc

#endif // HAKC_HAKCCOMPARTMENT_H
