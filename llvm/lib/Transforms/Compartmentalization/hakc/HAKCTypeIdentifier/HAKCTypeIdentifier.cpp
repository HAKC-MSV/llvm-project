//
// Created by derrick on 9/8/21.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCFunctionInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCGlobalInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeInfo.h"

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Path.h"

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindType(const DIType *type) {
    if (!type) {
        errs() << "Trying to find null type\n";
        throw std::exception();
    }
    auto it = types.find(type);
    if (it == types.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindPointeeType(hakc::HAKCPointerBase &HAKCPointer) {
  // https://llvm.org/docs/OpaquePointers.html
  // Q: under what circumstances would we want to search for uses of the pointee to determine the type?

  // Now we have a HAKCPointer, we need to figure out what it points to, and then return the type object

  // first, see if the type already exists
  HAKCTypeP PointeeType;
  //
  // if(HAKCPointer.GetType()->GetDbgType()) {
  //   PointeeType = FindType(HAKCPointer.GetType()->GetDbgType());
  // }
  // else if (HAKCPointer.GetType()->GetLLVMType()) {
    auto value = HAKCPointer.GetBaseDefinition();
    if (auto *LoadInst = dyn_cast<LoadInst>(value)) {
      PointeeType = FindType(LoadInst->getType());
    }
    else if(auto *StoreInst = dyn_cast<StoreInst>(value)){
      PointeeType = FindType(StoreInst->getValueOperand()->getType());
    }
    else if(auto *Ty = dyn_cast<GetElementPtrInst>(value)) {
      PointeeType = FindType(Ty->getSourceElementType());
    }
    else if(auto *Ty = dyn_cast<CallInst>(value)) {
      // pointee type should be the dereferenced type of the return type of the function
      // int* = foo(), we want to get the int, to do this we need debug info for foo
      PointeeType = FindType(Ty->getFunctionType());
    }
    else if(auto *Ty = dyn_cast<AllocaInst>(value)) {
      PointeeType = FindType(Ty->getAllocatedType());
    }
    else if(auto *Ty = dyn_cast<GlobalValue>(value)) {
      PointeeType = FindType(Ty->getValueType());
    }
    else if(auto *Ty = dyn_cast<Function>(value)) {
      // pointee type is null here because it doesnt make sense to transfer a function
      PointeeType = FindType(Ty->getFunctionType()); // CHECK THIS
    }
    else if(auto *Ty = dyn_cast<Argument>(value)) {
      // should be similar to call inst, look at DI type sub program then DI type for that arg
      PointeeType = FindType(Ty->getPointeeInMemoryValueType()); // CHECK
    }
    else if(auto *Ty = dyn_cast<Instruction>(value)) {
      PointeeType = FindType(Ty->getAccessType()); // CHECK
    }
    // else if(auto *Ty = dyn_cast<StructType>(value)) {
    //   PointeeType = FindType(Ty->getElementType(0)); // CHECK THIS
    // }
    else if(auto *Ty = dyn_cast<ConstantStruct>(value)) {
      PointeeType = FindType(Ty->getType()); // CHECK
    }
    // else if(auto *Ty = dyn_cast<GlobalObject>(value)) {
    //   PointeeType = FindType(Ty->());
    // }
    else if(auto *Ty = dyn_cast<ConstantArray>(value)) {
      PointeeType = FindType(Ty->getType()); // CHECK
    }
    else if(auto *Ty = dyn_cast<IntToPtrInst>(value)) {
      PointeeType = FindType(Ty->getDestTy()); // CHECK
    }
    else if(auto *Ty = dyn_cast<GEPOperator>(value)) {
      PointeeType = FindType(Ty->getSourceElementType()); // CHECK
    }
  // }
  // make shared pointer here if it doesnt exist?

  return PointeeType;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::GetPointeeType(hakc::HAKCPointerBase &HAKCPointer) {
  /* TODO: Implement me */
  // TODO: null check of hakcpointer?
  if (!HAKCPointer.GetAuthenticatedPointer()->getType()->isPointerTy()) {
    return nullptr;
  }
  // now we know that the Value is a pointer type; try to get pointee type using dynamic casts
  // get the type identifier object (not static function), then find the pointee type
  return AnalysisHelper.GetSystemInfo().GetTypeIdentifier().FindPointeeType(HAKCPointer);
}

void hakc::HAKCTypeIdentifier::AddTypeMapping(const DIType *type, const std::shared_ptr<HAKCTypeInfo> &HAKCType) {
    CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo()) << "Adding mapping " << *type <<
            " -> " << HAKCType->GetName() << "\n";
    std::set<dwarf::Tag> TagsToSize = {
        dwarf::DW_TAG_structure_type,
        dwarf::DW_TAG_union_type,
    };
    if (isa<DIBasicType>(type) || TagsToSize.contains(type->getTag())) {
        HAKCType->SetSizeInBits(type->getSizeInBits());
    }
    HAKCType->SetDbgType(type);
    auto DbgTypeName = GetTypeName(type);
    HAKCType->SetDbgTypeName(DbgTypeName);
    types[type] = HAKCType;
}

unsigned hakc::HAKCTypeIdentifier::GetAnonymousID(const DIType *type) {
    unsigned ID;
    if (AnonymousNumberMapping.contains(type)) {
        AnonymousNumberMapping[type] = CurrentAnonID;
        ID = CurrentAnonID;
        CurrentAnonID++;
    } else {
        ID = AnonymousNumberMapping[type];
    }

    return ID;
}

std::string hakc::HAKCTypeIdentifier::GetTypeName(const DIType *type) {
    std::string Name;
    llvm::raw_string_ostream out(Name);

    if (auto *SubroutineTy = dyn_cast<DISubroutineType>(type)) {
        for (unsigned i = 0; i < SubroutineTy->getTypeArray()->getNumOperands(); i++) {
            auto *CurrTy = SubroutineTy->getTypeArray()[i];
            if (!CurrTy) {
                if (i == 0) {
                    out << "void";
                } else if (i == SubroutineTy->getTypeArray()->getNumOperands() - 1) {
                    out << "...";
                } else {
                    errs() << "Null operand at " << i << " for " << *type << "\n";
                    errs() << "Current type: " << Name << "\n";
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
            CommonHAKCAnalysis::getWriter(true) << "Unhandled DIDerivedType tag\n" << DerivedTy << "\n";
            throw std::exception();
        }
    } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
        if (CompositeTy->getTag() == dwarf::DW_TAG_array_type) {
            out << GetTypeName(CompositeTy->getBaseType()) << "[]";
        } else if (CompositeTy->getTag() == dwarf::DW_TAG_structure_type) {
            out << "struct ";
            if (CompositeTy->getName().empty()) {
                out << "anon." << GetAnonymousID(CompositeTy);
            } else {
                out << CompositeTy->getName();
            }
        } else if (CompositeTy->getTag() == dwarf::DW_TAG_union_type) {
            out << "union ";
            if (CompositeTy->getName().empty()) {
                out << "anon." << GetAnonymousID(CompositeTy);
            } else {
                out << CompositeTy->getName();
            }
        } else if (CompositeTy->getTag() == dwarf::DW_TAG_enumeration_type) {
            out << "enum " << CompositeTy->getName();
        } else {
            errs() << "Unhandled DICompositeType tag\n" << CompositeTy << "\n";
            throw std::exception();
        }
    } else if (auto *BaseTy = dyn_cast<DIBasicType>(type)) {
        if (BaseTy->getEncoding() == dwarf::DW_ATE_boolean) {
            out << "bool";
        } else {
            out << BaseTy->getName();
        }
    } else {
        errs() << "Unhandled DIType\n" << type << "\n";
        throw std::exception();
    }

    return Name;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::HandleType(const DIType *type) {
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();

    CommonHAKCAnalysis::getWriter(debug) << "Analyzing DIType " << *type << "\n";
    auto TypeP = FindType(type);
    if (TypeP) {
        CommonHAKCAnalysis::getWriter(debug) << "Already created " << *type << "\n";
        return TypeP;
    }
    if (isa<DICompositeType>(type) || isa<DISubroutineType>(type) || isa<DIBasicType>(type)) {
        CommonHAKCAnalysis::getWriter(debug) << "Creating HAKCTypeInfo for\n" << type << "\n";
        auto TypeName = GetTypeName(type);
        TypeP = std::make_shared<HAKCTypeInfo>(AnalysisHelper, TypeName, debug);
        if (auto *BasicType = dyn_cast<DIBasicType>(type)) {
            auto *IntTy = IntegerType::get(GetModule().getContext(), BasicType->getSizeInBits());
            TypeP->SetLLVMType(IntTy);
            // TypeP->SetPointeeType(FindPointeeType((TypeP)));
        } else if (auto *CompositeTy = dyn_cast<DICompositeType>(type)) {
            std::string SearchName = ".";
            SearchName += CompositeTy->getName();
            StructType *LLVMTy = nullptr;
            for (auto *StructTy: GetModule().getIdentifiedStructTypes()) {
                if (StructTy->getName().ends_with(SearchName)) {
                    LLVMTy = StructTy;
                    break;
                }
            }

            if (LLVMTy) {
                TypeP->SetLLVMType(LLVMTy);
            }
        }
        AddTypeMapping(type, TypeP);
    } else if (auto *DerivedTy = dyn_cast<DIDerivedType>(type)) {
        std::set<unsigned> TagsToConsider = {
            dwarf::DW_TAG_pointer_type,
            dwarf::DW_TAG_array_type,
            dwarf::DW_TAG_const_type,
            dwarf::DW_TAG_typedef,
            dwarf::DW_TAG_volatile_type,
            dwarf::DW_TAG_restrict_type,
        };
        if (TagsToConsider.contains(DerivedTy->getTag())) {
            CommonHAKCAnalysis::getWriter(debug) << "Creating HAKCTypeInfo for\n" << type << "\n";
            auto TypeName = GetTypeName(type);
            TypeP = std::make_shared<HAKCTypeInfo>(AnalysisHelper, TypeName, debug);
            AddTypeMapping(type, TypeP);
        } else {
            CommonHAKCAnalysis::getWriter(debug) << "Not handling DITYpe " << type << "\n";
        }
    }

    return TypeP;
}

GlobalVariable *hakc::HAKCTypeIdentifier::FindGlobal(const DIGlobalVariable *DIGV) {
    auto *Scope = DIGV->getScope();
    std::string Name;
    llvm::raw_string_ostream sstream(Name);
    if (auto *SubProg = dyn_cast<DISubprogram>(Scope)) {
        sstream << SubProg->getName() << ".";
    }
    sstream << DIGV->getName();

    return GetModule().getGlobalVariable(Name, true);
}

std::shared_ptr<hakc::HAKCGlobalInfo> hakc::HAKCTypeIdentifier::HandleGlobal(const DIGlobalVariable *DIGV) {
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(DIGV->getName());
    CommonHAKCAnalysis::getWriter(debug) << "Analyzing Global " << *DIGV << "\n";

    auto *GV = FindGlobal(DIGV);
    if (!GV) {
        CommonHAKCAnalysis::getWriter(debug) << "\nCould not find Global " << DIGV->getName() << "\n";
        return nullptr;
    }
    auto DIGVTy = FindType(DIGV->getType());
    if (!DIGVTy) {
        DIGVTy = HandleType(DIGV->getType());
        if (!DIGVTy) {
            errs() << "Unable to handle DIType " << *DIGV->getType() << " for Global " << *DIGV
                    << "\n";
            throw std::exception();
        }
    }
    if (!DIGVTy->GetLLVMType()) {
        DIGVTy->SetLLVMType(GV->getValueType());
    }

    auto GVP = std::make_shared<HAKCGlobalInfo>(AnalysisHelper, DIGV->getName(), debug);
    GVP->SetType(DIGVTy);
    GVP->SetGlobalVariable(GV);
    GVP->SetDefiningLocation(DIGV->getFile(), DIGV->getLine());
    if (DIGV->isLocalToUnit()) {
        GVP->SetLocalScope(DIGV->getScope());
    }
    AddGlobalMapping(DIGV, GVP);

    return GVP;
}

void hakc::HAKCTypeIdentifier::AddGlobalMapping(const DIGlobalVariable *DIGV,
                                                const std::shared_ptr<HAKCGlobalInfo> &HAKCSymbol) {
    CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(DIGV->getName())) << "Adding mapping "
            << *DIGV << " -> " << HAKCSymbol->GetName() << "\n";
    globals[DIGV] = HAKCSymbol;
    //    AddLLVMTypeMapping(HAKCSymbol->GetType(), HAKCSymbol->GetGlobalVariable()->getValueType());
}

std::shared_ptr<hakc::HAKCFunctionInfo> hakc::HAKCTypeIdentifier::HandleFunction(const DISubprogram *SubProg) {
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(SubProg->getName());

    CommonHAKCAnalysis::getWriter(debug) << "Handling DISubprogram " << *SubProg << "\n";

    auto *F = GetModule().getFunction(SubProg->getName());
    if (!F) {
        for (auto &FM: GetModule().functions()) {
            if (FM.getSubprogram() == SubProg) {
                F = &FM;
                break;
            }
        }
    }

    if (!F) {
        CommonHAKCAnalysis::getWriter(debug) << "\nCould not find Function " << SubProg->getName() << "\n";
        return nullptr;
    }
    if (CommonHAKCAnalysis::IsOutsideTransferFunc(F) || F->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter(debug) << SubProg->getName() << " is a HAKC Transfer function\n";
        return nullptr;
    }
    auto DIGVTy = FindType(SubProg->getType());
    if (!DIGVTy) {
        errs() << GetModule() << "Could not find HAKCType of " << F->getName()
                << " with DIType " << *SubProg->getType() << "\n";
        throw std::exception();
    }

    if (!DIGVTy->GetLLVMType()) {
        DIGVTy->SetLLVMType(F->getFunctionType());
    }
    auto FP = std::make_shared<HAKCFunctionInfo>(AnalysisHelper, SubProg->getName(), debug);
    FP->SetType(DIGVTy);
    FP->SetFunction(F);
    FP->SetDefiningLocation(SubProg->getFile(), SubProg->getLine());
    if (SubProg->isLocalToUnit()) {
        FP->SetLocalScope(SubProg->getScope());
    }
    AddFunctionMapping(SubProg, FP);

    return FP;
}

void hakc::HAKCTypeIdentifier::AddFunctionMapping(const DISubprogram *SubProg,
                                                  const std::shared_ptr<HAKCFunctionInfo> &HAKCFunction) {
    CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(SubProg->getName())) <<
            "Adding mapping " << *SubProg << " -> " << *HAKCFunction << "\n";
    functions[SubProg] = HAKCFunction;
    HAKCFunction->GetType()->SetLLVMType(HAKCFunction->GetFunction()->getFunctionType());
    //    AddLLVMTypeMapping(HAKCFunction->GetType(), HAKCFunction->GetFunction()->getFunctionType());
}

void hakc::HAKCTypeIdentifier::FindAllGlobalsUsed(Value *V, std::set<GlobalObject *> &GlobalSet) {
    if (auto *ConstStruct = dyn_cast<ConstantStruct>(V)) {
        for (auto &Member: ConstStruct->operands()) {
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
        for (auto &Member: ConstArray->operands()) {
            auto MemberDef = AnalysisHelper.getDef(Member.get(), false);
            if (auto *GlobalMember = dyn_cast<GlobalObject>(MemberDef)) {
                GlobalSet.insert(GlobalMember);
            } else {
                FindAllGlobalsUsed(MemberDef, GlobalSet);
            }
        }
    } else if (auto *I = dyn_cast<Instruction>(V)) {
        for (auto &Op: I->operands()) {
            auto MemberDef = AnalysisHelper.getDef(Op.get(), false);
            if (auto *GlobalMember = dyn_cast<GlobalObject>(MemberDef)) {
                GlobalSet.insert(GlobalMember);
            }
        }
    }
}

std::shared_ptr<hakc::HAKCFunctionInfo> hakc::HAKCTypeIdentifier::AddUnmappedFunction(Function *F) {
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);

    for (const auto &UnmappedFunc: UnmappedFunctions) {
        if (UnmappedFunc->GetFunction() == F) {
            return UnmappedFunc;
        }
    }
    CommonHAKCAnalysis::getWriter(debug) << "Adding unmapped Function " << F->getName() << "\n";
    auto FuncInfo = std::make_shared<HAKCFunctionInfo>(AnalysisHelper, F->getName(), debug);
    FuncInfo->SetFunction(F);
    auto HAKCType = FindCalledFunctionType(F->getFunctionType());
    if (!HAKCType) {
        HAKCType = CreateNoDebugType(F->getFunctionType());
    }

    CommonHAKCAnalysis::getWriter(debug) << "HAKCType exists for " << F->getName() << "\n";

    if (!HAKCType->GetLLVMType()) {
        HAKCType->SetLLVMType(F->getFunctionType());
    }
    FuncInfo->SetType(HAKCType);
    UnmappedFunctions.insert(FuncInfo);
    return FuncInfo;
}

std::shared_ptr<hakc::HAKCSymbolInfo> hakc::HAKCTypeIdentifier::AddUnmappedGlobal(GlobalObject *GlobalObj) {
    auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(GlobalObj);
    if (CommonHAKCAnalysis::IsStringType(GlobalObj->getValueType())) {
        return nullptr;
    }
    if (auto *F = dyn_cast<Function>(GlobalObj)) {
        return AddUnmappedFunction(F);
    } else if (auto *GV = dyn_cast<GlobalVariable>(GlobalObj)) {
        for (auto UnmappedGlobal: UnmappedGlobals) {
            if (UnmappedGlobal->GetGlobalVariable() == GV) {
                return UnmappedGlobal;
            }
        }
        CommonHAKCAnalysis::getWriter(debug) << "Adding unmapped Global Variable " << GV->getName() << "\n";
        auto GlobalInfo = std::make_shared<HAKCGlobalInfo>(AnalysisHelper, GlobalObj->getName(), debug);
        auto HAKCType = FindType(GlobalObj->getValueType());
        if (!HAKCType) {
            HAKCType = CreateNoDebugType(GlobalObj->getValueType());
            //            AddLLVMTypeMapping(HAKCType, HAKCType->GetLLVMType());
        }
        CommonHAKCAnalysis::getWriter(debug) << "HAKCType exists for " << GV->getName() << "\n";
        if (!HAKCType->GetLLVMType()) {
            HAKCType->SetLLVMType(GV->getValueType());
        }
        GlobalInfo->SetGlobalVariable(GV);
        GlobalInfo->SetType(HAKCType);
        UnmappedGlobals.insert(GlobalInfo);
        return GlobalInfo;
    } else {
        CommonHAKCAnalysis::getWriter(true) << "Unsupported GlobalObj: " << *GlobalObj << "\n";
        throw std::exception();
    }
}

void hakc::HAKCTypeIdentifier::AddUsedGlobals(std::set<GlobalObject *> &GlobalObjects,
                                              const std::shared_ptr<hakc::HAKCSymbolInfo> &UserSymbol) {
    for (auto *UsedGlobal: GlobalObjects) {
        auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(UsedGlobal);
        auto Symbol = FindSymbol(UsedGlobal);
        if (!Symbol) {
            CommonHAKCAnalysis::getWriter(debug) << "\nGlobal " << UsedGlobal->getName() << " is used in "
                    << UserSymbol->GetGlobalObj()->getName()
                    << " but the Symbol could not be found\n";
            Symbol = AddUnmappedGlobal(UsedGlobal);
        }
        CommonHAKCAnalysis::getWriter(debug) << "Found Symbol " << Symbol->GetName() << "\n";
        if (Symbol) {
            UserSymbol->AddSymbolUse(Symbol);
        }
    }
}

void hakc::HAKCTypeIdentifier::FindUsesInGlobals() {
    for (auto &it: globals) {
        auto *GV = it.second->GetGlobalVariable();

        if (GV->hasInitializer()) {
            CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV)) <<
                    "Searching for globals in " << *GV << "\n";
            std::set<GlobalObject *> GlobalsUsed;
            FindAllGlobalsUsed(GV->getInitializer(), GlobalsUsed);
            AddUsedGlobals(GlobalsUsed, it.second);
        }
    }
}

void hakc::HAKCTypeIdentifier::CreateIndirectCallSourceLink(Value *V,
                                                            std::vector<std::shared_ptr<HAKCIndirectCallSourceLink> > &
                                                            Path) {
    if (auto *Arg = dyn_cast<Argument>(V)) {
        auto HAKCType = FindType(Arg->getType());
        if (!HAKCType) {
            CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(Arg->getParent())) <<
                    "Could not find HAKCType for Argument " << Arg->getArgNo()
                    << " of Function " << Arg->getParent()->getName() << "\n";
            return;
        }
        if (!HAKCType->GetLLVMType()) {
            HAKCType->SetLLVMType(Arg->getType());
        }
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(Arg, HAKCType,
                                                                 AnalysisHelper.GetSystemInfo().OutputDebugInfo(
                                                                     Arg->getParent()));
        Path.push_back(Link);
    } else if (auto *GV = dyn_cast<GlobalVariable>(V)) {
        auto HAKCSymbol = FindGlobal(GV, true);
        if (!HAKCSymbol) {
            CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV)) <<
                    "Could not find HAKC Symbol for " << *GV << "\n";
            return;
        }
        auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCSymbol,
                                                                 AnalysisHelper.GetSystemInfo().OutputDebugInfo(GV));
        Path.push_back(Link);
    } else if (auto *Load = dyn_cast<LoadInst>(V)) {
        auto *Pointer = Load->getPointerOperand();
        auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(Load->getFunction());
        if (auto *GEP = dyn_cast<GEPOperator>(Pointer)) {
            auto *TyToCheck = GEP->getSourceElementType();
            auto HAKCType = FindType(TyToCheck);
            if (HAKCType /*&& GEP->hasAllConstantIndices()*/) {
                APInt Offset(64, 0);
                GEP->stripAndAccumulateInBoundsConstantOffsets(GetModule().getDataLayout(), Offset);
                CommonHAKCAnalysis::getWriter(debug) << "Offset in bits for " << *GEP << ": " << Offset.
                        getZExtValue()
                        << "\n";
                if (!HAKCType->GetLLVMType()) {
                    HAKCType->SetLLVMType(TyToCheck);
                }
                auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCType, Offset.getSExtValue(), debug);
                Path.push_back(Link);
                CreateIndirectCallSourceLink(GEP->getPointerOperand(), Path);
            }
            if (!GEP->hasAllConstantIndices()) {
                CommonHAKCAnalysis::getWriter(debug) << "GEP does not have all constant indices: " << *GEP << "\n";
            } else {
                CommonHAKCAnalysis::getWriter(debug) << "Could not find Load Pointer HAKC Type for " << *TyToCheck
                        << "\n";
            }
        } else if (auto *GVal = dyn_cast<GlobalValue>(Pointer)) {
            auto HAKCSymbol = FindSymbol(GVal, true);
            if (!HAKCSymbol) {
                CommonHAKCAnalysis::getWriter(debug) << "Unable to find Global " << GVal->getName() << "\n";
                return;
            }
            auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCSymbol, debug);
            Path.push_back(Link);
        } else {
            CommonHAKCAnalysis::getWriter(debug) << "Unhandled Load Pointer Operand type: " << *Pointer << "\n";
            auto *LoadTy = Load->getPointerOperand()->getType();
            auto HAKCType = FindType(LoadTy);
            if (HAKCType) {
                auto Link = std::make_shared<HAKCIndirectCallSourceLink>(HAKCType, 0, debug);
                Path.push_back(Link);
            }
        }
    } else if (auto *IntToPtr = dyn_cast<IntToPtrInst>(V)) {
        CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(IntToPtr->getFunction())) <<
                "Adding IntToPtr Link\n";
        CreateIndirectCallSourceLink(IntToPtr->getOperand(0), Path);
    } else if (auto *CallI = dyn_cast<CallInst>(V)) {
        CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(CallI->getFunction())) <<
                "Adding Call Link\n";
        CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
    } else {
        CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo()) << "Unhandled Link type: " << V
                << "\n";
    }
}

void hakc::HAKCTypeIdentifier::FindIndirectCallSource(CallInst *CallI,
                                                      std::vector<std::shared_ptr<HAKCIndirectCallSourceLink> > &Path) {
    if (!CallI->isIndirectCall()) {
        errs() << *CallI << " is not an indirect call\n";
        throw std::exception();
    }

    CreateIndirectCallSourceLink(CallI->getCalledOperand(), Path);
    CommonHAKCAnalysis::getWriter(AnalysisHelper.GetSystemInfo().OutputDebugInfo(CallI->getFunction())) << "Found " <<
            Path.size() << " path links for " << *CallI->
            getCalledOperand()
            << "\n";
}

FunctionType *hakc::HAKCTypeIdentifier::GetIndirectCallFunctionType(CallInst *CallI) {
    if (!CallI->isIndirectCall()) {
        errs() << "Trying to get type from a Call that is not an indirect call\n";
        throw std::exception();
    }
    return CallI->getFunctionType();
}

void hakc::HAKCTypeIdentifier::FindUsesInFunctions() {
    for (auto &it: functions) {
        auto *F = it.second->GetFunction();
        auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);
        CommonHAKCAnalysis::getWriter(debug) << "Searching for globals in Function " << F->getName() << "\n";

        std::set<GlobalObject *> GlobalsUsed;
        for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
            auto *I = &(*InstIt);
            if (I->isDebugOrPseudoInst() || isa<IntrinsicInst>(I) || isa<BranchInst>(I)) {
                continue;
            }
            FindAllGlobalsUsed(I, GlobalsUsed);
        }
        AddUsedGlobals(GlobalsUsed, it.second);

        for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
            auto *I = &(*InstIt);
            if (I->isDebugOrPseudoInst() || isa<IntrinsicInst>(I) || isa<BranchInst>(I)) {
                continue;
            }
            if (auto *Call = dyn_cast<CallInst>(I)) {
                if (Call->getCalledFunction()) {
                    auto FoundFunction = FindFunction(Call->getCalledFunction(), true);
                    if (!FoundFunction) {
                        CommonHAKCAnalysis::getWriter(debug) << "Could not find HAKC Symbol for Function "
                                << Call->getCalledFunction()->getName() << "\n";
                        FoundFunction = AddUnmappedFunction(Call->getCalledFunction());
                    }
                    it.second->AddDirectCall(FoundFunction);
                } else if (Call->isIndirectCall()) {
                    auto *FunctionTy = GetIndirectCallFunctionType(Call);
                    CommonHAKCAnalysis::getWriter(debug) << "Source of indirect call operand in Function "
                            << F->getName() << ": "
                            << AnalysisHelper.getDef(Call->getCalledOperand(), true)
                            << "\n";
                    auto HAKCType = FindCalledFunctionType(FunctionTy);
                    if (!HAKCType) {
                        errs() << "Could not find called HAKCType for " << *Call
                                << " with Searched Type " << *FunctionTy
                                << " in Function " << F->getName() << "\n";
                        HAKCType = FindType(Call->getCalledOperand()->getType());
                        if (HAKCType) {
                            errs() << "But the Pointer HAKCType exists: "
                                    << HAKCType->GetName() << "\n";
                        }
                        throw std::exception();
                    }
                    std::vector<std::shared_ptr<HAKCIndirectCallSourceLink> > SourcePath;
                    if (!HAKCType->GetLLVMType()) {
                        HAKCType->SetLLVMType(FunctionTy);
                    }
                    FindIndirectCallSource(Call, SourcePath);
                    auto Source = std::make_shared<HAKCIndirectCallSource>(SourcePath, HAKCType, debug);
                    it.second->AddIndirectCall(Source);
                }
            }
        }
    }
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindType(Type *Ty) {
  // remove this functionality, because it would only work for non pointers and is logically incorrect 
    for (auto &it: types) {
        if (it.second->GetLLVMType() && it.second->GetLLVMType() == Ty) {
            return it.second;
        }
    }

    return nullptr;
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::FindCalledFunctionType(FunctionType *FunctionTy) {
    if (!FunctionTy) {
        errs() << "Trying to find null FunctionTy\n";
        throw std::exception();
    }

    return FindType(FunctionTy);
}

std::shared_ptr<hakc::HAKCFunctionInfo> hakc::HAKCTypeIdentifier::FindFunction(Function *F, bool SearchUnmapped) {
    for (auto &it: functions) {
        if (it.second->GetFunction() == F) {
            return it.second;
        }
    }
    if (SearchUnmapped) {
        for (auto UnmappedFunction: UnmappedFunctions) {
            if (UnmappedFunction->GetFunction() == F) {
                return UnmappedFunction;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<hakc::HAKCGlobalInfo> hakc::HAKCTypeIdentifier::FindGlobal(GlobalVariable *GV, bool SearchUnmapped) {
    for (auto &it: globals) {
        if (it.second->GetGlobalVariable() == GV) {
            return it.second;
        }
    }
    if (SearchUnmapped) {
        for (auto UnmappedGlobal: UnmappedGlobals) {
            if (UnmappedGlobal->GetGlobalVariable() == GV) {
                return UnmappedGlobal;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<hakc::HAKCSymbolInfo> hakc::HAKCTypeIdentifier::FindSymbol(Value *V, bool SearchUnmapped) {
    if (auto *GV = dyn_cast<GlobalVariable>(V)) {
        return FindGlobal(GV, SearchUnmapped);
    }
    if (auto *F = dyn_cast<Function>(V)) {
        return FindFunction(F, SearchUnmapped);
    }
    return nullptr;
}

hakc::HAKCSymbolP hakc::HAKCTypeIdentifier::FindYamlSymbol(const hakc::HAKCYamlSymbol &YamlSymbol) {
    for (auto &it: globals) {
        if (YamlSymbol == it.second) {
            return it.second;
        }
    }
    for (auto &Unmapped: UnmappedGlobals) {
        if (YamlSymbol == Unmapped) {
            return Unmapped;
        }
    }
    for (auto &it: functions) {
        if (YamlSymbol == it.second) {
            return it.second;
        }
    }
    for (auto &Unmapped: UnmappedFunctions) {
        if (YamlSymbol == Unmapped) {
            return Unmapped;
        }
    }
    return nullptr;
}

void hakc::HAKCTypeIdentifier::FindTypesInFunctions() {
    for (auto &it: functions) {
        auto *F = it.second->GetFunction();
        auto debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo(F);

        CommonHAKCAnalysis::getWriter(debug) << "Finding types in Function " << F->getName() << "\n";
        for (auto InstIt = inst_begin(F); InstIt != inst_end(F); ++InstIt) {
            auto *I = &(*InstIt);
            if (auto *DbgIntrinsic = dyn_cast<DbgVariableIntrinsic>(I)) {
                auto *V = DbgIntrinsic->getVariableLocationOp(0);
                if (isa<UndefValue>(V)) {
                    CommonHAKCAnalysis::getWriter(debug) << "Skipping undef Value in Instruction " << *I << "\n";
                    continue;
                }
                auto *DebugV = DbgIntrinsic->getVariable();
                CommonHAKCAnalysis::getWriter(debug) << "Found " << *V << " to " << *DebugV
                        << " mapping from Instruction " << *I << "\n";
                auto HAKCType = FindType(DebugV->getType());
                if (!HAKCType) {
                    errs() << "Could not find HAKCType for DIType " << *DebugV->getType()
                            << "\n";
                    throw std::exception();
                }
                auto *LLVMTy = V->getType();
                if (auto *Alloca = dyn_cast<AllocaInst>(V)) {
                    LLVMTy = Alloca->getAllocatedType();
                } else if (auto *CallI = dyn_cast<CallInst>(V)) {
                    if (CallI->isInlineAsm()) {
                        /* Inline assembly causes too much type confusion, so skip these mappings */
                        CommonHAKCAnalysis::getWriter(debug) << "Skipping inline assembly\n";
                        continue;
                    }
                }
                HAKCType->SetLLVMType(LLVMTy);
            } else if (auto *CallI = dyn_cast<CallInst>(I)) {
                if (CallI->isIndirectCall()) {
                    auto *FunctionTy = GetIndirectCallFunctionType(CallI);
                    auto HAKCType = FindCalledFunctionType(FunctionTy);
                    if (!HAKCType) {
                        HAKCType = CreateNoDebugType(FunctionTy);
                    }
                }
            }
        }
    }
}

std::shared_ptr<hakc::HAKCTypeInfo> hakc::HAKCTypeIdentifier::CreateNoDebugType(Type *Ty) {
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

    auto HAKCType = std::make_shared<HAKCTypeInfo>(AnalysisHelper, Name,
                                                   AnalysisHelper.GetSystemInfo().OutputDebugInfo());
    HAKCType->SetLLVMType(Ty);
    return HAKCType;
}

hakc::HAKCTypeIdentifier::HAKCTypeIdentifier(CommonHAKCAnalysis &AnalysisHelper)
    : AnalysisHelper(AnalysisHelper), DbgInfoFinder(), types(), globals(), functions(), CurrentAnonID(0) {
}

Module &hakc::HAKCTypeIdentifier::GetModule() {
    return AnalysisHelper.GetModule();
}

void hakc::HAKCTypeIdentifier::ProcessDebugInfo() {
    DbgInfoFinder.processModule(GetModule());
    auto Debug = AnalysisHelper.GetSystemInfo().OutputDebugInfo();

    CommonHAKCAnalysis::getWriter(Debug) << GetModule() << "\n";

    CommonHAKCAnalysis::getWriter(Debug) << "!!!! Starting Type Handling !!!!\n";
    for (auto *DITy: DbgInfoFinder.types()) {
        auto TypeP = HandleType(DITy);
    }
    CommonHAKCAnalysis::getWriter(Debug) << "!!!! Finished Type Handling !!!!\n";

    CommonHAKCAnalysis::getWriter(Debug) << "!!!! Starting Global Handling !!!!\n";
    for (auto *DIGlobal: DbgInfoFinder.global_variables()) {
        auto GlobalP = HandleGlobal(DIGlobal->getVariable());
    }
    CommonHAKCAnalysis::getWriter(Debug) << "!!!! Finished Global Handling !!!!\n";

    CommonHAKCAnalysis::getWriter(Debug) << "!!!! Starting Function Handling !!!!\n";
    for (auto *DISubProg: DbgInfoFinder.subprograms()) {
        auto SubProgP = HandleFunction(DISubProg);
    }
    CommonHAKCAnalysis::getWriter(Debug) << "!!!! Finished Function Handling !!!!\n";

    FindTypesInFunctions();
    FindUsesInGlobals();
    FindUsesInFunctions();
}

void hakc::HAKCTypeIdentifier::OutputYAML(raw_ostream &out) {
    auto RealPath = CommonHAKCAnalysis::GetModuleFullPath(GetModule());

    out << "---\n";
    out << "CU: ";
    out << AnalysisHelper.GetTransformedPath(RealPath);
    out << "\n";

    auto GlobalCount = globals.size() + UnmappedGlobals.size();
    if (GlobalCount > 0) {
        out << "globals:\n";
        std::vector<std::shared_ptr<HAKCGlobalInfo> > SortedGlobals;
        SortedGlobals.reserve(GlobalCount);
        for (auto &it: globals) {
            SortedGlobals.push_back(it.second);
        }
        for (const auto &Unmapped: UnmappedGlobals) {
            SortedGlobals.push_back(Unmapped);
        }
        llvm::sort(SortedGlobals.begin(), SortedGlobals.end(),
                   [](const std::shared_ptr<HAKCGlobalInfo> &LHS, const std::shared_ptr<HAKCGlobalInfo> &RHS) {
                       return LHS->GetName() < RHS->GetName();
                   });
        for (auto &it: SortedGlobals) {
            out.indent(HAKCInfo::IndentSpaces()) << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
        }
    }

    auto FunctionCount = functions.size() + UnmappedFunctions.size();
    if (FunctionCount > 0) {
        out << "functions:\n";
        std::vector<std::shared_ptr<HAKCFunctionInfo> > SortedFunctions;
        SortedFunctions.reserve(FunctionCount);
        for (auto &it: functions) {
            SortedFunctions.push_back(it.second);
        }
        for (const auto &Unmapped: UnmappedFunctions) {
            SortedFunctions.push_back(Unmapped);
        }
        llvm::sort(SortedFunctions.begin(), SortedFunctions.end(),
                   [](const std::shared_ptr<HAKCFunctionInfo> &LHS, const std::shared_ptr<HAKCFunctionInfo> &RHS) {
                       return LHS->GetName() < RHS->GetName();
                   });
        for (auto &it: SortedFunctions) {
            out.indent(HAKCInfo::IndentSpaces()) << "- " << it->GetYaml(HAKCInfo::IndentSpaces()) << "\n";
        }
    }
}

void hakc::HAKCTypeIdentifier::GetHAKCTypes(SmallVectorImpl<HAKCTypeP> &Results) {
    for (auto &it: types) {
        Results.push_back(it.second);
    }
}
