//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCTYPEINFO_H
#define HAKC_HAKCTYPEINFO_H

#include <map>
#include <set>

#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/yaml/HAKCYamlType.h"
#include "llvm/IR/Type.h"


namespace llvm::hakc {
    class HAKCTypeInfo;
    typedef std::shared_ptr<HAKCTypeInfo> HAKCTypeP;

    class HAKCTypeInfo : public HAKCInfo {
    public:
        HAKCTypeInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

        void AddMember(const std::shared_ptr<HAKCTypeInfo> &TypeUse, unsigned BitOffset);

        std::string GetYaml(unsigned Indents) const override;

        void SetSizeInBits(unsigned Size);

        unsigned GetSizeInBits() const;

        const DIType *GetDbgType() const;

        void SetDbgType(const DIType *DiDbgType);

        void SetDbgTypeName(const std::string &DbgTypeNameStr);

        Type *GetLLVMType() const;

        void SetLLVMType(Type *Ty);

        std::string GetYamlHeader(unsigned Indents) const override;

        StringRef GetYamlIdentifier() const override;

        static StringRef UnknownType;

        bool IsPointerToPointer();

        HAKCTypeP GetPointeeType();

        void SetPointeeType(const HAKCTypeP &PointeeType);

        bool IsIntegerType() const;

        bool IsPointerType() const;

        Value *GetObjectSizeInBytes();

        void SetObjectSizeInBytes(const HAKCTypeP &PointeeType);

    protected:
        std::map<unsigned, std::set<std::shared_ptr<HAKCTypeInfo> > > Members;
        unsigned SizeInBits;
        Value* ObjectSizeInBits;
        const DIType *DbgType;
        Type *LLVMType;
        std::string DbgTypeName;
        HAKCTypeP PointeeType;
        bool IsPointerToPointer(const DIType *DiType);

        const DIType *StripTypeModifiers(const DIType *DiType);

    public:
        friend bool operator==(const HAKCTypeInfo &lhs, const HAKCTypeInfo &rhs) {
            if (lhs.DbgType && rhs.DbgType) {
                return lhs.DbgType == rhs.DbgType;
            } else if (lhs.LLVMType && rhs.LLVMType) {
                return lhs.LLVMType == rhs.LLVMType;
            }

            return lhs.GetName() == rhs.GetName();
        }

        friend bool operator!=(const HAKCTypeInfo &lhs, const HAKCTypeInfo &rhs) {
            return !(lhs == rhs);
        }

        friend bool operator==(const std::shared_ptr<HAKCTypeInfo> &lhs, const std::shared_ptr<HAKCTypeInfo> &rhs) {
            return *lhs == *rhs;
        }

        friend bool operator!=(const std::shared_ptr<HAKCTypeInfo> &lhs, const std::shared_ptr<HAKCTypeInfo> &rhs) {
            return !(*lhs == *rhs);
        }

        friend bool operator==(StringRef TypeName, const std::shared_ptr<HAKCTypeInfo> &TypeInfo) {
            std::string LLVMTypeStr;
            llvm::raw_string_ostream ostr(LLVMTypeStr);
            if (TypeInfo->GetLLVMType()) {
                ostr << *TypeInfo->GetLLVMType();
            }
            return TypeName == TypeInfo->DbgTypeName || TypeName == LLVMTypeStr;
        }

        friend bool operator==(const HAKCYamlType &YamlType, const std::shared_ptr<HAKCTypeInfo> &TypeInfo) {
            return YamlType.DebugType == TypeInfo || YamlType.LLVMType == TypeInfo;
        }

        friend bool operator==(const std::shared_ptr<HAKCTypeInfo> &TypeInfo, const HAKCYamlType &YamlType) {
            return (YamlType == TypeInfo);
        }

        friend bool operator!=(const std::shared_ptr<HAKCTypeInfo> &TypeInfo, const HAKCYamlType &YamlType) {
            return !(YamlType == TypeInfo);
        }

        friend bool operator!=(const HAKCYamlType &YamlType, const std::shared_ptr<HAKCTypeInfo> &TypeInfo) {
            return !(YamlType == TypeInfo);
        }
    };
    typedef std::shared_ptr<HAKCTypeInfo> HAKCTypeP;

} // hakc

#endif //HAKC_HAKCTYPEINFO_H
