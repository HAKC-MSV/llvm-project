// llvm-project/llvm/unittests/Transforms/Compartmentalization/hakc/PythonBindingsTest.cpp
#include "gtest/gtest.h"

#include <stdexcept>
#include <string>

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace {

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST0) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = 0;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{0};
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            0b1);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST1) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = 1;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{1};
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            0b10000000000000010);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST2) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = 0;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{0, 1, 2, 3};
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            0b1111);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST3) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = 1;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{0, 1, 2, 3};
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            0b10000000000001111);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST4) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = 16;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{0, 1, 2, 3};
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            0b100000000000000001111);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST6) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = 16;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            0b100001111111111111111);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST7) {
  llvm::hakc::hakc_compartment_id_t CompartmentID =
      ((((unsigned long long)1) << 48) - 1);
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  // get max value minus 1 (aka 0b111....111)
  EXPECT_EQ(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                         DivisionIDs, 16),
            ((unsigned long long)-1));
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST8) {
  llvm::hakc::hakc_compartment_id_t CompartmentID =
      ((((unsigned long long)1) << 48) - 1);
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{16};
  EXPECT_THROW(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                            DivisionIDs, 16),
               std::exception);
}

TEST(HAKC_ACCESS_TOKEN_TESTS, ACCESS_TOKEN_TEST9) {
  llvm::hakc::hakc_compartment_id_t CompartmentID = (unsigned long long)1 << 48;
  std::set<llvm::hakc::hakc_compartment_division_t> DivisionIDs{0};
  EXPECT_THROW(hakc::CommonHAKCAnalysis::ComputeAccessToken(CompartmentID,
                                                            DivisionIDs, 16),
               std::exception);
}

} // namespace
