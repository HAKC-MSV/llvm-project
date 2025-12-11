//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the information associated with a function that is
/// being analyzed for compartmentalization
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCFUNCTIONINFO_H
#define HAKC_HAKCFUNCTIONINFO_H

#include "HAKCSymbolInfo.h"
#include "HAKCIndirectCallSource.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <bitset>

using namespace llvm;

namespace llvm::hakc {

    class HAKCFunctionInfo : public HAKCSymbolInfo {
    public:
        HAKCFunctionInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

        void SetFunction(Function *F);

        Function *GetFunction() const;

        void AddTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty);

        unsigned GetTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty);

        void ModifyTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty, TypePerms perm);

        void AddDirectCall(const std::shared_ptr<HAKCFunctionInfo> &DirectCall);

        void AddIndirectCall(const std::shared_ptr<HAKCIndirectCallSource> &Source);

        std::string GetYaml(unsigned Indents) const override;

        StringRef GetYamlIdentifier() const override;

    std::map<std::shared_ptr<HAKCTypeInfo>, unsigned> TypesUsed;
    protected:
        std::set<std::shared_ptr<HAKCFunctionInfo> > DirectCalls;
        std::set<std::shared_ptr<HAKCIndirectCallSource> > IndirectCalls;
    };
} // hakc

#endif //HAKC_HAKCFUNCTIONINFO_H
