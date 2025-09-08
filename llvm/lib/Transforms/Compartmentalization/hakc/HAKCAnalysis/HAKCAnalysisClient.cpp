//
// Created by de29664 on 7/29/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCAnalysisClient.h"

#include "../../../../../../lld/MachO/Config.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"

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
  CheckConnection();
  json::Object Parameters({{"CLIENT TERMINATING CONNECTION", true}});
  HAKCDatabaseRequest Request(SystemInformation.GetDatabaseInformation().GetTerminateConnectionEndpoint(), Parameters);
  Client.SendTerminateConnection(Request);
}

HAKCResult
HAKCAnalysisClient::Execute(StringRef Endpoint,
                                        json::Object &Parameters) {
  CheckConnection();
  HAKCDatabaseRequest Request(Endpoint, Parameters);
  HAKCDatabaseResponse Response = Client.HandleRequest(Request);
  if (!Response) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Error Handling Request to " << Endpoint << "\n";
    throw std::exception();
  }
  if ( Response.ShouldTerminateConnection() ) {
    DisconnectFromDatabase();
  }

  return Response.GetResult();
}

void HAKCAnalysisClient::set_dag_filename(StringRef filename) {
  json::Object Parameters({{"dag-filename", filename}});
  auto result = Execute(
      SystemInformation.GetDatabaseInformation().GetSetDagFilenameEndpoint(),
      Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid Response for set_dag_filename with error: " << result.error << "\n";
    throw std::exception();
  }
}


void HAKCAnalysisClient::add_symbols(ArrayRef<std::shared_ptr<HAKCFunctionInfo>> FIs, ArrayRef<std::shared_ptr<HAKCGlobalInfo>> GIs) {
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

  auto result = Execute(
  SystemInformation.GetDatabaseInformation().GetAddSymbolsEndpoint(), Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Invalid Response for AllSymbols\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::add_function(const HAKCFunctionInfo &FI) {
  CommonHAKCAnalysis::getLogger(Debug) << "Sending add-function: " << FI.GetFunction()->getName() << " of type " << FI.GetFunction()->getFunctionType() << "\n";
  std::string ObjectYaml;
  raw_string_ostream os(ObjectYaml);
  os << FI.GetYaml(0);
  json::Object Parameters({{"object", ObjectYaml}});

  auto result = Execute(
      SystemInformation.GetDatabaseInformation().GetAddFunctionEndpoint(),
      Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)<< "Invalid Response for " << *FI.GetFunction() << " with error: " << result.error << "\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::add_global_variable(const HAKCGlobalInfo &GI) {
	CommonHAKCAnalysis::getLogger(Debug) << "Sending add-global-variable: " << GI.GetGlobalVariable()->getName()  << " of type " << GI.GetGlobalVariable()->getType()  << "\n";
  std::string ObjectYaml;
  raw_string_ostream os(ObjectYaml);
  os << GI.GetYaml(0);
  json::Object Parameters({{"object", ObjectYaml}});

  auto result = Execute(
      SystemInformation.GetDatabaseInformation().GetAddGlobalVariableEndpoint(),
      Parameters);
  if (!result.success) {
    CommonHAKCAnalysis::getLogger(Fatal)<< "Invalid Response for " << *GI.GetGlobalVariable() << " with error: " << result.error << "\n";
    throw std::exception();
  }
}

void HAKCAnalysisClient::SendSymbolsToAnalysisServer(HAKCModuleAnalysis &ModuleAnalysis, StringRef filename)  {

  CommonHAKCAnalysis::getLogger(Debug) << "Starting to send symbols to analysis server\n";
  auto TypeIdentifier = ModuleAnalysis.GetTypeIdentifier();
  // Note: Sorting functions is not necessary since saving as HAKCCompartmentalization is a networkx graph and will rearrange symbol order

  auto FunctionCount = TypeIdentifier.GetFunctions().size() + TypeIdentifier.GetUnmappedFunctions().size();
  std::vector<std::shared_ptr<HAKCFunctionInfo>> Functions;
  if (FunctionCount > 0) {
    for (auto &it : TypeIdentifier.GetFunctions()) {
      Functions.push_back(it.second);
    }
    for (const auto &Unmapped : TypeIdentifier.GetUnmappedFunctions()) {
      Functions.push_back(Unmapped);
    }
  }

  auto GlobalCount = TypeIdentifier.GetGlobals().size() + TypeIdentifier.GetUnmappedGlobals().size();
  std::vector<std::shared_ptr<HAKCGlobalInfo>> Globals;
  if (GlobalCount > 0){
    for (auto &it : TypeIdentifier.GetGlobals()) {
      Globals.push_back(it.second);
    }
    for (const auto &Unmapped : TypeIdentifier.GetUnmappedGlobals()) {
      Globals.push_back(Unmapped);
    }
  }

  set_dag_filename(filename);
  add_symbols(Functions, Globals);
  CommonHAKCAnalysis::getLogger(Debug) << "Finished sending symbols to analysis server\n";
}

void HAKCAnalysisClient::CloseConnection() {
  CommonHAKCAnalysis::getLogger(Verbose) << "Closing connection\n";
  SendTerminateConnection();
  DisconnectFromDatabase();
  CommonHAKCAnalysis::getLogger(Verbose) << "Closed connection\n";
}

} // namespace llvm::hakc
