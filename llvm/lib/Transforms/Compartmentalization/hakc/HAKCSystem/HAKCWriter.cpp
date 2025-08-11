//
// Created by de29664 on 12/5/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCWriter.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm::hakc {

void HAKCWriter::printDIType(const DIType *type, unsigned indents) const {
  if (!type) {
    return;
  }
  for (unsigned i = 0; i < indents; i++) {
    *os << "\t";
  }
  *os << *type << "\n";
  if (auto *diDerivedType = dyn_cast<DIDerivedType>(type)) {
    if (diDerivedType->getBaseType()) {
      *os << "\n";
      printDIType(diDerivedType->getBaseType(), indents + 1);
    }
  } else if (auto *diCompositeType = dyn_cast<DICompositeType>(type)) {
    if (diCompositeType->getTag() == dwarf::DW_TAG_enumeration_type ||
        diCompositeType->getTag() == dwarf::DW_TAG_array_type) {
      *os << "\n";
      printDIType(diCompositeType->getBaseType(), indents + 1);
    }
  }
}

HAKCWriter &HAKCWriter::operator<<(Value *V) {
  if (V == nullptr) {
    *this << "!!nullptr!!";
  } else if (const auto *F = dyn_cast<Function>(V)) {
    *this << "Function " << F->getName();
  } else if (const auto *GV = dyn_cast<GlobalVariable>(V)) {
    *this << "Global " << GV->getName();
  } else if (auto *Arg = dyn_cast<Argument>(V)) {
    *os << "Argument " << Arg->getArgNo() << " of "
        << Arg->getParent()->getName();
  } else {
    *os << *V;
  }

  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Use &U) {
  *this << "Operand " << U.getOperandNo() << " of " << U.getUser() << "\n";
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(double d) {
  *os << d;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(User *User) {
  *this << *User;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(Value &V) {
  *this << &V;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(StringRef str) {
  *os << str;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(unsigned int i) {
  *os << i;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(unsigned long i) {
  *os << i;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(ssize_t i) {
  *os << i;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(bool b) {
  *os << (b ? "True" : "False");
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const std::string &str) {
  *os << str;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const char *s) {
  if (s) {
    *os << s;
  }
  return *this; // ~__shared_ptr() = default; called after this returns then
                // segfaults
}

HAKCWriter &HAKCWriter::operator<<(const Function &F) {
  F.print(*os, nullptr);
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Module &M) {
  M.print(*os, nullptr);
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Module *M) {
  if (M) {
    *this << *M;
  }
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Type *Ty) {
  if (Ty) {
    *this << *Ty;
  }
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Type &Ty) {
  *os << Ty;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DINode *DiNode) {
  if (DiNode) {
    *this << *DiNode;
  }
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DINode &DiNode) {
  *os << DiNode;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DIType *DiType) {
  if (DiType) {
    printDIType(DiType, 0);
  }
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCCompartment &Compartment) {
  *os << "Compartment " << Compartment.GetCompartmentIDValue();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCCompartmentDivision &Division) {
  *this << Division.GetHAKCCompartment();
  *os << " Division " << Division.GetDivisionID()->getZExtValue();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCTypeInfo &TypeInfo) {
  TypeInfo >> *os;
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const enum HAKCAllocationTypeEnum AllocationType) {
  switch (AllocationType) {
  case InvalidAllocationType:
    *os << "InvalidAllocationType";
    break;
  case SimpleArgumentSize:
    *os << "SimpleArgumentSize";
    break;
  case SimpleStaticSize:
    *os << "SimpleStaticSize";
    break;
  case StaticPlusArgument:
    *os << "StaticPlusArgument";
    break;
  case MultiplyTwoArguments:
    *os << "MultiplyTwoArguments";
    break;
  case ArgumentGEP:
    *os << "ArgumentGEP";
    break;
  }
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const ManagedHAKCPointerUse &HAKCPointerUse) {
  *os << "[" << HAKCPointerUse.getID() << "] Argument "
      << HAKCPointerUse.getOperandNo() << " of ";
  *this << HAKCPointerUse.getUser() << " for ";
  // getManagedPtr is a reference and is guaranteed to not be null
  *this << HAKCPointerUse.getManagedPtr();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCPointerBase &ManagedPointer) {
  *os << "Managed Pointer " << ManagedPointer.GetID();
  if (ManagedPointer.GetBaseDefinition()) {
    *os << " [";
    if (isa<Argument>(ManagedPointer.GetBaseDefinition()) ||
        isa<GlobalValue>(ManagedPointer.GetBaseDefinition())) {
      *os << "  ";
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
  HAKCFuncInfo >> *os;
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const HAKCPreTransferAction &PreTransferAction) {
  *this << "Pre Transfer Action " << PreTransferAction.GetLabel() << " "
        << PreTransferAction.GetHAKCActionFunction().GetName();
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const HAKCPostTargetAction &PostTargetAction) {
  *this << "Post Transfer Action " << PostTargetAction.GetLabel() << " "
        << PostTargetAction.GetHAKCActionFunction().GetName();
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const HAKCFunctionDefinition &FunctionDefinition) {
  *this << "HAKC Function " << FunctionDefinition.GetName();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCFunctionArgumentDefinition &Arg) {
  *this << "HAKCFunction Arg " << Arg.Idx << " (use: " << Arg.ArgUse << ")";
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const enum HAKCFunctionArgumentUse ArgUse) {
  for (auto &it : HAKCArgumentArgumentUseStringMap()) {
    if (it.first == ArgUse) {
      *this << it.second;
      break;
    }
  }

  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCTransferAction &TransferAction) {
  *this << "Transfer Action "
        << TransferAction.GetHAKCActionFunction().GetName();
  if (!TransferAction.GetLabel().empty()) {
    *this << "(" << TransferAction.GetLabel() << ")";
  }
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DbgVariableRecord &DVR) {
  *os << DVR;
  return *this;
}
HAKCWriter &HAKCWriter::operator<<(const DbgVariableIntrinsic &DVI) {
  *os << DVI;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DILocalVariable &DLV) {
  *os << DLV;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DILocation &DL) {
  *os << DL.getFilename() << ":" << DL.getLine();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DILocation &DL) {
  if (!debug) {
    return *this;
  }
  os << DL.getFilename() << ":" << DL.getLine();
  return *this;
}
} // namespace llvm::hakc
