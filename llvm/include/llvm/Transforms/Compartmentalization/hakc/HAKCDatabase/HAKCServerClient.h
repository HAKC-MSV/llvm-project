//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the server client class which executes queries to server
/// endpoints, and maintains a connection. This is used in both analysis
/// (sending all symbols used to the server for DAG creation), and
/// policy enforcement (enforcing the actual compartmentalization policy)
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCSERVERCLIENT_H
#define HAKC_HAKCSERVERCLIENT_H

#include "llvm/Support/JSON.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartment.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCDatabase.h"

namespace llvm::hakc {
    class HAKCModuleAnalysis;
    class HAKCModuleTransform;
    class HAKCSystemInformation;

    class HAKCServerClientBase {
    public:
        explicit HAKCServerClientBase(HAKCModuleAnalysis &ModuleAnalysis);

        virtual ~HAKCServerClientBase();

        virtual void add_symbols(ArrayRef<std::shared_ptr<HAKCFunctionInfo> > FIs,
                                 ArrayRef<std::shared_ptr<HAKCGlobalInfo> > GIs) = 0;

        virtual void SendSymbolsToAnalysisServer(HAKCTypeIdentifier &TypeIdentifier) = 0;

        virtual void CloseConnection() = 0;

        virtual HAKCCompartmentDivision &GetDivision(GlobalValue *GV) = 0;

        virtual void GetValidTargets(HAKCCompartment &Compartment) = 0;

        virtual HAKCCompartmentP GetCompartment(hakc_compartment_id_t CompartmentID);

        virtual HAKCCompartmentDivision &GetDefaultDivision();

    protected:
        HAKCModuleAnalysis &ModuleAnalysis;
        HAKCSystemInformation &SystemInformation;
        std::vector<HAKCCompartmentP> Compartments;
        std::vector<HAKCDivisionP> Divisions;

        virtual HAKCDivisionP GetDivision(hakc_compartment_id_t CompartmentID,
                                          hakc_compartment_division_t DivisionID);

        HAKCCompartmentP CreateCompartment(hakc_compartment_id_t CompartmentID, hakc_access_token_t EntryToken);

        HAKCDivisionP FindCachedDivision(hakc_compartment_id_t CompartmentID,
                                         hakc_compartment_division_t DivisionID);

        HAKCCompartmentP FindCachedCompartment(hakc_compartment_id_t CompartmentID);

        HAKCDivisionP CreateDivision(hakc_compartment_id_t CompartmentID, hakc_compartment_division_t DivisionID,
                                     hakc_access_token_t AccessToken);
    };

    class HAKCServerClient : public HAKCServerClientBase {
    public:
        explicit HAKCServerClient(HAKCModuleAnalysis &ModuleAnalysis);

        ~HAKCServerClient() override;

        void add_symbols(ArrayRef<std::shared_ptr<HAKCFunctionInfo> > FIs,
                         ArrayRef<std::shared_ptr<HAKCGlobalInfo> > GIs) override;

        void SendSymbolsToAnalysisServer(HAKCTypeIdentifier &TypeIdentifier) override;

        void CloseConnection() override;

        HAKCCompartmentDivision &GetDivision(GlobalValue *GV) override;

        HAKCCompartmentP GetCompartment(hakc_compartment_id_t CompartmentID) override;

        void GetValidTargets(HAKCCompartment &Compartment) override;

    protected:
        HAKCDatabaseConnection Client;
        std::map<HAKCSymbolP, HAKCDivisionP> SymbolDivisionMap;
        std::set<hakc_compartment_id_t> RetrievedTargetCompartments;

        void CheckConnection() const;

        void ConnectToDatabase();

        void DisconnectFromDatabase();

        HAKCResult Execute(StringRef Endpoint, json::Object &Parameters);

        void SendTerminateConnection() const;

        HAKCDivisionP GetDivision(hakc_compartment_id_t CompartmentID,
                                  hakc_compartment_division_t DivisionID) override;

        HAKCCompartmentP CreateCompartment(hakc_compartment_id_t CompartmentID,
                                           hakc_access_token_t AccessToken,
                                           bool CheckForExisting);

        HAKCDivisionP FindCachedSymbolDivision(HAKCSymbolP Symbol) const;
    };

    class FakeServerClient : public HAKCServerClientBase {
    public:
        explicit FakeServerClient(HAKCModuleAnalysis &ModuleAnalysis);

        void add_symbols(ArrayRef<std::shared_ptr<HAKCFunctionInfo> > FIs,
                         ArrayRef<std::shared_ptr<HAKCGlobalInfo> > GIs) override;

        void SendSymbolsToAnalysisServer(HAKCTypeIdentifier &TypeIdentifier) override;

        void CloseConnection() override;

        HAKCCompartmentDivision &GetDivision(GlobalValue *GV) override;

        void GetValidTargets(HAKCCompartment &Compartment) override;

    protected:
        unsigned CurrentComaprtmentID;
        std::map<GlobalValue *, HAKCDivisionP> SymbolDivisionMap;
    };
} // namespace llvm::hakc

#endif // HAKC_HAKCSERVERCLIENT_H
