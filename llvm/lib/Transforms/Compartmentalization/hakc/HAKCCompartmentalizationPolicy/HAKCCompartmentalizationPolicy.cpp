//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include <unistd.h>

namespace llvm::hakc {
HAKCDatabaseRequest::HAKCDatabaseRequest(StringRef Endpoint,
                                         json::Object &Parameters)
    : Request(nullptr) {
  json::Object FullRequest({{"Endpoint", Endpoint}});
  if (!Parameters.empty()) {
    FullRequest.insert({"Parameters", std::move(Parameters)});
  }
  Request = std::move(FullRequest);
}

HAKCDatabaseResponse::HAKCDatabaseResponse(std::chrono::milliseconds Timeout)
    : Response(), Timeout(Timeout), Success(false) {}

Expected<json::Value> HAKCDatabaseResponse::GetJSON() const {
  if (!Success) {
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
                                             ssize_t Size) const {
  ssize_t BytesRead;
  auto start = llvm::TimeRecord::getCurrentTime();
  do {
    BytesRead = OS.read(static_cast<char *>(Dest), Size);
  } while (BytesRead != Size);

  auto end = llvm::TimeRecord::getCurrentTime();
  auto duration = end.getWallTime() - start.getWallTime();

  if (OS.has_error()) {
    CommonHAKCAnalysis::getWriter(true)
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

  try {
    ReadFromSocket(OS, &ResponseSize, sizeof(ResponseSize));
  } catch (std::exception &E) {
    CommonHAKCAnalysis::getWriter(true) << "Read 1: " << E.what() << "\n";
    throw E;
  }
  Response.resize(ResponseSize);
  try {
    auto LastReadSize = ReadFromSocket(OS, Response.data(), ResponseSize);
    Success = LastReadSize > 0 && ResponseSize == LastReadSize;
  } catch (std::exception &E) {
    CommonHAKCAnalysis::getWriter(true) << "Read 2: " << E.what() << "\n";
    throw E;
  }
}

HAKCDatabaseConnection::HAKCDatabaseConnection(
    const HAKCDatabaseInformation &DatabaseInformation, bool Debug)
    : Socket(nullptr), DatabaseInformation(DatabaseInformation), Debug(Debug) {}

HAKCDatabaseResponse HAKCDatabaseConnection::HandleRequest(
    const HAKCDatabaseRequest &Request) const {
  HAKCDatabaseResponse Response(DatabaseInformation.GetServerTimeout());
  Request >> *Socket;
  Response << *Socket;
  return Response;
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
  while (true) {
    try {
      auto NewConnection = raw_socket_stream::createConnectedUnix(
          DatabaseInformation.GetServerURL());
      if (!NewConnection) {
        /* NB: calling consuming all the errors is required in order for the
         * Expected object to be properly destructed. llvm::toString does
         * this.
         */
        CommonHAKCAnalysis::getWriter(Debug)
            << "Error connecting to " << DatabaseInformation.GetServerURL()
            << ": " << llvm::toString(NewConnection.takeError()) << "\n";
        throw std::exception();
      }
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
    CommonHAKCAnalysis::getWriter(true)
        << "Could not connect to " << DatabaseInformation.GetServerURL()
        << "\n";
    throw std::exception();
  }
}

HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(
    HAKCSystemInformation &SystemInformation)
    : SystemInformation(SystemInformation), Compartments(), Divisions(),
      Client(SystemInformation.GetDatabaseInformation(),
             SystemInformation.OutputDebugInfo()) {
  ConnectToDatabase();
}

HAKCCompartmentalizationPolicy::~HAKCCompartmentalizationPolicy() {
  DisconnectFromDatabase();
}

void HAKCCompartmentalizationPolicy::DisconnectFromDatabase() {
  Client.close();
}

void HAKCCompartmentalizationPolicy::ConnectToDatabase() {
  CommonHAKCAnalysis::getWriter(SystemInformation.OutputDebugInfo())
      << "Connecting to "
      << SystemInformation.GetDatabaseInformation().GetServerURL() << "\n";
  Client.connect();
}

void HAKCCompartmentalizationPolicy::CheckConnection() const {
  if (!Client) {
    CommonHAKCAnalysis::getWriter(true) << "Client is unavailable\n";
    throw std::exception();
  }
}

hakc::HAKCCompartmentDivision &
HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
  // Get Division from Global Value -> query for (division_id, access_token,
  // compartment_id, entry_token)
  auto HAKCSymbol = SystemInformation.GetTypeIdentifier().FindSymbol(GV, true);
  if (!HAKCSymbol) {
    CommonHAKCAnalysis::getWriter(SystemInformation.OutputDebugInfo())
        << "Could not find HAKCSymbol for " << GV << "\n";
    return GetDefaultDivision();
  }

  std::string ObjectYaml;
  llvm::raw_string_ostream os(ObjectYaml);
  os << *HAKCSymbol;
  json::Object Parameters({{"object", ObjectYaml}});
  auto ResponseData = Execute(
      SystemInformation.GetDatabaseInformation().GetSymbolDivisionEndpoint(),
      Parameters);
  auto DivisionYAML = ResponseData.getObject("Division");
  if (!DivisionYAML) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid Response for " << *GV << "\n";
    throw std::exception();
  }
  auto CompartmentYAML = ResponseData.getObject("Compartment");
  if (!CompartmentYAML) {
    CommonHAKCAnalysis::getWriter(true)
        << "Invalid Response for " << *GV << "\n";
    throw std::exception();
  }

  auto DivisionID = DivisionYAML->getInteger("DivisionID");
  if (!DivisionID.has_value()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not get DivisionID for " << *GV << "\n";
    throw std::exception();
  }
  auto DivisionAccessToken = DivisionYAML->getInteger("AccessToken");
  if (!DivisionAccessToken.has_value()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Received No Entry Token for " << *GV << "\n";
    throw std::exception();
  }
  auto CompartmentID = CompartmentYAML->getInteger("CompartmentID");
  if (!CompartmentID.has_value()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Could not find CompartmentID for " << *GV << "\n";
    throw std::exception();
  }
  auto EntryToken = CompartmentYAML->getInteger("EntryToken");
  if (!EntryToken.has_value()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Received No Entry Token for " << *GV << "\n";
    throw std::exception();
  }
  auto Compartment =
      CreateCompartment(CompartmentID.value(), *EntryToken, false);
  auto Division = std::make_shared<hakc::HAKCCompartmentDivision>(
      *Compartment,
      static_cast<hakc_compartment_division_t>(DivisionID.value()),
      static_cast<hakc_access_token_t>(*DivisionAccessToken),
      SystemInformation.GetModule().getContext());
  Divisions.push_back(Division);
  return *Division;
}

hakc::HAKCCompartmentDivision &
HAKCCompartmentalizationPolicy::GetDefaultDivision() {
  return *GetDivision(0, 0);
}

HAKCDivisionP HAKCCompartmentalizationPolicy::GetDivision(
    hakc_compartment_id_t CompartmentID,
    hakc_compartment_division_t DivisionID) {
  auto Division = FindCachedDivision(CompartmentID, DivisionID);
  if (Division) {
    return Division;
  }

  json::Object Parameters({
      {"compartment-id", std::to_string(CompartmentID)},
      {"division-id", std::to_string(DivisionID)},
  });

  auto ResponseData =
      Execute(SystemInformation.GetDatabaseInformation().GetDivisionEndpoint(),
              Parameters);
  auto DivisionAccessToken = ResponseData.getInteger("AccessToken");
  if (!DivisionAccessToken.has_value()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Received No Entry Token for Division " << DivisionID << "\n";
    throw std::exception();
  }

  auto Compartment = GetCompartment(CompartmentID);
  Division = std::make_shared<hakc::HAKCCompartmentDivision>(
      *Compartment, DivisionID,
      static_cast<hakc_access_token_t>(*DivisionAccessToken),
      SystemInformation.GetModule().getContext());
  Divisions.push_back(Division);

  return Division;
}

HAKCCompartmentP HAKCCompartmentalizationPolicy::GetCompartment(
    hakc_compartment_id_t CompartmentID) {
  auto Compartment = FindCachedCompartment(CompartmentID);
  if (Compartment) {
    return Compartment;
  }

  json::Object Parameters({
      {"compartment-id", std::to_string(CompartmentID)},
  });

  auto ResponseData = Execute(
      SystemInformation.GetDatabaseInformation().GetCompartmentEndpoint(),
      Parameters);
  auto EntryToken = ResponseData.getInteger("EntryToken");
  if (!EntryToken.has_value()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Received No Entry Token for Compartment " << CompartmentID << "\n";
    throw std::exception();
  }
  Compartment = CreateCompartment(CompartmentID, *EntryToken, false);
  return Compartment;
}

void HAKCCompartmentalizationPolicy::GetValidTargets(
    HAKCCompartment &Compartment) const {
  hakc_compartment_id_t CompartmentID = Compartment.GetCompartmentIDValue();
  json::Object Parameters({
      {"compartment-id", std::to_string(CompartmentID)},
  });

  auto ResponseData = Execute(
      SystemInformation.GetDatabaseInformation().GetValidTargetsEndpoint(),
      Parameters);
  auto ValidTargets = ResponseData.getArray("ValidTargets");
  if (!ValidTargets) {
    CommonHAKCAnalysis::getWriter(SystemInformation.OutputDebugInfo())
        << "No ValidTargets found for CompartmentID: " << CompartmentID << "\n";
    return;
  }
  for (auto target = ValidTargets->begin(); target != ValidTargets->end();
       ++target) {
    auto *TargetCompartment = HAKCCompartment::CreateID(
        static_cast<hakc_compartment_id_t>(target->getAsInteger().value()),
        SystemInformation.GetModule());
    Compartment.AddTarget(TargetCompartment);
  }
}

HAKCCompartmentP HAKCCompartmentalizationPolicy::CreateCompartment(
    hakc_compartment_id_t CompartmentID, hakc_access_token_t AccessToken,
    bool CheckForExisting) {
  if (CheckForExisting) {
    if (auto Compartment = FindCachedCompartment(CompartmentID)) {
      return Compartment;
    }
  }

  auto Compartment = std::make_shared<HAKCCompartment>(
      CompartmentID, AccessToken, SystemInformation.GetModule().getContext());
  GetValidTargets(*Compartment);
  Compartments.push_back(Compartment);
  return Compartment;
}

json::Object
HAKCCompartmentalizationPolicy::Execute(StringRef Endpoint,
                                        json::Object &Parameters) const {
  CheckConnection();
  HAKCDatabaseRequest Request(Endpoint, Parameters);
  auto Response = Client.HandleRequest(Request);
  if (!Response) {
    CommonHAKCAnalysis::getWriter(true)
        << "Error Handling Request to " << Endpoint << "\n";
    throw std::exception();
  }
  auto ParsedJson = Response.GetJSON();
  if (auto E = ParsedJson.takeError()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Error Parsing JSON: " << llvm::toString(std::move(E)) << "\n";
    throw std::exception();
  }
  auto Obj = ParsedJson->getAsObject();
  return *Obj;
}

HAKCDivisionP HAKCCompartmentalizationPolicy::FindCachedDivision(
    hakc_compartment_id_t CompartmentID,
    hakc_compartment_division_t DivisionID) {
  for (auto &Division : Divisions) {
    if (Division->GetHAKCCompartment().GetCompartmentID()->equalsInt(
            CompartmentID) &&
        Division->GetDivisionID()->equalsInt(DivisionID)) {
      return Division;
    }
  }
  return nullptr;
}

HAKCCompartmentP HAKCCompartmentalizationPolicy::FindCachedCompartment(
    hakc_compartment_id_t CompartmentID) {
  for (auto &Compartment : Compartments) {
    if (Compartment->GetCompartmentID()->equalsInt(CompartmentID)) {
      return Compartment;
    }
  }
  return nullptr;
}
} // namespace llvm::hakc
