//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCTYPEINFO_H
#define HAKC_HAKCTYPEINFO_H

#include <map>
#include <set>

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/yaml/HAKCYamlType.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCInfo.h"

namespace llvm::hakc {
class HAKCTypeInfo;
typedef std::shared_ptr<HAKCTypeInfo> HAKCTypeP;

class HAKCTypeInfo : public HAKCInfo {
public:
  HAKCTypeInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

  void AddMember(const std::shared_ptr<HAKCTypeInfo> &TypeUse,
                 unsigned BitOffset);

  std::string GetYaml(unsigned Indents) const override;

  ConstantInt *GetSizeInBytes() const;

  unsigned GetSizeInBits() const;

  const DIType *GetDbgType() const;

  void SetDbgType(const DIType *DiDbgType);

  void SetDbgTypeName(const std::string &DbgTypeNameStr);

  StringRef GetDbgTypeName() const;

  Type *GetLLVMType() const;

  void SetLLVMType(Type *Ty);

  std::string GetYamlHeader(unsigned Indents) const override;

  std::string GetYamlHeader(unsigned int Indents, unsigned RWX) const;

  StringRef GetYamlIdentifier() const override;

  static StringRef UnknownType;

  bool IsPointerToPointer() const;

  HAKCTypeP GetPointeeType() const;

  void SetPointeeType(const HAKCTypeP &PointeeType);

  bool IsIntegerType() const;

  bool IsPointerType() const;

  bool IsFunctionType() const;

  bool IsIgnoredType() const;

  bool IsVoidPtrType() const;

  bool IsStructType() const;

  void SetIsIgnoredType(bool IsIgnored);

  static const DIType *StripTypeModifiers(const DIType *DiType);

protected:
  std::map<unsigned, std::set<std::shared_ptr<HAKCTypeInfo>>> Members;
  unsigned SizeInBits;
  const DIType *DbgType;
  Type *LLVMType;
  std::string DbgTypeName;
  HAKCTypeP PointeeType;
  bool IsIgnored;

  static bool IsPointerToPointer(const DIType *DiType);

public:
  friend bool operator==(const HAKCTypeInfo &Lhs, const HAKCTypeInfo &Rhs) {
    if (Lhs.DbgType && Rhs.DbgType) {
      return Lhs.DbgType == Rhs.DbgType;
    }

    return Lhs.GetDbgTypeName() == Rhs.GetDbgTypeName();
  }

  friend bool operator!=(const HAKCTypeInfo &Lhs, const HAKCTypeInfo &Rhs) {
    return !(Lhs == Rhs);
  }

  friend bool operator==(StringRef TypeName, const HAKCTypeInfo &TypeInfo) {
    SmallVector<StringRef> TypeNameTokens;
    SmallVector<StringRef> DbgTypeNameTokens;
    TypeName.split(TypeNameTokens, " ");
    TypeInfo.GetDbgTypeName().split(DbgTypeNameTokens, " ");
    std::string TypeNameStr;
    std::string DbgTypeNameStr;

    /* Remove all spaces and compare the results */
    for (auto Tok : TypeNameTokens) {
      TypeNameStr += Tok.trim();
    }
    for (auto Tok : DbgTypeNameTokens) {
      DbgTypeNameStr += Tok.trim();
    }

    return TypeNameStr == DbgTypeNameStr;
  }

  friend bool operator!=(StringRef TypeName, const HAKCTypeInfo &TypeInfo) {
    return !(TypeName == TypeInfo);
  }

  friend bool operator==(const HAKCYamlType &YamlType,
                         const HAKCTypeInfo &TypeInfo) {
    return YamlType.DebugType == TypeInfo;
  }

  friend bool operator!=(const HAKCYamlType &YamlType,
                         const HAKCTypeInfo &TypeInfo) {
    return YamlType.DebugType != TypeInfo;
  }

  friend bool operator==(const HAKCTypeInfo &TypeInfo,
                         const HAKCYamlType &YamlType) {
    return (YamlType == TypeInfo);
  }

  friend bool operator!=(const HAKCTypeInfo &TypeInfo,
                         const HAKCYamlType &YamlType) {
    return YamlType != TypeInfo;
  }
};
typedef std::shared_ptr<HAKCTypeInfo> HAKCTypeP;

} // namespace llvm::hakc

#endif // HAKC_HAKCTYPEINFO_H
