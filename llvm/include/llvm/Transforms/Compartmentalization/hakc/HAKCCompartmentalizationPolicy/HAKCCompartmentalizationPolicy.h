//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "llvm/Support/JSON.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCDatabase.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

namespace llvm::hakc {
class HAKCModuleAnalysis;
class HAKCModuleTransform;
class HAKCSystemInformation;
// class HAKCDatabaseConnection;
// class HAKCResult;

typedef std::shared_ptr<HAKCCompartment> HAKCCompartmentP;
typedef std::shared_ptr<HAKCCompartmentDivision> HAKCDivisionP;

class HAKCCompartmentalizationPolicy {
public:
  explicit HAKCCompartmentalizationPolicy(
      HAKCSystemInformation &SystemInformation);

  virtual ~HAKCCompartmentalizationPolicy();

  virtual HAKCCompartmentDivision &GetDivision(GlobalValue *GV);

  HAKCCompartmentP GetCompartment(hakc_compartment_id_t CompartmentID);

  void GetValidTargets(HAKCCompartment &Compartment);

  virtual HAKCCompartmentDivision &GetDefaultDivision();

protected:
  HAKCSystemInformation &SystemInformation;
  std::vector<HAKCCompartmentP> Compartments;
  std::vector<HAKCDivisionP> Divisions;
  HAKCDatabaseConnection Client;
  std::map<HAKCSymbolP, HAKCDivisionP> SymbolDivisionMap;
  std::set<hakc_compartment_id_t> RetrievedTargetCompartments;

  void CheckConnection() const;

  HAKCDivisionP GetDivision(hakc_compartment_id_t CompartmentID,
                            hakc_compartment_division_t DivisionID);

  void ConnectToDatabase();

  void DisconnectFromDatabase();

  HAKCDivisionP FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                   hakc_compartment_division_t DivisionID);

  HAKCCompartmentP FindCachedCompartment(hakc_compartment_id_t CompartmentID);

  HAKCResult Execute(StringRef Endpoint, json::Object &Parameters);

  HAKCCompartmentP CreateCompartment(hakc_compartment_id_t CompartmentID,
                                     hakc_access_token_t AccessToken,
                                     bool CheckForExisting);

  HAKCDivisionP FindCachedSymbolDivision(HAKCSymbolP Symbol) const;
};

class HAKCCompartmentalizationPolicyDAG
    : public HAKCCompartmentalizationPolicy {
public:
  explicit HAKCCompartmentalizationPolicyDAG(
      HAKCSystemInformation &SystemInformation);

  HAKCCompartmentDivision &GetDivision(GlobalValue *GV) override;
  HAKCCompartmentDivision &GetDefaultDivision() override;
  // std::map<GlobalValue *, HAKCCompartmentDivision*> GV_to_divs;
  std::map<GlobalValue *, std::shared_ptr<HAKCCompartmentDivision>> GV_to_divs;
  unsigned GV_to_div_incr;
};

} // namespace llvm::hakc

#endif // HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
