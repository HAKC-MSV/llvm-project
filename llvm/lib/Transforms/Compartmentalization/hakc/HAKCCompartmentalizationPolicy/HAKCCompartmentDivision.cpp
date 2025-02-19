//
// Created by de29664 on 8/8/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

namespace llvm::hakc {
    HAKCCompartmentDivision::HAKCCompartmentDivision(const HAKCCompartment &C,
                                                     const hakc_compartment_division_t DivisionID,
                                                     const hakc_access_token_t AccessToken,
                                                     LLVMContext &Context) : ParentCompartment(C),
                                                                             AccessToken(
                                                                                 ConstantInt::get(
                                                                                     IntegerType::get(Context, 64),
                                                                                     AccessToken)),
                                                                             DivisionID(ConstantInt::get(
                                                                                 IntegerType::get(
                                                                                     Context, DIVISION_ID_BIT_LENGTH),
                                                                                 DivisionID)) {
    }

    HAKCCompartmentDivision::HAKCCompartmentDivision()
        : ParentCompartment(), AccessToken(nullptr), DivisionID(nullptr) {
    }


    const HAKCCompartment &HAKCCompartmentDivision::GetHAKCCompartment() const {
        return ParentCompartment;
    }

    HAKCCompartment & HAKCCompartmentDivision::GetHAKCCompartment() {
        return ParentCompartment;
    }

    HAKC_Division_ID HAKCCompartmentDivision::GetDivisionID() const {
        return DivisionID;
    }

    HAKC_Access_Token HAKCCompartmentDivision::GetAccessToken() const {
        return AccessToken;
    }

    bool HAKCCompartmentDivision::operator==(const HAKCCompartmentDivision &RHS) const {
        return DivisionID == RHS.GetDivisionID() &&
               GetHAKCCompartment().GetCompartmentID() == RHS.GetHAKCCompartment().GetCompartmentID();
    }

    bool HAKCCompartmentDivision::operator!=(const HAKCCompartmentDivision &RHS) const {
        return !(DivisionID == RHS.GetDivisionID() &&
                 GetHAKCCompartment().GetCompartmentID() == RHS.GetHAKCCompartment().GetCompartmentID());
    }

    bool HAKCCompartmentDivision::operator<(const HAKCCompartmentDivision &Div) const {
        return GetHAKCCompartment().GetCompartmentIDValue() < Div.GetHAKCCompartment().GetCompartmentIDValue() &&
               GetDivisionID()->getSExtValue() < Div.GetDivisionID()->getSExtValue();
    }
} // hakc
