//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
#define HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H

#include "yaml/HAKCYamlSymbol.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_socket_stream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

namespace llvm::hakc {
class HAKCModuleAnalysis;
class HAKCSystemInformation;

typedef std::shared_ptr<HAKCCompartment> HAKCCompartmentP;
typedef std::shared_ptr<HAKCCompartmentDivision> HAKCDivisionP;

class HAKCDatabaseRequest {
public:
  HAKCDatabaseRequest(StringRef Endpoint, json::Object &Parameters);

  void operator>>(raw_ostream &OS) const;

protected:
  json::Value Request;
};

class HAKCDatabaseResponse {
public:
  HAKCDatabaseResponse(std::chrono::milliseconds Timeout);

  Expected<json::Value> GetJSON() const;

  operator bool() const;

  void operator<<(raw_socket_stream &OS);

protected:
  std::string Response;
  std::chrono::milliseconds Timeout;
  bool Success;

  ssize_t ReadFromSocket(raw_socket_stream &OS, void *Dest, ssize_t Size) const;
};

class HAKCDatabaseConnection {
public:
  HAKCDatabaseConnection(const HAKCDatabaseInformation &DatabaseInformation,
                         bool Debug);

  HAKCDatabaseResponse HandleRequest(const HAKCDatabaseRequest &Request) const;

  operator bool() const;

  void close();

  void connect();

protected:
  std::unique_ptr<raw_socket_stream> Socket;
  const HAKCDatabaseInformation &DatabaseInformation;
  bool Debug;

  bool CheckConnection() const;
};

class HAKCCompartmentalizationPolicy {
public:
  explicit HAKCCompartmentalizationPolicy(
      HAKCSystemInformation &SystemInformation);

  ~HAKCCompartmentalizationPolicy();

  hakc::HAKCCompartmentDivision &GetDivision(GlobalValue *GV);

  HAKCCompartmentP GetCompartment(hakc_compartment_id_t CompartmentID);

  void GetValidTargets(HAKCCompartment &Compartment) const;

  hakc::HAKCCompartmentDivision &GetDefaultDivision();

protected:
  HAKCSystemInformation &SystemInformation;
  std::vector<HAKCCompartmentP> Compartments;
  std::vector<HAKCDivisionP> Divisions;
  HAKCDatabaseConnection Client;

  void CheckConnection() const;

  HAKCDivisionP GetDivision(hakc_compartment_id_t CompartmentID,
                            hakc_compartment_division_t DivisionID);

  void ConnectToDatabase();

  void DisconnectFromDatabase();

  HAKCDivisionP FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                   hakc_compartment_division_t DivisionID);

  HAKCCompartmentP FindCachedCompartment(hakc_compartment_id_t CompartmentID);

  json::Object Execute(StringRef Endpoint, json::Object &Parameters) const;

  HAKCCompartmentP CreateCompartment(hakc_compartment_id_t CompartmentID,
                                     hakc_access_token_t AccessToken,
                                     bool CheckForExisting);
};
} // namespace llvm::hakc

#endif // HAKC_HAKCCOMPARTMENTALIZATIONPOLICY_H
