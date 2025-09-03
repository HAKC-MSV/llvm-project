//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCDATABASE_H
#define HAKC_HAKCDATABASE_H

#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_socket_stream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

namespace llvm::hakc {

class HAKCModuleAnalysis;
class HAKCModuleTransform;
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
                         bool debug);

  HAKCDatabaseResponse HandleRequest(const HAKCDatabaseRequest &Request) const;

  void SendTerminateConnection(const HAKCDatabaseRequest &Request) const;

  operator bool() const;

  void close();

  void connect();

protected:
  std::unique_ptr<raw_socket_stream> Socket;
  const HAKCDatabaseInformation &DatabaseInformation;
  bool debug;

  bool CheckConnection() const;
};


} // namespace llvm::hakc

#endif // HAKC_HAKCDATABASE_H
