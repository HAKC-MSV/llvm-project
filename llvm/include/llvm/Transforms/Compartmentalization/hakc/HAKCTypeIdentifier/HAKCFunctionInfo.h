//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCFUNCTIONINFO_H
#define HAKC_HAKCFUNCTIONINFO_H

#include "HAKCSymbolInfo.h"
#include "HAKCIndirectCallSource.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCFunctionInfo : public HAKCSymbolInfo {
    public:
        HAKCFunctionInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

        void SetFunction(Function *F);

        Function *GetFunction();

        void AddDirectCall(const std::shared_ptr<HAKCFunctionInfo> &DirectCall);

        void AddIndirectCall(const std::shared_ptr<HAKCIndirectCallSource> &Source);

        std::string GetYaml(unsigned Indents) override;

        StringRef GetYamlIdentifier() const override;

    protected:
        std::set<std::shared_ptr<HAKCFunctionInfo> > DirectCalls;
        std::set<std::shared_ptr<HAKCIndirectCallSource> > IndirectCalls;
    };
} // hakc

#endif //HAKC_HAKCFUNCTIONINFO_H
