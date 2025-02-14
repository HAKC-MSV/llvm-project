//
// Created by de29664 on 8/6/24.
//


#include "llvm/IR/Module.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"

namespace llvm::hakc {
    HAKCCompartment::HAKCCompartment(hakc_compartment_id_t Compartment, hakc_access_token_t EntryToken,
                                     class LLVMContext &Context) : Compartment(
                                                                       ConstantInt::get(
                                                                           IntegerType::get(
                                                                               Context, COMPARTMENT_ID_BIT_LENGTH),
                                                                           Compartment)),
                                                                   EntryToken(ConstantInt::get(
                                                                       IntegerType::get(Context, 64), EntryToken)),
                                                                   Targets() {
    }

    HAKCCompartment::HAKCCompartment() : Compartment(nullptr), EntryToken(nullptr), Targets() {
    }

    HAKC_Compartment_ID HAKCCompartment::GetCompartmentID() const {
        return Compartment;
    }

    bool HAKCCompartment::IsUncompartmentalized() const {
        return GetCompartmentIDValue() == KERNEL_COMPARTMENT;
    }

    iterator_range<CompartmentIDSet::const_iterator> HAKCCompartment::GetValidTargets() const {
        return make_range(Targets.begin(), Targets.end());
    }

    void HAKCCompartment::AddTarget(HAKC_Compartment_ID CompartmentID) {
        Targets.insert(CompartmentID);
    }

    hakc_compartment_id_t HAKCCompartment::GetCompartmentIDValue() const {
        return Compartment->getSExtValue();
    }

    HAKC_Access_Token HAKCCompartment::GetEntryToken() const {
        return EntryToken;
    }

    HAKC_Compartment_ID HAKCCompartment::CreateID(hakc_compartment_id_t ID, Module &M) {
        return ConstantInt::get(IntegerType::get(M.getContext(), CompartmentIDBitCount), ID);
    }
} // hakc
