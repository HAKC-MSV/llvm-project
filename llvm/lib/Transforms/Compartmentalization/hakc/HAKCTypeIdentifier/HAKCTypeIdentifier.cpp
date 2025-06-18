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

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::FindType(const DIType *type) {
  if (!type) {
    CommonHAKCAnalysis::getWriter(true) << "Trying to find null type\n";
    throw std::exception();
  }
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo())
      << "Finding HAKCTypeInfo for " << *type << "\n";
  auto it = types.find(type);
  if (it == types.end()) {
    return nullptr;
  }
  return it->second;
}

void hakc::HAKCTypeIdentifier::AddTypeMapping(
    const DIType *type, const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo())
      << "Adding mapping " << *type << " -> " << HAKCType->GetName() << "\n";
  HAKCType->SetDbgType(type);
  auto DbgTypeName = GetTypeName(type);
  HAKCType->SetDbgTypeName(DbgTypeName);
  types[type] = HAKCType;
}

std::string hakc::HAKCTypeIdentifier::GetTypeName(const DIType *type) const {
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
          CommonHAKCAnalysis::getWriter(true)
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
      CommonHAKCAnalysis::getWriter(true) << "Unhandled DIDerivedType tag\n"
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
      CommonHAKCAnalysis::getWriter(true) << "Unhandled DICompositeType tag\n"
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
    CommonHAKCAnalysis::getWriter(true) << "Unhandled DIType\n" << type << "\n";
    throw std::exception();
  }

  return Name;
}

Type *hakc::HAKCTypeIdentifier::FindNamedType(StringRef TypeName) const {
  auto UnionName = "union." + TypeName;
  auto StructName = "struct." + TypeName;
  for (auto *StructTy : GetModule().getIdentifiedStructTypes()) {
    auto LLVMName = StructTy->getName();
    if (LLVMName.ends_with(UnionName.str()) ||
        LLVMName.ends_with(StructName.str())) {
      return StructTy;
    }
  }
  return nullptr;
}

Type *hakc::HAKCTypeIdentifier::FindAnonymousType(
    const DICompositeType *CompositeTy) {
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();

  auto Cached = AnonymousTypes.find(CompositeTy);
  if (Cached != AnonymousTypes.end()) {
    return Cached->second;
  }

  SmallVector<StructType *> FoundTypes;
  for (auto *StructTy : GetModule().getIdentifiedStructTypes()) {
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

  CommonHAKCAnalysis::getWriter(debug)
      << "Found " << FoundTypes.size() << " Types for " << CompositeTy << ":\n";
  for (auto *Ty : FoundTypes) {
    CommonHAKCAnalysis::getWriter(debug) << Ty << "\n";
  }
  Type *FoundType = nullptr;
  if (!FoundTypes.empty()) {
    FoundType = FoundTypes.front();
  }

  AnonymousTypes[CompositeTy] = FoundType;
  return FoundType;
}

Type *hakc::HAKCTypeIdentifier::GetLLVMType(const DIType *Ty) {
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo())
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

FunctionType *hakc::HAKCTypeIdentifier::GetLLVMFunctionTy(
    const DISubroutineType *FunctionTy) {
  auto &Ctx = GetModule().getContext();
  auto *ReturnTy = Type::getVoidTy(Ctx);
  auto TyArray = FunctionTy->getTypeArray();
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();
  if (TyArray[0]) {
    ReturnTy = GetLLVMType(TyArray[0]);
    if (!ReturnTy) {
      CommonHAKCAnalysis::getWriter(debug)
          << "Could not find Return Type " << TyArray[0] << "\n";
      return nullptr;
    }
  }
  if (!FunctionType::isValidReturnType(ReturnTy)) {
    CommonHAKCAnalysis::getWriter(debug)
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
      CommonHAKCAnalysis::getWriter(debug)
          << "Could not find LLVM Type for " << TyArray[i] << "\n";
      return nullptr;
    }
    if (!FunctionType::isValidArgumentType(Ty)) {
      CommonHAKCAnalysis::getWriter(true)
          << "Type " << Ty << " for DIType " << TyArray[i]
          << " is not a valid argument type\n";
      return nullptr;
    }
    ArgTys.push_back(Ty);
  }

  auto *LLVMTy = FunctionType::get(ReturnTy, ArgTys, IsVarArg);
  CommonHAKCAnalysis::getWriter(debug)
      << "Found LLVM Type " << LLVMTy << " for " << FunctionTy << "\n";

  return LLVMTy;
}

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::HandleType(const DIType *type) {
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();

  CommonHAKCAnalysis::getWriter(debug) << "Analyzing DIType " << *type << "\n";
  auto TypeP = FindType(type);
  if (TypeP) {
    CommonHAKCAnalysis::getWriter(debug) << "Already created " << *type << "\n";
    return TypeP;
  }
  if (isa<DICompositeType>(type) || isa<DISubroutineType>(type) ||
      isa<DIBasicType>(type)) {
    CommonHAKCAnalysis::getWriter(debug) << "Creating HAKCTypeInfo for\n"
                                         << type << "\n";
    auto TypeName = GetTypeName(type);
    TypeP = std::make_shared<HAKCTypeInfo>(AnalysisHelper, TypeName, debug);
    if (auto *BasicType = dyn_cast<DIBasicType>(type)) {
      auto *IntTy = IntegerType::get(GetModule().getContext(),
                                     BasicType->getSizeInBits());
      TypeP->SetLLVMType(IntTy);
    } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
      Type *LLVMTy = nullptr;
      if (CompositeTy->getTag() == dwarf::DW_TAG_structure_type ||
          CompositeTy->getTag() == dwarf::DW_TAG_union_type) {
        std::string SearchName = ".";
        SearchName += CompositeTy->getName();
        for (auto *StructTy : GetModule().getIdentifiedStructTypes()) {
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
        CommonHAKCAnalysis::getWriter(debug)
            << "Setting " << *TypeP << " LLVM Type to be " << *LLVMTy << "\n";
        TypeP->SetLLVMType(LLVMTy);
      }
    } else if (auto *SubRoutineTy = dyn_cast<DISubroutineType>(type)) {
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
  } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
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
        CommonHAKCAnalysis::getWriter(debug) << "Creating HAKCTypeInfo for\n"
                                             << type << "\n";
        auto TypeName = GetTypeName(type);
        TypeP = std::make_shared<HAKCTypeInfo>(AnalysisHelper, TypeName, debug);
        auto *LLVMTy = GetLLVMType(DerivedTy);
        if (LLVMTy) {
          CommonHAKCAnalysis::getWriter(debug)
              << "Found LLVM Type " << *LLVMTy << "\n";
          TypeP->SetLLVMType(LLVMTy);
        } else {
          CommonHAKCAnalysis::getWriter(debug)
              << "Could not find LLVM Type for " << type << "\n";
          for (auto *STy : GetModule().getIdentifiedStructTypes()) {
            CommonHAKCAnalysis::getWriter(debug) << STy << "\n";
          }
        }
        AddTypeMapping(type, TypeP);
      } else {
        CommonHAKCAnalysis::getWriter(debug)
            << "Not handling DITYpe " << type << "\n";
      }
    }
  }
  return TypeP;
}

GlobalVariable *
hakc::HAKCTypeIdentifier::FindGlobal(const DIGlobalVariable *DIGV) const {
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

std::shared_ptr<hakc::HAKCGlobalInfo>
hakc::HAKCTypeIdentifier::HandleGlobal(const DIGlobalVariable *DIGV) {
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(DIGV->getName());
  CommonHAKCAnalysis::getWriter(debug) << "Analyzing Global " << *DIGV << "\n";

  auto *GV = FindGlobal(DIGV);
  if (!GV) {
    CommonHAKCAnalysis::getWriter(debug)
        << "\nCould not find Global " << DIGV->getName() << "\n";
    return nullptr;
  }
  auto DIGVTy = FindType(DIGV->getType());
  if (!DIGVTy) {
    DIGVTy = HandleType(DIGV->getType());
    if (!DIGVTy) {
      CommonHAKCAnalysis::getWriter(true)
          << "Unable to handle DIType " << *DIGV->getType() << " for Global "
          << *DIGV << "\n";
      throw std::exception();
    }
  }
  if (!DIGVTy->GetLLVMType()) {
    DIGVTy->SetLLVMType(GV->getValueType());
  }

  auto GVP =
      std::make_shared<HAKCGlobalInfo>(AnalysisHelper, GV->getName(), debug);
  GVP->SetType(DIGVTy);
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

void hakc::HAKCTypeIdentifier::AddGlobalMapping(
    const DIGlobalVariable *DIGV,
    const std::shared_ptr<HAKCGlobalInfo> &HAKCSymbol) {
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo(DIGV->getName()))
      << "Adding mapping " << *DIGV << " -> " << HAKCSymbol->GetName() << "\n";
  globals[DIGV] = HAKCSymbol;
}

std::shared_ptr<hakc::HAKCFunctionInfo>
hakc::HAKCTypeIdentifier::HandleFunction(const DISubprogram *SubProg) {
  auto debug =
      AnalysisHelper.GetSystemInfo().OutputDebugInfo(SubProg->getName());

  CommonHAKCAnalysis::getWriter(debug)
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
    CommonHAKCAnalysis::getWriter(debug)
        << "\nCould not find Function " << SubProg->getName() << "\n";
    return nullptr;
  }
  if (F->getSubprogram() != SubProg) {
    CommonHAKCAnalysis::getWriter(debug)
        << *F << " SubProgram does equal " << *SubProg << "\n";
  } else {
    CommonHAKCAnalysis::getWriter(debug)
        << F->getSubprogram() << " == " << SubProg << "\n";
  }

  if (CommonHAKCAnalysis::IsOutsideTransferFunc(F) || F->isIntrinsic()) {
    CommonHAKCAnalysis::getWriter(debug)
        << SubProg->getName() << " is a HAKC Transfer function\n";
    return nullptr;
  }
  auto DIGVTy = FindType(SubProg->getType());
  if (!DIGVTy) {
    CommonHAKCAnalysis::getWriter(true)
        << GetModule() << "Could not find HAKCType of " << F->getName()
        << " with DIType " << *SubProg->getType() << "\n";
    throw std::exception();
  }

  if (!DIGVTy->GetLLVMType()) {
    DIGVTy->SetLLVMType(F->getFunctionType());
  }
  auto FP = std::make_shared<HAKCFunctionInfo>(AnalysisHelper,
                                               SubProg->getName(), debug);
  FP->SetType(DIGVTy);
  FP->SetFunction(F);
  FP->SetDefiningLocation(SubProg->getFile(), SubProg->getLine());
  if (SubProg->isLocalToUnit()) {
    FP->SetLocalScope(SubProg->getScope());
  }
  AddFunctionMapping(SubProg, FP);

  return FP;
}

void hakc::HAKCTypeIdentifier::AddFunctionMapping(
    const DISubprogram *SubProg,
    const std::shared_ptr<HAKCFunctionInfo> &HAKCFunction) {
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo(SubProg->getName()))
      << "Adding mapping " << *SubProg << " -> " << *HAKCFunction << "\n";
  functions[SubProg] = HAKCFunction;
}

void hakc::HAKCTypeIdentifier::FindAllGlobalsUsed(
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

std::shared_ptr<hakc::HAKCFunctionInfo>
hakc::HAKCTypeIdentifier::AddNoDebugFunction(Function *F) {
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);

  for (const auto &UnmappedFunc : UnmappedFunctions) {
    if (UnmappedFunc->GetFunction() == F) {
      return UnmappedFunc;
    }
  }
  CommonHAKCAnalysis::getWriter(debug)
      << "Adding unmapped Function " << F->getName() << "\n";
  auto FuncInfo =
      std::make_shared<HAKCFunctionInfo>(AnalysisHelper, F->getName(), debug);
  FuncInfo->SetFunction(F);
  FuncInfo->SetLocalScope(CompilationUnitScope);
  auto HAKCType = FindCalledFunctionType(F->getFunctionType());
  if (!HAKCType) {
    HAKCType = CreateNoDebugType(F->getFunctionType());
  }

  CommonHAKCAnalysis::getWriter(debug)
      << "HAKCType exists for " << F->getName() << "\n";

  if (!HAKCType->GetLLVMType()) {
    HAKCType->SetLLVMType(F->getFunctionType());
  }
  FuncInfo->SetType(HAKCType);
  UnmappedFunctions.insert(FuncInfo);
  return FuncInfo;
}

std::shared_ptr<hakc::HAKCSymbolInfo>
hakc::HAKCTypeIdentifier::AddNoDebugGlobal(GlobalObject *GlobalObj) {
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(GlobalObj);
  if (auto *F = dyn_cast<Function>(GlobalObj)) {
    return AddNoDebugFunction(F);
  } else if (auto *GV = dyn_cast<GlobalVariable>(GlobalObj)) {
    for (auto UnmappedGlobal : UnmappedGlobals) {
      if (UnmappedGlobal->GetGlobalVariable() == GV) {
        return UnmappedGlobal;
      }
    }
    CommonHAKCAnalysis::getWriter(debug)
        << "Adding unmapped Global Variable " << GV->getName() << "\n";
    auto GlobalInfo = std::make_shared<HAKCGlobalInfo>(
        AnalysisHelper, GlobalObj->getName(), debug);
    auto HAKCType = FindType(GlobalObj->getValueType());
    if (!HAKCType) {
      HAKCType = CreateNoDebugType(GlobalObj->getValueType());
    }
    CommonHAKCAnalysis::getWriter(debug)
        << "HAKCType exists for " << GV->getName() << "\n";
    if (!HAKCType->GetLLVMType()) {
      HAKCType->SetLLVMType(GV->getValueType());
    }
    GlobalInfo->SetGlobalVariable(GV);
    GlobalInfo->SetType(HAKCType);
    GlobalInfo->SetLocalScope(CompilationUnitScope);

    UnmappedGlobals.insert(GlobalInfo);
    return GlobalInfo;
  } else {
    CommonHAKCAnalysis::getWriter(true)
        << "Unsupported GlobalObj: " << *GlobalObj << "\n";
    throw std::exception();
  }
}

void hakc::HAKCTypeIdentifier::AddUsedGlobals(
    const std::set<GlobalObject *> &GlobalObjects,
    const std::shared_ptr<hakc::HAKCSymbolInfo> &UserSymbol) {
  for (auto *UsedGlobal : GlobalObjects) {
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(UsedGlobal);
    auto Symbol = FindSymbol(UsedGlobal);
    if (!Symbol) {
      CommonHAKCAnalysis::getWriter(debug)
          << "\nGlobal " << UsedGlobal->getName() << " is used in "
          << UserSymbol->GetGlobalObj()->getName()
          << " but the Symbol could not be found\n";
      Symbol = AddNoDebugGlobal(UsedGlobal);
    }
    CommonHAKCAnalysis::getWriter(debug)
        << "Found Symbol " << Symbol->GetName() << "\n";
    if (Symbol) {
      UserSymbol->AddSymbolUse(Symbol);
    }
  }
}

void hakc::HAKCTypeIdentifier::FindUsesInGlobals() {
  for (auto &it : globals) {
    auto *GV = it.second->GetGlobalVariable();

    if (GV->hasInitializer()) {
      CommonHAKCAnalysis::getWriter(
          AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV))
          << "Searching for globals in " << *GV << "\n";
      std::set<GlobalObject *> GlobalsUsed;
      FindAllGlobalsUsed(GV->getInitializer(), GlobalsUsed);
      AddUsedGlobals(GlobalsUsed, it.second);
    }
  }
}

void hakc::HAKCTypeIdentifier::CreateIndirectCallSourceLink(
    Value *V, std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path) {
  if (auto *Arg = dyn_cast<Argument>(V)) {
    auto HAKCType = FindType(Arg->getType());
    if (!HAKCType) {
      CommonHAKCAnalysis::getWriter(
          AnalysisHelper.GetSystemInfo().OutputDebugInfo(Arg->getParent()))
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
      CommonHAKCAnalysis::getWriter(
          AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV))
          << "Could not find HAKC Symbol for " << *GV << "\n";
      return;
    }
    auto Link = std::make_shared<HAKCIndirectCallSourceLink>(
        HAKCSymbol, AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV));
    Path.push_back(Link);
  } else if (auto *Load = dyn_cast<LoadInst>(V)) {
    auto *Pointer = Load->getPointerOperand();
    auto debug =
        AnalysisHelper.GetSystemInfo().OutputDebugInfo(Load->getFunction());
    if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
      auto *TyToCheck = GEP->getSourceElementType();
      auto HAKCType = FindType(TyToCheck);
      if (HAKCType /*&& GEP->hasAllConstantIndices()*/) {
        APInt Offset(64, 0);
        GEP->stripAndAccumulateInBoundsConstantOffsets(
            GetModule().getDataLayout(), Offset);
        CommonHAKCAnalysis::getWriter(debug)
            << "Offset in bits for " << *GEP << ": " << Offset.getZExtValue()
            << "\n";
        if (!HAKCType->GetLLVMType()) {
          HAKCType->SetLLVMType(TyToCheck);
        }
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(
            HAKCType, Offset.getSExtValue(), debug);
        Path.push_back(Link);
        CreateIndirectCallSourceLink(GEP->getPointerOperand(), Path);
      }
      if (!GEP->hasAllConstantIndices()) {
        CommonHAKCAnalysis::getWriter(debug)
            << "GEP does not have all constant indices: " << *GEP << "\n";
      } else {
        CommonHAKCAnalysis::getWriter(debug)
            << "Could not find Load Pointer HAKC Type for " << *TyToCheck
            << "\n";
      }
    } else if (auto *GVal = dyn_cast<GlobalValue>(Pointer)) {
      auto HAKCSymbol = FindSymbol(GVal, true);
      if (!HAKCSymbol) {
        CommonHAKCAnalysis::getWriter(debug)
            << "Unable to find Global " << GVal->getName() << "\n";
        return;
      }
      auto Link =
          std::make_shared<HAKCIndirectCallSourceLink>(HAKCSymbol, debug);
      Path.push_back(Link);
    } else {
      CommonHAKCAnalysis::getWriter(debug)
          << "Unhandled Load Pointer Operand type: " << *Pointer << "\n";
      auto *LoadTy = Load->getPointerOperand()->getType();
      auto HAKCType = FindType(LoadTy);
      if (HAKCType) {
        auto Link =
            std::make_shared<HAKCIndirectCallSourceLink>(HAKCType, 0, debug);
        Path.push_back(Link);
      }
    }
  } else if (auto *IntToPtr = dyn_cast<IntToPtrInst>(V)) {
    CommonHAKCAnalysis::getWriter(
        AnalysisHelper.GetSystemInfo().OutputDebugInfo(IntToPtr->getFunction()))
        << "Adding IntToPtr Link\n";
    CreateIndirectCallSourceLink(IntToPtr->getOperand(0), Path);
  } else if (auto *CallI = dyn_cast<CallInst>(V)) {
    CommonHAKCAnalysis::getWriter(
        AnalysisHelper.GetSystemInfo().OutputDebugInfo(CallI->getFunction()))
        << "Adding Call Link\n";
    CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
  } else {
    CommonHAKCAnalysis::getWriter(
        AnalysisHelper.GetSystemInfo().OutputDebugInfo())
        << "Unhandled Link type: " << V << "\n";
  }
}

void hakc::HAKCTypeIdentifier::FindIndirectCallSource(
    CallInst *CallI,
    std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path) {
  if (!CallI->isIndirectCall()) {
    CommonHAKCAnalysis::getWriter(true)
        << *CallI << " is not an indirect call\n";
    throw std::exception();
  }

  CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo(CallI->getFunction()))
      << "Found " << Path.size() << " path links for "
      << *CallI->getCalledOperand() << "\n";
}

FunctionType *
hakc::HAKCTypeIdentifier::GetIndirectCallFunctionType(const CallInst *CallI) {
  if (!CallI->isIndirectCall()) {
    CommonHAKCAnalysis::getWriter(true)
        << "Trying to get type from a Call that is not an indirect call\n";
    throw std::exception();
  }
  return CallI->getFunctionType();
}

void hakc::HAKCTypeIdentifier::FindUsesInFunctions() {
  for (auto &it : functions) {
    auto *F = it.second->GetFunction();
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);
    CommonHAKCAnalysis::getWriter(debug)
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
            CommonHAKCAnalysis::getWriter(debug)
                << "Could not find HAKC Symbol for Function "
                << Call->getCalledFunction()->getName() << "\n";
            FoundFunction = AddNoDebugFunction(Call->getCalledFunction());
          }
          it.second->AddDirectCall(FoundFunction);
        } else if (Call->isIndirectCall()) {
          auto *FunctionTy = GetIndirectCallFunctionType(Call);
          CommonHAKCAnalysis::getWriter(debug)
              << "Source of indirect call operand in Function " << F->getName()
              << ": " << AnalysisHelper.getDef(Call->getCalledOperand(), true)
              << "\n";
          auto HAKCType = FindCalledFunctionType(FunctionTy);
          if (!HAKCType) {
            CommonHAKCAnalysis::getWriter(true)
                << "Could not find called HAKCType for " << *Call
                << " with Searched Type " << *FunctionTy << " in Function "
                << F->getName() << "\n";
            HAKCType = FindType(Call->getCalledOperand()->getType());
            if (HAKCType) {
              CommonHAKCAnalysis::getWriter(true)
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
              SourcePath, HAKCType, debug);
          it.second->AddIndirectCall(Source);
        }
      }
    }
  }
}

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::FindType(Type *Ty) {
  // remove this functionality, because it would only work for non pointers and
  // is logically incorrect
  if (isa<PointerType>(Ty)) {
    return nullptr;
  }
  auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();
  CommonHAKCAnalysis::getWriter(debug)
      << "Trying to find HAKCTypeInfo for " << *Ty << "\n";

  for (auto &it : types) {
    if (debug && it.second->GetLLVMType()) {
      CommonHAKCAnalysis::getWriter(debug)
          << "Comparing " << it.second->GetLLVMType() << " with " << Ty << "\n";
    }
    if (it.second->GetLLVMType() && it.second->GetLLVMType() == Ty) {
      return it.second;
    }
  }

  return nullptr;
}

hakc::HAKCTypeP
hakc::HAKCTypeIdentifier::FindType(HAKCPointerBase &HAKCPointer) {
  HAKCTypeP ReturnTy = nullptr;
  if (HAKCPointer.GetType()) {
    ReturnTy = HAKCPointer.GetType();
  } else {
    ReturnTy = FindHAKCType(HAKCPointer.GetBaseDefinition());
  }
  if (ReturnTy) {
    if (ReturnTy->IsPointerType() && !ReturnTy->GetPointeeType()) {
      if (auto PointeeType = FindPointeeType(*ReturnTy)) {
        ReturnTy->SetPointeeType(PointeeType);
      }
    }
    HAKCPointer.SetType(ReturnTy);
  }
  return ReturnTy;
}

hakc::HAKCTypeP
hakc::HAKCTypeIdentifier::FindPointeeType(HAKCPointerBase &HAKCPointer) {
  if (HAKCPointer.GetType() && HAKCPointer.GetType()->GetPointeeType()) {
    return HAKCPointer.GetType()->GetPointeeType();
  }

  HAKCTypeP BaseType = HAKCPointer.GetType();
  HAKCTypeP PointeeType = nullptr;
  if (!BaseType) {
    BaseType = FindType(HAKCPointer);
  }

  if (BaseType) {
    PointeeType = FindPointeeType(*BaseType);
    BaseType->SetPointeeType(PointeeType);
  }

  return PointeeType;
}

hakc::HAKCTypeP
hakc::HAKCTypeIdentifier::FindPointeeType(const HAKCTypeInfo &BaseType) {
  if (BaseType.GetPointeeType()) {
    return BaseType.GetPointeeType();
  }

  auto Debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();
  CommonHAKCAnalysis::getWriter(Debug)
      << "Finding Pointee Type for " << BaseType << "\n";
  DIType *TypeToFind = nullptr;
  if (BaseType.GetDbgType() &&
      (BaseType.GetDbgType()->getTag() == dwarf::DW_TAG_pointer_type ||
       BaseType.GetDbgType()->getTag() == dwarf::DW_TAG_array_type)) {
    if (BaseType.GetDbgType()->getTag() == dwarf::DW_TAG_pointer_type) {
      TypeToFind =
          dyn_cast<DIDerivedType>(BaseType.GetDbgType())->getBaseType();
    } else {
      TypeToFind =
          dyn_cast<DICompositeType>(BaseType.GetDbgType())->getBaseType();
    }
  }
  if (!TypeToFind) {
    CommonHAKCAnalysis::getWriter(Debug) << "Could not find PointeeType\n";
    return nullptr;
  }

  for (auto &it : types) {
    auto *DebugTy = it.first;
    if (DebugTy == TypeToFind) {
      CommonHAKCAnalysis::getWriter(Debug)
          << "Found PointeeType " << *it.second << "\n";
      return it.second;
    }
  }
  CommonHAKCAnalysis::getWriter(Debug)
      << "PointeeType " << TypeToFind << " not found\n";
  return nullptr;
}

hakc::HAKCTypeP hakc::HAKCTypeIdentifier::GetArgumentHAKCType(Argument *Arg) {
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

hakc::HAKCTypeP hakc::HAKCTypeIdentifier::GetArgumentHAKCType(
    const DISubroutineType *FunctionTy, unsigned ArgNo) {
  HAKCTypeP Result = nullptr;
  if (FunctionTy) {
    // The + 1 comes from the fact that the type array stores the return type
    // (including void, which is a null pointer) at index 0
    auto *ArgDIType = FunctionTy->getTypeArray()[ArgNo + 1];
    Result = FindType(ArgDIType);
  }
  return Result;
}

hakc::HAKCTypeP hakc::HAKCTypeIdentifier::FindHAKCTypeForUse(Use &U) {
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo())
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
      auto PointerTy = FindPointerType(*Result);
      if (!PointerTy && isa<AllocaInst>(U.get())) {
        Result = AddAllocaType(Result);
      } else {
        Result = PointerTy;
      }
    }
  } else if (auto *GEPI = dyn_cast<GetElementPtrInst>(U.getUser())) {
    if (U.getOperandNo() == GetElementPtrInst::getPointerOperandIndex()) {
      auto BaseType = FindType(GEPI->getSourceElementType());
      if (BaseType) {
        CommonHAKCAnalysis::getWriter(
            AnalysisHelper.GetSystemInfo().OutputDebugInfo())
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
        auto PointerType = FindPointerType(*Result);
        if (!PointerType && isa<AllocaInst>(U.get())) {
          Result = AddAllocaType(Result);
        } else {
          Result = PointerType;
        }
      } else {
        Result = FindPointeeType(*Result);
      }
      CommonHAKCAnalysis::getWriter(
          AnalysisHelper.GetSystemInfo().OutputDebugInfo())
          << "Found " << *Result << " for " << U.get() << "\n";
    }
  }

  return Result;
}

hakc::HAKCTypeP
hakc::HAKCTypeIdentifier::AddAllocaType(const HAKCTypeP &BaseType) {
  std::string AllocaName = BaseType->GetDbgTypeName().str();
  AllocaName += "*";
  auto HAKCType = std::make_shared<HAKCTypeInfo>(
      AnalysisHelper, AllocaName,
      AnalysisHelper.GetSystemInfo().OutputDebugInfo());
  HAKCType->SetPointeeType(BaseType);
  HAKCType->SetLLVMType(
      PointerType::get(GetModule().getContext(),
                       HAKCType->GetLLVMType()
                           ? HAKCType->GetLLVMType()->getPointerAddressSpace()
                           : 0));
  AllocaTypes.insert(HAKCType);

  return HAKCType;
}

hakc::HAKCTypeP
hakc::HAKCTypeIdentifier::FindPointerType(const HAKCTypeInfo &BaseType) {
  for (auto &It : types) {
    auto *DebugType = It.first;
    if (DebugType->getTag() == dwarf::DW_TAG_pointer_type) {
      if (dyn_cast<DIDerivedType>(DebugType)->getBaseType() ==
          BaseType.GetDbgType()) {
        return It.second;
      }
    }
  }

  for (auto &AllocaType : AllocaTypes) {
    if (AllocaType->GetPointeeType()->GetDbgType() == BaseType.GetDbgType()) {
      return AllocaType;
    }
  }

  return nullptr;
}

hakc::HAKCTypeP hakc::HAKCTypeIdentifier::FindHAKCType(Value *V) {
  HAKCTypeP FoundType = nullptr;
  Type *BaseType = nullptr;
  CommonHAKCAnalysis::getWriter(
      AnalysisHelper.GetSystemInfo().OutputDebugInfo())
      << "Attempting to find HAKCTypeInfo for " << *V << "\n";

  auto Declarations = llvm::findDVRDeclares(V);
  for (auto *Declaration : Declarations) {
    auto *DbgLocalVar = Declaration->getVariable();
    if (DbgLocalVar) {
      auto *DbgType = DbgLocalVar->getType();
      if (DbgType) {
        auto HAKCTy = FindType(DbgType);
        if (HAKCTy) {
          return HAKCTy;
        }
      }
    }
  }

  if (isa<LoadInst>(V) || isa<StoreInst>(V)) {
    BaseType = getLoadStoreType(V);
  } else if (isa<GetElementPtrInst>(V)) {
    BaseType = dyn_cast<GetElementPtrInst>(V)->getSourceElementType();
  } else if (auto *Arg = dyn_cast<Argument>(V)) {
    FoundType = GetArgumentHAKCType(Arg);
  } else if (auto *AllocaI = dyn_cast<AllocaInst>(V)) {
    BaseType = AllocaI->getAllocatedType();
  }
  // maybe remove below
  else if (auto *GlobalVar = dyn_cast<GlobalVariable>(V)) {
    auto HAKCGlob = FindGlobal(GlobalVar, true);
    if (HAKCGlob && HAKCGlob->GetType()) {
      FoundType = HAKCGlob->GetType();
    } else {
      BaseType = GlobalVar->getValueType();
    }
  } else if (auto *Func = dyn_cast<Function>(V)) {
    BaseType = Func->getReturnType();
  } else if (auto *CallI = dyn_cast<CallInst>(V)) {
    BaseType = CallI->getType();
  }
  // maybe remove above

  if (BaseType) {
    CommonHAKCAnalysis::getWriter(
        AnalysisHelper.GetSystemInfo().OutputDebugInfo())
        << "Using BaseType " << *BaseType << "\n";
    if (!isa<PointerType>(BaseType)) {
      FoundType = FindType(BaseType);
    } else {
      for (auto &U : V->uses()) {
        CommonHAKCAnalysis::getWriter(
            AnalysisHelper.GetSystemInfo().OutputDebugInfo())
            << "Finding Type of " << U << "\n";
        if (auto BaseHAKCType = FindHAKCTypeForUse(U)) {
          CommonHAKCAnalysis::getWriter(
              AnalysisHelper.GetSystemInfo().OutputDebugInfo())
              << "Found " << *BaseHAKCType << " for " << U << "\n";
          FoundType = BaseHAKCType;
          break;
        }
      }
    }
  }

  if (FoundType && FoundType->IsPointerType() &&
      FoundType->GetPointeeType() == nullptr) {
    if (auto PointeeType = FindPointeeType(*FoundType)) {
      FoundType->SetPointeeType(PointeeType);
    }
  }

  if (FoundType) {
    CommonHAKCAnalysis::getWriter(
        AnalysisHelper.GetSystemInfo().OutputDebugInfo())
        << "Found HAKCTypeInfo\n"
        << *FoundType << "\nfor " << V << "\n";
  } else {
    CommonHAKCAnalysis::getWriter(
        AnalysisHelper.GetSystemInfo().OutputDebugInfo())
        << "Cound not find HAKCTypeInfo for " << V << "\n";
  }

  return FoundType;
}

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::FindCalledFunctionType(FunctionType *FunctionTy) {
  if (!FunctionTy) {
    CommonHAKCAnalysis::getWriter(true) << "Trying to find null FunctionTy\n";
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

std::shared_ptr<hakc::HAKCFunctionInfo>
hakc::HAKCTypeIdentifier::FindFunction(const Function *F, bool SearchUnmapped) {
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

std::shared_ptr<hakc::HAKCGlobalInfo>
hakc::HAKCTypeIdentifier::FindGlobal(const GlobalVariable *GV,
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

std::shared_ptr<hakc::HAKCSymbolInfo>
hakc::HAKCTypeIdentifier::FindSymbol(Value *V, bool SearchUnmapped) {
  if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    return FindGlobal(GV, SearchUnmapped);
  }
  if (const auto *F = dyn_cast<Function>(V)) {
    return FindFunction(F, SearchUnmapped);
  }
  return nullptr;
}

hakc::HAKCSymbolP hakc::HAKCTypeIdentifier::FindYamlSymbol(
    const hakc::HAKCYamlSymbol &YamlSymbol) {
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

void hakc::HAKCTypeIdentifier::FindTypesInFunctions() {
  for (auto &it : functions) {
    auto *F = it.second->GetFunction();
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);

    CommonHAKCAnalysis::getWriter(debug)
        << "Finding types in Function " << F->getName() << "\n";
    for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
      auto *I = &(*InstIt);
      if (auto *DbgIntrinsic = dyn_cast<DbgVariableIntrinsic>(I)) {
        auto *V = DbgIntrinsic->getVariableLocationOp(0);
        if (isa<UndefValue>(V)) {
          CommonHAKCAnalysis::getWriter(debug)
              << "Skipping undef Value in Instruction " << *I << "\n";
          continue;
        }
        auto *DebugV = DbgIntrinsic->getVariable();
        CommonHAKCAnalysis::getWriter(debug)
            << "Found " << *V << " to " << *DebugV
            << " mapping from Instruction " << *I << "\n";
        const auto HAKCType = FindType(DebugV->getType());
        if (!HAKCType) {
          CommonHAKCAnalysis::getWriter(true)
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
            CommonHAKCAnalysis::getWriter(debug)
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

std::shared_ptr<hakc::HAKCTypeInfo>
hakc::HAKCTypeIdentifier::CreateNoDebugType(Type *Ty) const {
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

  auto HAKCType = std::make_shared<HAKCTypeInfo>(
      AnalysisHelper, Name, AnalysisHelper.GetSystemInfo().OutputDebugInfo());
  HAKCType->SetLLVMType(Ty);
  return HAKCType;
}

hakc::HAKCTypeP hakc::HAKCTypeIdentifier::HandleIndirectCall(CallInst *CallI) {
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

hakc::HAKCTypeIdentifier::HAKCTypeIdentifier(CommonHAKCAnalysis &AnalysisHelper)
    : AnalysisHelper(AnalysisHelper), DbgInfoFinder(), types(), globals(),
      functions(), AllocaTypes(), IndirectCallsTypes(), AnonymousTypes(),
      CompilationUnitScope(nullptr) {}

Module &hakc::HAKCTypeIdentifier::GetModule() const {
  return AnalysisHelper.GetModule();
}

void hakc::HAKCTypeIdentifier::ProcessDebugInfo() {
  DbgInfoFinder.processModule(GetModule());
  auto Debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();

  CommonHAKCAnalysis::getWriter(Debug) << GetModule() << "\n";

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

  if (!CompilationUnitScope) {
    CommonHAKCAnalysis::getWriter(Debug)
        << "Could not find Compilation Unit Scope\n";
  } else {
    CommonHAKCAnalysis::getWriter(Debug)
        << "Found Compilation Unit Scope " << CompilationUnitScope << "\n";
  }

  CommonHAKCAnalysis::getWriter(Debug) << "!!!! Starting Type Handling !!!!\n";
  unsigned TypesProcessed = 0;
  for (auto *DITy : DbgInfoFinder.types()) {
    CommonHAKCAnalysis::getWriter(Debug)
        << "Processing Type " << ++TypesProcessed << " of "
        << DbgInfoFinder.type_count() << "\n";
    auto TypeP = HandleType(DITy);
  }
  CommonHAKCAnalysis::getWriter(Debug) << "!!!! Finished Type Handling !!!!\n";

  CommonHAKCAnalysis::getWriter(Debug)
      << "!!!! Starting Global Handling !!!!\n";
  for (auto *DIGlobal : DbgInfoFinder.global_variables()) {
    auto GlobalP = HandleGlobal(DIGlobal->getVariable());
  }
  CommonHAKCAnalysis::getWriter(Debug)
      << "!!!! Finished Global Handling !!!!\n";

  CommonHAKCAnalysis::getWriter(Debug)
      << "!!!! Starting Function Handling !!!!\n";
  for (auto *DISubProg : DbgInfoFinder.subprograms()) {
    auto SubProgP = HandleFunction(DISubProg);
  }
  CommonHAKCAnalysis::getWriter(Debug)
      << "!!!! Finished Function Handling !!!!\n";

  FindTypesInFunctions();
  FindUsesInGlobals();
  FindUsesInFunctions();

  for (auto &it : types) {
    auto HAKCType = it.second;
    if (HAKCType->IsPointerType()) {
      if (auto PointeeType = FindPointeeType((*HAKCType))) {
        HAKCType->SetPointeeType(PointeeType);
      }
    }
  }
}

void hakc::HAKCTypeIdentifier::OutputYAML(raw_ostream &out) const {
  SmallString<256> RealPath;
  CommonHAKCAnalysis::GetModuleFullPath(GetModule(), RealPath);

  out << "---\n";
  out << "CU: ";
  out << RealPath;
  out << "\n";

  auto GlobalCount = globals.size() + UnmappedGlobals.size();
  if (GlobalCount > 0) {
    out << "globals:\n";
    std::vector<std::shared_ptr<HAKCGlobalInfo>> SortedGlobals;
    SortedGlobals.reserve(GlobalCount);
    for (auto &it : globals) {
      SortedGlobals.push_back(it.second);
    }
    for (const auto &Unmapped : UnmappedGlobals) {
      SortedGlobals.push_back(Unmapped);
    }
    llvm::sort(SortedGlobals.begin(), SortedGlobals.end(),
               [](const std::shared_ptr<HAKCGlobalInfo> &LHS,
                  const std::shared_ptr<HAKCGlobalInfo> &RHS) {
                 return LHS->GetName() < RHS->GetName();
               });
    for (auto &it : SortedGlobals) {
      out.indent(HAKCInfo::IndentSpaces())
          << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
    }
  }

  auto FunctionCount = functions.size() + UnmappedFunctions.size();
  if (FunctionCount > 0) {
    out << "functions:\n";
    std::vector<std::shared_ptr<HAKCFunctionInfo>> SortedFunctions;
    SortedFunctions.reserve(FunctionCount);
    for (auto &it : functions) {
      SortedFunctions.push_back(it.second);
    }
    for (const auto &Unmapped : UnmappedFunctions) {
      SortedFunctions.push_back(Unmapped);
    }
    llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
               [](const std::shared_ptr<HAKCFunctionInfo> &LHS,
                  const std::shared_ptr<HAKCFunctionInfo> &RHS) {
                 return LHS->GetName() < RHS->GetName();
               });
    for (auto &it : SortedFunctions) {
      out.indent(HAKCInfo::IndentSpaces())
          << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
    }
  }
}

void hakc::HAKCTypeIdentifier::GetHAKCTypes(
    SmallVectorImpl<HAKCTypeP> &Results) const {
  for (auto &it : types) {
    Results.push_back(it.second);
  }
}

Type *hakc::HAKCTypeIdentifier::GetTypeFromString(StringRef TypeStr) const {
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

void hakc::HAKCTypeIdentifier::AddIgnoredType(StringRef TypeName) const {
  for (auto &it : types) {
    auto TyName = GetTypeName(it.first);
    if (TyName == TypeName) {
      it.second->SetIsIgnoredType(true);
      break;
    }
  }
}
