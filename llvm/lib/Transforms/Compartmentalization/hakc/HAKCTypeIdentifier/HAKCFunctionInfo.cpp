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
            CommonHAKCAnalysis::getWriter(true) << "Trying to add null indirect call source\n";
            throw std::exception();
        }
        IndirectCalls.insert(Source);
    }

    void HAKCFunctionInfo::AddDirectCall(const std::shared_ptr<HAKCFunctionInfo> &DirectCall) {
        if (!DirectCall) {
            CommonHAKCAnalysis::getWriter(true) << "Trying to add null Direct Call\n";
            throw std::exception();
        }
        DirectCalls.insert(DirectCall);
    }

    std::string HAKCFunctionInfo::GetYaml(unsigned Indents) const {
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
// if (!ReadStructs.empty()) {
//   sstream << "\n";
//   sstream.indent(Indents + EntrySpaces()) << "ReadsStructs:\n";
//   for (auto pair : ReadStructs) {
//     sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- \n";
//     sstream.indent(Indents + (HAKCInfo::IndentSpaces() + 2)) << "Offset: " << pair.second << "\n";
//     sstream.indent(Indents + (HAKCInfo::IndentSpaces() + 2)) << "Type:\n";
//     sstream.indent(Indents + (HAKCInfo::IndentSpaces() + 4))<< pair.first->GetYamlHeader(Indents + HAKCInfo::IndentSpaces() + 2);
//     sstream << "\n";
//   }
// }
// if (!WrittenStructs.empty()) {
//   sstream << "\n";
//   sstream.indent(Indents + EntrySpaces()) << "WritesStructs:\n";
//   for (auto pair : WrittenStructs) {
//     sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- \n";
//     sstream.indent(Indents + (HAKCInfo::IndentSpaces() + 2)) << "Offset: " << pair.second << "\n";
//     sstream.indent(Indents + (HAKCInfo::IndentSpaces() + 2)) << "Type:\n";
//     sstream.indent(Indents + (HAKCInfo::IndentSpaces() + 4)) << pair.first->GetYamlHeader(Indents + HAKCInfo::IndentSpaces() + 2);
//     sstream << "\n";
//   }
// }
//
// void HAKCFunctionInfo::setReadStructs(std::set<std::pair<std::shared_ptr<hakc::HAKCTypeInfo>, int64_t>> read) {
//       ReadStructs = read;
//     }
//
// void HAKCFunctionInfo::setWrittenStructs(std::set<std::pair<std::shared_ptr<hakc::HAKCTypeInfo>, int64_t>> write) {
//       WrittenStructs = write;
//     }
} // hakc
