//
// Created by de29664 on 7/29/24.
//

#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCDatabase.h"
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
}

HAKCDatabaseConnection::HAKCDatabaseConnection(
    const HAKCDatabaseInformation &DatabaseInformation, bool debug)
    : Socket(nullptr), DatabaseInformation(DatabaseInformation), debug(debug) {}

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
        CommonHAKCAnalysis::getLogger(Debug)
            << "Error connecting to " << DatabaseInformation.GetServerURL()
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
