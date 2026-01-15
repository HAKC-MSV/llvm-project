//
// Created by de29664 on 8/6/24.
//

#include "llvm/IR/Module.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"

namespace llvm::hakc {
HAKCCompartment::HAKCCompartment(const hakc_compartment_id_t Compartment,
                                 const hakc_access_token_t EntryToken,
                                 LLVMContext &Context)
    : Compartment(ConstantInt::get(
          IntegerType::get(Context, COMPARTMENT_ID_BIT_LENGTH), Compartment)),
      EntryToken(ConstantInt::get(HAKCCompartment::GetEntryTokenType(Context),
                                  EntryToken)),
      Targets() {}

HAKCCompartment::HAKCCompartment() {}

HAKC_Compartment_ID HAKCCompartment::GetCompartmentID() const {
  return Compartment;
}

iterator_range<CompartmentIDSet::const_iterator>
HAKCCompartment::GetValidTargets() const {
  return make_range(Targets.begin(), Targets.end());
}

void HAKCCompartment::AddTarget(const HAKC_Compartment_ID CompartmentID) {
  Targets.insert(CompartmentID);
}
unsigned HAKCCompartment::GetValidTargetsSize() const { return Targets.size(); }

hakc_compartment_id_t HAKCCompartment::GetCompartmentIDValue() const {
  return Compartment->getSExtValue();
  // TODO: Should we use getZExtValue instead? Can a compartment ID ever be
  // negative?
}

HAKC_Access_Token HAKCCompartment::GetEntryToken() const { return EntryToken; }

HAKC_Compartment_ID HAKCCompartment::CreateID(const hakc_compartment_id_t ID,
                                              const Module &M) {
  return ConstantInt::get(
      IntegerType::get(M.getContext(), CompartmentIDBitCount), ID);
}

IntegerType *HAKCCompartment::GetEntryTokenType(LLVMContext &Ctx) {
  return IntegerType::get(Ctx, ENTRY_TOKEN_BIT_LENGTH);
}
} // namespace llvm::hakc
