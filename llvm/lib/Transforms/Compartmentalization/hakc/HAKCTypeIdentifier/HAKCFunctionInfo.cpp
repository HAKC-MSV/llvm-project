//
// Created by de29664 on 5/2/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace llvm::hakc {
HAKCFunctionInfo::HAKCFunctionInfo(CommonHAKCAnalysis &Analysis, StringRef Name,
                                   bool DebugActive)
    : HAKCSymbolInfo(Analysis, Name, DebugActive), TypesUsed(), DirectCalls(),
      IndirectCalls() {}

void HAKCFunctionInfo::SetFunction(Function *F) { SetGlobalObj(F); }

Function *HAKCFunctionInfo::GetFunction() const {
  auto glob = GetGlobalObj();
  if (!glob) {
    CommonHAKCAnalysis::getWriter(true) << "GetGlobalObj() is NULL!\n";
    return nullptr;
  }
  return dyn_cast<Function>(glob);
}

void HAKCFunctionInfo::AddTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty) {
  if (!Ty) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to add type use of type null\n";
    throw std::exception();
  }
  if (!TypesUsed.contains(Ty)) {
    TypesUsed[Ty] = 0b0;
  }
  CommonHAKCAnalysis::getWriter(true) << "Adding type use " << *Ty << "\n";
}

unsigned HAKCFunctionInfo::GetTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty) {
  if (!Ty) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to get type use of type null\n";
    throw std::exception();
  }
  if (!TypesUsed.contains(Ty)) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to get type use of type that is not in TypesUsed\n";
    throw std::exception();
  }
  return TypesUsed[Ty];
}

void HAKCFunctionInfo::ModifyTypeUse(const std::shared_ptr<HAKCTypeInfo> &Ty, TypePerms perm){
  if (!Ty) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to modify type use of type null\n";
    throw std::exception();
  }
  // if not in TypesUsed
  if (!TypesUsed.contains(Ty)) {
    AddTypeUse(Ty);
  }
  // Note: Only to be used internally, and only to add uses, not remove them
  CommonHAKCAnalysis::getWriter(true) << "Modifying TypeUse with mask " << static_cast<unsigned>(perm) << " perm from " << TypesUsed[Ty] << " -> ";
  TypesUsed[Ty] |= perm;
  CommonHAKCAnalysis::getWriter(true) << TypesUsed[Ty] << " for type " << *Ty << "\n";
}

StringRef HAKCFunctionInfo::GetYamlIdentifier() const {
  return "!HAKCFunction";
}

void HAKCFunctionInfo::AddIndirectCall(
    const std::shared_ptr<HAKCIndirectCallSource> &Source) {
  if (!Source) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to add null indirect call source\n";
    throw std::exception();
  }
  IndirectCalls.insert(Source);
}

void HAKCFunctionInfo::AddDirectCall(
    const std::shared_ptr<HAKCFunctionInfo> &DirectCall) {
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
  // adding HAKCCompilation_unit to function
  if (DefiningLocation) {
    SmallString<256> PathName;
    GetTransformedPathName(DefiningLocation, PathName);
    sstream << "\n";
    sstream.indent(Indents + EntrySpaces()) << "CompilationUnit:\n";
    sstream.indent(Indents + EntrySpaces() + 4) << "!HAKCCompilationUnit\n";
    sstream.indent(Indents + EntrySpaces() + 4)
        << "DefiningFile: \"" << PathName << "\"\n";
    sstream.indent(Indents + EntrySpaces() + 4)
        << "DefiningLine: " << DefiningLine;
  }
  if (!DirectCalls.empty()) {
    sstream << "\n";
    sstream.indent(Indents + EntrySpaces()) << "DirectCalls:\n";
    Count = 0;
    for (auto &Symbol : DirectCalls) {
      sstream.indent(Indents + HAKCInfo::IndentSpaces())
          << "- " << Symbol->GetYamlHeader(Indents + HAKCInfo::IndentSpaces());
      if (++Count != DirectCalls.size()) {
        sstream << "\n";
      }
    }
  }
  if (!IndirectCalls.empty()) {
    Count = 0;
    sstream << "\n";
    sstream.indent(Indents + EntrySpaces()) << "IndirectCalls:\n";
    for (auto &IndirectSource : IndirectCalls) {
      sstream.indent(Indents + HAKCInfo::IndentSpaces())
          << "- "
          << IndirectSource->GetYaml(Indents + HAKCInfo::IndentSpaces());
      if (++Count != IndirectCalls.size()) {
        sstream << "\n";
      }
    }
  }
  if (!TypesUsed.empty()) {
    sstream << "\n";
    sstream.indent(Indents + EntrySpaces()) << "TypesUsed:\n";
    Count = 0;
    for (auto &it : TypesUsed) {
      sstream.indent(Indents + HAKCInfo::IndentSpaces())
          << "- " << it.first->GetYamlHeader(Indents + HAKCInfo::IndentSpaces(), it.second);
      if (++Count != TypesUsed.size()) {
        sstream << "\n";
      }
    }
  }
  return Yaml;
}
} // namespace llvm::hakc
