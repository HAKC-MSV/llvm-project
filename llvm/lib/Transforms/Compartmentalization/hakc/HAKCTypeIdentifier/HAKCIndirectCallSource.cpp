//
// Created by de29664 on 6/18/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCIndirectCallSource.h"

namespace llvm::hakc {
    HAKCIndirectCallSource::HAKCIndirectCallSource(std::vector<std::shared_ptr<HAKCIndirectCallSourceLink> > SourcePath,
                                                   const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                   bool debug) : HAKCInfo(HAKCType->GetCommonHAKCAnalysis(),
                                                                          HAKCType->GetName(), debug),
                                                                 HAKCType(HAKCType), SourcePath(SourcePath) {
    }

    std::string HAKCIndirectCallSource::GetYaml(unsigned Indents) const {
        std::string Yaml = HAKCInfo::GetYamlHeader(Indents);
        raw_string_ostream sstream(Yaml);

        sstream << "\n";
        sstream.indent(Indents + EntrySpaces()) << "Type:\n";
        sstream.indent(Indents + EntrySpaces() + HAKCInfo::IndentSpaces())
                << HAKCType->GetYamlHeader(Indents + HAKCInfo::IndentSpaces());
        if (!SourcePath.empty()) {
            sstream << "\n";
            sstream.indent(Indents + EntrySpaces()) << "Source:\n";
            unsigned Count = 0;
            for (auto &link: SourcePath) {
                sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- "
                        << link->GetYaml(Indents + HAKCInfo::IndentSpaces());
                if (++Count != SourcePath.size()) {
                    sstream << "\n";
                }
            }
        }

        return Yaml;
    }

    StringRef HAKCIndirectCallSource::GetYamlIdentifier() const {
        return "!HAKCIndirectSource";
    }

    StringRef HAKCIndirectCallSourceLink::GetYamlIdentifier() const {
        return "!HAKCIndirectSourceLink";
    }

    void HAKCIndirectCallSourceLink::SplitString(StringRef S, unsigned Indents) {
        SmallVector<StringRef> SplitTokens;
        S.split(SplitTokens, "\n");
        std::string Yaml;
        raw_string_ostream sstream(Yaml);
        for (auto Tok: SplitTokens) {
            Yaml = "";
            sstream.indent(Indents) << Tok;
            LinkYamlTokens.push_back(Yaml);
        }
    }

    void HAKCIndirectCallSourceLink::SplitTypeYaml(const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
        auto TypeTokensStr = HAKCType->GetYamlHeader(0);
        SplitString(TypeTokensStr, HAKCInfo::IndentSpaces());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(Argument *Arg, const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                           bool Debug) : HAKCInfo(HAKCType->GetCommonHAKCAnalysis(),
                                                                             "Argument Indirect Call Link", Debug),
                                                                         LinkYamlTokens() {
        InputYamlHeader();
        InputLinkType("Argument");
        InputArgument(Arg);
        InputType(HAKCType);
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           bool Debug)
        : HAKCInfo(GlobalInfo->GetCommonHAKCAnalysis(), "Global Indirect Call Link", Debug), LinkYamlTokens() {
        InputHAKCSymbol(GlobalInfo);
    }


    void HAKCIndirectCallSourceLink::InputHAKCSymbol(const std::shared_ptr<HAKCSymbolInfo> &HAKCSymbol) {
        InputYamlHeader();
        InputLinkType("Global");
        InputGlobalObject(HAKCSymbol->GetGlobalObj());
        InputType(HAKCSymbol->GetType());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCSymbolInfo> &HAKCSymbol,
                                                           bool Debug) : HAKCInfo(HAKCSymbol->GetCommonHAKCAnalysis(),
                                                                             "Global Indirect Call Link", Debug),
                                                                         LinkYamlTokens() {
        InputHAKCSymbol(HAKCSymbol);
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                                                           int OffsetInBits,
                                                           bool Debug) : HAKCInfo(GlobalInfo->GetCommonHAKCAnalysis(),
                                                                             "Global Member Indirect Call Link",
                                                                             Debug), LinkYamlTokens() {
        InputYamlHeader();
        InputLinkType("GlobalMember");
        InputBitoffset(OffsetInBits);
        InputType(GlobalInfo->GetType());
    }

    HAKCIndirectCallSourceLink::HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                                                           int OffsetInBits,
                                                           bool Debug) : HAKCInfo(HAKCType->GetCommonHAKCAnalysis(),
                                                                             "Type member dereference", Debug),
                                                                         LinkYamlTokens() {
        InputYamlHeader();
        InputLinkType("PointerDereference");
        InputBitoffset(OffsetInBits);
        InputType(HAKCType);
    }

    void HAKCIndirectCallSourceLink::InputLinkType(StringRef LinkType) {
        std::string Yaml;
        raw_string_ostream sstream(Yaml);

        sstream.indent(EntrySpaces()) << "LinkType: " << "\"" << LinkType << "\"";
        LinkYamlTokens.push_back(Yaml);
    }

    void HAKCIndirectCallSourceLink::InputType(const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
        std::string Yaml;
        raw_string_ostream sstream(Yaml);

        sstream.indent(EntrySpaces()) << "Type:";
        LinkYamlTokens.push_back(Yaml);

        SplitTypeYaml(HAKCType);
    }

    void HAKCIndirectCallSourceLink::InputGlobalObject(GlobalObject *GlobalObj) {
        std::string Yaml;
        raw_string_ostream sstream(Yaml);

        sstream.indent(EntrySpaces()) << "GlobalName: " << "\"" << GlobalObj->getName() << "\"";
        LinkYamlTokens.push_back(Yaml);
    }

    void HAKCIndirectCallSourceLink::InputYamlHeader() {
        std::string Yaml = HAKCInfo::GetYamlHeader(0);
        SplitString(Yaml, 0);
    }

    void HAKCIndirectCallSourceLink::InputBitoffset(unsigned int BitOffset) {
        std::string Yaml;
        raw_string_ostream sstream(Yaml);

        sstream.indent(EntrySpaces()) << "Offset: " << BitOffset;
        LinkYamlTokens.push_back(Yaml);
    }

    void HAKCIndirectCallSourceLink::InputArgument(Argument *Arg) {
        std::string Yaml;
        raw_string_ostream sstream(Yaml);

        sstream.indent(EntrySpaces()) << "ArgNumber: " << Arg->getArgNo();
        LinkYamlTokens.push_back(Yaml);

        Yaml = "";
        sstream.indent(EntrySpaces()) << "Function: \"" << Arg->getParent()->getName() << "\"";
        LinkYamlTokens.push_back(Yaml);
    }

    std::string HAKCIndirectCallSourceLink::GetYaml(unsigned int Indents) const {
        std::string Yaml;
        raw_string_ostream sstream(Yaml);

        unsigned Count = 0;
        for (const auto &YamlLine: LinkYamlTokens) {
            if (Count == 0) {
                sstream << YamlLine;
            } else {
                sstream.indent(Indents + HAKCInfo::EntrySpaces()) << YamlLine;
            }
            if (++Count < LinkYamlTokens.size()) {
                sstream << "\n";
            }
        }

        return Yaml;
    }
} // hakc
