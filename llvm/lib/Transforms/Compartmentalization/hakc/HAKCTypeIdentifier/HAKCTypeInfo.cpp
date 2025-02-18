//
// Created by de29664 on 5/2/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace llvm::hakc {
    HAKCTypeInfo::HAKCTypeInfo(CommonHAKCAnalysis &Analysis, StringRef Name,
                               bool DebugActive) : HAKCInfo(Analysis, Name, DebugActive), Members(),
                                                   SizeInBits(0), DbgType(nullptr), LLVMType(nullptr),
                                                   DbgTypeName() {
    }

    void HAKCTypeInfo::SetSizeInBits(unsigned int Size) {
        SizeInBits = Size;
    }

    unsigned HAKCTypeInfo::GetSizeInBits() const {
        return SizeInBits;
    }

    const DIType *HAKCTypeInfo::GetDbgType() {
        return DbgType;
    }

    void HAKCTypeInfo::SetDbgType(const DIType *DiDbgType) {
        this->DbgType = DiDbgType;
    }

    void HAKCTypeInfo::AddMember(const std::shared_ptr<HAKCTypeInfo> &TypeUse, unsigned int BitOffset) {
        Members[BitOffset].insert(TypeUse);
    }

    void HAKCTypeInfo::SetDbgTypeName(std::string DbgTypeNameStr) {
        this->DbgTypeName = DbgTypeNameStr;
    }

    Type *HAKCTypeInfo::GetLLVMType() {
        return LLVMType;
    }

    bool HAKCTypeInfo::IsIntegerType() const {
        if (DbgType) {
            if (auto *DiBasicTy = dyn_cast<DIBasicType>(DbgType)) {
                ArrayRef<unsigned> IntegerEncodings = {
                    dwarf::DW_ATE_address,
                    dwarf::DW_ATE_signed,
                    dwarf::DW_ATE_unsigned
                };
                auto Encoding = DiBasicTy->getEncoding();
                auto Search = [Encoding](unsigned E) {
                    return Encoding == E;
                };
                return llvm::any_of(IntegerEncodings, Search);
            }
        } else if (LLVMType) {
            return LLVMType->isPointerTy();
        }
        return false;
    }

    bool HAKCTypeInfo::IsPointerType() {
        if (DbgType) {
            return DbgType->getTag() == dwarf::DW_TAG_pointer_type;
        } else if (LLVMType) {
            return LLVMType->isPointerTy();
        }
        return false;
    }

    std::shared_ptr<HAKCTypeInfo> HAKCTypeInfo::GetPointeeType() {
        /* TODO: Implement me */
        // Look at this Derrick
        // create HAKCTypeInfo (check for cache?)
        // Q: how do I get the pointee?
        if (DbgType) {
          return std::make_shared<HAKCTypeInfo>(Analysis, DbgType->getName(), DebugActive);
        } else if (LLVMType) {
          // https://llvm.org/docs/OpaquePointers.html
          // maybe get the first operand, then get its type -> but then how to create type info
          return nullptr;
        }
        return nullptr;
    }

    const DIType *HAKCTypeInfo::StripTypeModifiers(const DIType *DiType) {
        auto *Result = DiType;

        if (auto *DiDerivedType = dyn_cast<DIDerivedType>(DiType)) {
            auto TagToFind = DiDerivedType->getTag();

            auto Search = [TagToFind](dwarf::Tag Tag) {
                return Tag == TagToFind;
            };

            SmallVector<dwarf::Tag> TagsToRemove = {
                dwarf::DW_TAG_volatile_type,
                dwarf::DW_TAG_const_type,
                dwarf::DW_TAG_restrict_type
            };
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

    bool HAKCTypeInfo::IsPointerToPointer() {
        if (DbgType) {
            return IsPointerToPointer(DbgType);
        }

        return false;
    }

    void HAKCTypeInfo::SetLLVMType(Type *Ty) {
        if (!Ty) {
            CommonHAKCAnalysis::getWriter(true) << "Trying to set null LLVM Type for " << GetName() << "\n";
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

            CommonHAKCAnalysis::getWriter(true) << "Trying to change LLVM Type for " << GetName() << " from "
                    << *LLVMType << " to " << *Ty << "\n";
            throw std::exception();
        }
        LLVMType = Ty;
    }

    StringRef HAKCTypeInfo::GetYamlIdentifier() const {
        return "!HAKCType";
    }

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

    std::string hakc::HAKCTypeInfo::GetYaml(unsigned Indents) const {
        std::string Yaml;
        llvm::raw_string_ostream sstream(Yaml);

        sstream << GetYamlHeader(Indents);
        if (!Members.empty()) {
            std::vector<unsigned> SortedBitOffsets;
            SortedBitOffsets.reserve(Members.size());
            for (auto &it: Members) {
                SortedBitOffsets.push_back(it.first);
            }

            llvm::sort(SortedBitOffsets.begin(), SortedBitOffsets.end());
            sstream.indent(Indents + EntrySpaces()) << "Members:\n";
            for (auto BitOffset: SortedBitOffsets) {
                auto MemberSet = Members.find(BitOffset)->second;
                sstream.indent(Indents + HAKCInfo::IndentSpaces()) << "- Offset: " << BitOffset << "\n";
                sstream.indent(Indents + HAKCInfo::IndentSpaces() + EntrySpaces()) << "Type:\n";
                for (auto &Member: MemberSet) {
                    sstream << Member->GetYamlHeader(Indents + 2 * HAKCInfo::IndentSpaces()) << "\n";
                }
            }
        }

        return Yaml;
    }
} // namespace llvm::hakc
