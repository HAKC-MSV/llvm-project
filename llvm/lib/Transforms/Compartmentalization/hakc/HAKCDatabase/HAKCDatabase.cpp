//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCDatabase.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include <unistd.h>

namespace llvm::hakc {

StringRef GetString(json::Object *payload, StringRef key) {
  auto maybe_value = payload->getString(key);
  if (!maybe_value.has_value()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unable to get " << key << " from payload!\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Debug) << "Got " << key << " from payload!\n";
  return maybe_value.value();
}

bool GetBool(json::Object *payload, StringRef key) {
  auto maybe_value = payload->getBoolean(key);
  if (!maybe_value.has_value()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unable to get " << key << " from payload!\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Debug) << "Got " << key << " from payload!\n";
  return maybe_value.value();
}

unsigned GetInteger(json::Object *payload, StringRef key) {
  auto maybe_value = payload->getInteger(key);
  if (!maybe_value.has_value()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unable to get " << key << " from payload!\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Debug) << "Got " << key << " from payload!\n";
  return maybe_value.value();
}

std::vector<unsigned> GetIntegerArray(json::Object *payload, StringRef key) {
  auto json_array = payload->getArray(key);
  std::vector<unsigned> array;
  for (const auto &element : *json_array) {
    // TODO: add value check here, but it should really never fail
    array.push_back(element.getAsInteger().value());
  }
  CommonHAKCAnalysis::getLogger(Debug) << "Got " << key << " from payload!\n";
  return array;
}

json::Object* GetObject(json::Object* payload, StringRef key) {
  if (!payload) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Payload is null!\n";
    throw std::exception();
  }
  auto obj = payload->getObject(key);
  if (!obj) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Unable to get " << key << " from payload!\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Debug) << "Got " << key << " from payload!\n";
  return obj;
}

template <typename T> T GetValue(json::Object *payload, StringRef key) {
  if (std::is_same_v<T, int> || std::is_same_v<T, unsigned>) {
    return static_cast<T>(GetInteger(payload, key));
  }
  if (std::is_same_v<T, StringRef>, std::is_same_v<T, std::string>, std::is_same_v<T, std::basic_string<char>> ){
    return  static_cast<T>(GetString(payload, key));
  }
  if (std::is_same_v<T, bool>) {
    return  static_cast<T>(GetBool(payload, key));
  }
  CommonHAKCAnalysis::getLogger(Fatal)
      << "Unable to get " << key
      << " of from payload because the type is unrecognized!\n";
  throw std::exception();

}

HAKCDivisionPayload::HAKCDivisionPayload(json::Object *payload)
    : HAKCPayload(payload), DivisionID(0), Salt(0), AccessToken(0) {
  auto division = GetObject(payload, "Division");
  DivisionID = GetInteger(division, "DivisionID");
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'DivisionID' from payload!\n";
  Salt = GetInteger(division, "Salt");
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'Salt' from payload!\n";
  AccessToken = GetInteger(division, "AccessToken");
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'AccessToken' from payload!\n";
}

HAKCCompartmentPayload::HAKCCompartmentPayload(json::Object *payload)
    : HAKCPayload(payload), CompartmentID(), EntryToken() {
  auto compartment = GetObject(payload, "Compartment");
  CompartmentID = GetInteger(compartment, "CompartmentID");
  EntryToken = GetInteger(compartment, "EntryToken");
}

HAKCDivisionCompartmentPayload::HAKCDivisionCompartmentPayload(
    json::Object *payload)
    : HAKCPayload(payload) {
  auto division = GetObject(payload, "Division");
  auto compartment = GetObject(payload, "Compartment");
  DivisionID = GetInteger(division, "DivisionID");
  Salt = GetInteger(division,"Salt");
  AccessToken = GetInteger(division,"AccessToken");
  CompartmentID = GetInteger(compartment,"CompartmentID");
  EntryToken = GetInteger(compartment,"EntryToken");
}

HAKCValidTargetsPayload::HAKCValidTargetsPayload(json::Object *payload)
    : HAKCPayload(payload), ValidTargets(std::vector<uint>()) {
  ValidTargets = GetIntegerArray(payload, "ValidTargets");
}

HAKCResult::HAKCResult() : success(), error(), data() {}

HAKCDatabaseRequest::HAKCDatabaseRequest(StringRef Endpoint,
                                         json::Object &Parameters)
    : Request(nullptr) {
  json::Object FullRequest({{"Endpoint", Endpoint}});
  if (!Parameters.empty()) {
    FullRequest.insert({"Parameters", std::move(Parameters)});
  }
  Request = std::move(FullRequest);
}

void HAKCDatabaseResponse::parse_result(json::Object *_result) {
  result.success = _result->getBoolean("Success").value_or(false);
  result.error = _result->getString("Error").value_or(
      "Unable to parse Error from HAKCResult!");
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)
            << "Request failed with error " << result.error << "!\n";
    throw std::exception();
  }
  // TODO: update if endpoints change
  if (response_endpoint ==
      database_information.GetTerminateConnectionEndpoint()) {
    terminate_connection = true;
    return;
  }
  auto payload = GetObject(_result, "Data");
  if (response_endpoint == database_information.GetCompartmentEndpoint()) {
    result.data = std::make_shared<HAKCCompartmentPayload>(payload);
  } else if (response_endpoint == database_information.GetDivisionEndpoint()) {
    result.data = std::make_shared<HAKCDivisionPayload>(payload);
  } else if (response_endpoint ==
             database_information.GetSymbolDivisionEndpoint()) {
    result.data = std::make_shared<HAKCDivisionCompartmentPayload>(payload);
  } else if (response_endpoint ==
             database_information.GetValidTargetsEndpoint()) {
    result.data = std::make_shared<HAKCValidTargetsPayload>(payload);
  } else if (response_endpoint ==
                 database_information.GetSetDagFilenameEndpoint() ||
             response_endpoint ==
                 database_information.GetAddSymbolsEndpoint() ||
             response_endpoint ==
                 database_information.GetAddFunctionEndpoint() ||
             response_endpoint ==
                 database_information.GetAddGlobalVariableEndpoint()) {
    result.data = std::make_shared<HAKCPayload>(payload);
  } else {
    // unknown endpoint
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unknown endpoint " << response_endpoint << "\n";
    throw std::exception();
  }
}

HAKCDatabaseResponse::HAKCDatabaseResponse(
    const HAKCDatabaseInformation &database_information)
    : Response(), Timeout(database_information.GetServerTimeout()),
      Success(false), response_endpoint(), result(HAKCResult()),
      database_information(database_information), terminate_connection(false) {}


void HAKCDatabaseRequest::operator>>(raw_ostream &OS) const {
  std::string RequestJSON;
  raw_string_ostream RequestStream(RequestJSON);
  RequestStream << Request;
  size_t RequestSize = RequestJSON.size();
  /* The << operator for size_t does not seem to write 8 bytes, so specifically
   * write 8 bytes */
  OS.write((const char *)&RequestSize, sizeof(RequestSize));
  OS << RequestJSON;
  OS.flush();
}

HAKCDatabaseResponse::operator bool() const { return Success; }

ssize_t HAKCDatabaseResponse::ReadFromSocket(raw_socket_stream &OS, void *Dest,
                                             ssize_t Size) {
  ssize_t BytesRead;
  auto start = TimeRecord::getCurrentTime();
  do {
    BytesRead = OS.read(static_cast<char *>(Dest), Size);
  } while (BytesRead != Size);
  auto end = TimeRecord::getCurrentTime();
  auto duration = end.getWallTime() - start.getWallTime();

  if (OS.has_error()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "There was an error reading " << Size
        << " bytes from the policy server socket: " << OS.error().message()
        << "\nThe duration was " << duration << "\n";
    throw std::exception();
  }

  return BytesRead;
}

void HAKCDatabaseResponse::operator<<(raw_socket_stream &OS) {
  Response = "";
  ssize_t ResponseSize = 0;

  ReadFromSocket(OS, &ResponseSize, sizeof(ResponseSize));
  Response.resize(ResponseSize);
  auto LastReadSize = ReadFromSocket(OS, Response.data(), ResponseSize);

  Success = LastReadSize > 0 && ResponseSize == LastReadSize;
  if (!Success) {
    CommonHAKCAnalysis::getLogger(Fatal)
    << "Received invalid / malformed HAKCResponse\n";
    throw std::exception();
  }

  auto parsed_json = json::parse(Response);
  if (auto E = parsed_json.takeError()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Error Parsing HAKCDatabaseResponse JSON: "
        << llvm::toString(std::move(E)) << "\n";
    throw std::exception();
  }

  // now unpack the first wrapper (result, response_endpoint)
  auto hakc_response = parsed_json->getAsObject();
  if (!hakc_response) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Unable to get HAKCResponse!\n";
    throw std::exception();
  }

  // response_endpoint = GetValue<StringRef>(hakc_response, "ResponseEndpoint");
  response_endpoint = GetString(hakc_response, "ResponseEndpoint");
  auto hakc_result = hakc_response->getObject("Result");
  if (!hakc_result) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Unable to get HAKCResult!\n";
    throw std::exception();
  }

  parse_result(hakc_result);
  CommonHAKCAnalysis::getLogger(Debug) << "Successfully read from socket, returning payload!\n";
}

HAKCDatabaseConnection::HAKCDatabaseConnection(
    const HAKCDatabaseInformation &DatabaseInformation, bool debug)
    : Socket(nullptr), DatabaseInformation(DatabaseInformation), debug(debug) {}

HAKCDatabaseResponse HAKCDatabaseConnection::HandleRequest(
    const HAKCDatabaseRequest &Request) const {
  HAKCDatabaseResponse Response(DatabaseInformation);
  Request >> *Socket;
  Response << *Socket;
  return Response;
}

void HAKCDatabaseConnection::SendTerminateConnection(
    const HAKCDatabaseRequest &Request) const {
  Request >> *Socket;
}

HAKCDatabaseConnection::operator bool() const { return CheckConnection(); }

void HAKCDatabaseConnection::close() {
  if (Socket) {
    Socket->flush();
    Socket->close();
    Socket = nullptr;
  }
}

bool HAKCDatabaseConnection::CheckConnection() const {
  return Socket != nullptr;
}

void HAKCDatabaseConnection::connect() {
  close();
  unsigned current_try = 0;
  auto TimeoutInSeconds = DatabaseInformation.GetServerTimeout().count() / 1000;
  if (TimeoutInSeconds == 0) {
    TimeoutInSeconds = 1;
  }
  CommonHAKCAnalysis::getLogger(Debug) << "Connecting...";
  while (true) {
    try {
      CommonHAKCAnalysis::getLogger(Debug) << "...";
      auto NewConnection = raw_socket_stream::createConnectedUnix(
          DatabaseInformation.GetServerURL());
      if (!NewConnection) {
        /* NB: calling consuming all the errors is required in order for the
         * Expected object to be properly destructed. llvm::toString does
         * this.
         */
        CommonHAKCAnalysis::getLogger(Verbose)
            << "\nError connecting to " << DatabaseInformation.GetServerURL()
            << ": " << llvm::toString(NewConnection.takeError()) << "\n";
        throw std::exception();
      }
      CommonHAKCAnalysis::getLogger(Debug)
          << "Connected to " << DatabaseInformation.GetServerURL() << "\n";
      Socket = std::move(*NewConnection);
      break;
    } catch (...) {
      current_try++;
      if (current_try >= DatabaseInformation.GetMaxRetries()) {
        break;
      }
      sleep(TimeoutInSeconds);
    }
  }

  if (!CheckConnection()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Could not connect to " << DatabaseInformation.GetServerURL()
        << "\n";
    throw std::exception();
  }
}

} // namespace llvm::hakc
