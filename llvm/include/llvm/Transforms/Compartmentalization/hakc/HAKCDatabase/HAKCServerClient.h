//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCSERVERCLIENT_H
#define HAKC_HAKCSERVERCLIENT_H

#include "llvm/Support/JSON.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCDatabase.h"

namespace llvm::hakc {
class HAKCModuleAnalysis;
class HAKCModuleTransform;
class HAKCSystemInformation;

class HAKCServerClient {
public:
  explicit HAKCServerClient(HAKCSystemInformation &SystemInformation);

  virtual ~HAKCServerClient();

  void set_dag_filename(StringRef filename) ;

  void add_symbols(ArrayRef<std::shared_ptr<HAKCFunctionInfo>> FIs, ArrayRef<std::shared_ptr<HAKCGlobalInfo>> GIs) ;

  void add_function(const HAKCFunctionInfo &Function) ;

  void add_global_variable(const HAKCGlobalInfo &Global) ;

  void SendSymbolsToAnalysisServer(HAKCModuleAnalysis &ModuleAnalysis, StringRef filename) ;

  void CloseConnection();

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

  void ConnectToDatabase();

  void DisconnectFromDatabase();

  HAKCResult Execute(StringRef Endpoint, json::Object &Parameters) ;

  void SendTerminateConnection() const;

  HAKCDivisionP GetDivision(hakc_compartment_id_t CompartmentID,
                            hakc_compartment_division_t DivisionID);

  HAKCDivisionP FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                   hakc_compartment_division_t DivisionID);

  HAKCCompartmentP FindCachedCompartment(hakc_compartment_id_t CompartmentID);

  HAKCCompartmentP CreateCompartment(hakc_compartment_id_t CompartmentID,
                                     hakc_access_token_t AccessToken,
                                     bool CheckForExisting);

  HAKCDivisionP FindCachedSymbolDivision(HAKCSymbolP Symbol) const;

};

} // namespace llvm::hakc

#endif // HAKC_HAKCSERVERCLIENT_H
