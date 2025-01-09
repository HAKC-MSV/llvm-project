//
// Created by de29664 on 12/5/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCWriter.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

#include "llvm/BinaryFormat/Dwarf.h"

namespace llvm::hakc {
    HAKCWriter::HAKCWriter(): os(errs()) {
    }

    raw_ostream &HAKCWriter::ostream() const {
        return os;
    }

    void HAKCWriter::printDIType(const DIType *type, unsigned indents) const {
        if (!type) {
            return;
        }
        for (unsigned i = 0; i < indents; i++) {
            os << "\t";
        }
        os << *type << "\n";
        if (auto *diDerivedType = dyn_cast<DIDerivedType>(type)) {
            if (diDerivedType->getBaseType()) {
                os << "\n";
                printDIType(diDerivedType->getBaseType(), indents + 1);
            }
        } else if (auto *diCompositeType = dyn_cast<DICompositeType>(type)) {
            if (diCompositeType->getTag() == dwarf::DW_TAG_enumeration_type ||
                diCompositeType->getTag() == dwarf::DW_TAG_array_type) {
                os << "\n";
                printDIType(diCompositeType->getBaseType(), indents + 1);
            }
        }
    }

    HAKCWriter &HAKCWriter::operator<<(llvm::Value *V) {
        if (V == nullptr) {
            os << "!!nullptr!!";
        } else if (const auto *F = dyn_cast<Function>(V)) {
            os << "Function " << F->getName();
        } else if (const auto *GV = dyn_cast<GlobalVariable>(V)) {
            os << "Global " << GV->getName();
        } else if (auto *Arg = dyn_cast<Argument>(V)) {
            os << "__Argument " << Arg->getArgNo() << " of " << Arg->getParent()->getName();
        } else {
            os << *V;
        }

        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(llvm::Value &V) {
        *this << &V;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(StringRef str) {
        os << str;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(unsigned int i) {
        os << i;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(unsigned long i) {
        os << i;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(bool b) {
        os << (b ? "True" : "False");
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const std::string &str) {
        os << str;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const char *s) {
        os << s;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Function &F) {
        F.print(os, nullptr);
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Module &M) {
        M.print(os, nullptr);
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Module *M) {
        *this << *M;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Type *Ty) {
        *this << *Ty;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Type &Ty) {
        os << Ty;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const DINode *DiNode) {
        *this << *DiNode;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const DINode &DiNode) {
        os << DiNode;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const DIType *DiType) {
        printDIType(DiType, 0);
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCCompartment &Compartment) {
        os << "Compartment " << Compartment.GetCompartmentIDValue();
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCCompartmentDivision &Division) {
        *this << Division.GetHAKCCompartment();
        os << " Division " << Division.GetDivisionID()->getZExtValue();
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCTypeInfo &TypeInfo) {
        TypeInfo >> os;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const enum HAKCAllocationTypeEnum AllocationType) {
        switch (AllocationType) {
            case InvalidAllocationType:
                os << "InvalidAllocationType";
                break;
            case SimpleArgumentSize:
                os << "SimpleArgumentSize";
                break;
            case SimpleStaticSize:
                os << "SimpleStaticSize";
                break;
            case StaticPlusArgument:
                os << "StaticPlusArgument";
                break;
            case MultiplyTwoArguments:
                os << "MultiplyTwoArguments";
                break;
            case ArgumentGEP:
                os << "ArgumentGEP";
                break;
        }
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const ManagedHAKCPointerUse &HAKCPointerUse) {
        os << "[" << HAKCPointerUse.getID() << "] Argument " << HAKCPointerUse.getOperandNo() << " of "
                << HAKCPointerUse.getUser() << " for ";
        *this << HAKCPointerUse.getManagedPtr();
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCPointerBase &ManagedPointer) {
        os << "Managed Pointer " << ManagedPointer.GetID();
        if (ManagedPointer.GetBaseDefinition()) {
            os << " [";
            if (isa<Argument>(ManagedPointer.GetBaseDefinition()) ||
                isa<GlobalValue>(ManagedPointer.GetBaseDefinition())) {
                os << "  ";
            }
            os << ManagedPointer.GetBaseDefinition() << "  ]";
        }
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCPointerBaseP &ManagedPointer) {
        *this << *ManagedPointer;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCFunctionInfo &HAKCFuncInfo) {
        HAKCFuncInfo >> os;
        return *this;
    }
} // hakc
