//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the base class for information associated with a symbol.
/// Note that both functions and global variables are subclasses of a symbol.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 6/11/24.
//

#ifndef HAKC_HAKCSYMBOLINFO_H
#define HAKC_HAKCSYMBOLINFO_H

#include "HAKCTypeInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/yaml/HAKCYamlSymbol.h"

namespace llvm::hakc {
class HAKCSymbolInfo : public HAKCInfo {
public:
  HAKCSymbolInfo(CommonHAKCAnalysis &Analysis, StringRef Name,
                 bool DebugActive);

  void SetType(std::shared_ptr<HAKCTypeInfo> HAKCType);

  std::shared_ptr<HAKCTypeInfo> GetType();

  void AddSymbolUse(const std::shared_ptr<HAKCSymbolInfo> &Symbol);

  std::string GetYaml(unsigned Indents) const override;

  std::string GetYamlHeader(unsigned Indents) const override;

  GlobalObject *GetGlobalObj() const;

  void SetDefiningLocation(const DIFile *File, unsigned Line);

  void SetLocalScope(const DIScope *Scope);

  void GetLocalScopePath(SmallVectorImpl<char> &Result) const;

  friend bool operator==(const HAKCYamlSymbol &YamlSymbol,
                         const HAKCSymbolInfo &SymbolInfo) {
    return SymbolInfo.Matches(YamlSymbol);
  }

  friend bool operator==(const HAKCSymbolInfo &SymbolInfo,
                         const HAKCYamlSymbol &YamlSymbol) {
    return YamlSymbol == SymbolInfo;
  }

  friend bool operator!=(const HAKCYamlSymbol &YamlSymbol,
                         const HAKCSymbolInfo &SymbolInfo) {
    return !(YamlSymbol == SymbolInfo);
  }

  friend bool operator!=(const HAKCSymbolInfo &SymbolInfo,
                         const HAKCYamlSymbol &YamlSymbol) {
    return YamlSymbol != SymbolInfo;
  }

protected:
  std::shared_ptr<HAKCTypeInfo> Type;
  std::set<std::shared_ptr<HAKCSymbolInfo>> UsedSymbols;
  GlobalObject *GlobalObj;
  const DIType *DbgType;
  const DIFile *DefiningLocation;
  unsigned DefiningLine;
  const DIScope *LocalScope;

  void SetGlobalObj(GlobalObject *GlobalObj);

  static void GetTransformedPathName(const DIFile *File,
                                     SmallVectorImpl<char> &Result);

  bool Matches(const HAKCYamlSymbol &YamlSymbol) const;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCSYMBOLINFO_H
