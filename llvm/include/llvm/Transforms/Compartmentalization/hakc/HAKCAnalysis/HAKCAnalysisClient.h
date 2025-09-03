//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCANALYSISCLIENT_H
#define HAKC_HAKCANALYSISCLIENT_H

#include "llvm/Support/JSON.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCDatabase.h"

namespace llvm::hakc {
class HAKCModuleAnalysis;
class HAKCModuleTransform;
class HAKCSystemInformation;

class HAKCAnalysisClient {
public:
  explicit HAKCAnalysisClient(
      HAKCSystemInformation &SystemInformation);

  virtual ~HAKCAnalysisClient();

  void set_dag_filename(StringRef filename) const;

  void add_function(const HAKCFunctionInfo &Function) const;

  void add_global_variable(const HAKCGlobalInfo &Global) const;

  void SendSymbolsToAnalysisServer(HAKCModuleAnalysis &ModuleAnalysis) const;

  void CloseConnection();

protected:
  HAKCSystemInformation &SystemInformation;
  HAKCDatabaseConnection Client;

  void CheckConnection() const;

  void ConnectToDatabase();

  void DisconnectFromDatabase();

  json::Object Execute(StringRef Endpoint, json::Object &Parameters) const;

  void SendTerminateConnection() const;

};

} // namespace llvm::hakc

#endif // HAKC_HAKCANALYSISCLIENT_H
