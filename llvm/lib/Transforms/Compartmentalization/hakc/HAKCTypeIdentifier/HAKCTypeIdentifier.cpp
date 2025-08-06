//
// Created by derrick on 9/8/21.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"

#include "llvm/AsmParser/Parser.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCGlobalInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/XRay/BlockPrinter.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallPrinter.h"

namespace llvm::hakc {
std::shared_ptr<HAKCTypeInfo> HAKCTypeIdentifier::FindType(const DIType *type) {
  if (!type) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Trying to find null type\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Finding HAKCTypeInfo for " << *type << "\n";
  if (auto *Derived = dyn_cast<DIDerivedType>(type)) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << " with base type ";
    if (Derived->getBaseType()) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << Derived->getBaseType();
    } else {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "void";
    }
    CommonHAKCAnalysis::getLogger(Verbose)
        << "\n";
  }
  auto it = TypesWithDebugInfo.find(type);
  if (it == TypesWithDebugInfo.end()) {
    return nullptr;
  }
  return it->second;
}

bool hakc::HAKCTypeIdentifier::IsStructTypeThatStartsWithPointerLikeType(
    const HAKCTypeInfo &HAKCTy) {
  if (HAKCTy.GetDbgType()) {
    auto *StrippedTy = HAKCTypeInfo::StripTypeModifiers(HAKCTy.GetDbgType());
    if (auto *CompositeTy = dyn_cast<DICompositeType>(StrippedTy)) {
      auto *FirstMemberTy = GetFirstStructMemberType(CompositeTy);
      return IsPointerLikeType(FirstMemberTy);
    }
  }

  return false;
}

bool hakc::HAKCTypeIdentifier::IsPointerLikeType(const DIType *DIType) {
  std::set<unsigned> PointerLike_Tags = {dwarf::DW_TAG_pointer_type,
                                         dwarf::DW_TAG_array_type};
  auto *StrippedTy = HAKCTypeInfo::StripTypeModifiers(DIType);
  if (!StrippedTy) {
    return false;
  }

  if (PointerLike_Tags.contains(StrippedTy->getTag())) {
    return true;
  }

  if (auto *BasicTy = dyn_cast<DIBasicType>(StrippedTy)) {
    std::set<unsigned> PointerLike_Encodings = {dwarf::DW_ATE_unsigned,
                                                dwarf::DW_ATE_address};
    return PointerLike_Encodings.contains(BasicTy->getEncoding()) &&
           BasicTy->getSizeInBits() == 64;
  } else if (auto *CompositeTy = dyn_cast<DICompositeType>(StrippedTy)) {
    if (CompositeTy->getTag() == dwarf::DW_TAG_union_type) {
      for (auto *UnionMember : CompositeTy->getElements()) {
        auto *MemberTy = dyn_cast<DIDerivedType>(UnionMember);
        if (IsPointerLikeType(MemberTy->getBaseType())) {
          return true;
        }
      }
    } else {
      auto *FirstMemberTy = GetFirstStructMemberType(CompositeTy);
      return IsPointerLikeType(FirstMemberTy);
    }
  }

  return false;
}

const DIType *hakc::HAKCTypeIdentifier::GetFirstStructMemberType(
    const DICompositeType *DICompositeTy) {
  auto *StrippedTy = dyn_cast<DICompositeType>(
      HAKCTypeInfo::StripTypeModifiers(DICompositeTy));
  if (!StrippedTy || (StrippedTy->getTag() != dwarf::DW_TAG_structure_type &&
                      StrippedTy->getTag() != dwarf::DW_TAG_union_type)) {
    return nullptr;
  }
  const auto MemberTypes = StrippedTy->getElements();
  if (MemberTypes.empty()) {
    return nullptr;
  }
  auto *FirstMemberType = HAKCTypeInfo::StripTypeModifiers(
      dyn_cast<DIDerivedType>(MemberTypes[0])->getBaseType());
  return FirstMemberType;
}

void hakc::HAKCTypeIdentifier::AddTypeMapping(
    const DIType *type, const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Adding mapping " << *type << " -> " << HAKCType->GetName() << "\n";
  HAKCType->SetDbgType(type);
  auto DbgTypeName = GetTypeName(type);
  HAKCType->SetDbgTypeName(DbgTypeName);
  TypesWithDebugInfo[type] = HAKCType;
}

std::string hakc::HAKCTypeIdentifier::GetDbgName(const HAKCTypeInfo &HAKCTy) {
  std::string Name = "";
  if (HAKCTy.GetDbgType()) {
    Name = GetTypeName(HAKCTy.GetDbgType());
  } else if (HAKCTy.GetLLVMType()) {
    Name = GetTypeName(HAKCTy.GetLLVMType());
  }
  return Name;
}

std::string hakc::HAKCTypeIdentifier::GetTypeName(Type *Ty) {
  std::string Name;
  raw_string_ostream NameStream(Name);
  Ty->print(NameStream);
  return Name;
}

std::string hakc::HAKCTypeIdentifier::GetTypeName(const DIType *type) {
  std::string Name;
  llvm::raw_string_ostream out(Name);

  if (auto *SubroutineTy = dyn_cast<DISubroutineType>(type)) {
    for (unsigned i = 0; i < SubroutineTy->getTypeArray()->getNumOperands();
         i++) {
      auto *CurrTy = SubroutineTy->getTypeArray()[i];
      if (!CurrTy) {
        if (i == 0) {
          out << "void";
        } else if (i == SubroutineTy->getTypeArray()->getNumOperands() - 1) {
          out << "...";
        } else {
          CommonHAKCAnalysis::getLogger(Fatal)
              << "Null operand at " << i << " for " << *type << "\n"
              << "Current type: " << Name << "\n";
          throw std::exception();
        }
      } else {
        out << GetTypeName(CurrTy);
      }

      if (i == 0) {
        out << " (";
      } else if (i < SubroutineTy->getTypeArray()->getNumOperands() - 1) {
        out << ", ";
      }
    }
    out << ")";
  } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
    if (DerivedTy->getTag() == dwarf::DW_TAG_pointer_type) {
      if (!DerivedTy->getBaseType()) {
        out << "void";
      } else {
        out << GetTypeName(DerivedTy->getBaseType());
      }
      out << "*";
    } else if (DerivedTy->getTag() == dwarf::DW_TAG_typedef) {
      out << GetTypeName(DerivedTy->getBaseType());
    } else if (DerivedTy->getTag() == dwarf::DW_TAG_volatile_type) {
      out << "volatile ";
      if (!DerivedTy->getBaseType()) {
        out << "void";
      } else {
        out << GetTypeName(DerivedTy->getBaseType());
      }
    } else if (DerivedTy->getTag() == dwarf::DW_TAG_const_type) {
      out << "const ";
      if (!DerivedTy->getBaseType()) {
        out << "void";
      } else {
        out << GetTypeName(DerivedTy->getBaseType());
      }
    } else if (DerivedTy->getTag() == dwarf::DW_TAG_restrict_type) {
      out << "restrict ";
      if (!DerivedTy->getBaseType()) {
        out << "void";
      } else {
        out << GetTypeName(DerivedTy->getBaseType());
      }
    } else {
      CommonHAKCAnalysis::getLogger(Fatal) << "Unhandled DIDerivedType tag\n"
                                           << DerivedTy << "\n";
      throw std::exception();
    }
  } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
    if (CompositeTy->getTag() == dwarf::DW_TAG_array_type) {
      out << GetTypeName(CompositeTy->getBaseType()) << "[]";
    } else if (CompositeTy->getTag() == dwarf::DW_TAG_structure_type) {
      out << "struct ";
      if (CompositeTy->getName().empty()) {
        out << "anon." << CompositeTy->getLine();
      } else {
        out << CompositeTy->getName();
      }
    } else if (CompositeTy->getTag() == dwarf::DW_TAG_union_type) {
      out << "union ";
      if (CompositeTy->getName().empty()) {
        out << "anon." << CompositeTy->getLine();
      } else {
        out << CompositeTy->getName();
      }
    } else if (CompositeTy->getTag() == dwarf::DW_TAG_enumeration_type) {
      out << "enum " << CompositeTy->getName();
    } else {
      CommonHAKCAnalysis::getLogger(Fatal) << "Unhandled DICompositeType tag\n"
                                           << CompositeTy << "\n";
      throw std::exception();
    }
  } else if (auto *BaseTy = dyn_cast<DIBasicType>(type)) {
    if (BaseTy->getEncoding() == dwarf::DW_ATE_boolean) {
      out << "bool";
    } else {
      out << BaseTy->getName();
    }
  } else {
    CommonHAKCAnalysis::getLogger(Fatal) << "Unhandled DIType\n"
                                         << type << "\n";
    throw std::exception();
  }

  return Name;
}

Type *HAKCTypeIdentifier::FindNamedType(StringRef TypeName) const {
  const auto UnionName = "union." + TypeName;
  const auto StructName = "struct." + TypeName;
  for (auto *StructTy : IdentifiedStructTypes) {
    auto LLVMName = StructTy->getName();
    if (LLVMName.ends_with(UnionName.str()) ||
        LLVMName.ends_with(StructName.str())) {
      return StructTy;
    }
  }
  return nullptr;
}

Type *
HAKCTypeIdentifier::FindAnonymousType(const DICompositeType *CompositeTy) {
  auto Cached = AnonymousTypes.find(CompositeTy);
  if (Cached != AnonymousTypes.end()) {
    return Cached->second;
  }

  SmallVector<StructType *> FoundTypes;
  for (auto *StructTy : IdentifiedStructTypes) {
    if (StructTy->getName().contains(".anon")) {
      bool TypesMatch = true;
      unsigned i;
      for (i = 0; i < CompositeTy->getElements().size(); i++) {
        auto *ElementTy = dyn_cast<DIType>(CompositeTy->getElements()[i]);
        if (!ElementTy || !StructTy->indexValid(i)) {
          TypesMatch = false;
          break;
        }
        auto *ElementLLVMTy = GetLLVMType(ElementTy);
        if (!ElementLLVMTy || ElementLLVMTy != StructTy->getTypeAtIndex(i)) {
          TypesMatch = false;
          break;
        }
      }
      if (TypesMatch && !StructTy->indexValid(i + 1)) {
        FoundTypes.push_back(StructTy);
      }
    }
  }

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Found " << FoundTypes.size() << " Types for " << CompositeTy << ":\n";
  for (auto *Ty : FoundTypes) {
    CommonHAKCAnalysis::getLogger(Verbose) << Ty << "\n";
  }
  Type *FoundType = nullptr;
  if (!FoundTypes.empty()) {
    FoundType = FoundTypes.front();
  }

  AnonymousTypes[CompositeTy] = FoundType;
  return FoundType;
}

Type *HAKCTypeIdentifier::GetLLVMType(const DIType *Ty) {
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Finding LLVM Type for " << Ty << "\n";
  auto &Ctx = GetModule().getContext();
  if (!Ty) {
    return Type::getVoidTy(Ctx);
  } else if (Ty->getTag() == dwarf::DW_TAG_pointer_type) {
    return PointerType::get(Ctx, 0);
  } else if (auto *BasicTy = dyn_cast<DIBasicType>(Ty)) {
    if (BasicTy->getEncoding() == dwarf::DW_ATE_boolean) {
      return IntegerType::get(Ctx, 1);
    }
    return IntegerType::get(Ctx, BasicTy->getSizeInBits());
  } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(Ty)) {
    if (Ty->getTag() == dwarf::DW_TAG_typedef && !Ty->getName().empty()) {
      auto *LLVMTy = FindNamedType(Ty->getName());
      if (LLVMTy) {
        return LLVMTy;
      }
    }
    if (DerivedTy->getBaseType())
      return GetLLVMType(DerivedTy->getBaseType());
  } else if (Ty->getTag() == dwarf::DW_TAG_structure_type) {
    if (!Ty->getName().empty()) {
      return FindNamedType(Ty->getName());
    } else {
      return FindAnonymousType(dyn_cast<DICompositeType>(Ty));
    }
  } else if (Ty->getTag() == dwarf::DW_TAG_enumeration_type) {
    return GetLLVMType(dyn_cast<DICompositeType>(Ty)->getBaseType());
  }
  return nullptr;
}

FunctionType *
HAKCTypeIdentifier::GetLLVMFunctionTy(const DISubroutineType *FunctionTy) {
  auto &Ctx = GetModule().getContext();
  auto *ReturnTy = Type::getVoidTy(Ctx);
  auto TyArray = FunctionTy->getTypeArray();
  if (TyArray[0]) {
    ReturnTy = GetLLVMType(TyArray[0]);
    if (!ReturnTy) {
      CommonHAKCAnalysis::getLogger(Error)
          << "Could not find Return Type " << TyArray[0] << "\n";
      return nullptr;
    }
  }
  if (!FunctionType::isValidReturnType(ReturnTy)) {
    CommonHAKCAnalysis::getLogger(Error)
        << "Type " << ReturnTy << " is not a valid return type\n";
    return nullptr;
  }
  SmallVector<Type *> ArgTys;

  bool IsVarArg = false;
  for (unsigned i = 1; i < TyArray.size(); i++) {
    if (TyArray[i] == nullptr) {
      IsVarArg = true;
      break;
    }
    auto *Ty = GetLLVMType(TyArray[i]);
    if (!Ty) {
      CommonHAKCAnalysis::getLogger(Error)
          << "Could not find LLVM Type for " << TyArray[i] << "\n";
      return nullptr;
    }
    if (!FunctionType::isValidArgumentType(Ty)) {
      CommonHAKCAnalysis::getLogger(Error)
          << "Type " << Ty << " for DIType " << TyArray[i]
          << " is not a valid argument type\n";
      return nullptr;
    }
    ArgTys.push_back(Ty);
  }

  auto *LLVMTy = FunctionType::get(ReturnTy, ArgTys, IsVarArg);
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Found LLVM Type " << LLVMTy << " for " << FunctionTy << "\n";

  return LLVMTy;
}

std::shared_ptr<HAKCTypeInfo>
HAKCTypeIdentifier::HandleType(const DIType *type) {
  auto debug = false;
  if (!type) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Trying to find null type\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Analyzing DIType " << *type << "\n";
  auto TypeP = FindType(type);
  if (TypeP) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Already created " << *type << "\n";
    return TypeP;
  }
  auto *StrippedTy = HAKCTypeInfo::StripTypeModifiers(type);
  if (isa<DICompositeType>(StrippedTy) || isa<DISubroutineType>(StrippedTy) ||
      isa<DIBasicType>(StrippedTy)) {
    CommonHAKCAnalysis::getLogger(Debug) << "Creating HAKCTypeInfo for\n"
                                         << type << "\n";
    auto TypeName = GetTypeName(type);
    TypeP = std::make_shared<HAKCTypeInfo>(AnalysisHelper, TypeName, debug);
    if (auto *BasicType = dyn_cast<DIBasicType>(type)) {
      auto BitSize = BasicType->getSizeInBits();
      if (BasicType->getEncoding() == dwarf::DW_ATE_boolean) {
        BitSize = 1;
      }
      auto *IntTy = IntegerType::get(GetModule().getContext(), BitSize);
      TypeP->SetLLVMType(IntTy);
    } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
      Type *LLVMTy = nullptr;
      if (CompositeTy->getTag() == dwarf::DW_TAG_structure_type ||
          CompositeTy->getTag() == dwarf::DW_TAG_union_type) {
        std::string SearchName = ".";
        SearchName += CompositeTy->getName();
        for (auto *StructTy : IdentifiedStructTypes) {
          if (StructTy->getName().ends_with(SearchName)) {
            LLVMTy = StructTy;
            break;
          }
        }
      } else if (CompositeTy->getTag() == dwarf::DW_TAG_array_type) {
        if (CompositeTy->getBaseType()) {
          auto ArrayMemberTy = HandleType(CompositeTy->getBaseType());
          if (ArrayMemberTy && ArrayMemberTy->GetLLVMType()) {
            auto SizeInBits = ArrayMemberTy->GetSizeInBits();
            if (SizeInBits > 0 && CompositeTy->getSizeInBits()) {
              auto ArraySize = CompositeTy->getSizeInBits() / SizeInBits;
              LLVMTy = ArrayType::get(ArrayMemberTy->GetLLVMType(), ArraySize);
              TypeP->SetPointeeType(ArrayMemberTy);
            }
          }
        }
      }

      if (LLVMTy) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Setting " << *TypeP << " LLVM Type to be " << *LLVMTy << "\n";
        TypeP->SetLLVMType(LLVMTy);
      }
    } else if (auto *SubRoutineTy = dyn_cast<DISubroutineType>(StrippedTy)) {
      for (auto *FuncTy : SubRoutineTy->getTypeArray()) {
        if (FuncTy) {
          HandleType(FuncTy);
        }
      }
      auto *LLVMTy = GetLLVMFunctionTy(SubRoutineTy);
      if (LLVMTy) {
        TypeP->SetLLVMType(LLVMTy);
      }
    }
    AddTypeMapping(type, TypeP);
  } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(StrippedTy)) {
    if (DerivedTy->getTag() == dwarf::DW_TAG_member) {
      if (DerivedTy->getBaseType()) {
        HandleType(DerivedTy->getBaseType());
      }
    } else {
      std::set<unsigned> TagsToConsider = {
          dwarf::DW_TAG_pointer_type,  dwarf::DW_TAG_array_type,
          dwarf::DW_TAG_const_type,    dwarf::DW_TAG_typedef,
          dwarf::DW_TAG_volatile_type, dwarf::DW_TAG_restrict_type,
      };
      if (TagsToConsider.contains(DerivedTy->getTag())) {
        CommonHAKCAnalysis::getLogger(Verbose) << "Creating HAKCTypeInfo for\n"
                                               << type << "\n";
        auto TypeName = GetTypeName(type);
        TypeP = std::make_shared<HAKCTypeInfo>(AnalysisHelper, TypeName, debug);
        auto *LLVMTy = GetLLVMType(DerivedTy);
        if (LLVMTy) {
          CommonHAKCAnalysis::getLogger(Verbose)
              << "Found LLVM Type " << *LLVMTy << "\n";
          TypeP->SetLLVMType(LLVMTy);
        } else {
          if (debug) {
            CommonHAKCAnalysis::getLogger(Error)
                << "Could not find LLVM Type for " << type << "\n";
            for (auto *STy : IdentifiedStructTypes) {
              CommonHAKCAnalysis::getLogger(Verbose) << STy << "\n";
            }
          }
        }
        AddTypeMapping(type, TypeP);
        if (DerivedTy->getBaseType()) {
          HandleType(DerivedTy->getBaseType());
        }
      } else {
        CommonHAKCAnalysis::getLogger(Error)
            << "Not handling DITYpe " << type << "\n";
      }
    }
  }
  return TypeP;
}

GlobalVariable *
HAKCTypeIdentifier::FindGlobal(const DIGlobalVariable *DIGV) const {
  for (auto &GV : GetModule().global_objects()) {
    SmallVector<MDNode *, 8> DebugMetadata;
    GV.getMetadata(LLVMContext::MD_dbg, DebugMetadata);
    for (auto *DbgInfo : DebugMetadata) {
      if (auto *GVExpression = dyn_cast<DIGlobalVariableExpression>(DbgInfo)) {
        if (GVExpression->getVariable() == DIGV) {
          return dyn_cast<GlobalVariable>(&GV);
        }
      }
    }
  }

  return nullptr;
}

std::shared_ptr<HAKCGlobalInfo>
HAKCTypeIdentifier::HandleGlobal(const DIGlobalVariable *DIGV) {
  auto SuppressOutput =
      !AnalysisHelper.GetSystemInfo().OutputDebugInfo(DIGV->getName());
  CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
      << "Analyzing Global " << *DIGV << "\n";

  auto *GV = FindGlobal(DIGV);
  if (!GV) {
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
        << "\nCould not find Global " << DIGV->getName() << "\n";
    return nullptr;
  }
  auto DIGVTy = FindType(DIGV->getType());
  if (!DIGVTy) {
    DIGVTy = HandleType(DIGV->getType());
    if (!DIGVTy) {
      CommonHAKCAnalysis::getLogger(Fatal, SuppressOutput)
          << "Unable to handle DIType " << *DIGV->getType() << " for Global "
          << *DIGV << "\n";
      throw std::exception();
    }
  }
  if (!DIGVTy->GetLLVMType()) {
    DIGVTy->SetLLVMType(GV->getValueType());
  }

  /* In LLVM, Globals are always pointers */
  auto HAKCTy = FindPointerType(*DIGVTy);
  if (!HAKCTy) {
    HAKCTy = AddMissingPointerType(DIGVTy);
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
        << "Added missing pointer type " << *HAKCTy << " for Global " << *GV
        << "\n";
  }

  auto GVP = std::make_shared<HAKCGlobalInfo>(AnalysisHelper, GV->getName(),
                                              SuppressOutput);
  GVP->SetType(HAKCTy);
  GVP->SetGlobalVariable(GV);
  GVP->SetDefiningLocation(DIGV->getFile(), DIGV->getLine());
  if (DIGV->isLocalToUnit()) {
    const auto *Scope = DIGV->getScope();
    if (!Scope) {
      Scope = CompilationUnitScope;
    }
    GVP->SetLocalScope(Scope);
  }
  AddGlobalMapping(DIGV, GVP);

  return GVP;
}

void HAKCTypeIdentifier::AddGlobalMapping(
    const DIGlobalVariable *DIGV,
    const std::shared_ptr<HAKCGlobalInfo> &HAKCSymbol) {

  CommonHAKCAnalysis::getLogger(
      Debug, !AnalysisHelper.GetSystemInfo().OutputDebugInfo(DIGV->getName()))
      << "Adding mapping " << *DIGV << " -> " << HAKCSymbol->GetName() << "\n";

  globals[DIGV] = HAKCSymbol;
}

std::shared_ptr<HAKCFunctionInfo>
HAKCTypeIdentifier::HandleFunction(const DISubprogram *SubProg) {
  auto SuppressOutput =
      !AnalysisHelper.GetSystemInfo().OutputDebugInfo(SubProg->getName());

  CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
      << "Handling DISubprogram " << *SubProg << "\n";

  auto *F = GetModule().getFunction(SubProg->getName());
  if (!F) {
    for (auto &FM : GetModule().functions()) {
      if (FM.getSubprogram() == SubProg) {
        F = &FM;
        break;
      }
    }
  }

  if (!F) {
    CommonHAKCAnalysis::getLogger(Error)
        << "\nCould not find Function " << SubProg->getName() << "\n";
    return nullptr;
  }
  if (F->getSubprogram() != SubProg) {
    CommonHAKCAnalysis::getLogger(Error, SuppressOutput)
        << *F << " SubProgram does (not?) equal " << *SubProg << "\n";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
        << F->getSubprogram() << " == " << SubProg << "\n";
  }

  if (CommonHAKCAnalysis::IsOutsideTransferFunc(F) || F->isIntrinsic()) {
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
        << SubProg->getName() << " is a HAKC Transfer function\n";
    return nullptr;
  }
  auto DIGVTy = FindType(SubProg->getType());
  if (!DIGVTy) {
    CommonHAKCAnalysis::getLogger(Fatal, SuppressOutput)
        << GetModule() << "Could not find HAKCType of " << F->getName()
        << " with DIType " << *SubProg->getType() << "\n";
    throw std::exception();
  }

  if (!DIGVTy->GetLLVMType()) {
    DIGVTy->SetLLVMType(F->getFunctionType());
  }
  auto FP = std::make_shared<HAKCFunctionInfo>(
      AnalysisHelper, SubProg->getName(), !SuppressOutput);
  FP->SetType(DIGVTy);
  FP->SetFunction(F);
  FP->SetDefiningLocation(SubProg->getFile(), SubProg->getLine());
  if (SubProg->isLocalToUnit()) {
    FP->SetLocalScope(SubProg->getScope());
  }
  AddFunctionMapping(SubProg, FP);

  return FP;
}

void HAKCTypeIdentifier::AddFunctionMapping(
    const DISubprogram *SubProg,
    const std::shared_ptr<HAKCFunctionInfo> &HAKCFunction) {

  CommonHAKCAnalysis::getLogger(
      Debug,
      !AnalysisHelper.GetSystemInfo().OutputDebugInfo(SubProg->getName()))
      << "Adding mapping " << *SubProg << " -> " << *HAKCFunction << "\n";

  functions[SubProg] = HAKCFunction;
}

void HAKCTypeIdentifier::FindAllGlobalsUsed(
    Value *V, std::set<GlobalObject *> &GlobalSet) {
  if (auto *ConstStruct = dyn_cast<ConstantStruct>(V)) {
    for (auto &Member : ConstStruct->operands()) {
      auto *MemberDef = AnalysisHelper.getDef(Member.get(), false);
      if (auto *GlobalObj = dyn_cast<GlobalObject>(MemberDef)) {
        GlobalSet.insert(GlobalObj);
      } else {
        FindAllGlobalsUsed(MemberDef, GlobalSet);
      }
    }
  } else if (auto *GlobalObj = dyn_cast<GlobalObject>(V)) {
    GlobalSet.insert(GlobalObj);
  } else if (auto *ConstArray = dyn_cast<ConstantArray>(V)) {
    for (auto &Member : ConstArray->operands()) {
      auto MemberDef = AnalysisHelper.getDef(Member.get(), false);
      if (auto *GlobalMember = dyn_cast<GlobalObject>(MemberDef)) {
        GlobalSet.insert(GlobalMember);
      } else {
        FindAllGlobalsUsed(MemberDef, GlobalSet);
      }
    }
  } else if (auto *I = dyn_cast<Instruction>(V)) {
    for (auto &Op : I->operands()) {
      auto MemberDef = AnalysisHelper.getDef(Op.get(), false);
      if (auto *GlobalMember = dyn_cast<GlobalObject>(MemberDef)) {
        GlobalSet.insert(GlobalMember);
      }
    }
  }
}

std::shared_ptr<HAKCFunctionInfo>
HAKCTypeIdentifier::AddNoDebugFunction(Function *F) {
  auto SuppressOutput = !AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);

  for (const auto &UnmappedFunc : UnmappedFunctions) {
    if (UnmappedFunc->GetFunction() == F) {
      return UnmappedFunc;
    }
  }
  CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
      << "Adding unmapped Function " << F->getName() << "\n";
  auto FuncInfo = std::make_shared<HAKCFunctionInfo>(
      AnalysisHelper, F->getName(), !SuppressOutput);
  FuncInfo->SetFunction(F);
  FuncInfo->SetLocalScope(CompilationUnitScope);
  auto HAKCType = FindCalledFunctionType(F->getFunctionType());
  if (!HAKCType) {
    HAKCType = CreateNoDebugType(F->getFunctionType());
  }

  CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
      << "HAKCType exists for " << F->getName() << "\n";

  if (!HAKCType->GetLLVMType()) {
    HAKCType->SetLLVMType(F->getFunctionType());
  }
  FuncInfo->SetType(HAKCType);
  UnmappedFunctions.insert(FuncInfo);
  return FuncInfo;
}

std::shared_ptr<HAKCSymbolInfo>
HAKCTypeIdentifier::AddNoDebugGlobal(GlobalObject *GlobalObj) {
  auto SuppressOutput =
      !AnalysisHelper.GetSystemInfo().OutputDebugInfo(GlobalObj);
  if (auto *F = dyn_cast<Function>(GlobalObj)) {
    return AddNoDebugFunction(F);
  } else if (auto *GV = dyn_cast<GlobalVariable>(GlobalObj)) {
    for (auto UnmappedGlobal : UnmappedGlobals) {
      if (UnmappedGlobal->GetGlobalVariable() == GV) {
        return UnmappedGlobal;
      }
    }
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
        << "Adding unmapped Global Variable " << GV->getName() << "\n";
    auto GlobalInfo = std::make_shared<HAKCGlobalInfo>(
        AnalysisHelper, GlobalObj->getName(), !SuppressOutput);
    auto BaseType = FindType(GlobalObj->getValueType());
    if (!BaseType) {
      BaseType = CreateNoDebugType(GlobalObj->getValueType());
    }
    if (!BaseType->GetLLVMType()) {
      BaseType->SetLLVMType(GV->getValueType());
    }
    auto HAKCType = FindPointerType(*BaseType);
    if (!HAKCType) {
      HAKCType = AddMissingPointerType(BaseType);
    }

    GlobalInfo->SetGlobalVariable(GV);
    GlobalInfo->SetType(HAKCType);
    GlobalInfo->SetLocalScope(CompilationUnitScope);

    UnmappedGlobals.insert(GlobalInfo);
    return GlobalInfo;
  } else {
    CommonHAKCAnalysis::getLogger(Fatal, SuppressOutput)
        << "Unsupported GlobalObj: " << *GlobalObj << "\n";
    throw std::exception();
  }
}

void HAKCTypeIdentifier::AddUsedGlobals(
    const std::set<GlobalObject *> &GlobalObjects,
    const std::shared_ptr<HAKCSymbolInfo> &UserSymbol) {
  for (auto *UsedGlobal : GlobalObjects) {
    auto Symbol = FindSymbol(UsedGlobal);
    if (!Symbol) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "\nGlobal " << UsedGlobal->getName() << " is used in "
          << UserSymbol->GetGlobalObj()->getName()
          << " but the Symbol could not be found\n";
      Symbol = AddNoDebugGlobal(UsedGlobal);
    }
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Found Symbol " << Symbol->GetName() << "\n";
    if (Symbol) {
      UserSymbol->AddSymbolUse(Symbol);
    }
  }
}

void HAKCTypeIdentifier::FindUsesInGlobals() {
  for (auto &it : globals) {
    auto *GV = it.second->GetGlobalVariable();

    if (GV->hasInitializer()) {

      CommonHAKCAnalysis::getLogger(
          Debug, !AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV))
          << "Searching for globals in " << *GV << "\n";

      std::set<GlobalObject *> GlobalsUsed;
      FindAllGlobalsUsed(GV->getInitializer(), GlobalsUsed);
      AddUsedGlobals(GlobalsUsed, it.second);
    }
  }
}

void HAKCTypeIdentifier::CreateIndirectCallSourceLink(
    Value *V, std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path) {
  if (auto *Arg = dyn_cast<Argument>(V)) {
    auto HAKCType = FindType(Arg->getType());
    if (!HAKCType) {

      CommonHAKCAnalysis::getLogger(
          Debug,
          !AnalysisHelper.GetSystemInfo().OutputDebugInfo(Arg->getParent()))
          << "Could not find HAKCType for Argument " << Arg->getArgNo()
          << " of Function " << Arg->getParent()->getName() << "\n";

      return;
    }
    if (!HAKCType->GetLLVMType()) {
      HAKCType->SetLLVMType(Arg->getType());
    }
    auto Link = std::make_shared<HAKCIndirectCallSourceLink>(
        Arg, HAKCType,
        AnalysisHelper.GetSystemInfo().OutputDebugInfo(Arg->getParent()));
    Path.push_back(Link);
  } else if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    auto HAKCSymbol = FindGlobal(GV, true);
    if (!HAKCSymbol) {

      CommonHAKCAnalysis::getLogger(
          Debug, AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV))
          << "Could not find HAKC Symbol for " << *GV << "\n";

      return;
    }
    auto Link = std::make_shared<HAKCIndirectCallSourceLink>(
        HAKCSymbol, AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV));
    Path.push_back(Link);
  } else if (auto *Load = dyn_cast<LoadInst>(V)) {
    auto *Pointer = Load->getPointerOperand();
    auto SuppressOutput =
        AnalysisHelper.GetSystemInfo().OutputDebugInfo(Load->getFunction());
    if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
      auto *TyToCheck = GEP->getSourceElementType();
      auto HAKCType = FindType(TyToCheck);
      if (HAKCType /*&& GEP->hasAllConstantIndices()*/) {
        APInt Offset(64, 0);
        GEP->stripAndAccumulateInBoundsConstantOffsets(
            GetModule().getDataLayout(), Offset);
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Offset in bits for " << *GEP << ": " << Offset.getZExtValue()
            << "\n";
        if (!HAKCType->GetLLVMType()) {
          HAKCType->SetLLVMType(TyToCheck);
        }
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(
            HAKCType, Offset.getSExtValue(), !SuppressOutput);
        Path.push_back(Link);
        CreateIndirectCallSourceLink(GEP->getPointerOperand(), Path);
      }
      if (!GEP->hasAllConstantIndices()) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "GEP does not have all constant indices: " << *GEP << "\n";
      } else {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Could not find Load Pointer HAKC Type for " << *TyToCheck
            << "\n";
      }
    } else if (auto *GVal = dyn_cast<GlobalValue>(Pointer)) {
      auto HAKCSymbol = FindSymbol(GVal, true);
      if (!HAKCSymbol) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Unable to find Global " << GVal->getName() << "\n";
        return;
      }
      auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCSymbol,
                                                               !SuppressOutput);
      Path.push_back(Link);
    } else {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Unhandled Load Pointer Operand type: " << *Pointer << "\n";
      auto *LoadTy = Load->getPointerOperand()->getType();
      auto HAKCType = FindType(LoadTy);
      if (HAKCType) {
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(
            HAKCType, 0, !SuppressOutput);
        Path.push_back(Link);
      }
    }
  } else if (auto *IntToPtr = dyn_cast<IntToPtrInst>(V)) {

    CommonHAKCAnalysis::getLogger(
        Debug, !AnalysisHelper.GetSystemInfo().OutputDebugInfo(
                   IntToPtr->getFunction()))
        << "Adding IntToPtr Link\n";

    CreateIndirectCallSourceLink(IntToPtr->getOperand(0), Path);
  } else if (auto *CallI = dyn_cast<CallInst>(V)) {

    CommonHAKCAnalysis::getLogger(
        Debug,
        !AnalysisHelper.GetSystemInfo().OutputDebugInfo(CallI->getFunction()))
        << "Adding Call Link\n";

    CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
  } else {
    CommonHAKCAnalysis::getLogger(Error)
        << "Unhandled Link type: " << V << "\n";
  }
}

void HAKCTypeIdentifier::FindIndirectCallSource(
    CallInst *CallI,
    std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path) {
  if (!CallI->isIndirectCall()) {
    CommonHAKCAnalysis::getLogger(Error)
        << *CallI << " is not an indirect call\n";
    throw std::exception();
  }

  CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);

  CommonHAKCAnalysis::getLogger(
      Debug,
      !AnalysisHelper.GetSystemInfo().OutputDebugInfo(CallI->getFunction()))
      << "Found " << Path.size() << " path links for "
      << *CallI->getCalledOperand() << "\n";
}

FunctionType *
HAKCTypeIdentifier::GetIndirectCallFunctionType(const CallInst *CallI) {
  if (!CallI->isIndirectCall()) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to get type from a Call that is not an indirect call\n";
    throw std::exception();
  }
  return CallI->getFunctionType();
}

void HAKCTypeIdentifier::FindUsesInFunctions() {
  for (auto &it : functions) {
    auto *F = it.second->GetFunction();
    auto SuppressOutput = !AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);
    CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
        << "Searching for globals in Function " << F->getName() << "\n";

    std::set<GlobalObject *> GlobalsUsed;
    for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
      auto *I = &(*InstIt);
      if (I->isDebugOrPseudoInst() || isa<IntrinsicInst>(I) ||
          isa<BranchInst>(I)) {
        continue;
      }
      FindAllGlobalsUsed(I, GlobalsUsed);
    }
    AddUsedGlobals(GlobalsUsed, it.second);

    for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
      auto *I = &(*InstIt);
      if (I->isDebugOrPseudoInst() || isa<IntrinsicInst>(I) ||
          isa<BranchInst>(I)) {
        continue;
      }
      if (auto *Call = dyn_cast<CallInst>(I)) {
        if (Call->getCalledFunction()) {
          auto FoundFunction = FindFunction(Call->getCalledFunction(), true);
          if (!FoundFunction) {
            CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
                << "Could not find HAKC Symbol for Function "
                << Call->getCalledFunction()->getName() << "\n";
            FoundFunction = AddNoDebugFunction(Call->getCalledFunction());
          }
          it.second->AddDirectCall(FoundFunction);
        } else if (Call->isIndirectCall()) {
          auto *FunctionTy = GetIndirectCallFunctionType(Call);
          CommonHAKCAnalysis::getLogger(Verbose, SuppressOutput)
              << "Source of indirect call operand in Function " << F->getName()
              << ": " << AnalysisHelper.getDef(Call->getCalledOperand(), true)
              << "\n";
          auto HAKCType = FindCalledFunctionType(FunctionTy);
          if (!HAKCType) {
            CommonHAKCAnalysis::getLogger(Error, SuppressOutput)
                << "Could not find called HAKCType for " << *Call
                << " with Searched Type " << *FunctionTy << " in Function "
                << F->getName() << "\n";
            HAKCType = FindType(Call->getCalledOperand()->getType());
            if (HAKCType) {
              CommonHAKCAnalysis::getLogger(Fatal, SuppressOutput)
                  << "But the Pointer HAKCType exists: " << HAKCType->GetName()
                  << "\n";
            }
            throw std::exception();
          }
          std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> SourcePath;
          if (!HAKCType->GetLLVMType()) {
            HAKCType->SetLLVMType(FunctionTy);
          }
          FindIndirectCallSource(Call, SourcePath);
          auto Source = std::make_shared<HAKCIndirectCallSource>(
              SourcePath, HAKCType, !SuppressOutput);
          it.second->AddIndirectCall(Source);
        }
      }
    }
  }
}

void hakc::HAKCTypeIdentifier::FindAllTypes(
    Type *Ty, SmallVectorImpl<HAKCTypeP> &Results) const {
  if (isa<PointerType>(Ty)) {
    return;
  }

  for (auto &it : TypesWithDebugInfo) {
    if (it.second->GetLLVMType() == Ty) {
      Results.push_back(it.second);
    }
  }

  for (auto HAKCTy : TypesMissingDebugInfo) {
    if (HAKCTy->GetLLVMType() == Ty) {
      Results.push_back(HAKCTy);
    }
  }
}

std::shared_ptr<HAKCTypeInfo> HAKCTypeIdentifier::FindType(Type *Ty) const {
  // remove this functionality, because it would only work for non pointers and
  // is logically incorrect
  if (isa<PointerType>(Ty)) {
    return nullptr;
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Trying to find HAKCTypeInfo for " << *Ty << "\n";

  for (auto &it : TypesWithDebugInfo) {
    if (it.second->GetLLVMType()) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Comparing " << it.second->GetLLVMType() << " with " << Ty << "\n";
    }
    if (it.second->GetLLVMType() && it.second->GetLLVMType() == Ty) {
      return it.second;
    }
  }

  return nullptr;
}

HAKCTypeP HAKCTypeIdentifier::FindType(HAKCPointerBase &HAKCPointer) {
  HAKCTypeP ReturnTy = nullptr;
  if (HAKCPointer.GetType()) {
    ReturnTy = HAKCPointer.GetType();
  } else {
    ReturnTy = FindHAKCType(HAKCPointer.GetBaseDefinition());
  }
  if (ReturnTy) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Found ReturnTy: " << *ReturnTy << "\n";
    if (ReturnTy->IsPointerType() && !ReturnTy->GetPointeeType()) {
      if (auto PointeeType = FindPointeeType(ReturnTy)) {
        ReturnTy->SetPointeeType(PointeeType);
      }
    }
    HAKCPointer.SetType(ReturnTy);
  }
  return ReturnTy;
}

HAKCTypeP HAKCTypeIdentifier::FindPointeeType(HAKCPointerBase &HAKCPointer) {
  if (HAKCPointer.GetType() && HAKCPointer.GetType()->GetPointeeType()) {
    return HAKCPointer.GetType()->GetPointeeType();
  }

  HAKCTypeP BaseType = HAKCPointer.GetType();
  HAKCTypeP PointeeType = nullptr;
  if (!BaseType) {
    BaseType = FindType(HAKCPointer);
  }

  if (BaseType) {
    PointeeType = FindPointeeType(BaseType);
    BaseType->SetPointeeType(PointeeType);
  }

  return PointeeType;
}

HAKCTypeP HAKCTypeIdentifier::FindPointeeType(HAKCTypeP BaseType) {
  if (!BaseType) {
    return nullptr;
  }

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Finding Pointee Type for " << *BaseType << "\n";

  if (BaseType->IsVoidPtrType()) {
    return GetVoidPointerPointeeType();
  }

  if (BaseType->GetPointeeType()) {
    return BaseType->GetPointeeType();
  }

  if (BaseType->IsIntegerType() && IsPointerLikeType(BaseType->GetDbgType())) {
    return GetVoidPointerPointeeType();
  }

  DIType *TypeToFind = nullptr;

  if (BaseType->IsPointerType()) {
    auto *StrippedDbgTy =
        HAKCTypeInfo::StripTypeModifiers(BaseType->GetDbgType());
    if (StrippedDbgTy->getTag() == dwarf::DW_TAG_pointer_type) {
      TypeToFind = dyn_cast<DIDerivedType>(StrippedDbgTy)->getBaseType();
    } else {
      TypeToFind = dyn_cast<DICompositeType>(StrippedDbgTy)->getBaseType();
    }

    if (!TypeToFind) {
      // BaseType is a pointer, but no known base type, so it is a void*
      CommonHAKCAnalysis::getLogger(Verbose) << "Adding missing void type\n";
      return GetVoidPointerPointeeType();
    }
  }

  if (!TypeToFind && !BaseType->GetDbgType()) {
    auto *LLVMTy = BaseType->GetLLVMType();
    if (auto *ArrayTy = dyn_cast<ArrayType>(LLVMTy)) {
      return FindType(ArrayTy->getElementType());
    }
  }

  if (!TypeToFind) {
    CommonHAKCAnalysis::getLogger(Verbose) << "Could not find PointeeType\n";
    return nullptr;
  }

  for (auto &it : TypesWithDebugInfo) {
    auto *DebugTy = it.first;
    if (DebugTy == TypeToFind) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Found PointeeType " << *it.second << "\n";
      return it.second;
    }
  }
  CommonHAKCAnalysis::getLogger(Verbose)
      << "PointeeType " << TypeToFind << " not found\n";
  return nullptr;
}

HAKCTypeP HAKCTypeIdentifier::GetArgumentHAKCType(Argument *Arg) {
  if (!Arg) {
    throw std::exception();
  }
  auto *F = Arg->getParent();
  if (AnalysisHelper.IsOutsideTransferFunc(F)) {
    F = AnalysisHelper.GetOriginalFunctionFromTransferFunction(F);
  }

  auto *DISubprog = F->getSubprogram();
  HAKCTypeP Result = nullptr;
  if (DISubprog) {
    Result = GetArgumentHAKCType(DISubprog->getType(), Arg->getArgNo());
  } else {
    Result = FindType(Arg->getType());
  }
  return Result;
}

HAKCTypeP
HAKCTypeIdentifier::GetArgumentHAKCType(const DISubroutineType *FunctionTy,
                                        unsigned ArgNo) {
  HAKCTypeP Result = nullptr;
  if (FunctionTy) {
    // The + 1 comes from the fact that the type array stores the return type
    // (including void, which is a null pointer) at index 0
    auto *ArgDIType = FunctionTy->getTypeArray()[ArgNo + 1];
    if (!ArgDIType) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Could not find argument " << ArgNo + 1 << " type in "
          << *FunctionTy << "\n";
      return nullptr;
    }
    Result = FindType(ArgDIType);
  }
  return Result;
}

HAKCTypeP HAKCTypeIdentifier::FindHAKCTypeForUse(Use &U) {
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Attempting to find HAKCTypeInfo for Use " << U << "\n";
  HAKCTypeP Result = nullptr;
  if (auto *CallI = dyn_cast<CallInst>(U.getUser())) {
    auto CallTy = FindCalledFunctionType(CallI->getFunctionType());
    if (CallTy) {
      if (U.getOperandNo() == CallI->getCalledOperandUse().getOperandNo()) {
        auto FuncTy =
            FindType(dyn_cast<DISubroutineType>(CallTy->GetDbgType()));
        Result = FindPointerType(*FuncTy);
      } else {
        Result = GetArgumentHAKCType(
            dyn_cast<DISubroutineType>(CallTy->GetDbgType()), U.getOperandNo());
      }
    }
  } else if (isa<LoadInst>(U.getUser())) {
    Result = FindHAKCType(U.getUser());
    if (Result) {
      if (isa<AllocaInst>(U.get())) {
        if (!Result->IsPointerToPointer()) {
          auto PointerType = FindPointerType(*Result);
          if (!PointerType) {
            Result = AddMissingPointerType(Result);
          }
        }
      } else {
        Result = FindPointerType(*Result);
      }
    }
  } else if (auto *GEPI = dyn_cast<GetElementPtrInst>(U.getUser())) {
    if (U.getOperandNo() == GetElementPtrInst::getPointerOperandIndex()) {
      auto BaseType = FindType(GEPI->getSourceElementType());
      if (BaseType) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Found BaseType " << *BaseType << " for "
            << *GEPI->getSourceElementType() << "\n";
        Result = FindPointerType(*BaseType);
      }
    } else {
      Result = FindType(U->getType());
    }
  } else if (auto *StoreI = dyn_cast<StoreInst>(U.getUser())) {
    Result = FindHAKCType(
        StoreI->getOperand((U.getOperandNo() + 1) % StoreI->getNumOperands()));
    if (Result) {
      if (U.getOperandNo() == StoreInst::getPointerOperandIndex()) {
        if (isa<AllocaInst>(U.get())) {
          if (!Result->IsPointerToPointer()) {
            auto PointerType = FindPointerType(*Result);
            if (!PointerType) {
              Result = AddMissingPointerType(Result);
            }
          }
        } else {
          Result = FindPointerType(*Result);
        }
      } else {
        Result = FindPointeeType(Result);
      }
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Found " << *Result << " for " << U.get() << "\n";
    }
  }

  return Result;
}

HAKCTypeP HAKCTypeIdentifier::AddMissingPointerType(const HAKCTypeP &BaseType) {
  if (BaseType->GetDbgTypeName().empty()) {
    auto Name = GetDbgName(*BaseType);
    BaseType->SetDbgTypeName(Name);
  }
  std::string AllocaName = BaseType->GetDbgTypeName().str();
  AllocaName += "*";
  for (auto MissingTy : TypesMissingDebugInfo) {
    if (MissingTy->GetDbgTypeName() == AllocaName) {
      return MissingTy;
    }
  }

  auto HAKCType =
      std::make_shared<HAKCTypeInfo>(AnalysisHelper, AllocaName, Debug);
  HAKCType->SetPointeeType(BaseType);
  HAKCType->SetDbgTypeName(AllocaName);
  HAKCType->SetLLVMType(
      PointerType::get(GetModule().getContext(),
                       HAKCType->GetLLVMType()
                           ? HAKCType->GetLLVMType()->getPointerAddressSpace()
                           : 0));
  TypesMissingDebugInfo.insert(HAKCType);

  return HAKCType;
}

HAKCTypeP HAKCTypeIdentifier::FindPointerType(const HAKCTypeInfo &BaseType) {
  std::set<dwarf::Tag> TagsToRecurseInto = {dwarf::DW_TAG_typedef,
                                            dwarf::DW_TAG_const_type};
  CommonHAKCAnalysis::getLogger(Verbose)
      << "Trying to find a pointer type to " << BaseType << "\n";

  for (auto &It : TypesWithDebugInfo) {
    auto *DebugType = It.first;
    if (DebugType->getTag() == dwarf::DW_TAG_pointer_type) {
      auto *DbgBaseTy = dyn_cast<DIDerivedType>(DebugType)->getBaseType();
      while (true) {
        if (DbgBaseTy && DbgBaseTy == BaseType.GetDbgType()) {
          CommonHAKCAnalysis::getLogger(Verbose)
              << "DbgBaseTy " << *DbgBaseTy << " equals "
              << BaseType.GetDbgType() << "\n";
          return It.second;
        }
        if (DbgBaseTy && TagsToRecurseInto.contains(DbgBaseTy->getTag())) {
          DbgBaseTy = dyn_cast<DIDerivedType>(DbgBaseTy)->getBaseType();
        } else {
          break;
        }
      }
    }
  }

  for (auto &AllocaType : TypesMissingDebugInfo) {
    if (!AllocaType->GetPointeeType()) {
      continue;
    }
    if (AllocaType->GetPointeeType()->GetDbgType() == BaseType.GetDbgType()) {
      return AllocaType;
    }

    if (AllocaType->GetPointeeType()->GetLLVMType() &&
        !AllocaType->GetPointeeType()->GetLLVMType()->isPointerTy() &&
        AllocaType->GetPointeeType()->GetLLVMType() == BaseType.GetLLVMType()) {
      return AllocaType;
    }
  }

  return nullptr;
}

HAKCTypeP
HAKCTypeIdentifier::FindTypeFromDebug(const DbgVariableRecord &DVR,
                                            Value *V) {
  auto *DITy = DVR.getVariable()->getType();
  int64_t FragmentSize = 0;
  auto IsOffset = DVR.getExpression()->extractIfOffset(FragmentSize);
  CommonHAKCAnalysis::getLogger(Debug)
      << "Finding type for " << DVR << " with Variable " << *DVR.getVariable()
      << " and DITy " << *DITy;
  if (IsOffset) {
    CommonHAKCAnalysis::getLogger(Debug)
        << " with Fragment size " << FragmentSize;
  }
  CommonHAKCAnalysis::getLogger(Debug)
      << "\n";
  if (DITy->getTag() == dwarf::DW_TAG_structure_type) {
    auto *CompositeTy = dyn_cast<DICompositeType>(DITy);
    if (FragmentSize) {
      for (auto *Member : CompositeTy->getElements()) {
        auto *CompositeMember = dyn_cast<DIDerivedType>(Member);
        if (!CompositeMember) {
          errs() << *Member << " of " << *DVR.getVariable() << " with type "
                 << *DITy << " is not a derived type\n";
          continue;
        }
        if (CompositeMember->getOffsetInBits() == FragmentSize) {
          CommonHAKCAnalysis::getLogger(Verbose)
              << "Found Member " << *CompositeMember << " at offset "
              << FragmentSize << " with BaseType ";
          if (!CompositeMember->getBaseType()) {
            CommonHAKCAnalysis::getLogger(Verbose)
                << "void*\n";
            return GetVoidPointerType();
          }
          DITy = CompositeMember->getBaseType();
          CommonHAKCAnalysis::getLogger(Verbose)
              << *DITy << "\n";
          break;
        }
      }
    }
  }

  auto FoundType = FindType(DITy);
  if (FoundType) {
    if (isa<AllocaInst>(V)) {
      auto PointerType = FindPointerType(*FoundType);
      if (!PointerType) {
        FoundType = AddMissingPointerType(FoundType);
      } else {
        FoundType = PointerType;
      }
    }
  }
  return FoundType;
}

HAKCTypeP HAKCTypeIdentifier::CheckCallUses(Value *V) {
  HAKCTypeP FoundType = nullptr;
  for (auto &U : V->uses()) {
    if (auto *Call = dyn_cast<CallInst>(U.getUser())) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Examining Call Site " << *Call << "\n";
      if (U.getOperandNo() == Call->getCalledOperandUse().getOperandNo()) {
        if (auto PointeeTy = FindType(Call->getFunctionType())) {
          FoundType = FindPointerType(*PointeeTy);
          if (!FoundType) {
            FoundType = AddMissingPointerType(PointeeTy);
          }
          if (FoundType) {
            goto exit;
          }
        }
      } else {
        auto FuncTy = FindCalledFunctionType(Call->getFunctionType());
        if (FuncTy && FuncTy->GetDbgType()) {
          auto *SubroutineTy = dyn_cast<DISubroutineType>(FuncTy->GetDbgType());
          FoundType = GetArgumentHAKCType(SubroutineTy, U.getOperandNo());
          if (FoundType) {
            goto exit;
          }
        }
      }
    }
  }

exit:
  return FoundType;
}

hakc::HAKCTypeP hakc::HAKCTypeIdentifier::FindHAKCType(Value *V) {
  HAKCTypeP FoundType = nullptr;
  SmallVector<DbgVariableIntrinsic *> DbgUsers;
  SmallVector<DbgVariableRecord *> DVRUsers;

  CommonHAKCAnalysis::getLogger(Verbose)
      << "Attempting to find HAKCTypeInfo for " << *V << "\n";

  if (const auto *GlobalVar = dyn_cast<GlobalVariable>(V)) {
    auto HAKCGlob = FindGlobal(GlobalVar, true);
    if (HAKCGlob && HAKCGlob->GetType()) {
      FoundType = HAKCGlob->GetType();
    }
  } else if (const auto *Func = dyn_cast<Function>(V)) {
    auto HAKCFunc = FindFunction(Func, true);
    if (HAKCFunc && HAKCFunc->GetType()) {
      auto FuncTy = HAKCFunc->GetType();
      FoundType = FindPointerType(*FuncTy);
      if (!FoundType) {
        FoundType = AddMissingPointerType(FuncTy);
      }
    }
  } else if (isa<Instruction>(V)) {
    findDbgUsers(DbgUsers, V, &DVRUsers);
    for (const auto *DVI : DbgUsers) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Examining Debug Intrinsic " << *DVI << "\n";
      if (isa<DbgDeclareInst>(DVI) ||
          (isa<DbgValueInst>(DVI) && !isa<DbgAssignIntrinsic>(DVI))) {
        FoundType = FindTypeFromDebug(DVI, V);
        if (FoundType) {
          goto exit;
        }
      }
    }
    for (auto *DVR : DVRUsers) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Examining Debug Record " << *DVR << "\n";
      FoundType = FindTypeFromDebug(*DVR, V);
      if (FoundType) {
        goto exit;
      }
    }
  }

  if (auto *LoadI = dyn_cast<LoadInst>(V)) {
    FoundType = CheckCallUses(LoadI);
    if (FoundType) {
      goto exit;
    }
    if (!getLoadStoreType(LoadI)->isPointerTy()) {
      SmallVector<HAKCTypeP> Types;
      auto *LoadTy = getLoadStoreType(LoadI);
      FindAllTypes(LoadTy, Types);
      for (auto HAKCTy : Types) {
        CommonHAKCAnalysis::getLogger(Verbose)
            << "Testing Load Type " << *HAKCTy << "\n";
        if (HAKCTy->GetDbgType()) {
          CommonHAKCAnalysis::getLogger(Verbose)
              << " with DIType " << *HAKCTy->GetDbgType() << "\n";
        }
        if (LoadTy->isIntegerTy()) {
          auto UsedAsPointer = AnalysisHelper.ValueIsUsedAsPointer(LoadI);
          if (UsedAsPointer && IsPointerLikeType(HAKCTy->GetDbgType())) {
            FoundType = HAKCTy;
          } else {
            FoundType = HAKCTy;
          }
        } else {
          FoundType = HAKCTy;
        }
      }
      if (FoundType) {
        goto exit;
      }
    }
    if (auto *PHI = dyn_cast<PHINode>(LoadI->getPointerOperand())) {
      for (auto &IncomingV : PHI->incoming_values()) {
        auto *Def = AnalysisHelper.getDef(IncomingV.get(), false);
        if (Def != LoadI) {
          if (auto PointeeType = FindHAKCType(LoadI->getPointerOperand())) {
            FoundType = FindPointeeType(PointeeType);
            if (FoundType) {
              goto exit;
            }
          }
        }
      }
    } else {
      if (auto PointeeType = FindHAKCType(LoadI->getPointerOperand())) {
        FoundType = FindPointeeType(PointeeType);
        if (FoundType) {
          goto exit;
        }
      }
    }
  }

  // We have no debug information for V here, so try our best to deduce
  if (isa<GetElementPtrInst>(V) || isa<GEPOperator>(V)) {
    Type *SourceType, *DestTy;
    Value *SourceValue;
    APInt ByteOffset(64, 0);
    bool FoundOffset = false;
    if (isa<GetElementPtrInst>(V)) {
      auto *GEPI = dyn_cast<GetElementPtrInst>(V);
      SourceType = GEPI->getSourceElementType();
      DestTy = GEPI->getResultElementType();
      SourceValue = GEPI->getPointerOperand();
      FoundOffset = GEPI->accumulateConstantOffset(GetModule().getDataLayout(),
                                                   ByteOffset);
    } else {
      auto *GEPO = dyn_cast<GEPOperator>(V);
      SourceType = GEPO->getSourceElementType();
      SourceValue = GEPO->getPointerOperand();
      DestTy = GEPO->getResultElementType();
      FoundOffset = GEPO->accumulateConstantOffset(GetModule().getDataLayout(),
                                                   ByteOffset);
    }
    if (FoundOffset) {
      auto SourceHAKCTy = FindHAKCType(SourceValue);
      if (SourceHAKCTy && SourceHAKCTy->GetPointeeType()) {
        if (isa_and_nonnull<DICompositeType>(
                SourceHAKCTy->GetPointeeType()->GetDbgType())) {
          auto *DICompositeTy = dyn_cast<DICompositeType>(
              SourceHAKCTy->GetPointeeType()->GetDbgType());
          for (auto *Element : DICompositeTy->getElements()) {
            if (Element->getTag() == dwarf::DW_TAG_member) {
              auto *Member = dyn_cast<DIDerivedType>(Element);
              if (Member->getOffsetInBits() / BITS_PER_BYTE ==
                  ByteOffset.getZExtValue()) {
                auto PointeeType = FindType(Member->getBaseType());
                FoundType = FindPointerType(*PointeeType);
                if (!FoundType) {
                  FoundType = AddMissingPointerType(PointeeType);
                }
                goto exit;
              }
            }
          }
        }
      }
    } else if (DestTy->isPointerTy() || DestTy->isArrayTy()) {
      CommonHAKCAnalysis::getLogger(Error)
          << "Could not determine type for GEP " << *V << "\n";
      FoundType = GetVoidPointerType();
      goto exit;
    }

    HAKCTypeP ElementType = FindType(SourceType);
    if (!ElementType) {
      ElementType = GetVoidPointerPointeeType();
    }
    FoundType = FindPointerType(*ElementType);
    if (!FoundType) {
      FoundType = AddMissingPointerType(ElementType);
    }
  } else if (auto *PHI = dyn_cast<PHINode>(V)) {
    for (auto &IncomingV : PHI->incoming_values()) {
      auto *Def = AnalysisHelper.getDef(IncomingV, false);

      if (isa<PHINode>(Def) || isa<ConstantPointerNull>(Def)) {
        continue;
      }

      FoundType = FindHAKCType(IncomingV);
      if (FoundType) {
        goto exit;
      }
    }
  }

  if (auto *Arg = dyn_cast<Argument>(V)) {
    FoundType = GetArgumentHAKCType(Arg);
  } else if (auto *Func = dyn_cast<Function>(V)) {
    if (Func->getSubprogram()) {
      const auto FuncTy = FindType(Func->getSubprogram()->getType());
      FoundType = FindPointerType(*FuncTy);
      if (!FoundType) {
        FoundType = AddMissingPointerType(FuncTy);
      }
    }
  } else if (auto *CallI = dyn_cast<CallInst>(V)) {
    if (CallI->getCalledFunction() &&
        CallI->getCalledFunction()->getSubprogram()) {
      const auto *SubprogramTy =
          CallI->getCalledFunction()->getSubprogram()->getType();
      const auto *ReturnTy = SubprogramTy->getTypeArray()[0];
      FoundType = FindType(ReturnTy);
    }
  } else if (auto *ZExtI = dyn_cast<ZExtInst>(V)) {
    FoundType = FindType(ZExtI->getType());
  } else if (isa<AllocaInst>(V)) {
    FoundType = CheckCallUses(V);
  }

exit:
  if (FoundType && FoundType->GetPointeeType() == nullptr) {
    if (FoundType->IsPointerType()) {
      if (auto PointeeType = FindPointeeType(FoundType)) {
        FoundType->SetPointeeType(PointeeType);
      }
    } else if (IsStructTypeThatStartsWithPointerLikeType(*FoundType)) {
      auto *FirstMemberType = GetFirstStructMemberType(
          dyn_cast<DICompositeType>(FoundType->GetDbgType()));
      auto PointeeType = FindType(FirstMemberType);
      FoundType->SetPointeeType(PointeeType);
    }
  }
  if (FoundType && V->getType()->isPointerTy() && !FoundType->IsPointerType()) {
    FoundType = FindPointerType(*FoundType);
  }

  if (FoundType) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Found HAKCTypeInfo\n"
        << *FoundType << "\nfor " << V << "\n";
  } else {
    CommonHAKCAnalysis::getLogger(Error)
        << "Cound not find HAKCTypeInfo for " << V << "\n";
  }

  return FoundType;
}

void HAKCTypeIdentifier::ModifyTypeUse(
    Function *F, const std::shared_ptr<HAKCTypeInfo> &HAKCTy, TypePerms perm) {

  if (!F) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Function is NULL!\n";
    throw std::exception();
  }
  auto subprog = F->getSubprogram();
  if (!subprog) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Subprog is null for " << *F << "\n";
    throw std::exception();
  }
  if (!functions.contains(subprog)) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "functions does not contain " << *F << "\n";
    throw std::exception();
  }
  auto FunctionP = functions[subprog];

  if (!HAKCTy) {
    CommonHAKCAnalysis::getLogger(Fatal)
        << "Trying to set permission of NULL HAKCTy\n";
    throw std::exception();
  }
  FunctionP->ModifyTypeUse(HAKCTy, perm);
}

std::shared_ptr<HAKCTypeInfo>
HAKCTypeIdentifier::FindCalledFunctionType(FunctionType *FunctionTy) const {

  if (!FunctionTy) {
    CommonHAKCAnalysis::getLogger(Fatal) << "Trying to find null FunctionTy\n";
    throw std::exception();
  }

  auto FoundType = FindType(FunctionTy);
  if (!FoundType) {
    for (auto &it : IndirectCallsTypes) {
      if (it.second->GetPointeeType()->GetLLVMType() == FunctionTy) {
        FoundType = it.second;
        break;
      }
    }
  }
  return FoundType;
}

std::shared_ptr<HAKCFunctionInfo>
HAKCTypeIdentifier::FindFunction(const Function *F, bool SearchUnmapped) {
  for (auto &it : functions) {
    if (it.second->GetFunction() == F) {
      return it.second;
    }
  }
  if (SearchUnmapped) {
    for (auto UnmappedFunction : UnmappedFunctions) {
      if (UnmappedFunction->GetFunction() == F) {
        return UnmappedFunction;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<HAKCGlobalInfo>
HAKCTypeIdentifier::FindGlobal(const GlobalVariable *GV,
                               bool SearchUnmapped) const {
  for (auto &it : globals) {
    if (it.second->GetGlobalVariable() == GV) {
      return it.second;
    }
  }
  if (SearchUnmapped) {
    for (auto UnmappedGlobal : UnmappedGlobals) {
      if (UnmappedGlobal->GetGlobalVariable() == GV) {
        return UnmappedGlobal;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<HAKCSymbolInfo>
HAKCTypeIdentifier::FindSymbol(Value *V, bool SearchUnmapped) {
  if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    return FindGlobal(GV, SearchUnmapped);
  }
  if (const auto *F = dyn_cast<Function>(V)) {
    return FindFunction(F, SearchUnmapped);
  }
  return nullptr;
}

HAKCSymbolP
HAKCTypeIdentifier::FindYamlSymbol(const HAKCYamlSymbol &YamlSymbol) {
  for (auto &it : globals) {
    if (YamlSymbol == *it.second) {
      return it.second;
    }
  }
  for (auto &Unmapped : UnmappedGlobals) {
    if (YamlSymbol == *Unmapped) {
      return Unmapped;
    }
  }
  for (auto &it : functions) {
    if (YamlSymbol == *it.second) {
      return it.second;
    }
  }
  for (auto &Unmapped : UnmappedFunctions) {
    if (YamlSymbol == *Unmapped) {
      return Unmapped;
    }
  }
  return nullptr;
}

void HAKCTypeIdentifier::FindTypesInFunctions() {
  for (auto &it : functions) {
    auto *F = it.second->GetFunction();
    auto SuppressOutput = !AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);

    CommonHAKCAnalysis::getLogger(Debug, SuppressOutput)
        << "Finding types in Function " << F->getName() << "\n";

    for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
      auto *I = &(*InstIt);
      if (auto *DbgIntrinsic = dyn_cast<DbgVariableIntrinsic>(I)) {
        auto *V = DbgIntrinsic->getVariableLocationOp(0);
        if (isa<UndefValue>(V)) {
          CommonHAKCAnalysis::getLogger(Debug, SuppressOutput)
              << "Skipping undef Value in Instruction " << *I << "\n";
          continue;
        }
        auto *DebugV = DbgIntrinsic->getVariable();
        CommonHAKCAnalysis::getLogger(Debug, SuppressOutput)
            << "Found " << *V << " to " << *DebugV
            << " mapping from Instruction " << *I << "\n";
        const auto HAKCType = FindType(DebugV->getType());
        if (!HAKCType) {
          CommonHAKCAnalysis::getLogger(Fatal)
              << "Could not find HAKCType for DIType " << *DebugV->getType()
              << "\n";
          throw std::exception();
        }
        auto *LLVMTy = V->getType();
        if (auto *Alloca = dyn_cast<AllocaInst>(V)) {
          LLVMTy = Alloca->getAllocatedType();
        } else if (auto *CallI = dyn_cast<CallInst>(V)) {
          if (CallI->isInlineAsm()) {
            /* Inline assembly causes too much type confusion, so skip these
             * mappings */
            CommonHAKCAnalysis::getLogger(Debug, SuppressOutput)
                << "Skipping inline assembly\n";
            continue;
          }
        }
        HAKCType->SetLLVMType(LLVMTy);
      } else if (auto *CallI = dyn_cast<CallInst>(I)) {
        if (CallI->isIndirectCall()) {
          HandleIndirectCall(CallI);
        }
      }
    }
  }
}

std::shared_ptr<HAKCTypeInfo>
HAKCTypeIdentifier::CreateNoDebugType(Type *Ty) const {
  std::string Name;
  llvm::raw_string_ostream sstream(Name);
  if (auto *StructTy = dyn_cast<StructType>(Ty)) {
    if (StructTy->hasName()) {
      sstream << StructTy->getName();
    } else {
      sstream << *Ty;
    }
  } else {
    sstream << *Ty;
  }

  auto HAKCType = std::make_shared<HAKCTypeInfo>(AnalysisHelper, Name, Debug);
  HAKCType->SetLLVMType(Ty);
  HAKCTypeP PointeeTy = nullptr;
  if (auto *ArrayTy = dyn_cast<ArrayType>(Ty)) {
    PointeeTy = FindType(ArrayTy->getElementType());
    if (!PointeeTy) {
      PointeeTy = CreateNoDebugType(ArrayTy->getElementType());
    }
  } else if (auto *PointerTy = dyn_cast<PointerType>(Ty)) {
    /* We can't know what this points to, so have it point to one byte */
    PointeeTy = FindType(IntegerType::getInt8Ty(Ty->getContext()));
  }
  HAKCType->SetPointeeType(PointeeTy);
  return HAKCType;
}

HAKCTypeP HAKCTypeIdentifier::HandleIndirectCall(CallInst *CallI) {
  if (IndirectCallsTypes.contains(CallI)) {
    return IndirectCallsTypes[CallI];
  }

  auto *FunctionTy = GetIndirectCallFunctionType(CallI);
  auto HAKCType = FindCalledFunctionType(FunctionTy);
  if (!HAKCType) {
    HAKCType = CreateNoDebugType(FunctionTy);
  }
  auto PointerType =
      CreateNoDebugType(PointerType::get(GetModule().getContext(), 0));
  PointerType->SetPointeeType(HAKCType);

  IndirectCallsTypes[CallI] = PointerType;
  return HAKCType;
}

HAKCTypeIdentifier::HAKCTypeIdentifier(CommonHAKCAnalysis &AnalysisHelper)
    : AnalysisHelper(AnalysisHelper), DbgInfoFinder(), TypesWithDebugInfo(),
      globals(), functions(), TypesMissingDebugInfo(), IndirectCallsTypes(),
      AnonymousTypes(), CompilationUnitScope(nullptr),
      IdentifiedStructTypes(
          AnalysisHelper.GetModule().getIdentifiedStructTypes()) {}

Module &HAKCTypeIdentifier::GetModule() const {
  return AnalysisHelper.GetModule();
}

ModuleAnalysisManager &HAKCTypeIdentifier::GetMAM() const {
  return AnalysisHelper.GetMAM();
}

void HAKCTypeIdentifier::ProcessDebugInfo() {
  // this is where the dag analysis actually happens!
  DbgInfoFinder.processModule(GetModule());
  // segfault on line below?
  CommonHAKCAnalysis::getLogger(Verbose) << GetModule() << "\n";

  StringRef ModulePath = GetModule().getSourceFileName();
  for (auto *Scope : DbgInfoFinder.scopes()) {
    if (auto *File = dyn_cast<DIFile>(Scope)) {
      auto Filename = File->getFilename();
      if (ModulePath.contains(Filename)) {
        CompilationUnitScope = Scope;
        break;
      }
    }
  }
  // HAKCWriter seems to die around here, for some reason
  if (!CompilationUnitScope) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Could not find Compilation Unit Scope\n";
  } else {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Found Compilation Unit Scope " << CompilationUnitScope << "\n";
  }

  CommonHAKCAnalysis::getLogger(Debug) << "!!!! Starting Type Handling !!!!\n";
  unsigned TypesProcessed = 0;
  for (auto *DITy : DbgInfoFinder.types()) {
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Processing Type " << ++TypesProcessed << " of "
        << DbgInfoFinder.type_count() << "\n";
    auto TypeP = HandleType(DITy);
  }
  CommonHAKCAnalysis::getLogger(Debug) << "!!!! Finished Type Handling !!!!\n";

  CommonHAKCAnalysis::getLogger(Debug)
      << "!!!! Starting Global Handling !!!!\n";
  for (auto *DIGlobal : DbgInfoFinder.global_variables()) {
    // The DIGlobal->getVariable seems to be null sometimes
    // if (!DIGlobal->getVariable()) {
    //   CommonHAKCAnalysis::getLogger() << "DIGlobal->getVariable() is
    //   NULL!\n"; throw std::exception();
    // }
    auto GlobalP = HandleGlobal(DIGlobal->getVariable());
  }
  CommonHAKCAnalysis::getLogger(Debug)
      << "!!!! Finished Global Handling !!!!\n";

  CommonHAKCAnalysis::getLogger(Debug)
      << "!!!! Starting Function Handling !!!!\n";
  for (auto *DISubProg : DbgInfoFinder.subprograms()) {
    auto SubProgP = HandleFunction(DISubProg);
  }
  CommonHAKCAnalysis::getLogger(Debug)
      << "!!!! Finished Function Handling !!!!\n";

  FindTypesInFunctions();
  FindUsesInGlobals();
  FindUsesInFunctions();

  for (auto &it : TypesWithDebugInfo) {
    auto HAKCType = it.second;
    HAKCTypeP PointeeType = nullptr;
    CommonHAKCAnalysis::getLogger(Verbose)
        << "Determining Pointee Type of " << *HAKCType << "\n";

    if (HAKCType->IsPointerType()) {
      PointeeType = FindPointeeType(HAKCType);
    }
    if (IsStructTypeThatStartsWithPointerLikeType(*HAKCType)) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << HAKCType->GetDbgType()
          << " is a struct type that starts with a pointer like type\n";
      auto *FirstMemberType =
          GetFirstStructMemberType(dyn_cast<DICompositeType>(
              HAKCTypeInfo::StripTypeModifiers(HAKCType->GetDbgType())));
      PointeeType = FindType(FirstMemberType);
    }

    if (PointeeType) {
      CommonHAKCAnalysis::getLogger(Verbose)
          << "Setting Pointee Type of " << *HAKCType << " to\n"
          << *PointeeType << "\n";
      HAKCType->SetPointeeType(PointeeType);
    }
  }
}

// need getters for globals and functions to move yaml output writing to
// module analysis!
std::map<const DIGlobalVariable *, HAKCGlobalP>
HAKCTypeIdentifier::GetGlobals() {
  return globals;
}

std::set<HAKCGlobalP> HAKCTypeIdentifier::GetUnmappedGlobals() {
  return UnmappedGlobals;
}

std::map<const DISubprogram *, HAKCFunctionP>
HAKCTypeIdentifier::GetFunctions() {
  return functions;
}

std::set<HAKCFunctionP> HAKCTypeIdentifier::GetUnmappedFunctions() {
  return UnmappedFunctions;
}

DebugInfoFinder HAKCTypeIdentifier::GetDbgInfoFinder() { return DbgInfoFinder; }

void HAKCTypeIdentifier::GetHAKCTypes(
    SmallVectorImpl<HAKCTypeP> &Results) const {
  for (auto &it : TypesWithDebugInfo) {
    Results.push_back(it.second);
  }
}

Type *HAKCTypeIdentifier::GetTypeFromString(StringRef TypeStr) const {
  if (TypeStr.empty()) {
    return nullptr;
  }

  if (TypeStr == "void") {
    /* parseType only allows parsing void types for functions so explicitly
     * check for that */
    return Type::getVoidTy(GetModule().getContext());
  }

  SMDiagnostic Err;
  auto *ParsedType = parseType(TypeStr, Err, GetModule());
  return ParsedType;
}

void HAKCTypeIdentifier::AddIgnoredType(StringRef TypeName) const {
  for (auto &it : TypesWithDebugInfo) {
    auto TyName = GetTypeName(it.first);
    if (TyName == TypeName) {
      it.second->SetIsIgnoredType(true);
      break;
    }
  }
}
HAKCTypeP HAKCTypeIdentifier::GetVoidPointerPointeeType() {
  auto Result =
      FindType(IntegerType::get(GetModule().getContext(), BITS_PER_BYTE));
  if (!Result) {
    // TODO: fix debugging true/false
    Result = std::make_shared<HAKCTypeInfo>(
        AnalysisHelper, "unsigned char", false);
    Result->SetDbgTypeName("unsigned char");
    Result->SetLLVMType(
        IntegerType::get(GetModule().getContext(), BITS_PER_BYTE));
    TypesMissingDebugInfo.insert(Result);
  }
  return Result;
}
HAKCTypeP HAKCTypeIdentifier::GetVoidPointerType() {
  auto VoidPointerTy = FindPointerType(*GetVoidPointerPointeeType());
  if (!VoidPointerTy) {
    VoidPointerTy = AddMissingPointerType(GetVoidPointerPointeeType());
  }
  return VoidPointerTy;
}
} // namespace llvm::hakc
