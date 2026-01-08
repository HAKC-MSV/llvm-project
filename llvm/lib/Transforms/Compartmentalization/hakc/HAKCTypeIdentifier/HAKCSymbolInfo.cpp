//
// Created by de29664 on 6/11/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCSymbolInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/Support/Path.h"
#include <utility>

hakc::HAKCSymbolInfo::HAKCSymbolInfo(CommonHAKCAnalysis &Analysis,
                                     StringRef Name, bool DebugActive)
    : HAKCInfo(Analysis, Name, DebugActive), Type(nullptr), UsedSymbols(),
      GlobalObj(nullptr), DbgType(nullptr), DefiningLocation(nullptr),
      DefiningLine(0), LocalScope(nullptr) {}

void hakc::HAKCSymbolInfo::SetType(std::shared_ptr<HAKCTypeInfo> HAKCType) {
  Type = std::move(HAKCType);
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCSymbolInfo::GetType() {
  return Type;
}

void hakc::HAKCSymbolInfo::AddSymbolUse(
    const std::shared_ptr<HAKCSymbolInfo> &Symbol) {
  UsedSymbols.insert(Symbol);
}

void hakc::HAKCSymbolInfo::GetTransformedPathName(
    const DIFile *File, SmallVectorImpl<char> &Result) {
  sys::path::append(Result, File->getDirectory(), File->getFilename());
}

void hakc::HAKCSymbolInfo::GetLocalScopePath(
    SmallVectorImpl<char> &Result) const {
  if (LocalScope) {
    GetTransformedPathName(LocalScope->getFile(), Result);
  }
}

std::string hakc::HAKCSymbolInfo::GetYamlHeader(unsigned int Indents) const {
  if (!this->Type) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Symbol " << GetName() << " has no HAKCType!\n";
    throw std::exception();
  }

  std::string Yaml = HAKCInfo::GetYamlHeader(Indents);
  llvm::raw_string_ostream sstream(Yaml);

  sstream << "\n";
  // Note: IsDefinition can be inferred by whether there is a
  // HAKCCompilationUnit which points to the source file and line
  // sstream.indent(Indents + EntrySpaces()) << "IsDefinition: ";
  // bool IsDefinition;
  //
  // if (auto *Global = dyn_cast<GlobalVariable>(GlobalObj)) {
  //   IsDefinition = Global->hasInitializer();
  // } else if (auto *F = dyn_cast<Function>(GlobalObj)) {
  //   IsDefinition = !F->isDeclaration();
  // } else {
  //   CommonHAKCAnalysis::getLogger(Error) << "Unexpected GlobalObj\n";
  //   throw std::exception();
  // }
  // if (IsDefinition) {
  //   sstream << "true";
  // } else {
  //   sstream << "false";
  // }
  // sstream << "\n";

  sstream.indent(Indents + EntrySpaces()) << "Scope:\n";
  sstream.indent(Indents + EntrySpaces() + HAKCInfo::IndentSpaces())
      << "!HAKCScope\n";
  sstream.indent(Indents + EntrySpaces() + HAKCInfo::IndentSpaces())
      << "Scope: ";
  if (LocalScope) {
    sstream << "\"local\"\n";
    sstream.indent(Indents + EntrySpaces() + HAKCInfo::IndentSpaces())
        << "LocalScopeName: ";
    SmallString<256> PathName;
    GetLocalScopePath(PathName);
    sstream << "\"" << PathName << "\"";
  } else {
    sstream << "\"global\"";
  }
  sstream << "\n";

  sstream.indent(Indents + EntrySpaces()) << "Type:\n";
  sstream.indent(Indents + EntrySpaces() + HAKCInfo::IndentSpaces())
      << this->Type->GetYamlHeader(Indents + HAKCInfo::IndentSpaces());
  return Yaml;
}

std::string hakc::HAKCSymbolInfo::GetYaml(unsigned Indents) const {
  auto Yaml = GetYamlHeader(Indents);
  llvm::raw_string_ostream sstream(Yaml);

  if (!UsedSymbols.empty()) {
    sstream << "\n";
    sstream.indent(Indents + EntrySpaces()) << "UsedSymbols:\n";
    unsigned Count = 0;
    for (auto &Symbol : UsedSymbols) {
      sstream.indent(Indents + HAKCInfo::IndentSpaces())
          << "- " << Symbol->GetYamlHeader(Indents + HAKCInfo::IndentSpaces());
      if (++Count != UsedSymbols.size()) {
        sstream << "\n";
      }
    }
  }

  return Yaml;
}

void hakc::HAKCSymbolInfo::SetGlobalObj(GlobalObject *Global) {
  if (!Global) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to set null GlobalVariable\n";
    throw std::exception();
  }
  this->GlobalObj = Global;
}

GlobalObject *hakc::HAKCSymbolInfo::GetGlobalObj() const { return GlobalObj; }

void hakc::HAKCSymbolInfo::SetDefiningLocation(const DIFile *File,
                                               unsigned int Line) {
  DefiningLine = Line;
  DefiningLocation = File;
}

void hakc::HAKCSymbolInfo::SetLocalScope(const DIScope *Scope) {
  LocalScope = Scope;

  if (!LocalScope) {
    return;
  }

  if (!(isa<DISubprogram>(LocalScope) || isa<DIFile>(LocalScope) ||
        isa<DICompileUnit>(LocalScope))) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Unexpected LocalScope: " << *LocalScope << "\n";
    throw std::exception();
  }
}

bool hakc::HAKCSymbolInfo::Matches(
    const hakc::HAKCYamlSymbol &YamlSymbol) const {
  hakc_scope_t SymbolInfoScope =
      (LocalScope ? hakc_local_scope : hakc_global_scope);
  bool ScopesMatch = SymbolInfoScope == YamlSymbol.Scope.Scope;
  if (ScopesMatch && SymbolInfoScope == hakc_local_scope) {
    SmallString<256> PathName;
    GetLocalScopePath(PathName);
    ScopesMatch = (YamlSymbol.Scope.LocalScope == PathName);
  }
  return ScopesMatch && YamlSymbol.Name == Name && YamlSymbol.Type == *Type;
}
