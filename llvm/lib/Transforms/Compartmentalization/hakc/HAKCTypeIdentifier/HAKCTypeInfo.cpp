//
// Created by de29664 on 5/2/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace llvm::hakc {
HAKCTypeInfo::HAKCTypeInfo(CommonHAKCAnalysis &Analysis, const StringRef Name,
                           bool DebugActive)
    : HAKCInfo(Analysis, Name, DebugActive) {}

unsigned HAKCTypeInfo::GetSizeInBits() const {
  unsigned SizeInBits = 0;
  if (DbgType) {
    SizeInBits = DbgType->getSizeInBits();
  }

  return SizeInBits > 0 ? SizeInBits : BITS_PER_BYTE;
}

bool HAKCTypeInfo::IsIgnoredType() const {
  bool Result = IsIgnored;
  if (!Result && IsPointerType() && PointeeType && !IsVoidPtrType()) {
    Result = PointeeType->IsIgnoredType();
  }
  return Result;
}
void HAKCTypeInfo::SetIsIgnoredType(bool IsIgnored) {
  this->IsIgnored = IsIgnored;
}

bool HAKCTypeInfo::IsVoidPtrType() const {
  auto *StrippedDbgTy = StripTypeModifiers(DbgType);
  if (isa_and_nonnull<DIDerivedType>(StrippedDbgTy)) {
    return dyn_cast<DIDerivedType>(StrippedDbgTy)->getBaseType() == nullptr;
  }
  return false;
}

ConstantInt *HAKCTypeInfo::GetSizeInBytes() const {
  unsigned SizeInBits = GetSizeInBits();
  LLVMContext &Ctx = Analysis.GetModule().getContext();
  return ConstantInt::get(IntegerType::get(Ctx, 64), SizeInBits / BITS_PER_BYTE,
                          false);
}

const DIType *HAKCTypeInfo::GetDbgType() const { return DbgType; }

void HAKCTypeInfo::SetDbgType(const DIType *DiDbgType) { DbgType = DiDbgType; }

void HAKCTypeInfo::AddMember(const std::shared_ptr<HAKCTypeInfo> &TypeUse,
                             unsigned int BitOffset) {
  Members[BitOffset].insert(TypeUse);
}

void HAKCTypeInfo::SetDbgTypeName(const std::string &DbgTypeNameStr) {
  this->DbgTypeName = DbgTypeNameStr;
}

Type *HAKCTypeInfo::GetLLVMType() const { return LLVMType; }

void HAKCTypeInfo::SetPointeeType(const HAKCTypeP &PointeeType) {
  this->PointeeType = PointeeType;
}

bool HAKCTypeInfo::IsIntegerType() const {
  if (DbgType) {
    auto *StrippedTy = StripTypeModifiers(DbgType);
    if (auto *DiBasicTy = dyn_cast<DIBasicType>(StrippedTy)) {
      auto IntegerEncodings = {dwarf::DW_ATE_address, dwarf::DW_ATE_signed,
                               dwarf::DW_ATE_unsigned,
                               dwarf::DW_ATE_unsigned_char};
      for (const auto Encoding : IntegerEncodings) {
        if (Encoding == DiBasicTy->getEncoding()) {
          return true;
        }
      }
    }
  } else if (LLVMType) {
    return LLVMType->isIntegerTy();
  }
  return false;
}

bool HAKCTypeInfo::IsPointerType() const {
  if (DbgType) {
    return IsTag(dwarf::DW_TAG_pointer_type) || IsTag(dwarf::DW_TAG_array_type);
  }
  if (LLVMType) {
    return LLVMType->isPointerTy() || LLVMType->isArrayTy();
  }
  return false;
}

bool HAKCTypeInfo::IsFunctionType() const {
  if (DbgType) {
    return isa<DISubroutineType>(StripTypeModifiers(DbgType));
  }

  if (LLVMType) {
    return isa<FunctionType>(LLVMType);
  }
  return false;
}

bool HAKCTypeInfo::IsStructType() const {
  if (DbgType) {
    return IsTag(dwarf::DW_TAG_array_type);
  }

  if (LLVMType) {
    return isa<StructType>(LLVMType);
  }

  return false;
}

bool HAKCTypeInfo::IsEnumType() const {
  return IsTag(dwarf::DW_TAG_enumeration_type);
}

bool HAKCTypeInfo::IsTag(dwarf::Tag Tag) const {
  if (DbgType) {
    auto *StrippedDbgTy = StripTypeModifiers(DbgType);
    return StrippedDbgTy->getTag() == Tag;
  }
  return false;
}

bool HAKCTypeInfo::IsUnionType() const {
  return IsTag(dwarf::DW_TAG_union_type);
}

std::shared_ptr<HAKCTypeInfo> HAKCTypeInfo::GetPointeeType() const {
  return PointeeType;
}

const DIType *HAKCTypeInfo::StripTypeModifiers(const DIType *DiType) {
  auto *Result = DiType;
  if (!Result) {
    return nullptr;
  }

  if (auto *DiDerivedType = dyn_cast<DIDerivedType>(DiType)) {
    auto TagToFind = DiDerivedType->getTag();

    auto Search = [TagToFind](const dwarf::Tag Tag) {
      return Tag == TagToFind;
    };

    SmallVector<dwarf::Tag> TagsToRemove = {
        dwarf::DW_TAG_volatile_type, dwarf::DW_TAG_const_type,
        dwarf::DW_TAG_restrict_type, dwarf::DW_TAG_typedef};
    if (llvm::any_of(TagsToRemove, Search)) {
      if (DiDerivedType->getBaseType()) {
        Result = StripTypeModifiers(DiDerivedType->getBaseType());
      }
    }
  }

  return Result;
}

bool HAKCTypeInfo::IsPointerToPointer(const DIType *DiType) {
  DiType = StripTypeModifiers(DiType);
  if (auto *DerivedTy = dyn_cast<DIDerivedType>(DiType)) {
    if (DerivedTy->getTag() == dwarf::DW_TAG_pointer_type) {
      if (DerivedTy->getBaseType()) {
        auto *BaseTy = StripTypeModifiers(DerivedTy->getBaseType());
        return BaseTy->getTag() == dwarf::DW_TAG_pointer_type;
      }
    }
  }

  return false;
}

bool HAKCTypeInfo::IsPointerToPointer() const {
  if (DbgType) {
    return IsPointerToPointer(DbgType);
  }

  return false;
}

void HAKCTypeInfo::SetLLVMType(Type *Ty) {
  if (!Ty) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to set null LLVM Type for " << GetName() << "\n";
    throw std::exception();
  }
  if (LLVMType && Ty != LLVMType) {
    if (auto *NewStructTy = dyn_cast<StructType>(Ty)) {
      if (auto *OrigStructTy = dyn_cast<StructType>(LLVMType)) {
        StructType *NamedStructType = nullptr;
        if (OrigStructTy->hasName()) {
          NamedStructType = OrigStructTy;
        } else if (NewStructTy->hasName()) {
          NamedStructType = NewStructTy;
        }

        if (NamedStructType) {
          LLVMType = NamedStructType;
          return;
        }
      }
    }

    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to change LLVM Type for " << GetName() << " from "
        << *LLVMType << " to " << *Ty << "\n";
    throw std::exception();
  }
  LLVMType = Ty;
}

StringRef HAKCTypeInfo::GetYamlIdentifier() const { return "!HAKCType"; }

StringRef HAKCTypeInfo::UnknownType = "@UNKNOWN@";

std::string HAKCTypeInfo::GetYamlHeader(unsigned int Indents) const {
  std::string Yaml = HAKCInfo::GetYamlHeader(Indents);
  llvm::raw_string_ostream sstream(Yaml);

  sstream << "\n";
  sstream.indent(Indents + EntrySpaces()) << "DebugType: \"";
  if (!DbgTypeName.empty()) {
    sstream << DbgTypeName;
  } else {
    sstream << UnknownType;
  }
  sstream << "\"\n";
  sstream.indent(Indents + EntrySpaces()) << "LLVMType: \"";
  if (LLVMType) {
    if (auto *StructTy = dyn_cast<StructType>(LLVMType)) {
      if (StructTy->hasName()) {
        sstream << StructTy->getName();
      } else {
        sstream << *LLVMType;
      }
    } else {
      sstream << *LLVMType;
    }
  } else {
    sstream << UnknownType;
  }
  sstream << "\"";

  return Yaml;
}
std::string HAKCTypeInfo::GetYamlHeader(unsigned int Indents, unsigned RWX) const {
  // function to generate HAKCTypePerm yaml
  std::string Yaml;
  llvm::raw_string_ostream sstream(Yaml);

  sstream << "!HAKCTypePerm\n";
  sstream.indent(Indents + EntrySpaces()) << "RWX: " << RWX << "\n";
  sstream.indent(Indents + EntrySpaces()) << "Type:\n";

  sstream.indent(Indents + EntrySpaces() + 4) << HAKCInfo::GetYamlHeader(Indents + 4);

  sstream << "\n";
  sstream.indent(Indents + EntrySpaces() + 4) << "DebugType: \"";
  if (!DbgTypeName.empty()) {
    sstream << DbgTypeName;
  } else {
    sstream << UnknownType;
  }
  sstream << "\"\n";
  sstream.indent(Indents + EntrySpaces() + 4) << "LLVMType: \"";
  if (LLVMType) {
    if (auto *StructTy = dyn_cast<StructType>(LLVMType)) {
      if (StructTy->hasName()) {
        sstream << StructTy->getName();
      } else {
        sstream << *LLVMType;
      }
    } else {
      sstream << *LLVMType;
    }
  } else {
    sstream << UnknownType;
  }
  sstream << "\"";

  return Yaml;
}

std::string hakc::HAKCTypeInfo::GetYaml(unsigned Indents) const {
  std::string Yaml;
  llvm::raw_string_ostream sstream(Yaml);

  sstream << GetYamlHeader(Indents);
  if (!Members.empty()) {
    std::vector<unsigned> SortedBitOffsets;
    SortedBitOffsets.reserve(Members.size());
    for (auto &it : Members) {
      SortedBitOffsets.push_back(it.first);
    }

    llvm::sort(SortedBitOffsets.begin(), SortedBitOffsets.end());
    sstream.indent(Indents + EntrySpaces()) << "Members:\n";
    for (auto BitOffset : SortedBitOffsets) {
      auto MemberSet = Members.find(BitOffset)->second;
      sstream.indent(Indents + HAKCInfo::IndentSpaces())
          << "- Offset: " << BitOffset << "\n";
      sstream.indent(Indents + HAKCInfo::IndentSpaces() + EntrySpaces())
          << "Type:\n";
      for (auto &Member : MemberSet) {
        sstream << Member->GetYamlHeader(Indents + 2 * HAKCInfo::IndentSpaces())
                << "\n";
      }
    }
  }

  return Yaml;
}

StringRef HAKCTypeInfo::GetDbgTypeName() const { return DbgTypeName; }
} // namespace llvm::hakc
