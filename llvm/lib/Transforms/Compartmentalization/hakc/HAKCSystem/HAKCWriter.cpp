//
// Created by de29664 on 12/5/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCWriter.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

#include "llvm/BinaryFormat/Dwarf.h"

namespace llvm::hakc {
    HAKCWriter::HAKCWriter(): os(errs()), debug(false) {
    }

    raw_ostream &HAKCWriter::ostream() const {
        return os;
    }

    void HAKCWriter::SetDebug(bool Debug) {
        debug = Debug;
    }

    void HAKCWriter::printDIType(const DIType *type, unsigned indents) const {
        if (!debug) {
            return;
        }

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
        if (!debug) {
            return *this;
        }
        if (V == nullptr) {
            *this << "!!nullptr!!";
        } else if (const auto *F = dyn_cast<Function>(V)) {
            *this << "Function " << F->getName();
        } else if (const auto *GV = dyn_cast<GlobalVariable>(V)) {
            *this << "Global " << GV->getName();
        } else if (auto *Arg = dyn_cast<Argument>(V)) {
            os << "Argument " << Arg->getArgNo() << " of " << Arg->getParent()->getName();
        } else {
            os << *V;
        }

        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(llvm::Use &U) {
        if (!debug) {
            return *this;
        }

        *this << "Operand " << U.getOperandNo() << " of " << U.getUser() << "\n";
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(llvm::Value &V) {
        *this << &V;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(StringRef str) {
        if (!debug) {
            return *this;
        }
        os << str;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(unsigned int i) {
        if (!debug) {
            return *this;
        }
        os << i;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(unsigned long i) {
        if (!debug) {
            return *this;
        }
        os << i;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(bool b) {
        if (!debug) {
            return *this;
        }
        os << (b ? "True" : "False");
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const std::string &str) {
        if (!debug) {
            return *this;
        }
        os << str;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const char *s) {
        if (!debug) {
            return *this;
        }
        os << s;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Function &F) {
        if (!debug) {
            return *this;
        }
        F.print(os, nullptr);
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const Module &M) {
        if (!debug) {
            return *this;
        }
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
        if (!debug) {
            return *this;
        }
        os << Ty;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const DINode *DiNode) {
        *this << *DiNode;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const DINode &DiNode) {
        if (!debug) {
            return *this;
        }
        os << DiNode;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const DIType *DiType) {
        printDIType(DiType, 0);
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCCompartment &Compartment) {
        if (!debug) {
            return *this;
        }
        os << "Compartment " << Compartment.GetCompartmentIDValue();
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCCompartmentDivision &Division) {
        if (!debug) {
            return *this;
        }
        *this << Division.GetHAKCCompartment();
        os << " Division " << Division.GetDivisionID()->getZExtValue();
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCTypeInfo &TypeInfo) {
        if (!debug) {
            return *this;
        }
        TypeInfo >> os;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const enum HAKCAllocationTypeEnum AllocationType) {
        if (!debug) {
            return *this;
        }
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
        if (!debug) {
            return *this;
        }
        os << "[" << HAKCPointerUse.getID() << "] Argument " << HAKCPointerUse.getOperandNo() << " of ";
        *this << HAKCPointerUse.getUser() << " for ";
        *this << HAKCPointerUse.getManagedPtr();
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCPointerBase &ManagedPointer) {
        if (!debug) {
            return *this;
        }
        os << "Managed Pointer " << ManagedPointer.GetID();
        if (ManagedPointer.GetBaseDefinition()) {
            os << " [";
            if (isa<Argument>(ManagedPointer.GetBaseDefinition()) ||
                isa<GlobalValue>(ManagedPointer.GetBaseDefinition())) {
                os << "  ";
            }
            *this << ManagedPointer.GetBaseDefinition() << "  ]";
        }
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCPointerBaseP &ManagedPointer) {
        *this << *ManagedPointer;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCFunctionInfo &HAKCFuncInfo) {
        if (!debug) {
            return *this;
        }
        HAKCFuncInfo >> os;
        return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCPreTransferAction &PreTransferAction) {
      if (!debug) {
        return *this;
      }
      os << *PreTransferAction.GetActionFunction() << "\n";
      return *this;
    }

    HAKCWriter &HAKCWriter::operator<<(const HAKCPostTargetAction &PostTargetAction) {
        if (!debug) {
          return *this;
        }
        os << *PostTargetAction.GetActionFunction() << "\n";
        return *this;
    }



} // namespace llvm::hakc
