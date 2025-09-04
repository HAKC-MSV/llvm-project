//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCAnalysisClient.h"

#include "../../../../../../lld/MachO/Config.h"

#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include <unistd.h>

namespace llvm::hakc {

HAKCAnalysisClient::HAKCAnalysisClient(
    HAKCSystemInformation &SystemInformation)
    : SystemInformation(SystemInformation), Client(SystemInformation.GetDatabaseInformation(), false){
	ConnectToDatabase();
}

HAKCAnalysisClient::~HAKCAnalysisClient() {
  DisconnectFromDatabase();
}

void HAKCAnalysisClient::DisconnectFromDatabase() {
  Client.close();
}

void HAKCAnalysisClient::ConnectToDatabase() {
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Connecting to "
      << SystemInformation.GetDatabaseInformation().GetServerURL() << "\n";
  Client.connect();
}

void HAKCAnalysisClient::CheckConnection() const {
  if (!Client) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Client is unavailable\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::SendTerminateConnection() const {
  auto currentTime = std::chrono::system_clock::now();
  auto milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          currentTime.time_since_epoch());
  CommonHAKCAnalysis::getLogger(Verbose)
      << milliseconds_since_epoch.count() << " "
      << "Executing command \n";

  CheckConnection();
  json::Object Parameters({{"CLIENT TERMINATING CONNECTION", true}});
  HAKCDatabaseRequest Request(SystemInformation.GetDatabaseInformation().GetTerminateConnectionEndpoint(), Parameters);
  Client.SendTerminateConnection(Request);
}

json::Object
HAKCAnalysisClient::Execute(StringRef Endpoint,
                                        json::Object &Parameters) const {
  auto currentTime = std::chrono::system_clock::now();
  auto milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          currentTime.time_since_epoch());
  CommonHAKCAnalysis::getLogger(Verbose)
      << milliseconds_since_epoch.count() << " "
      << "Executing command \n";

  CheckConnection();
  HAKCDatabaseRequest Request(Endpoint, Parameters);
  auto Response = Client.HandleRequest(Request);
  if (!Response) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Error Handling Request to " << Endpoint << "\n";
    throw std::exception();
  }
  auto ParsedJson = Response.GetJSON();
  if (auto E = ParsedJson.takeError()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Error Parsing JSON: " << llvm::toString(std::move(E)) << "\n";
    throw std::exception();
  }
  for (auto pair : *ParsedJson->getAsObject()) {
    if (pair.getFirst() == "CLIENT TERMINATING CONNECTION") {
      auto currentTime = std::chrono::system_clock::now();
      auto milliseconds_since_epoch =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              currentTime.time_since_epoch());
      CommonHAKCAnalysis::getLogger(Fatal)
          << milliseconds_since_epoch.count() << " "
          << "Unrecoverable error on Analysis Server; Analysis Client stopping!!\n";

      throw std::runtime_error(
          "Unrecoverable error on Analysis Server; Analysis Client stopping!!\n");
    }
  }
  auto Obj = ParsedJson->getAsObject();
  return *Obj;
}

void HAKCAnalysisClient::set_dag_filename(StringRef filename) const {
  json::Object Parameters({{"dag-filename", filename}});
  auto ResponseData = Execute(
      SystemInformation.GetDatabaseInformation().GetSetDagFilenameEndpoint(),
      Parameters);
  auto Success = ResponseData.getBoolean("Success");
  if (!Success) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid Response for set_dag_filename\n";
    throw std::exception();
  }
}


void HAKCAnalysisClient::add_symbols(std::vector<std::shared_ptr<HAKCFunctionInfo>> &FIs, std::vector<std::shared_ptr<HAKCGlobalInfo>> &GIs) const{
  CommonHAKCAnalysis::getLogger(Debug) << "Sending add-symbols with " << FIs.size() << " functions and " << GIs.size() << " global variables\n";

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

  auto ResponseData = Execute(
  SystemInformation.GetDatabaseInformation().GetAddSymbolsEndpoint(), Parameters);
  auto Success = ResponseData.getBoolean("Success");
  if (!Success) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid Response for AllSymbols\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::add_function(const HAKCFunctionInfo &FI) const{
  CommonHAKCAnalysis::getLogger(Debug) << "Sending add-function: " << FI.GetFunction()->getName() << " of type " << FI.GetFunction()->getFunctionType() << "\n";
  std::string ObjectYaml;
  raw_string_ostream os(ObjectYaml);
  os << FI.GetYaml(0);
  json::Object Parameters({{"object", ObjectYaml}});

  auto ResponseData = Execute(
      SystemInformation.GetDatabaseInformation().GetAddFunctionEndpoint(),
      Parameters);
  auto Success = ResponseData.getBoolean("Success");
  if (!Success) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Invalid Response for " << *FI.GetFunction() << "\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::add_global_variable(const HAKCGlobalInfo &GI) const{
	CommonHAKCAnalysis::getLogger(Debug) << "Sending add-global-variable: " << GI.GetGlobalVariable()->getName()  << " of type " << GI.GetGlobalVariable()->getType() << "\n";
  std::string ObjectYaml;
  raw_string_ostream os(ObjectYaml);
  os << GI.GetYaml(0);
  json::Object Parameters({{"object", ObjectYaml}});

  auto ResponseData = Execute(
      SystemInformation.GetDatabaseInformation().GetAddGlobalVariableEndpoint(),
      Parameters);
  auto Success = ResponseData.getBoolean("Success");
  if (!Success) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Invalid Response for " << *GI.GetGlobalVariable() << "\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::SendSymbolsToAnalysisServer(HAKCModuleAnalysis &ModuleAnalysis) const {

  CommonHAKCAnalysis::getLogger(Debug) << "Starting to send symbols to analysis server\n";
  auto TypeIdentifier = ModuleAnalysis.GetTypeIdentifier();

  std::vector<std::shared_ptr<HAKCGlobalInfo>> SortedGlobals;
  auto GlobalCount = TypeIdentifier.GetGlobals().size() +
                     TypeIdentifier.GetUnmappedGlobals().size();
  SortedGlobals.reserve(GlobalCount);
  for (auto &it : TypeIdentifier.GetGlobals()) {
    SortedGlobals.push_back(it.second);
  }
  for (const auto &Unmapped : TypeIdentifier.GetUnmappedGlobals()) {
    SortedGlobals.push_back(Unmapped);
  }
  llvm::sort(SortedGlobals.begin(), SortedGlobals.end(),
             [](const std::shared_ptr<HAKCGlobalInfo> &LHS,
                const std::shared_ptr<HAKCGlobalInfo> &RHS) {
               return LHS->GetName() < RHS->GetName();
             });

  // for (auto &it: SortedGlobals) {
  //   add_global_variable(*it);
  // }

  auto FunctionCount = TypeIdentifier.GetFunctions().size() +
                       TypeIdentifier.GetUnmappedFunctions().size();

  std::vector<std::shared_ptr<HAKCFunctionInfo>> SortedFunctions;
  SortedFunctions.reserve(FunctionCount);
  for (auto &it : TypeIdentifier.GetFunctions()) {
    SortedFunctions.push_back(it.second);
  }
  for (const auto &Unmapped : TypeIdentifier.GetUnmappedFunctions()) {
    SortedFunctions.push_back(Unmapped);
  }
  llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
             [](const std::shared_ptr<HAKCFunctionInfo> &LHS,
                const std::shared_ptr<HAKCFunctionInfo> &RHS) {
               return LHS->GetName() < RHS->GetName();
             });

  // for (auto &it: SortedFunctions) {
  //   add_function(*it);
  // }
  add_symbols(SortedFunctions, SortedGlobals);
  CommonHAKCAnalysis::getLogger(Debug) << "Finished sending symbols to analysis server\n";
}

void HAKCAnalysisClient::CloseConnection() {
  CommonHAKCAnalysis::getLogger(Verbose) << "Closing connection\n";
  SendTerminateConnection();
  DisconnectFromDatabase();
  CommonHAKCAnalysis::getLogger(Verbose) << "Closed connection\n";
}


} // namespace llvm::hakc
