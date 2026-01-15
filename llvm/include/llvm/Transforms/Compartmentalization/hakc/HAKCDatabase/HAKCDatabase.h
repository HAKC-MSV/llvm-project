//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the database class which allows the pass to connect to a
/// tcp socket and query a database which is hosted by a server process. These
/// queries are used to determine valid targets, symbol divisions, etc.
///
//===----------------------------------------------------------------------===//
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
class HAKCDatabaseResponse;
class HAKCDatabaseConnection;

typedef std::shared_ptr<HAKCCompartment> HAKCCompartmentP;
typedef std::shared_ptr<HAKCCompartmentDivision> HAKCDivisionP;

class HAKCDatabaseRequest {
public:
  HAKCDatabaseRequest(StringRef Endpoint, json::Object &Parameters);

  void operator>>(raw_ostream &OS) const;

protected:
  json::Value Request;
};

class HAKCPayload {
public:
  HAKCPayload(json::Object *payload) : payload(*payload) {};

  virtual ~HAKCPayload() = default;

  // template <typename T> T GetValue(StringRef key);
  // unsigned GetInteger(StringRef key);
  // std::vector<unsigned> GetIntegerArray(StringRef key);

protected:
  json::Object payload;
};

class HAKCDivisionPayload : public HAKCPayload {

public:
  HAKCDivisionPayload(json::Object *payload);

  unsigned DivisionID;
  unsigned Salt;
  unsigned AccessToken;
};

class HAKCCompartmentPayload : public HAKCPayload {

public:
  HAKCCompartmentPayload(json::Object *payload);

  unsigned CompartmentID;
  unsigned EntryToken;
};

class HAKCDivisionCompartmentPayload : public HAKCPayload {

public:
  HAKCDivisionCompartmentPayload(json::Object *payload);

  unsigned DivisionID;
  unsigned Salt;
  unsigned AccessToken;
  unsigned CompartmentID;
  unsigned EntryToken;
};

class HAKCBoolPayload final : public HAKCPayload {

public:
  HAKCBoolPayload(json::Object *payload);
  bool value;
};

class HAKCValidTargetsPayload final : public HAKCPayload {

public:
  HAKCValidTargetsPayload(json::Object *payload);
  std::vector<uint> ValidTargets;
};

class HAKCResult {
  friend HAKCDatabaseResponse;

public:
  HAKCResult();

  bool success;
  std::string error;

  std::shared_ptr<HAKCPayload> GetData() { return data; };

protected:
  std::shared_ptr<HAKCPayload> data;
};

class HAKCDatabaseResponse {
public:
  friend HAKCResult;

  // read in the HAKCResponse structure from server
  HAKCDatabaseResponse(const HAKCSystemInformation &SystemInformation);

  void parse_result(json::Object *_result);

  operator bool() const;

  void operator<<(raw_socket_stream &OS);

  HAKCResult &GetResult() { return result; }

  bool ShouldTerminateConnection() { return terminate_connection; }

protected:
  std::string Response;
  bool Success;
  std::string response_endpoint;
  HAKCResult result;
  const HAKCSystemInformation &SystemInformation;
  bool terminate_connection;

  ssize_t ReadFromSocket(raw_socket_stream &OS, void *Dest, ssize_t Size);
};

class HAKCDatabaseConnection {
public:
  HAKCDatabaseConnection(const HAKCSystemInformation &SystemInformation,
                         bool debug);

  HAKCDatabaseResponse HandleRequest(const HAKCDatabaseRequest &Request) const;

  void SendTerminateConnection(const HAKCDatabaseRequest &Request) const;

  operator bool() const;

  void close();

  void connect();

  const HAKCSystemInformation &GetSystemInformation() {
    return SystemInformation;
  }

protected:
  std::unique_ptr<raw_socket_stream> Socket;
  const HAKCSystemInformation &SystemInformation;
  bool debug;

  bool CheckConnection() const;
};

} // namespace llvm::hakc

#endif // HAKC_HAKCDATABASE_H
