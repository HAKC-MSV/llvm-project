//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"


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

    Expected<json::Value> HAKCDatabaseResponse::GetJSON() {
        if (!Success) {
            return llvm::createStringError(std::errc::timed_out, "Response timed out");
        }
        return json::parse(Response);
    }

    void HAKCDatabaseRequest::operator>>(raw_ostream &OS) const {
        OS << Request;
    }

    HAKCDatabaseResponse::operator bool() const {
        return Success;
    }

    void HAKCDatabaseResponse::operator<<(raw_socket_stream &OS) {
        ssize_t LastReadSize = 0;
        Response = "";
        raw_string_ostream ResponseOstream(Response);
        do {
            SmallString<64> Buffer;
            LastReadSize = OS.read(Buffer.data(), Buffer.size(), Timeout);
            if (LastReadSize > 0) {
                ResponseOstream << Buffer;
                if (LastReadSize != (ssize_t) Buffer.size()) {
                    break;
                }
            }
        } while (LastReadSize > 0);

        Success = LastReadSize >= 0;
    }

    HAKCDatabaseConnection::HAKCDatabaseConnection(std::chrono::milliseconds Timeout) : Socket(nullptr),
        Timeout(Timeout) {
    }

    HAKCDatabaseResponse HAKCDatabaseConnection::HandleRequest(HAKCDatabaseRequest &Request) {
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
        auto NewConnection = raw_socket_stream::createConnectedUnix(ServerURL);
        if (!NewConnection) {
            CommonHAKCAnalysis::getWriter() << "Could not connect to " << ServerURL << "\n";
            throw std::exception();
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
        if (SystemInformation.OutputDebugInfo()) {
            CommonHAKCAnalysis::getWriter() << "Connecting to " << SystemInformation.GetDatabaseInformation().
                    GetServerURL() << "\n";
        }
        Client.connect(SystemInformation.GetDatabaseInformation().GetServerURL());
    }

    void HAKCCompartmentalizationPolicy::CheckConnection() const {
        if (!Client) {
            CommonHAKCAnalysis::getWriter() << "Client is unavailable\n";
            throw std::exception();
        }
    }

    hakc::HAKCCompartmentDivision &HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
        // TODO: Implement this end point
        return *GetDivision(0, 0);
    }

    HAKCDivisionP HAKCCompartmentalizationPolicy::GetDivision(hakc_compartment_id_t CompartmentID,
                                                              hakc_compartment_division_t DivisionID) {
        auto Division = FindCachedDivision(CompartmentID, DivisionID);
        if (Division) {
            return Division;
        }

        auto Compartment = GetCompartment(CompartmentID);
        // TODO: Create Get Division Endpoint
        Division = std::make_shared<hakc::HAKCCompartmentDivision>(*Compartment, 0, 0,
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
            CommonHAKCAnalysis::getWriter() << "Received No Entry Token for Compartment " << CompartmentID << "\n";
            throw std::exception();
        }
        Compartment = std::make_shared<HAKCCompartment>(CompartmentID, *EntryToken,
                                                        SystemInformation.GetModule().getContext());
        Compartments.push_back(Compartment);

        return Compartment;
    }

    json::Object HAKCCompartmentalizationPolicy::Execute(StringRef Endpoint, json::Object &Parameters) {
        CheckConnection();
        HAKCDatabaseRequest Request(Endpoint, Parameters);
        auto Response = Client.HandleRequest(Request);
        if (!Response) {
            CommonHAKCAnalysis::getWriter() << "Error Handling Request to " << Endpoint << "\n";
            throw std::exception();
        }
        auto ParsedJson = Response.GetJSON();
        if (!ParsedJson) {
            CommonHAKCAnalysis::getWriter() << "Error Parsing JSON\n";
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
} // hakc
