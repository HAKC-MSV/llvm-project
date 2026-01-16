#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

#include "gtest/gtest.h"

namespace llvm {
namespace {

TEST(HAKCUnitTests, FakeClientTest0) {
  LLVMContext Context;
  // Construct FakeServerClient and test behavior
  // Default policy allows all compartments to access any other compartment
  // (i.e., a valid target)
  auto Client = std::make_unique<hakc::FakeServerClient>(Context, false);
  std::vector<hakc::HAKCCompartmentP> Compartments;
  for (unsigned i = 0; i < 4; ++i) {
    Compartments.push_back(Client->GetCompartment(i));
    EXPECT_EQ(Compartments[i]->GetCompartmentIDValue(), i);
  }

  for (int i = 0; i < 4; ++i) {
    errs() << "Valid targets for compartment " << i << ":\n";
    // force the compartment targets to be updated
    Client->GetValidTargets(*Compartments[i]);
    for (auto CompartmentID : Compartments[i]->GetValidTargets()) {
      errs() << *CompartmentID << ", ";
    }
    errs() << "\n";
  }

  // Current buggy behavior: create two compartments, 0 and 1. There seem to be
  // no valid targets for compartment 0
  errs() << "\n";
  // EXPECT_EQ(Compartment0->GetCompartmentIDValue(), 0);
  // EXPECT_TRUE();
  // EXPECT_FALSE();
}

} // namespace
} // namespace llvm
