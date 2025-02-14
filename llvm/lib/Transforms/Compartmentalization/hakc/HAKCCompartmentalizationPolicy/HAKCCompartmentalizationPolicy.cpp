//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include <unistd.h>

namespace llvm::hakc {
    HAKCDatabaseRequest::HAKCDatabaseRequest(StringRef Endpoint, json::Object &Parameters) : Request(nullptr) {
        json::Object FullRequest({{"Endpoint", Endpoint}});
        if (!Parameters.empty()) {
            FullRequest.insert({"Parameters", std::move(Parameters)});
        }
        Request = std::move(FullRequest);
    }

    HAKCDatabaseResponse::HAKCDatabaseResponse(std::chrono::milliseconds Timeout) : Response(), Timeout(Timeout),
        Success(false) {
    }

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
        /* The << operator for size_t does not seem to write 8 bytes, so specifically write 8 bytes */
        OS.write((const char *) &RequestSize, sizeof(RequestSize));
        OS << RequestJSON;
        OS.flush();
    }

    HAKCDatabaseResponse::operator bool() const {
        return Success;
    }

    void HAKCDatabaseResponse::operator<<(raw_socket_stream &OS) {
        Response = "";
        size_t ResponseSize = 0;
        OS.read((char *) &ResponseSize, sizeof(ResponseSize), Timeout);
        Response.resize(ResponseSize);
        auto LastReadSize = OS.read(Response.data(), ResponseSize, Timeout);
        Success = LastReadSize >= 0;
    }

    HAKCDatabaseConnection::HAKCDatabaseConnection(std::chrono::milliseconds Timeout) : Socket(nullptr),
        Timeout(Timeout) {
    }

    HAKCDatabaseResponse HAKCDatabaseConnection::HandleRequest(const HAKCDatabaseRequest &Request) const {
        HAKCDatabaseResponse Response(Timeout);
        Request >> *Socket;
        Response << *Socket;
        return Response;
    }

    HAKCDatabaseConnection::operator bool() const {
        return CheckConnection();
    }

    void HAKCDatabaseConnection::close() {
        if (Socket) {
            Socket->close();
            Socket = nullptr;
        }
    }

    bool HAKCDatabaseConnection::CheckConnection() const {
        return Socket != nullptr;
    }

    void HAKCDatabaseConnection::connect(StringRef ServerURL) {
        close();
        constexpr int max_tries = 5;
        int current_try = 1;
        auto TimeoutInSeconds = Timeout.count() / 1000;
        if (TimeoutInSeconds == 0) {
            TimeoutInSeconds = 1;
        }
        auto NewConnection = raw_socket_stream::createConnectedUnix(ServerURL);
        while (!NewConnection) {
            // connection failed, try again with a timeout 
            NewConnection = raw_socket_stream::createConnectedUnix(ServerURL);
            if (current_try >= max_tries) {
                CommonHAKCAnalysis::getWriter(true) << "Could not connect to " << ServerURL << "\n";
                throw std::exception();
            }
            current_try++;
            sleep(TimeoutInSeconds);
        }
        Socket = std::move(*NewConnection);
    }

    HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(
        HAKCSystemInformation &SystemInformation) : SystemInformation(SystemInformation), Compartments(), Divisions(),
                                                    Client(
                                                        SystemInformation.GetDatabaseInformation().GetServerTimeout()) {
        ConnectToDatabase();
    }

    HAKCCompartmentalizationPolicy::~HAKCCompartmentalizationPolicy() {
        DisconnectFromDatabase();
    }

    void HAKCCompartmentalizationPolicy::DisconnectFromDatabase() {
        Client.close();
    }

    void HAKCCompartmentalizationPolicy::ConnectToDatabase() {
        CommonHAKCAnalysis::getWriter(SystemInformation.OutputDebugInfo()) << "Connecting to " << SystemInformation.
                GetDatabaseInformation().
                GetServerURL() << "\n";
        Client.connect(SystemInformation.GetDatabaseInformation().GetServerURL());
    }

    void HAKCCompartmentalizationPolicy::CheckConnection() const {
        if (!Client) {
            CommonHAKCAnalysis::getWriter(true) << "Client is unavailable\n";
            throw std::exception();
        }
    }

    hakc::HAKCCompartmentDivision &HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
        auto HAKCSymbol = SystemInformation.GetTypeIdentifier().FindSymbol(GV, true);
        if (!HAKCSymbol) {
            CommonHAKCAnalysis::getWriter(SystemInformation.OutputDebugInfo()) << "Could not find HAKCSymbol for " << GV
                    << "\n";
            return GetDefaultDivision();
        }

        std::string ObjectYaml;
        llvm::raw_string_ostream os(ObjectYaml);
        os << *HAKCSymbol;
        json::Object Parameters({
            {"object", ObjectYaml}
        });
        auto ResponseData = Execute(SystemInformation.GetDatabaseInformation().GetSymbolDivisionEndpoint(), Parameters);
        auto CompartmentID = ResponseData.getInteger("CompartmentID");
        if (!CompartmentID.has_value()) {
            CommonHAKCAnalysis::getWriter(true) << "Could not find CompartmentID for " << *GV << "\n";
            throw std::exception();
        }
        auto DivisionID = ResponseData.getInteger("DivisionID");
        if (!DivisionID.has_value()) {
            CommonHAKCAnalysis::getWriter(true) << "Could not get DivisionID for " << *GV << "\n";
            throw std::exception();
        }
        return *GetDivision(*CompartmentID, *DivisionID);
    }

    hakc::HAKCCompartmentDivision &HAKCCompartmentalizationPolicy::GetDefaultDivision() {
        return *GetDivision(0, 0);
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::GetDivision(hakc_compartment_id_t CompartmentID,
                                                              hakc_compartment_division_t DivisionID) {
        auto Division = FindCachedDivision(CompartmentID, DivisionID);
        if (Division) {
            return Division;
        }

        json::Object Parameters(
            {
                {"compartment-id", std::to_string(CompartmentID)},
                {"division-id", std::to_string(DivisionID)},
            }
        );

        auto ResponseData = Execute(SystemInformation.GetDatabaseInformation().GetDivisionEndpoint(), Parameters);
        auto DivisionAccessToken = ResponseData.getInteger("AccessToken");
        if (!DivisionAccessToken.has_value()) {
            CommonHAKCAnalysis::getWriter(true) << "Received No Entry Token for Division " << DivisionID << "\n";
            throw std::exception();
        }

        auto Compartment = GetCompartment(CompartmentID);
        Division = std::make_shared<hakc::HAKCCompartmentDivision>(*Compartment,
                                                                   (hakc_compartment_division_t) DivisionID,
                                                                   (hakc_access_token_t) *DivisionAccessToken,
                                                                   SystemInformation.GetModule().getContext());
        Divisions.push_back(Division);

        return Division;
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::GetCompartment(hakc_compartment_id_t CompartmentID) {
        auto Compartment = FindCachedCompartment(CompartmentID);
        if (Compartment) {
            return Compartment;
        }

        json::Object Parameters(
            {
                {"compartment-id", std::to_string(CompartmentID)},
            }
        );

        auto ResponseData = Execute(SystemInformation.GetDatabaseInformation().GetCompartmentEndpoint(), Parameters);
        auto EntryToken = ResponseData.getInteger("EntryToken");
        if (!EntryToken.has_value()) {
            CommonHAKCAnalysis::getWriter(true) << "Received No Entry Token for Compartment " << CompartmentID << "\n";
            throw std::exception();
        }
        Compartment = CreateCompartment(CompartmentID, *EntryToken, false);
        return Compartment;
    }

    void HAKCCompartmentalizationPolicy::GetValidTargets(HAKCCompartment &Compartment) {
      hakc_compartment_id_t CompartmentID = Compartment.GetCompartmentIDValue();
      json::Object Parameters(
          {
              {"compartment-id", std::to_string(CompartmentID)},
          }
      );

      auto ResponseData = Execute(SystemInformation.GetDatabaseInformation().GetValidTargetsEndpoint(), Parameters);

      auto valid_targets_array = ResponseData.getArray("valid_targets");
      if (SystemInformation.OutputDebugInfo()) {
        CommonHAKCAnalysis::getWriter(true) << "Got valid_targets response: " << valid_targets_array << "\n";
      }
      for (auto target = valid_targets_array->begin(); target != valid_targets_array->end(); ++target){
        auto *TargetCompartment = HAKCCompartment::CreateID(target->getAsInteger().value(), SystemInformation.GetModule());
        Compartment.AddTarget(TargetCompartment);
      }
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::CreateCompartment(hakc_compartment_id_t CompartmentID,
                                                                       hakc_access_token_t AccessToken,
                                                                       bool CheckForExisting) {
        if (CheckForExisting) {
            auto Compartment = FindCachedCompartment(CompartmentID);
            if (Compartment) {
                return Compartment;
            }
        }

        auto Compartment = std::make_shared<HAKCCompartment>(CompartmentID, AccessToken,
                                                             SystemInformation.GetModule().getContext());
        GetValidTargets(*Compartment);
        Compartments.push_back(Compartment);
        return Compartment;
    }

    json::Object HAKCCompartmentalizationPolicy::Execute(StringRef Endpoint, json::Object &Parameters) const {
        CheckConnection();
        HAKCDatabaseRequest Request(Endpoint, Parameters);
        auto Response = Client.HandleRequest(Request);
        if (!Response) {
            CommonHAKCAnalysis::getWriter(true) << "Error Handling Request to " << Endpoint << "\n";
            throw std::exception();
        }
        auto ParsedJson = Response.GetJSON();
        if (!ParsedJson) {
            CommonHAKCAnalysis::getWriter(true) << "Error Parsing JSON\n";
            throw std::exception();
        }
        return *Response.GetJSON()->getAsObject();
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                                                     hakc_compartment_division_t DivisionID) {
        for (auto &Division: Divisions) {
            if (Division->GetHAKCCompartment().GetCompartmentID()->equalsInt(CompartmentID) &&
                Division->GetDivisionID()->equalsInt(DivisionID)) {
                return Division;
            }
        }
        return nullptr;
    }

    HAKCCompartmentP HAKCCompartmentalizationPolicy::FindCachedCompartment(hakc_compartment_id_t CompartmentID) {
        for (auto &Compartment: Compartments) {
            if (Compartment->GetCompartmentID()->equalsInt(CompartmentID)) {
                return Compartment;
            }
        }
        return nullptr;
    }
} // namespace llvm::hakc
