//
// Created by de29664 on 12/5/24.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCWriter.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

#include "llvm/BinaryFormat/Dwarf.h"

namespace llvm::hakc {

HAKCWriter::HAKCWriter() : os(errs()) {}

HAKCWriter::~HAKCWriter() { fd_os->close(); }

raw_ostream &HAKCWriter::ostream() const { return os; }

void HAKCWriter::SetConfiguredLogLevel(HAKCLogLevel log_level) {
  ConfiguredLogLevel = log_level;
}

void HAKCWriter::SetTempLogLevel(HAKCLogLevel log_level) {
  TempLogLevel = log_level;
}

HAKCLogLevel HAKCWriter::GetConfiguredLogLevel() { return ConfiguredLogLevel; }

HAKCLogLevel HAKCWriter::GetTempLogLevel() { return TempLogLevel; }

void HAKCWriter::SetLogPath(std::string fname) { log_path = fname; }

void HAKCWriter::CreateLog() {
  std::error_code EC;
  fd_os = new raw_fd_ostream(log_path, EC);
}

std::string HAKCWriter::GetLogPath() { return log_path; }

raw_fd_ostream &HAKCWriter::GetFdOstream() { return *fd_os; }

HAKCWriter &HAKCWriter::operator<<(const char *s) {
  // Pretty sure that all print statements call this eventually
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }

  os << s;
  *fd_os << s; // also output text to log file
  return *this;
}

void HAKCWriter::printDIType(const DIType *type, unsigned indents) const {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return;
  }

  if (!type) {
    return;
  }
  for (unsigned i = 0; i < indents; i++) {
    os << "\t";
    *fd_os << "\t";
  }
  os << *type << "\n";
  *fd_os << *type << "\n";
  if (auto *diDerivedType = dyn_cast<DIDerivedType>(type)) {
    if (diDerivedType->getBaseType()) {
      os << "\n";
      *fd_os << "\n";
      printDIType(diDerivedType->getBaseType(), indents + 1);
    }
  } else if (auto *diCompositeType = dyn_cast<DICompositeType>(type)) {
    if (diCompositeType->getTag() == dwarf::DW_TAG_enumeration_type ||
        diCompositeType->getTag() == dwarf::DW_TAG_array_type) {
      os << "\n";
      *fd_os << "\n";
      printDIType(diCompositeType->getBaseType(), indents + 1);
    }
  }
}

HAKCWriter &HAKCWriter::operator<<(llvm::Value *V) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  if (V == nullptr) {
    *this << "!!nullptr!!";
  } else if (const auto *F = dyn_cast<Function>(V)) {
    *this << "Function " << F->getName();
  } else if (const auto *GV = dyn_cast<GlobalVariable>(V)) {
    *this << "Global " << GV->getName();
  } else if (auto *Arg = dyn_cast<Argument>(V)) {
    os << "Argument " << Arg->getArgNo() << " of "
       << Arg->getParent()->getName();
    (*fd_os) << "Argument " << Arg->getArgNo() << " of "
             << Arg->getParent()->getName();
  } else {
    os << *V;
    *fd_os << *V;
  }

  return *this;
}

HAKCWriter &HAKCWriter::operator<<(llvm::Use &U) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
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
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << str;
  *fd_os << str;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(unsigned int i) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << i;
  (*fd_os) << i;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(unsigned long i) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << i;
  (*fd_os) << i;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(ssize_t i) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << i;
  (*fd_os) << i;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(bool b) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << (b ? "True" : "False");
  (*fd_os) << (b ? "True" : "False");
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const std::string &str) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << str;
  *fd_os << str;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Function &F) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  F.print(os, nullptr);
  F.print(*fd_os, nullptr);
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const Module &M) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  M.print(os, nullptr);
  M.print(*fd_os, nullptr);
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
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << Ty;
  *fd_os << Ty;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DINode *DiNode) {
  *this << *DiNode;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DINode &DiNode) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << DiNode;
  *fd_os << DiNode;
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const DIType *DiType) {
  printDIType(DiType, 0);
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCCompartment &Compartment) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << "Compartment " << Compartment.GetCompartmentIDValue();
  *fd_os << "Compartment " << Compartment.GetCompartmentIDValue();
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const hakc::HAKCCompartmentDivision &Division) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  *this << Division.GetHAKCCompartment();
  os << " Division " << Division.GetDivisionID()->getZExtValue();
  *fd_os << " Division " << Division.GetDivisionID()->getZExtValue();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const hakc::HAKCTypeInfo &TypeInfo) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  TypeInfo >> os;
  TypeInfo >> *fd_os;
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const enum HAKCAllocationTypeEnum AllocationType) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  switch (AllocationType) {
  case InvalidAllocationType:
    os << "InvalidAllocationType";
    *fd_os << "InvalidAllocationType";
    break;
  case SimpleArgumentSize:
    os << "SimpleArgumentSize";
    *fd_os << "SimpleArgumentSize";
    break;
  case SimpleStaticSize:
    os << "SimpleStaticSize";
    *fd_os << "SimpleStaticSize";
    break;
  case StaticPlusArgument:
    os << "StaticPlusArgument";
    *fd_os << "StaticPlusArgument";
    break;
  case MultiplyTwoArguments:
    os << "MultiplyTwoArguments";
    *fd_os << "MultiplyTwoArguments";
    break;
  case ArgumentGEP:
    os << "ArgumentGEP";
    *fd_os << "ArgumentGEP";
    break;
  }
  return *this;
}

HAKCWriter &
HAKCWriter::operator<<(const ManagedHAKCPointerUse &HAKCPointerUse) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << "[" << HAKCPointerUse.getID() << "] Argument "
     << HAKCPointerUse.getOperandNo() << " of ";
  *fd_os << "[" << HAKCPointerUse.getID() << "] Argument "
         << HAKCPointerUse.getOperandNo() << " of ";
  *this << HAKCPointerUse.getUser() << " for ";
  *this << HAKCPointerUse.getManagedPtr();
  return *this;
}

HAKCWriter &HAKCWriter::operator<<(const HAKCPointerBase &ManagedPointer) {
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  os << "Managed Pointer " << ManagedPointer.GetID();
  *fd_os << "Managed Pointer " << ManagedPointer.GetID();
  if (ManagedPointer.GetBaseDefinition()) {
    os << " [";
    *fd_os << " [";
    if (isa<Argument>(ManagedPointer.GetBaseDefinition()) ||
        isa<GlobalValue>(ManagedPointer.GetBaseDefinition())) {
      os << "  ";
      *fd_os << "  ";
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
  if (!TempLogLevel || (TempLogLevel < ConfiguredLogLevel)) {
    return *this;
  }
  HAKCFuncInfo >> os;
  HAKCFuncInfo >> *fd_os;
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
} // namespace llvm::hakc
