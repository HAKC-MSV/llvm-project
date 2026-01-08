//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"

namespace llvm::hakc {
    HAKCServerClientBase::HAKCServerClientBase(HAKCModuleAnalysis &ModuleAnalysis) : ModuleAnalysis(ModuleAnalysis),
        SystemInformation(ModuleAnalysis.GetCommonAnalysis().GetSystemInfo()) {
    }

HAKCServerClientBase::~HAKCServerClientBase() {}

    HAKCCompartmentP HAKCServerClientBase::GetCompartment(hakc_compartment_id_t CompartmentID) {
        if (auto Compartment = FindCachedCompartment(CompartmentID))
            return Compartment;
        return CreateCompartment(CompartmentID, 0);
    }

    HAKCCompartmentDivision &HAKCServerClientBase::GetDefaultDivision() {
        return *GetDivision(SystemInformation.GetDefaultCompartmentID(), SystemInformation.GetDefaultDivisionID());
    }

    HAKCDivisionP HAKCServerClientBase::GetDivision(hakc_compartment_id_t CompartmentID,
                                                    hakc_compartment_division_t DivisionID) {
        if (auto Division = FindCachedDivision(CompartmentID, DivisionID))
            return Division;

        return CreateDivision(CompartmentID, DivisionID,
                              ModuleAnalysis.GetCommonAnalysis().GetDefaultDivisionAccessToken(
                                  CompartmentID, DivisionID));
    }

    HAKCDivisionP HAKCServerClientBase::CreateDivision(hakc_compartment_id_t CompartmentID,
                                                       hakc_compartment_division_t DivisionID,
                                                       hakc_access_token_t AccessToken) {
        auto compartment = GetCompartment(CompartmentID);
        auto Division = std::make_shared<HAKCCompartmentDivision>(
            *compartment, DivisionID, AccessToken,
            SystemInformation.GetModule().getContext());
        Divisions.push_back(Division);
        return Division;
    }

    HAKCDivisionP
    HAKCServerClientBase::FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                             hakc_compartment_division_t DivisionID) {
        for (auto &Division: Divisions) {
            if (Division->GetHAKCCompartment().GetCompartmentID()->equalsInt(
                    CompartmentID) &&
                Division->GetDivisionID()->equalsInt(DivisionID)) {
                return Division;
            }
        }
        return nullptr;
    }

    HAKCCompartmentP
    HAKCServerClientBase::FindCachedCompartment(hakc_compartment_id_t CompartmentID) {
        for (auto &Compartment: Compartments) {
            if (Compartment->GetCompartmentID()->equalsInt(CompartmentID)) {
                return Compartment;
            }
        }
        return nullptr;
    }

    HAKCCompartmentP HAKCServerClientBase::CreateCompartment(hakc_compartment_id_t CompartmentID,
                                                             hakc_access_token_t EntryToken) {
        auto Compartment = std::make_shared<HAKCCompartment>(
            CompartmentID, EntryToken, SystemInformation.GetModule().getContext());
        GetValidTargets(*Compartment);
        Compartments.push_back(Compartment);
        return Compartment;
    }

    HAKCServerClient::HAKCServerClient(HAKCModuleAnalysis &ModuleAnalysis)
        : HAKCServerClientBase(ModuleAnalysis), Client(SystemInformation, false) {
        ConnectToDatabase();
    }

    HAKCServerClient::~HAKCServerClient() { DisconnectFromDatabase(); }

    void HAKCServerClient::DisconnectFromDatabase() { Client.close(); }

    void HAKCServerClient::ConnectToDatabase() {
        CommonHAKCAnalysis::getLogger(Verbose)
                << "Connecting to " << SystemInformation.GetSocketPath() << "\n";
        Client.connect();
    }

    void HAKCServerClient::CheckConnection() const {
        if (!Client) {
            CommonHAKCAnalysis::getLogger(Fatal) << "Client is unavailable\n";
            throw std::exception();
        }
    }

    void HAKCServerClient::SendTerminateConnection() const {
        CheckConnection();
        json::Object Parameters({{"CLIENT TERMINATING CONNECTION", true}});
        HAKCDatabaseRequest Request(
            SystemInformation.GetTerminateConnectionEndpoint(), Parameters);
        Client.SendTerminateConnection(Request);
    }

    HAKCResult HAKCServerClient::Execute(StringRef Endpoint,
                                         json::Object &Parameters) {
        CheckConnection();
        HAKCDatabaseRequest Request(Endpoint, Parameters);
        HAKCDatabaseResponse Response = Client.HandleRequest(Request);
        if (!Response) {
            CommonHAKCAnalysis::getLogger(Fatal)
                    << "Error Handling Request to " << Endpoint << "\n";
            throw std::exception();
        }
        if (Response.ShouldTerminateConnection()) {
            DisconnectFromDatabase();
        }

        return Response.GetResult();
    }

    void HAKCServerClient::add_symbols(
        ArrayRef<std::shared_ptr<HAKCFunctionInfo> > FIs,
        ArrayRef<std::shared_ptr<HAKCGlobalInfo> > GIs) {
        CommonHAKCAnalysis::getLogger(Debug)
                << "Sending add-symbols with " << FIs.size() << " functions and "
                << GIs.size() << " global variables\n";

        std::vector<std::string> AllSymbols;

        for (auto &it: FIs) {
            std::string ObjectYaml;
            raw_string_ostream os(ObjectYaml);
            os << it->GetYaml(0);
            AllSymbols.push_back(ObjectYaml);
        }
        for (auto &it: GIs) {
            std::string ObjectYaml;
            raw_string_ostream os(ObjectYaml);
            os << it->GetYaml(0);
            AllSymbols.push_back(ObjectYaml);
        }

        json::Object Parameters({{"allSymbols", AllSymbols}});

        auto result = Execute(SystemInformation.GetAddSymbolsEndpoint(), Parameters);
        if (!result.success) {
            CommonHAKCAnalysis::getLogger(Fatal) << "Invalid Response for AllSymbols\n";
            throw std::exception();
        }
    }

    void HAKCServerClient::SendSymbolsToAnalysisServer(
        HAKCTypeIdentifier &TypeIdentifier) {
        CommonHAKCAnalysis::getLogger(Debug)
                << "Starting to send symbols to analysis server\n";

        auto FunctionCount = TypeIdentifier.GetFunctions().size() +
                             TypeIdentifier.GetUnmappedFunctions().size();
        std::vector<std::shared_ptr<HAKCFunctionInfo> > Functions;
        if (FunctionCount > 0) {
            for (auto &it: TypeIdentifier.GetFunctions()) {
                Functions.push_back(it.second);
            }
            for (const auto &Unmapped: TypeIdentifier.GetUnmappedFunctions()) {
                Functions.push_back(Unmapped);
            }
        }

        auto GlobalCount = TypeIdentifier.GetGlobals().size() +
                           TypeIdentifier.GetUnmappedGlobals().size();
        std::vector<std::shared_ptr<HAKCGlobalInfo> > Globals;
        if (GlobalCount > 0) {
            for (auto &it: TypeIdentifier.GetGlobals()) {
                Globals.push_back(it.second);
            }
            for (const auto &Unmapped: TypeIdentifier.GetUnmappedGlobals()) {
                Globals.push_back(Unmapped);
            }
        }

        add_symbols(Functions, Globals);
        CommonHAKCAnalysis::getLogger(Debug)
                << "Finished sending symbols to analysis server\n";
    }

    void HAKCServerClient::CloseConnection() {
        SendSymbolsToAnalysisServer(ModuleAnalysis.GetTypeIdentifier());
        CommonHAKCAnalysis::getLogger(Verbose) << "Closing connection\n";
        SendTerminateConnection();
        DisconnectFromDatabase();
        CommonHAKCAnalysis::getLogger(Verbose) << "Closed connection\n";
    }

    HAKCDivisionP
    HAKCServerClient::FindCachedSymbolDivision(HAKCSymbolP Symbol) const {
        for (auto &it: SymbolDivisionMap) {
            if (it.first == Symbol) {
                return it.second;
            }
        }
        return nullptr;
    }

    hakc::HAKCCompartmentDivision &HAKCServerClient::GetDivision(GlobalValue *GV) {
        // Get Division from Global Value -> query for (division_id, access_token,
        // compartment_id, entry_token)
        auto HAKCSymbol = SystemInformation.GetTypeIdentifier().FindSymbol(GV, true);
        if (!HAKCSymbol) {
            CommonHAKCAnalysis::getLogger(Debug)
                    << "Could not find HAKCSymbol for " << GV << "\n";
            return GetDefaultDivision();
        }
        if (auto CachedDivision = FindCachedSymbolDivision(HAKCSymbol)) {
            return *CachedDivision;
        }

        std::string ObjectYaml;
        raw_string_ostream os(ObjectYaml);
      os << *HAKCSymbol;
        json::Object Parameters({{"object", ObjectYaml}});
      HAKCResult result =
          Execute(SystemInformation.GetSymbolDivisionEndpoint(), Parameters);
      if (!result.success) {
        CommonHAKCAnalysis::getLogger(Fatal)
            << "Failed request to "
            << SystemInformation.GetSymbolDivisionEndpoint() << " on GV " << *GV
            << " with error " << result.error << "\n";
        throw std::exception();
      }

        std::shared_ptr<HAKCDivisionCompartmentPayload> division_compartment_payload =
                std::dynamic_pointer_cast<HAKCDivisionCompartmentPayload>(
                    result.GetData());

        hakc_compartment_division_t division_id =
                division_compartment_payload->DivisionID;
        hakc_access_token_t access_token = division_compartment_payload->AccessToken;

        hakc_compartment_division_t compartment_id =
                division_compartment_payload->CompartmentID;
        hakc_compartment_division_t entry_token =
                division_compartment_payload->EntryToken;

        auto compartment = CreateCompartment(compartment_id, entry_token, false);
        auto division = std::make_shared<HAKCCompartmentDivision>(
            *compartment, division_id, access_token,
            SystemInformation.GetModule().getContext());
        Divisions.push_back(division);
        SymbolDivisionMap[HAKCSymbol] = division;
        return *division;
    }

    HAKCDivisionP
    HAKCServerClient::GetDivision(hakc_compartment_id_t CompartmentID,
                                  hakc_compartment_division_t DivisionID) {
        auto Division = FindCachedDivision(CompartmentID, DivisionID);
        if (Division) {
            return Division;
        }

        json::Object Parameters({
            {"compartment-id", std::to_string(CompartmentID)},
            {"division-id", std::to_string(DivisionID)},
        });

        auto result = Execute(SystemInformation.GetDivisionEndpoint(), Parameters);
        if (!result.success) {
            CommonHAKCAnalysis::getLogger(Fatal)
                    << "Failed request to " << SystemInformation.GetDivisionEndpoint()
                    << " on compartment_id " << CompartmentID << " and division_id "
                    << DivisionID << " with error " << result.error << "\n";
            throw std::exception();
        }
        // TODO: put isa check here?
        std::shared_ptr<HAKCDivisionPayload> division_payload =
                std::dynamic_pointer_cast<HAKCDivisionPayload>(result.GetData());

        // TODO: consolidate into one query?
        auto access_token = division_payload->AccessToken;

        return CreateDivision(CompartmentID, DivisionID, access_token);
    }

    HAKCCompartmentP
    HAKCServerClient::GetCompartment(hakc_compartment_id_t CompartmentID) {
        auto Compartment = FindCachedCompartment(CompartmentID);
        if (Compartment) {
            return Compartment;
        }

        json::Object Parameters({
            {"compartment-id", std::to_string(CompartmentID)},
        });

        auto result =
                Execute(SystemInformation.GetCompartmentEndpoint(), Parameters);
        if (!result.success) {
            CommonHAKCAnalysis::getLogger(Fatal)
                    << "Failed request to " << SystemInformation.GetCompartmentEndpoint()
                    << " on compartment_id " << CompartmentID << " with error "
                    << result.error << "\n";
            throw std::exception();
        }
        auto compartment_payload =
                std::dynamic_pointer_cast<HAKCCompartmentPayload>(result.GetData());
        auto entry_token = compartment_payload->EntryToken;
        Compartment = CreateCompartment(CompartmentID, entry_token, false);
        return Compartment;
    }

    void HAKCServerClient::GetValidTargets(HAKCCompartment &Compartment) {
        hakc_compartment_id_t CompartmentID = Compartment.GetCompartmentIDValue();
        if (RetrievedTargetCompartments.contains(CompartmentID)) {
            return;
        }

        json::Object Parameters({
            {"compartment-id", std::to_string(CompartmentID)},
        });

        auto result =
                Execute(SystemInformation.GetValidTargetsEndpoint(), Parameters);
        if (!result.success) {
            CommonHAKCAnalysis::getLogger(Fatal)
                    << "Failed request to " << SystemInformation.GetValidTargetsEndpoint()
                    << " on compartment " << Compartment << " with error " << result.error
                    << "\n";
            throw std::exception();
        }

        auto valid_targets_payload =
                std::dynamic_pointer_cast<HAKCValidTargetsPayload>(result.GetData());
        auto valid_targets = valid_targets_payload->ValidTargets;

        RetrievedTargetCompartments.insert(CompartmentID);
        if (valid_targets.size() == 0) {
            CommonHAKCAnalysis::getLogger(Debug)
                    << "No ValidTargets found for CompartmentID: " << CompartmentID << "\n";
            return;
        }
        for (auto target = valid_targets.begin(); target != valid_targets.end();
             ++target) {
            auto *TargetCompartment =
                    HAKCCompartment::CreateID(*target, SystemInformation.GetModule());
            Compartment.AddTarget(TargetCompartment);
        }
    }

    HAKCCompartmentP
    HAKCServerClient::CreateCompartment(hakc_compartment_id_t CompartmentID,
                                        hakc_access_token_t EntryToken,
                                        bool CheckForExisting) {
        if (CheckForExisting) {
            if (auto Compartment = FindCachedCompartment(CompartmentID)) {
                return Compartment;
            }
        }

        return HAKCServerClientBase::CreateCompartment(CompartmentID, EntryToken);
    }


    FakeServerClient::FakeServerClient(HAKCModuleAnalysis &ModuleAnalysis) : HAKCServerClientBase(ModuleAnalysis),
                                                                             CurrentComaprtmentID(
                                                                                 ModuleAnalysis.GetCommonAnalysis().
                                                                                 GetSystemInfo().
                                                                                 GetDefaultCompartmentID() + 1) {
    }

    void FakeServerClient::add_symbols(ArrayRef<std::shared_ptr<HAKCFunctionInfo> > FIs,
                                       ArrayRef<std::shared_ptr<HAKCGlobalInfo> > GIs) {
        return;
    }

    void FakeServerClient::SendSymbolsToAnalysisServer(HAKCTypeIdentifier &TypeIdentifier) {
        return;
    }

    void FakeServerClient::CloseConnection() {
        return;
    }

    HAKCCompartmentDivision &FakeServerClient::GetDivision(GlobalValue *GV) {
        if (SymbolDivisionMap.contains(GV)) {
            return *SymbolDivisionMap[GV];
        }
        auto CompartmentID = CurrentComaprtmentID++;
        auto DivisionID = ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().GetDefaultDivisionID();
        auto Division = CreateDivision(CompartmentID, DivisionID,
                                       ModuleAnalysis.GetCommonAnalysis().GetDefaultDivisionAccessToken(
                                           CompartmentID, DivisionID));
        SymbolDivisionMap[GV] = Division;
        return *Division;
    }

    void FakeServerClient::GetValidTargets(HAKCCompartment &Compartment) {
        for (auto &ExistingCompartment: Compartments) {
            Compartment.AddTarget(ExistingCompartment->GetCompartmentID());
        }
    }
} // namespace llvm::hakc
