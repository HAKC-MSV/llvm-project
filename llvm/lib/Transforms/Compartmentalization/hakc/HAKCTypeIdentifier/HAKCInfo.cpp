//
// Created by de29664 on 5/2/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

namespace llvm::hakc {
    HAKCInfo::HAKCInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive) : Analysis(Analysis),
        DebugActive(DebugActive), Name(Name.str()) {
        if (Name.empty()) {
            CommonHAKCAnalysis::getWriter(true) << "Name is empty!\n";
            throw std::exception();
        }
    }

    CommonHAKCAnalysis &HAKCInfo::GetCommonHAKCAnalysis() {
        return Analysis;
    }

    StringRef HAKCInfo::GetName() const {
        return Name;
    }

    raw_ostream &HAKCInfo::operator>>(raw_ostream &os) const {
        os << GetYaml(HAKCInfo::IndentSpaces());
        return os;
    }

    raw_ostream &operator<<(raw_ostream &os, const HAKCInfo &HAKCInfo) {
        HAKCInfo >> os;
        return os;
    }

    unsigned int HAKCInfo::IndentSpaces() {
        return 4;
    }

    unsigned int HAKCInfo::EntrySpaces() {
        return 2;
    }

    std::string HAKCInfo::GetYamlHeader(unsigned int Indents) const {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream << GetYamlIdentifier() << "\n";
        sstream.indent(Indents + EntrySpaces()) << "Name: \"" << GetName() << "\"";

        return Yaml;
    }
} // hakc
