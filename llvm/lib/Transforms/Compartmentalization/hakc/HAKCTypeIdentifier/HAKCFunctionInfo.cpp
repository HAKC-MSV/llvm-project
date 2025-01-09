//
// Created by de29664 on 5/2/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace llvm::hakc {
    HAKCFunctionInfo::HAKCFunctionInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive) : HAKCSymbolInfo(
            Analysis, Name, DebugActive), DirectCalls(), IndirectCalls() {
    }

    void HAKCFunctionInfo::SetFunction(Function *F) {
        SetGlobalObj(F);
    }

    Function *HAKCFunctionInfo::GetFunction() {
        return dyn_cast<Function>(GetGlobalObj());
    }

    StringRef HAKCFunctionInfo::GetYamlIdentifier() const {
        return "!HAKCFunction";
    }

    void HAKCFunctionInfo::AddIndirectCall(const std::shared_ptr<HAKCIndirectCallSource> &Source) {
        if (!Source) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null indirect call source\n";
            throw std::exception();
        }
        IndirectCalls.insert(Source);
    }

    void HAKCFunctionInfo::AddDirectCall(const std::shared_ptr<HAKCFunctionInfo> &DirectCall) {
        if (!DirectCall) {
            CommonHAKCAnalysis::getWriter() << "Trying to add null Direct Call\n";
            throw std::exception();
        }
        DirectCalls.insert(DirectCall);
    }

    std::string HAKCFunctionInfo::GetYaml(unsigned Indents) {
        auto Yaml = HAKCSymbolInfo::GetYaml(Indents);
        llvm::raw_string_ostream sstream(Yaml);

        unsigned Count;
        if (!DirectCalls.empty()) {
            sstream << "\n";
            sstream.indent(Indents + EntrySpaces()) << "DirectCalls:\n";
            Count = 0;
            for (auto &Symbol: DirectCalls) {
                sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- " << Symbol->GetYamlHeader(
                    Indents + HAKCInfo::IndentSpaces());
                if (++Count != DirectCalls.size()) {
                    sstream << "\n";
                }
            }
        }
        if (!IndirectCalls.empty()) {
            Count = 0;
            sstream << "\n";
            sstream.indent(Indents + EntrySpaces()) << "IndirectCalls:\n";
            for (auto &IndirectSource: IndirectCalls) {
                sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- " << IndirectSource->GetYaml(
                    Indents + HAKCInfo::IndentSpaces());
                if (++Count != IndirectCalls.size()) {
                    sstream << "\n";
                }
            }
        }
        return Yaml;
    }
} // hakc
