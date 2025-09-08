//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace llvm::hakc {

HAKCCompartmentalizationPolicyDAG::HAKCCompartmentalizationPolicyDAG(
    HAKCSystemInformation &SystemInformation)
    : HAKCCompartmentalizationPolicy(SystemInformation), GV_to_divs({}),
      GV_to_div_incr(0) {}

HAKCCompartmentDivision &
HAKCCompartmentalizationPolicyDAG::GetDivision(GlobalValue *GV) {
  // TODO: make a proper Null hakccompartmentdivision and hakccompartment
  // used in dag analysis; always put each symbol in its own compartment (but
  // all in the same division)
  if (!GV_to_divs.contains(GV)) {
    LLVMContext &ctx = SystemInformation.GetModule().getContext();
    unsigned CompartmentID = GV_to_div_incr;
    std::shared_ptr<HAKCCompartmentDivision> Division =
        std::make_shared<HAKCCompartmentDivision>(
            HAKCCompartment(CompartmentID, 0, ctx), 0, 0, ctx);
    GV_to_divs[GV] = Division;
    GV_to_div_incr++;
  }

  return *GV_to_divs[GV];
}

HAKCCompartmentDivision &
HAKCCompartmentalizationPolicyDAG::GetDefaultDivision() {
  LLVMContext &ctx = SystemInformation.GetModule().getContext();
  std::shared_ptr<HAKCCompartmentDivision> Division =
      std::make_shared<HAKCCompartmentDivision>(HAKCCompartment(0, 0, ctx), 0,
                                                0, ctx);
  return *Division;
}

HAKCCompartmentalizationPolicy::HAKCCompartmentalizationPolicy(
    HAKCSystemInformation &SystemInformation)
    : SystemInformation(SystemInformation), Compartments(), Divisions(),
      Client(SystemInformation.GetDatabaseInformation(), false),
      SymbolDivisionMap() {
  // no need to connect to database if doing purely analysis
  if (SystemInformation.GetPassMode() == RunCompartmentalization) {
    ConnectToDatabase();
  }
}

HAKCCompartmentalizationPolicy::~HAKCCompartmentalizationPolicy() {
  DisconnectFromDatabase();
}

void HAKCCompartmentalizationPolicy::DisconnectFromDatabase() {
  Client.close();
}

void HAKCCompartmentalizationPolicy::ConnectToDatabase() {
  CommonHAKCAnalysis::getLogger(Debug)
      << "Connecting to "
      << SystemInformation.GetDatabaseInformation().GetServerURL() << "\n";
  Client.connect();
}

void HAKCCompartmentalizationPolicy::CheckConnection() const {
  if (!Client) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Client is unavailable\n";
    throw std::exception();
  }
}

HAKCDivisionP HAKCCompartmentalizationPolicy::FindCachedSymbolDivision(
    HAKCSymbolP Symbol) const {
  for (auto &it : SymbolDivisionMap) {
    if (it.first == Symbol) {
      return it.second;
    }
  }
  return nullptr;
}

hakc::HAKCCompartmentDivision &
HAKCCompartmentalizationPolicy::GetDivision(GlobalValue *GV) {
  // Get Division from Global Value -> query for (division_id, access_token,
  // compartment_id, entry_token)
  auto HAKCSymbol = SystemInformation.GetTypeIdentifier().FindSymbol(GV, true);
  if (!HAKCSymbol) {
    CommonHAKCAnalysis::getLogger(Debug)
        << "Could not find HAKCSymbol for " << GV << "\n";
    return GetDefaultDivision();
  }
  auto CachedDivision = FindCachedSymbolDivision(HAKCSymbol);
  if (CachedDivision) {
    return *CachedDivision;
  }

  std::string ObjectYaml;
  raw_string_ostream os(ObjectYaml);
  os << *HAKCSymbol;
  json::Object Parameters({{"object", ObjectYaml}});
  HAKCResult result = Execute(
      SystemInformation.GetDatabaseInformation().GetSymbolDivisionEndpoint(),
      Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Failed request to "
        << SystemInformation.GetDatabaseInformation()
               .GetSymbolDivisionEndpoint()
        << " on GV " << *GV << " with error " << result.error << "\n";
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

HAKCCompartmentDivision &HAKCCompartmentalizationPolicy::GetDefaultDivision() {
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

  auto result =
      Execute(SystemInformation.GetDatabaseInformation().GetDivisionEndpoint(),
              Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Failed request to "
        << SystemInformation.GetDatabaseInformation().GetDivisionEndpoint()
        << " on compartment_id " << CompartmentID << " and division_id "
        << DivisionID << " with error " << result.error << "\n";
    throw std::exception();
  }
  // TODO: put isa check here?
  std::shared_ptr<HAKCDivisionPayload> division_payload =
      std::dynamic_pointer_cast<HAKCDivisionPayload>(
          result.GetData());

  // TODO: consolidate into one query?
  auto access_token = division_payload->AccessToken;
  auto compartment = GetCompartment(CompartmentID);
  Division = std::make_shared<HAKCCompartmentDivision>(
      *compartment, DivisionID, static_cast<hakc_access_token_t>(access_token),
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

  auto result = Execute(
      SystemInformation.GetDatabaseInformation().GetCompartmentEndpoint(),
      Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Failed request to "
        << SystemInformation.GetDatabaseInformation().GetCompartmentEndpoint()
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

void HAKCCompartmentalizationPolicy::GetValidTargets(
    HAKCCompartment &Compartment) {
  hakc_compartment_id_t CompartmentID = Compartment.GetCompartmentIDValue();
  if (RetrievedTargetCompartments.contains(CompartmentID)) {
    return;
  }

  json::Object Parameters({
      {"compartment-id", std::to_string(CompartmentID)},
  });

  auto result = Execute(
      SystemInformation.GetDatabaseInformation().GetValidTargetsEndpoint(),
      Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Failed request to "
        << SystemInformation.GetDatabaseInformation().GetValidTargetsEndpoint()
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

HAKCCompartmentP HAKCCompartmentalizationPolicy::CreateCompartment(
    hakc_compartment_id_t CompartmentID, hakc_access_token_t EntryToken,
    bool CheckForExisting) {
  if (CheckForExisting) {
    if (auto Compartment = FindCachedCompartment(CompartmentID)) {
      return Compartment;
    }
  }

  auto Compartment = std::make_shared<HAKCCompartment>(
      CompartmentID, EntryToken, SystemInformation.GetModule().getContext());
  GetValidTargets(*Compartment);
  Compartments.push_back(Compartment);
  return Compartment;
}

HAKCResult HAKCCompartmentalizationPolicy::Execute(StringRef Endpoint,
                                                   json::Object &Parameters) {
  auto currentTime = std::chrono::system_clock::now();
  auto milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          currentTime.time_since_epoch());
  CommonHAKCAnalysis::getLogger(Verbose)
      << milliseconds_since_epoch.count() << " "
      << "Executing command \n";

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
