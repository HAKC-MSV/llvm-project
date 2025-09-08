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

template <typename T> T GetValue(json::Object *payload, StringRef key) {

  T maybe_value;
  if (std::is_same_v<T, int> || std::is_same_v<T, unsigned>) {
    maybe_value = payload->getInteger(key);
  } else if (std::is_same_v<T, StringRef>, std::is_same_v<T, std::string>) {
    maybe_value = payload->getString(key);
  } else if (std::is_same_v<T, bool>) {
    maybe_value = payload->getBoolean(key);
  } else {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unable to get " << key
        << " of from payload because the type is unrecognized!\n";
    throw std::exception();
  }

  if (!maybe_value.has_value()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unable to get " << key << " of from payload!\n";
    throw std::exception();
  }
  return maybe_value.value();
}

unsigned GetInteger(json::Object *payload, StringRef key) {
  auto maybe_value = payload->getInteger(key);
  if (!maybe_value.has_value()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unable to get " << key << " from payload!\n";
    throw std::exception();
  }
  return maybe_value.value();
}

std::vector<unsigned> GetIntegerArray(json::Object *payload, StringRef key) {
  auto json_array = payload->getArray(key);
  std::vector<unsigned> array;
  for (const auto &element : *json_array) {
    // TODO: add value check here, but it should really never fail
    array.push_back(element.getAsInteger().value());
  }
  return array;
}

HAKCDivisionPayload::HAKCDivisionPayload(json::Object *payload)
    : HAKCPayload(payload), DivisionID(0), Salt(0), AccessToken(0) {
  auto division = payload->getObject("Division");
  if (!division) {
    CommonHAKCAnalysis::getLogger(Fatal)
            << "Unable to get 'Division' from payload!\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'Division' from payload!\n";
  DivisionID = GetInteger(division, "DivisionID");
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'DivisionID' from payload!\n";
  Salt = GetInteger(division, "Salt");
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'Salt' from payload!\n";
  AccessToken = GetInteger(division, "AccessToken");
  CommonHAKCAnalysis::getLogger(Fatal) << "Got 'AccessToken' from payload!\n";
}

HAKCCompartmentPayload::HAKCCompartmentPayload(json::Object *payload)
    : HAKCPayload(payload), CompartmentID(), EntryToken() {
  auto compartment = payload->getObject("Compartment");
  if (!compartment) {
    CommonHAKCAnalysis::getLogger(Fatal)
            << "Unable to get 'Compartment' from payload!\n";
    throw std::exception();
  }
  CompartmentID = GetInteger(compartment, "CompartmentID");
  EntryToken = GetInteger(compartment, "EntryToken");
}

HAKCDivisionCompartmentPayload::HAKCDivisionCompartmentPayload(
    json::Object *payload)
    : HAKCPayload(payload) {
  auto division = payload->getObject("Division");
  if (!division) {
    CommonHAKCAnalysis::getLogger(Fatal)
            << "Unable to get 'Division' from payload!\n";
    throw std::exception();
  }
  auto compartment = payload->getObject("Compartment");
  if (!compartment) {
    CommonHAKCAnalysis::getLogger(Fatal)
            << "Unable to get 'Compartment' from payload!\n";
    throw std::exception();
  }
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
  CommonHAKCAnalysis::getLogger(Fatal) << "Got successful result!\n";
  // TODO: update if endpoints change
  if (response_endpoint ==
      database_information.GetTerminateConnectionEndpoint()) {
    terminate_connection = true;
    return;
  }
  auto payload = _result->getObject("Data");
  if (!payload){
    CommonHAKCAnalysis::getLogger(Fatal) << "Unable to get 'Data' payload!\n";
  }
  if (response_endpoint == database_information.GetCompartmentEndpoint()) {
    result.data = std::make_shared<HAKCCompartmentPayload>(payload);
  } else if (response_endpoint == database_information.GetDivisionEndpoint()) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Trying to get Division payload!\n";
    result.data = std::make_shared<HAKCDivisionPayload>(payload);
    CommonHAKCAnalysis::getLogger(Fatal) << " Got complete Division payload!\n";
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
  CommonHAKCAnalysis::getLogger(Fatal) << "Exiting parse result successfully\n";
}

HAKCDatabaseResponse::HAKCDatabaseResponse(
    const HAKCDatabaseInformation &database_information)
    : Response(), Timeout(database_information.GetServerTimeout()),
      Success(false), response_endpoint(), result(HAKCResult()),
      database_information(database_information), terminate_connection(false) {}

Expected<json::Value> HAKCDatabaseResponse::GetJSON() const {
  if (!Success) {
    // TODO: we need to put better error handling here
    return llvm::createStringError(std::errc::timed_out, "Response timed out");
  }
  return json::parse(Response);
}

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
  CommonHAKCAnalysis::getLogger(Fatal) << "Read " << BytesRead << " bytes, with size = " << Size << "\n";
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
  CommonHAKCAnalysis::getLogger(Fatal) << "Last ResponseSize " << ResponseSize << "\n";
  auto LastReadSize = ReadFromSocket(OS, Response.data(), ResponseSize);
  Success = LastReadSize > 0 && ResponseSize == LastReadSize;

  auto ParsedJson = GetJSON();
  if (auto E = ParsedJson.takeError()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Error Parsing HAKCDatabaseResponse JSON: "
        << llvm::toString(std::move(E)) << "\n";
    throw std::exception();
  }

  // now unpack the first wrapper (result, response_endpoint)
  auto hakc_response = ParsedJson->getAsObject();
  auto _response_endpoint = hakc_response->getString("ResponseEndpoint");
  if (!_response_endpoint.has_value()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Error Parsing HAKCDatabaseResponse ResponseEndpoint: Null\n";
    throw std::exception();
  }
  response_endpoint = _response_endpoint.value().str();
  parse_result(hakc_response->getObject("Result"));
  CommonHAKCAnalysis::getLogger(Fatal) << "Successfully read from socket, returning payload!\n";
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
