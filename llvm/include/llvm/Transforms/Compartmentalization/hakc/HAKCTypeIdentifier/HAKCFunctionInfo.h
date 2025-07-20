//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCFUNCTIONINFO_H
#define HAKC_HAKCFUNCTIONINFO_H

#include "HAKCSymbolInfo.h"
#include "HAKCIndirectCallSource.h"
#include <bitset>

using namespace llvm;

namespace llvm::hakc {

    enum RWX_MASK {
        Read = 0b100,
        Write = 0b010,
        Execute = 0b001
    };

    class HAKCFunctionInfo : public HAKCSymbolInfo {
    public:
        HAKCFunctionInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

        void SetFunction(Function *F);

        Function *GetFunction() const;

        void AddTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty);

        unsigned GetTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty);

        void ModifyTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty, RWX_MASK mask);

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
