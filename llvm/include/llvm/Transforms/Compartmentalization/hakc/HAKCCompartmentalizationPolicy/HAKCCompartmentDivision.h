//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the division, which is contained in a compartment, and
/// tracks the division id, access token, and parent compartment.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 8/8/24.
//

#ifndef HAKC_HAKCCOMPARTMENTDIVISION_H
#define HAKC_HAKCCOMPARTMENTDIVISION_H

#include "HAKCCompartment.h"

namespace llvm::hakc {
class HAKCCompartmentDivision {
public:
  HAKCCompartmentDivision(const HAKCCompartment &C,
                          hakc_compartment_division_t DivisionID,
                          hakc_access_token_t AccessToken,
                          LLVMContext &Context);

  HAKCCompartmentDivision();

  const HAKCCompartment &GetHAKCCompartment() const;

  HAKCCompartment &GetHAKCCompartment();

  HAKC_Division_ID GetDivisionID() const;

  HAKC_Access_Token GetAccessToken() const;

  bool operator==(const HAKCCompartmentDivision &RHS) const;

  bool operator!=(const HAKCCompartmentDivision &RHS) const;

  bool operator<(const HAKCCompartmentDivision &Div) const;

protected:
  HAKCCompartment ParentCompartment;
  HAKC_Access_Token AccessToken;
  HAKC_Division_ID DivisionID;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCCOMPARTMENTDIVISION_H
