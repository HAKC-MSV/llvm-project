//===- llvm/unittests/Transforms/Vectorize/VPDomTreeTests.cpp - -----------===//
//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCPass.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

#include "gtest/gtest.h"

namespace llvm {
namespace {



TEST(HAKC_UNIT_TESTS, HAKC_UNIT_TEST_0) {

  // Construct FakeServerClient and test behavior
  // Default policy allows all compartments to access any other compartment (i.e., a valid target)
  auto Client = std::make_unique<hakc::FakeServerClient>();
  std::vector<hakc::HAKCCompartmentP> Compartments;
  for (int i = 0; i < 4; ++i) {
    Compartments.push_back(Client->GetCompartment(i));
    EXPECT_EQ(Compartments[i]->GetCompartmentIDValue(), i);
  }

  for (int i = 0; i < 4; ++i) {
    errs() << "Valid targets for compartment " << i << ":\n";
    // force the compartment targets to be updated
    Client->GetValidTargets(*Compartments[i]);
    for (auto CompartmentID: Compartments[i]->GetValidTargets()) {
      errs() << *CompartmentID << ", " ;
    }
    errs() << "\n";
  }

  // Current buggy behavior: create two compartments, 0 and 1. There seem to be no valid targets for compartment 0
  errs() << "\n";
  // EXPECT_EQ(Compartment0->GetCompartmentIDValue(), 0);
  // EXPECT_TRUE();
  // EXPECT_FALSE();


}

} // namespace
} // namespace llvm
