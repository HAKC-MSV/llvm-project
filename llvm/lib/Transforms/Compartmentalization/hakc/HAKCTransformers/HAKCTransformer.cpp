//
// Created by de29664 on 3/21/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"

hakc::HAKCTransformer::HAKCTransformer(HAKCCompartmentalizationPolicy &Policy,
                                       HAKCModuleAnalysis &HAKCAnalysis) : HAKCIRBuilder(
                                                                               HAKCAnalysis.GetModule().getContext()),
                                                                           CompartmentalizationPolicy(Policy),
                                                                           ModuleAnalysis(HAKCAnalysis),
                                                                           VariadicTransferFunctions() {
}

Type *hakc::HAKCTransformer::GetEntryTokenType(unsigned AddrSpace) {
    return HAKCIRBuilder.getPtrTy(AddrSpace);
}

std::string hakc::HAKCTransformer::getUniqueAddressable_Name(Function *F) {
    std::string unique_addressable_name = "__UNIQUE_ID___addressable_";
    unique_addressable_name += F->getName();
    for (auto &G: getModule().globals()) {
        if (G.getName().starts_with(unique_addressable_name)) {
            return G.getName().str();
        }
    }
    return unique_addressable_name;
}

std::string hakc::HAKCTransformer::getKstrtab_entry_name(Function *F) {
    std::string ksymtab_symbol_name = "__kstrtab_";
    ksymtab_symbol_name += F->getName();
    return ksymtab_symbol_name;
}

std::string hakc::HAKCTransformer::getKstrtabns_entry_name(Function *F) {
    std::string ksymtabns_symbol_name = "__kstrtabns_";
    ksymtabns_symbol_name += F->getName();
    return ksymtabns_symbol_name;
}

void hakc::HAKCTransformer::CreateDataAuthArguments(hakc::HAKCPointerBase &HAKCPointer, Instruction *I,
                                                    SmallVectorImpl<Value *> &Result) {
    Function *F = I->getFunction();
    Value *HAKCPointerBitCast;
    auto Division = CompartmentalizationPolicy.GetDivision(F);
    auto AccessToken = Division.GetAccessToken();
    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
    auto *DataAuthFuncTy = hakc::CommonHAKCAnalysis::GetDataAuthenticationFunctionType(ModuleAnalysis.GetModule(),
        AddrSpace);

    if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
        HAKCPointerBitCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(),
                                                          DataAuthFuncTy->getParamType(0));
    } else {
        HAKCPointerBitCast = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(),
                                                         DataAuthFuncTy->getParamType(0));
    }

    SmallVector<Value *> Args = {
        HAKCPointerBitCast,
        Division.GetHAKCCompartment().GetCompartmentID(),
        AccessToken
    };
    Result.append(Args);
}

void hakc::HAKCTransformer::CreateCodeAuthArguments(hakc::HAKCPointerBase &HAKCPointer, Instruction *I,
                                                    SmallVectorImpl<Value *> &Results) {
    Function *F = I->getFunction();
    auto *ExitTokens = GetValidTargetCompartments(F);
    auto Division = CompartmentalizationPolicy.GetDivision(F);
    auto AccessToken = Division.GetAccessToken();

    if (!ExitTokens->getValueType()->isArrayTy()) {
        CommonHAKCAnalysis::getWriter(true) << "Invalid ExitToken Type (" << *ExitTokens->getValueType() << ") for "
                << *ExitTokens << "\n";
        throw std::exception();
    }
    Value *FirstExitToken = HAKCIRBuilder.CreateGEP(ExitTokens->getValueType(),
                                                    ExitTokens,
                                                    {HAKCIRBuilder.getInt64(0), HAKCIRBuilder.getInt64(0)});
    unsigned AddrSpace = GetPointerAddrSpace(FirstExitToken);
    Value *IndirectCallTarget = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(),
                                                            HAKCIRBuilder.getPtrTy(AddrSpace));

    SmallVector<Value *> Args = {
        IndirectCallTarget,
        Division.GetHAKCCompartment().GetCompartmentID(),
        AccessToken,
        FirstExitToken,
        HAKCIRBuilder.getInt64(ExitTokens->getValueType()->getArrayNumElements())
    };

    Results.append(Args);
}

void
hakc::HAKCTransformer::CreateTransferArguments(hakc::HAKCPointerBase &HAKCPointer, GlobalValue *Target, bool IsData,
                                               ConstantInt *Size, SmallVector<Value *> &Result) {
    Value *OperandCast;
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
    bool IsPerCPU = CommonHAKCAnalysis::IsPerCPUPointer(HAKCPointer.GetBaseDefinition());
    auto Division = CompartmentalizationPolicy.GetDivision(Target);

    OperandCast = HAKCIRBuilder.CreateBitOrPointerCast(HAKCPointer.GetBaseDefinition(),
                                                       HAKCIRBuilder.getPtrTy(AddrSpace));

    SmallVector<Value *> FullArgSet = {
        OperandCast,
        Size,
        Division.GetHAKCCompartment().GetCompartmentID(),
        Division.GetDivisionID()
    };
    if (!IsPerCPU) {
        /* Function signature uses is_code which is !isData */
        FullArgSet.push_back(IsData ? getFalse() : getTrue());
    }

    Result.append(FullArgSet);
}

Module &hakc::HAKCTransformer::getModule() {
    return ModuleAnalysis.GetModule();
}

void hakc::HAKCTransformer::ValidateLocation(Instruction *I) {
    if (I == nullptr) {
        CommonHAKCAnalysis::getWriter(true) << "I is null\n";
        throw std::exception();
    }
    HAKCIRBuilder.SetInsertPoint(I);
}

void hakc::HAKCTransformer::ValidateHAKCPointer(const HAKCPointerBase &HAKCPointer) {
    // TODO: this should be implemented
}

void hakc::HAKCTransformer::ValidateHAKCPointerAndLocation(const HAKCPointerBase &HAKCPointer, Instruction *I) {
    try {
        ValidateHAKCPointer(HAKCPointer);
        ValidateLocation(I);
    } catch (std::exception &e) {
        if (I) {
            CommonHAKCAnalysis::getWriter(DebugIsActive()) << "Validation failed for " << HAKCPointer <<
                    " for Instruction in "
                    << I->getFunction()->getName() << ": " << *I << "\n";
            throw e;
        }
    }
}

Value *hakc::HAKCTransformer::CreateSafePointer(HAKCPointerBase &HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    if (isa<PHINode>(I)) {
        CommonHAKCAnalysis::getWriter(true) << "exception in CreateSafePointer\n";
        CommonHAKCAnalysis::getWriter(true) << "Trying to insert data auth check at " << *I << " for " << HAKCPointer
                << "\n" << *I->getFunction() << "\n";
        throw std::exception();
    } else if (isa<ConstantPointerNull>(HAKCPointer.GetBaseDefinition())) {
        CommonHAKCAnalysis::getWriter(true) << "exception in CreateSafePointer\n";
        CommonHAKCAnalysis::getWriter(true) << "HAKCPointerBase is a ConstantPointerNull: " << HAKCPointer << "\n";
        throw std::exception();
    }

    if (HAKCPointer.GetAuthenticatedPointer()) {
        return HAKCPointer.GetAuthenticatedPointer();
    }

    Value *voidCast;

    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);

    if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
        voidCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));
    } else {
        voidCast = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));
    }

    Value *maxUserAddr = HAKCIRBuilder.CreateIntToPtr(ConstantInt::get(HAKCIRBuilder.getInt64Ty(), 0x0000ffffffffffff),
                                                      voidCast->getType());
    Value *addrCheck = HAKCIRBuilder.CreateICmpUGT(voidCast, maxUserAddr);
    Value *ptrToInt = HAKCIRBuilder.CreatePtrToInt(voidCast, HAKCIRBuilder.getInt64Ty());
    Value *orValue = HAKCIRBuilder.CreateOr(ptrToInt, 0xFFFF000000000000);
    Value *orCast = HAKCIRBuilder.CreateIntToPtr(orValue, HAKCPointer.GetBaseDefinition()->getType());
    auto SafePtr = HAKCIRBuilder.CreateSelect(addrCheck, orCast, HAKCPointer.GetBaseDefinition());
    ////

    // todo: anesathu; fix this, also add better debugging here probably
    // auto *SafePtr = CreateSafePointer_Arch(HAKCPointer, I);
    // auto *SafePtr = CreateSafePointer(HAKCPointer, I);

    if (SafePtr->getType() != HAKCPointer.GetBaseDefinition()->getType()) {
        CommonHAKCAnalysis::getWriter(true) << "SafePtr and HAKCPointerBase are not the same Type!\n"
                << "SafePtr: " << SafePtr->getType() << "\nHAKCPointerBase: " << HAKCPointer.GetBaseDefinition()->
                getType() << "\n";
        throw std::exception();
    }
    HAKCPointer.SetAuthenticatedPointer(SafePtr);
    return SafePtr;
}

Value *hakc::HAKCTransformer::CreateDataAuthentication(hakc::HAKCPointerBase &HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    if (HAKCPointer.GetAuthenticatedPointer()) {
        return HAKCPointer.GetAuthenticatedPointer();
    }

    if (isa<PHINode>(I)) {
        CommonHAKCAnalysis::getWriter(true) << "exception in CreateDataAuthentication\n";
        CommonHAKCAnalysis::getWriter(true) << "Trying to insert data auth check at " << *I << " for " << HAKCPointer
                << "\n" << *I->getFunction();
        throw std::exception();
    }

    Value *HAKCPointerBitCast;
    SmallVector<Value *> Args;
    unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
    auto *DataAuthFuncTy = CommonHAKCAnalysis::GetDataAuthenticationFunctionType(getModule(), AddrSpace);
    CreateDataAuthArguments(HAKCPointer, I, Args);
    for (unsigned i = 0; i < DataAuthFuncTy->getNumParams(); i++) {
        if (Args[i]->getType() != DataAuthFuncTy->getParamType(i)) {
            CommonHAKCAnalysis::getWriter(true) << "Types do not match at index " << std::to_string(i) << "\n"
                    << *DataAuthFuncTy << "\n" << *Args[i] << "\n";
            throw std::exception();
        }
    }

    auto *DataAuthCall = CreateCall(ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().DataValidation(), Args);
    HAKCPointerBitCast = CreateReturnCast(HAKCPointer, DataAuthCall);
    HAKCPointer.SetAuthenticatedPointer(HAKCPointerBitCast);

    return HAKCPointerBitCast;
}

Value *hakc::HAKCTransformer::CreateReturnCast(hakc::HAKCPointerBase &HAKCPointer, Value *V) {
    if (!V) {
        CommonHAKCAnalysis::getWriter(true) << "NULL V\n";
        throw std::exception();
    }
    if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
        return HAKCIRBuilder.CreatePtrToInt(V, HAKCPointer.GetBaseDefinition()->getType());
    } else {
        return HAKCIRBuilder.CreateBitCast(V, HAKCPointer.GetBaseDefinition()->getType());
    }
}

Value *hakc::HAKCTransformer::CreatePointerCast(hakc::HAKCPointerBase &HAKCPointer, PointerType *PointerTy) {
    if (!PointerTy) {
        CommonHAKCAnalysis::getWriter(true) << "NULL PointerTy\n";
        throw std::exception();
    }

    if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
        return HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(), PointerTy);
    } else {
        return HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(), PointerTy);
    }
}

Value *hakc::HAKCTransformer::CreateCodeAuthentication(hakc::HAKCPointerBase &HAKCPointer, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    SmallVector<Value *> Args;
    CreateCodeAuthArguments(HAKCPointer, I, Args);
    auto *AuthResult = CreateCall(ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().CodeValidation(), Args);
    auto *BitCast = HAKCIRBuilder.CreateBitCast(AuthResult, HAKCPointer.GetBaseDefinition()->getType());
    return BitCast;
}

GlobalVariable *hakc::HAKCTransformer::GetValidTargetCompartments(Function *F) {
    auto Division = CompartmentalizationPolicy.GetDivision(F);

    GlobalVariable *EntryTokenArray;
    auto CompartmentID = Division.GetHAKCCompartment().GetCompartmentID();
    std::string name = "entry_tokens_" + std::to_string(CompartmentID->getZExtValue());
    EntryTokenArray = getModule().getNamedGlobal(name);
    if (EntryTokenArray) {
        if (!EntryTokenArray->getValueType()->isArrayTy()) {
            CommonHAKCAnalysis::getWriter(true) << "Invalid type for " << *EntryTokenArray << "\n";
            throw std::exception();
        }
        return EntryTokenArray;
    }

    auto Targets = Division.GetHAKCCompartment().GetValidTargets();
    if (Targets.empty()) {
        CommonHAKCAnalysis::getWriter(true) << "No valid transitions exist for " << F->getName() << " in Compartment "
                << std::to_string(CompartmentID->getZExtValue()) << "\n";
        throw std::exception();
    }

    SmallVector<Constant *> EntryTokenValues;
    SmallVector<hakc_compartment_id_t> IDs;
    IDs.push_back(CompartmentID->getZExtValue());
    for (auto &t: Targets) {
        IDs.push_back(t->getZExtValue());
    }
    llvm::sort(IDs.begin(), IDs.end(),
               [](hakc_compartment_id_t LHS, hakc_compartment_id_t RHS) { return LHS < RHS; });

    for (auto ID: IDs) {
        auto TargetCompartment = CompartmentalizationPolicy.GetCompartment(ID);
        EntryTokenValues.push_back(TargetCompartment->GetEntryToken());
    }

    Type *EntryTokenTy = GetEntryTokenType(GetPointerAddrSpace(*EntryTokenValues.begin()));

    for (auto *Token: EntryTokenValues) {
        if (Token->getType() != EntryTokenTy) {
            errs() << "Token Type of " << *Token << " (" << *Token->getType()
                    << ") does not match " << *EntryTokenTy << "\n";
            throw std::exception();
        }
    }

    auto *Initializer = ConstantArray::get(ArrayType::get(EntryTokenTy, EntryTokenValues.size()), EntryTokenValues);

    EntryTokenArray = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(name, Initializer->getType()));
    EntryTokenArray->setConstant(true);
    EntryTokenArray->setLinkage(GlobalValue::InternalLinkage);
    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter(true) << "Setting initializer for " << EntryTokenArray->getName() << " to be "
                << Initializer << " from token values ";
        for (auto *TokenValue: EntryTokenValues) {
            CommonHAKCAnalysis::getWriter(true) << "\n\t" << TokenValue;
        }
        CommonHAKCAnalysis::getWriter(true) << "\n";
    }
    EntryTokenArray->setInitializer(Initializer);

    return EntryTokenArray;
}

CallInst *hakc::HAKCTransformer::CreateCall(Function *Callee, ArrayRef<Value *> Args) {
    auto *Call = HAKCIRBuilder.CreateCall(Callee, Args);

    /* The LLVM function checker throws an error when an inline-able function with debug info contains a function
    * call with no debug information.  So try to set the appropriate debug info for this transfer */
    if (!Call->getDebugLoc()) {
        auto *I = &*HAKCIRBuilder.GetInsertPoint();
        if (I->getDebugLoc()) {
            Call->setDebugLoc(I->getDebugLoc());
        } else {
            /* Use the closest debug info to I */
            bool PastI = false;
            for (auto BBI = I->getParent()->begin(), BBE = I->getParent()->end(); BBI != BBE; ++BBI) {
                if (BBI->getDebugLoc()) {
                    Call->setDebugLoc(BBI->getDebugLoc());
                }
                if (&*BBI == I) {
                    PastI = true;
                }
                if (PastI && Call->getDebugLoc()) {
                    break;
                }
            }
        }
    }


    return Call;
}

CallInst *hakc::HAKCTransformer::CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args) {
    std::vector<Type *> FunctionParamTypes;
    for (auto *Arg: Args) {
        FunctionParamTypes.push_back(Arg->getType());
    }

    FunctionType *FunctionCallTy = FunctionType::get(RetTy, FunctionParamTypes, false);

    auto Func = ModuleAnalysis.GetFunctionByName(name, FunctionCallTy);
    if (!Func) {
        CommonHAKCAnalysis::getWriter(true) << "Could not find function " << name << " of type " << FunctionCallTy
                << " to be inserted into\n" << HAKCIRBuilder.GetInsertBlock()->getParent()
                << "\n";
        throw std::exception();
    }
    return CreateCall(Func, Args);
}

Instruction *
hakc::HAKCTransformer::CreateSizedCompartmentTransfer(hakc::HAKCPointerBase &HAKCPointer, Instruction *I,
                                                      GlobalValue *Target, bool IsData, ConstantInt *Size) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    Instruction *Transfer;
    if (TargetIsKernel(Target)) {
        auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *SafePtr = dyn_cast<Instruction>(V);
        if (!SafePtr) {
            CommonHAKCAnalysis::getWriter(true) << "Unexpected Safe Pointer Type: " << *V << "\n";
            throw std::exception();
        }
        return SafePtr;
    }

    if (HAKCPointerHasCustomTransfer(HAKCPointer)) {
        Transfer = CreateCustomTransfer(HAKCPointer, Target, IsData, Size);
    } else {
        Transfer = CreateDefaultTransfer(HAKCPointer, Target, IsData, Size);
    }

    return Transfer;
}

Instruction *
hakc::HAKCTransformer::CreateCustomTransfer(hakc::HAKCPointerBase &HAKCPointer, GlobalValue *Target, bool IsData,
                                            ConstantInt *Size) {
    auto CustomTransfer = GetCustomTransferFunction(HAKCPointer);
    if (!CustomTransfer) {
        CommonHAKCAnalysis::getWriter(true) << "Could not find Transfer Function for "
                << HAKCPointer.GetBaseDefinition()->getType() << "\n";
        throw std::exception();
    }

    auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);
    return CustomTransfer->CreateTransfer(HAKCIRBuilder, TargetDivision, HAKCPointer, Size, IsData);
}

Instruction *
hakc::HAKCTransformer::CreateSignWithDivision(hakc::HAKCPointerBase &HAKCPointer, Instruction *I,
                                              GlobalValue *Target, bool IsData) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    HAKC_Compartment_ID CompartmentIDValue;
    if (auto *GV = dyn_cast<GlobalValue>(HAKCPointer.GetBaseDefinition())) {
        auto Division = CompartmentalizationPolicy.GetDivision(GV);
        CompartmentIDValue = Division.GetHAKCCompartment().GetCompartmentID();
    } else {
        auto Division = CompartmentalizationPolicy.GetDivision(Target);
        CompartmentIDValue = Division.GetHAKCCompartment().GetCompartmentID();
    }

    auto *IsCodeValue = HAKCIRBuilder.getInt1(!IsData);
    auto *OperandCast = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(),
                                                    HAKCIRBuilder.getPtrTy(AddrSpace));
    SmallVector<Value *> Args = {
        OperandCast, CompartmentIDValue, IsCodeValue
    };

    return CreateCallWithResultCast(ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().SignWithDivision()->getName(),
                                    HAKCAuthenticationRetType(AddrSpace),
                                    Args, HAKCPointer.GetBaseDefinition());
    // return CreateCallWithResultCast(ModuleAnalysis.HAKCSignWithColor(), HAKCAuthenticationRetType(AddrSpace),
    //                                 Args, HAKCPointer.GetBaseDefinition());
}

bool hakc::HAKCTransformer::HAKCPointerHasCustomTransfer(hakc::HAKCPointerBase &HAKCPointer) {
    return GetCustomTransferFunction(HAKCPointer) != nullptr;
}

hakc::hakc_custom_transfer_def_t
hakc::HAKCTransformer::GetCustomTransferFunctionForType(hakc::HAKCTypeP HAKCTy) {
    for (auto &it: ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().HAKCCustomTransfers()) {
        if (HAKCTy == it->GetTargetType()) {
            return it;
        }
    }
    return nullptr;
}

hakc::hakc_custom_transfer_def_t
hakc::HAKCTransformer::GetCustomTransferFunction(hakc::HAKCPointerBase &HAKCPointer) {
    for (auto &it: ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().HAKCCustomTransfers()) {
        if (HAKCPointer.GetType() == it->GetTargetType()) {
            return it;
        }
    }
    return nullptr;
}

Instruction *
hakc::HAKCTransformer::CreateDefaultTransfer(hakc::HAKCPointerBase &HAKCPointer, GlobalValue *Target, bool IsData,
                                             ConstantInt *Size) {
    SmallVector<Value *> TransferOperations;
    CreateTransferArguments(HAKCPointer, Target, IsData, Size, TransferOperations);
    bool IsPerCPU = CommonHAKCAnalysis::IsPerCPUPointer(HAKCPointer.GetBaseDefinition());

    auto CompartmentTransfer = ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().CompartmentTransfer(IsPerCPU);

    return CreateCallWithResultCast(CompartmentTransfer->GetFunction(), TransferOperations,
                                    HAKCPointer.GetBaseDefinition());
}

Instruction *
hakc::HAKCTransformer::CreateCallWithResultCast(StringRef Name, Type *RetTy, ArrayRef<Value *> Args,
                                                Value *ValueToTypeMatch) {
    auto *Call = CreateCall(Name, RetTy, Args);
    return CastCallToType(Call, ValueToTypeMatch);
}

Instruction *
hakc::HAKCTransformer::CreateCallWithResultCast(Function *Callee, ArrayRef<Value *> Args, Value *ValueToTypeMatch) {
    auto *Call = CreateCall(Callee, Args);
    return CastCallToType(Call, ValueToTypeMatch);
}

Instruction *hakc::HAKCTransformer::CastCallToType(CallInst *Call, Value *ValueToTypeMatch) {
    Value *ResultCast;
    if (isa<PtrToIntInst>(ValueToTypeMatch) || ValueToTypeMatch->getType()->isIntegerTy()) {
        ResultCast = HAKCIRBuilder.CreatePtrToInt(Call, ValueToTypeMatch->getType());
    } else {
        ResultCast = HAKCIRBuilder.CreateBitCast(Call, ValueToTypeMatch->getType());
    }

    auto *Result = dyn_cast<Instruction>(ResultCast);
    if (!Result) {
        Result = Call;
    }

    return Result;
}

/* Called with Values of type "void *" ("i8*") */
hakc::HAKCTypeP
hakc::HAKCTransformer::FindEntryBitcast(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, Function *Target) {
    /*
     * Checking V to see if it is an argument of the function that contains instruction I.
     * I is contained within a pass-generated HAKC_XFER function.
     *
     * If V is an argument, we determine V's argument index for the function.
     * The argument will have the same index in Target.
     */
    Argument *TargetV = nullptr;
    Value *BitcastV = nullptr;
    User *BitcastUser = nullptr;
    for (auto &Arg: I->getFunction()->args()) {
        if (HAKCPointer.GetBaseDefinition() == &Arg) {
            TargetV = Target->getArg(Arg.getArgNo());
            break;
        }
    }

    /* not an argument, nothing to return */
    if (!TargetV) {
        return nullptr;
    }

    std::set<Use *> WorkingList;
    for (auto &TargetUse: TargetV->uses()) {
        WorkingList.insert(&TargetUse);
    }

    while (!WorkingList.empty()) {
        auto *CurrentUse = *WorkingList.begin();
        auto *CurrentUser = CurrentUse->getUser();
        WorkingList.erase(CurrentUse);

        if (auto *BitCastOp = dyn_cast<BitCastOperator>(CurrentUser)) {
            BitcastV = BitCastOp;
            BitcastUser = CurrentUser;
            break;
        } else if (auto *BitCastI = dyn_cast<BitCastInst>(CurrentUser)) {
            BitcastV = BitCastI;
            BitcastUser = CurrentUser;
            break;
        } else if (auto *StoreI = dyn_cast<StoreInst>(CurrentUser)) {
            /* This may be unoptimized code that does
             *     %tmp     = alloca i8*
             *                store i8* %arg, i8** %tmp
             *     %voidarg = load i8*, i8** %tmp
             *     %bcarg   = bitcast i8* %voidarg to %struct.type*
             * instead of just directly bitcasting the argument
             *
             * the following code will only handle this case correctly in the event that
             *   there is only the one level of indirection through memory
             */
            if (CurrentUse->getOperandNo() != StoreInst::getPointerOperandIndex()) {
                auto *StorePtrDef = ModuleAnalysis.GetCommonAnalysis().getDef(StoreI->getPointerOperand(), false);
                if (isa<AllocaInst>(StorePtrDef)) {
                    for (auto *AllocaUser: StorePtrDef->users()) {
                        if (isa<LoadInst>(AllocaUser)) {
                            for (auto &LoadUse: AllocaUser->uses()) {
                                if (isa<BitCastInst>(LoadUse.getUser()) ||
                                    isa<BitCastOperator>(LoadUse.getUser())) {
                                    WorkingList.insert(&LoadUse);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (DebugIsActive()) {
        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "Value " << HAKCPointer;
        if (BitcastV && BitcastUser) {
            CommonHAKCAnalysis::getWriter(DebugIsActive()) << " is cast to " << *BitcastV->getType() <<
                    " by Instruction " << *BitcastUser;
        } else {
            CommonHAKCAnalysis::getWriter(DebugIsActive()) << " is not bitcast";
        }
        CommonHAKCAnalysis::getWriter(DebugIsActive()) << " in function " << Target->getName() << "\n";
    }

    return ModuleAnalysis.GetTypeIdentifier().FindHAKCType(BitcastV);
}

/**
 * Sometimes, a function will take a "void *" ("i8*") parameter and immediately cast it to some destination type (struct).
 * Sometimes, the destination type (struct) has a custom transfer function.
 *
 * If we want to make this work, we need to do a little extra work.
 *
 * This function will try to find a custom transfer function by Type instead of from the Value (which is of type "i8*")
 * and generate a call to the custom function instead of "hakc_transfer_to_clique".
 */
Instruction *
hakc::HAKCTransformer::CreateVoidCastCompartmentTransfer(hakc::HAKCPointerBase &HAKCPointer, Instruction *I,
                                                         GlobalValue *Target, HAKCTypeP TypeToUse) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    /* just give safe pointer to kernel targets */
    if (TargetIsKernel(Target)) {
        auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *SafePtr = dyn_cast<Instruction>(V);
        if (!SafePtr) {
            CommonHAKCAnalysis::getWriter(true) << "Unexpected Safe Pointer Type: " << *V << "\n";
            throw std::exception();
        }
        return SafePtr;
    }


    if (TypeToUse->IsPointerToPointer()) {
        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "TypeToUse " << *TypeToUse
                << " is a pointer to a pointer.\nAdding transfer starting at "
                << *HAKCIRBuilder.GetInsertPoint() << " in function "
                << *HAKCIRBuilder.GetInsertBlock()->getParent() << "\n";
        auto *FinalLocation = &*HAKCIRBuilder.GetInsertPoint();
        auto *SafePtr = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
        auto *Load = HAKCIRBuilder.CreateLoad(
            PointerType::get(getModule().getContext(), GetPointerAddrSpace(HAKCPointer)), SafePtr);
        auto ManagedPointer = CreateNewManagedPointer(Load);
        CreateVoidCastCompartmentTransfer(*ManagedPointer,
                                          Load->getNextNonDebugInstruction(), Target,
                                          TypeToUse->GetPointeeType());
        auto *FinalTransfer = CreateSizedCompartmentTransfer(HAKCPointer, FinalLocation,
                                                             Target, true, HAKCIRBuilder.getInt64(64));
        return FinalTransfer;
    }

    auto *size = GetObjectSizeInBytes(TypeToUse->GetPointeeType());

    if (size->equalsInt(0)) {
        errs() << "Zero size for HAKCType " << *TypeToUse->GetPointeeType() << "\n";
        throw std::exception();
    }

    auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);

    /*
     * at this point, we know the dest type is a struct* and we know the actual size
     *
     * even if a custom transfer function doesn't exist for the struct type, we can do a
     * more accurate transfer than the previous void* single-byte transfer
     */
    Instruction *Transfer;
    CommonHAKCAnalysis::getWriter(DebugIsActive()) << "LLVM type: " << *TypeToUse << "\nsize of type: " << *size <<
            "\n";

    if (auto CustomTransfer = GetCustomTransferFunctionForType(TypeToUse)) {
        /* custom transfer exists, give the most specific transfer possible */
        Transfer = CustomTransfer->CreateTransferWithCasts(HAKCIRBuilder, TargetDivision, HAKCPointer, size,
                                                           HAKCPointer.GetType(), TypeToUse,
                                                           HAKCPointer.GetBaseDefinition()->getType()->isFunctionTy());

        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "custom xfer result:\n";
    } else {
        /* no custom transfer exists, give the next-most specific transfer possible, correctly-sized generic transfer */
        Transfer = CreateSizedCompartmentTransfer(HAKCPointer, I, Target, true, size);

        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "sized xfer result:\n";
    }
    CommonHAKCAnalysis::getWriter(DebugIsActive()) << *Transfer << "\n";
    return Transfer;
}

Instruction *
hakc::HAKCTransformer::CreateCompartmentTransfer(hakc::HAKCPointerBase &HAKCPointer, Instruction *I,
                                                 GlobalValue *Target,
                                                 bool IsData) {
    // this validation seems to not be working correctly -> the HAKCPointer is not initialized, and passes validation, causing a segfault later
    ValidateHAKCPointerAndLocation(HAKCPointer, I);

    auto ObjectSize = GetObjectSizeInBytes(HAKCPointer);

    /*
     * If HAKCPointer is of type "void *" ("i8*"), ObjectSize will be 1.
     * I don't think this is the only case where that happens, so we do another check.
     * Only care about data transfer.
     */
    if (!ObjectSize) {
        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "Could not get ObjectSize for " << HAKCPointer << "\n";

        ObjectSize = GetDefaultObjectSize();
    }
    if (ObjectSize == HAKCIRBuilder.getInt64(1) && IsData && isa<Function>(Target)) {
        /*
         * TODO
         *
         * This functionality could and should be extended.
         *
         * Currently it is limited to checking if a transferred "void *" function argument
         * gets used as a different type in the entry basic block of the Target function.
         *
         * This code probably only behaves as expected if there is a single bitcast of the
         * argument in the entry block.
         * I do not know what would happen if there were more than one.
         *
         * Furthermore, if the bitcast did not occur until a later block, this implementation
         * will not find it. A single-byte "hakc_transfer_to_clique" call will be emitted.
         * This is probably not the desired behavior. This should be extended to follow
         * argument's use-chain through entire Target function.
         *
         * Another issue that could arise is an argument being bitcast multiple times,
         * to different dest types each time. This is usually due to the following:
         *
         * HAKCPointer ("argument") is a void* ("func(void *argument) {")
         * It gets bitcast to some "struct.realtype*" ("casted_argument = (struct realtype*)argument;")
         * The first field of "struct.realtype" is some other type "other_type";
         * offsetof(struct realtype, other_type_field) == 0
         * Some code in Target uses "casted_argument" AND "casted_argument->other_type_field"
         *
         * The IR will contain
         * %a = bitcast i8* %0, %struct.realtype*
         * ...
         * %b = bitcast i8* %0, %other_type
         * ...
         *
         * More sophisticated analysis would be required to figure out if this is happening
         * and only create the struct.realtype transfer.
         */

        /* look for a bitcast from i8* to a struct type in the entry basic block of Target */
        auto EntryCastType = FindEntryBitcast(HAKCPointer, I, dyn_cast<Function>(Target));
        /*
         * If a bitcast is found in entry block, Target takes HAKCPointer as "void*" but
         * immediately uses it as if it some other type.
         * This situation is often found in functions used to run kthreads.
         */
        if (EntryCastType) {
            CommonHAKCAnalysis::getWriter(DebugIsActive()) << Target->getName() <<
                    " has entry block bitcast from void* to:\n"
                    << *EntryCastType << "\n";

            /* Void* -> Struct* cast compartment transfers do things slightly differently. */
            auto *castTransfer = CreateVoidCastCompartmentTransfer(HAKCPointer, I, Target, EntryCastType);
            if (castTransfer) {
                return castTransfer;
            }
        }
    }


    return CreateSizedCompartmentTransfer(HAKCPointer, I, Target, IsData, ObjectSize);
}

Function *hakc::HAKCTransformer::GetTransferFunction(Function *F) {
    auto TransferFunctionName = ModuleAnalysis.GetCommonAnalysis().GetOutsideTransferName(F);
    auto *TransferFunction = ModuleAnalysis.GetFunctionByName(TransferFunctionName, F->getFunctionType());
    if (TransferFunction == nullptr) {
        CommonHAKCAnalysis::getWriter(true) << "Could not create HAKC transfer function " << TransferFunctionName
                << "\n";
        throw std::exception();
    }

    TransferFunction->setCallingConv(F->getCallingConv());
    TransferFunction->setLinkage(F->getLinkage());
    TransferFunction->copyAttributesFrom(F);
    TransferFunction->setSection(F->getSection());

    return TransferFunction;
}

bool hakc::HAKCTransformer::NoKernelTransfers(Function *Target) {
    return hakc::CommonHAKCAnalysis::IsUncompartmentalizedSymbol(Target, CompartmentalizationPolicy) /*&&
           !CommonHAKCAnalysis::NoKernelTransferFunctionsSet()*/;
}

void
hakc::HAKCTransformer::CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction,
                                                      SmallVectorImpl<Value *> &ArgsList) {
    bool NoKernelXfers = NoKernelTransfers(Target);

    for (auto Arg = TransferFunction->arg_begin(); Arg != TransferFunction->arg_end(); Arg++) {
        if (!CommonHAKCAnalysis::argShouldTransfer(Arg) || NoKernelXfers) {
            ArgsList.push_back(Arg);
            continue;
        }
        auto ManagedPointer = CreateNewManagedPointer(Arg);
        bool IsData = !Arg->getType()->isFunctionTy();
        CreateTransferFunctionArg_PreCall(Target, TransferFunction, Arg);
        Instruction *Transfer = CreateCompartmentTransfer(*ManagedPointer, &*HAKCIRBuilder.GetInsertPoint(), Target,
                                                          IsData);
        ArgsList.push_back(Transfer);
    }
}

void hakc::HAKCTransformer::CreateTransferFunctionArg_PreCall(Function *F, Function *TransferFunction, Value *Arg) {
    // TODO - Implement me
}

void hakc::HAKCTransformer::CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg) {
    // TODO - Implement me
}

void hakc::HAKCTransformer::CreateBackwardArgumentTransfers(Function *Target, Function *TransferFunction) {
    bool NoKernelXfers = NoKernelTransfers(Target);

    for (auto Arg = TransferFunction->arg_begin(); Arg != TransferFunction->arg_end(); Arg++) {
        if (!CommonHAKCAnalysis::argShouldTransfer(Arg) || NoKernelXfers) {
            continue;
        }
        CreateTransferFunctionArg_PostCall(Target, TransferFunction, Arg);
    }
}


Function *hakc::HAKCTransformer::CreateTransferFunction(Function *F) {
    if (F->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter(true) << "Trying to create a HAKC Transfer function for " << F->getName() << "\n";
        throw std::exception();
    } else if (F->isVarArg()) {
        CommonHAKCAnalysis::getWriter(true) << "Trying to create HAKC Transfer function for variadic function " <<
                F->getName() << "\n";
        throw std::exception();
    }

    Function *TransferFunction = CreateNonVariadicTransferFunction(F);

    return TransferFunction;
}

Function *hakc::HAKCTransformer::CreateTransferToVariadic(CallInst *Call) {
    auto *Target = Call->getCalledFunction();
    if (!Target) {
        CommonHAKCAnalysis::getWriter(true) << "Null Call target\n";
        throw std::exception();
    }
    if (Target->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter(true) << "Trying to create a HAKC Transfer function for " << Target->getName() <<
                "\n";
        throw std::exception();
    }

    std::vector<Type *> ArgTypes;
    for (auto &Arg: Call->args()) {
        ArgTypes.push_back(Arg->getType());
    }

    FunctionType *TransferType = FunctionType::get(Target->getReturnType(), ArgTypes, false);
    Function *TransferFunction = nullptr;
    unsigned TargetTransferCount = 0;

    for (auto &it: VariadicTransferFunctions) {
        Function *Transfer = it.first;
        Function *TransferTarget = it.second;
        if (TransferTarget == Target) {
            TargetTransferCount += 1;
        }

        if (Transfer->getFunctionType() == TransferType && TransferTarget == Target) {
            TransferFunction = Transfer;
        }
    }

    if (!TransferFunction) {
        auto TransferName = CommonHAKCAnalysis::getVariadicTransferName(Target);
        TransferName += "_";
        TransferName += std::to_string(TargetTransferCount);

        TransferFunction = ModuleAnalysis.GetFunctionByName(TransferName, TransferType);
        PopulateTransferFunction(Target, TransferFunction);
        TransferFunction->setLinkage(GlobalValue::PrivateLinkage);
        VariadicTransferFunctions[TransferFunction] = Target;
    }

    return TransferFunction;
}

void hakc::HAKCTransformer::TransferStructMembers(ConstantStruct *ConstStruct, Function *GlobalTransfer,
                                                  GlobalValue *GlobalVar, bool Debug) {
    CommonHAKCAnalysis::getWriter(Debug) << "Transferring " << *ConstStruct << "\n";

    for (auto &Member: ConstStruct->operands()) {
        GlobalValue *Target = GlobalVar;
        if (auto *GlobalMember = dyn_cast<GlobalValue>(Member.get())) {
            Target = GlobalMember;
        }
        if (!TransferShouldBeCreated(Member.get(), Target)) {
            CommonHAKCAnalysis::getWriter(Debug) << "No transfer of member " << std::to_string(Member.getOperandNo())
                    << " to " << Target << "\n";
            continue;
        }

        if (auto *StructMember = dyn_cast<ConstantStruct>(Member.get())) {
            TransferStructMembers(StructMember, GlobalTransfer, GlobalVar, Debug);
            continue;
        }

        if (CommonHAKCAnalysis::IsPointerLikeType(Member->getType())) {
            Value *Transfer, *GEP, *Load;

            CommonHAKCAnalysis::getWriter(Debug) << "Creating Transfer of Member "
                    << std::to_string(Member.getOperandNo()) << " " << Member.get()
                    << "\n";
            GEP = HAKCIRBuilder.CreateStructGEP(GlobalVar->getValueType(), GlobalVar, Member.getOperandNo());
            Load = HAKCIRBuilder.CreateLoad(Member->getType(), GEP);
            auto ManagedPointer = CreateNewManagedPointer(Load);
            Transfer = CreateCompartmentTransfer(*ManagedPointer, GlobalTransfer->getEntryBlock().getTerminator(),
                                                 Target, !isa<Function>(Member.get()));
            HAKCIRBuilder.CreateStore(Transfer, GEP);
        }
    }
}

bool hakc::HAKCTransformer::TransferShouldBeCreated(Value *V, GlobalValue *Target) {
    bool CreateTransfer = !TargetIsKernel(Target) && !isa<ConstantPointerNull>(V) &&
                          CommonHAKCAnalysis::IsPointerLikeType(V->getType());
    if (auto *I = dyn_cast<ConstantInt>(V)) {
        CreateTransfer = !I->equalsInt(0) && !I->isMinusOne();
    }

    if (ModuleAnalysis.GetCommonAnalysis().IsIgnoredType(V->getType())) {
        CreateTransfer = false;
    }

    return CreateTransfer;
}

Function *
hakc::HAKCTransformer::PopulateGlobalTransfer(Function *GlobalTransfer, GlobalVariable *GlobalVar, bool Debug) {
    if (!GlobalTransfer->empty()) {
        return GlobalTransfer;
    }

    CommonHAKCAnalysis::getWriter(Debug) << "Initializing New Function " << GlobalTransfer->getName() << "\n";
    InitNewFunction(GlobalTransfer, "HAKCGlobalTransferEntry");
    auto *VoidRet = HAKCIRBuilder.CreateRetVoid();
    HAKCIRBuilder.SetInsertPoint(VoidRet);

    if (GlobalVar->hasInitializer()) {
        CommonHAKCAnalysis::getWriter(Debug) << "Creating Init Transfer of " << GlobalVar << "\n";
        if (auto *InitStruct = dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
            CommonHAKCAnalysis::getWriter(Debug) << "Transferring struct members\n";
            TransferStructMembers(InitStruct, GlobalTransfer, GlobalVar, Debug);
        } else if (CommonHAKCAnalysis::IsPointerLikeType(GlobalVar->getInitializer()->getType())) {
            GlobalValue *Target = GlobalVar;
            if (auto *FuncPtr = dyn_cast<Function>(GlobalVar->getInitializer())) {
                Target = FuncPtr;
            }

            if (TransferShouldBeCreated(GlobalVar->getInitializer(), Target)) {
                CommonHAKCAnalysis::getWriter(Debug) << "Creating Transfer of " << Target << "\n";
                auto ManagedPointer = CreateNewManagedPointer(GlobalVar->getInitializer());
                auto *Transfer = CreateCompartmentTransfer(*ManagedPointer, VoidRet, Target,
                                                           !isa<Function>(GlobalVar->getInitializer()));
                HAKCIRBuilder.CreateStore(Transfer, GlobalVar);
            }
        }
    }

    CommonHAKCAnalysis::getWriter(Debug) << "Finished initializing " << GlobalTransfer->getName() << "\n";
    return GlobalTransfer;
}

void hakc::HAKCTransformer::InitNewFunction(Function *F, StringRef EntryBlockName) {
    if (!F->empty()) {
        return;
    }

    auto *EntryBB = BasicBlock::Create(getModule().getContext(), EntryBlockName, F);
    if (EntryBB != &F->getEntryBlock()) {
        CommonHAKCAnalysis::getWriter(true) << "Invalid Entry BasicBlock created\n";
        throw std::exception();
    }

    HAKCIRBuilder.SetInsertPoint(EntryBB);
}

Function *hakc::HAKCTransformer::PopulateTransferFunction(Function *Target, Function *TransferFunction) {
    if (!TransferFunction->empty()) {
        return TransferFunction;
    }

    InitNewFunction(TransferFunction, "HAKCTransferEntry");

    // Create a temporary terminator
    auto *Unreachable = HAKCIRBuilder.CreateUnreachable();
    HAKCIRBuilder.SetInsertPoint(Unreachable);

    // This is where the issue with the type info seems to originate
    SmallVector<Value *> TransferredArguments;
    CreateForwardArgumentTransfers(Target, TransferFunction, TransferredArguments);
    CallInst *TargetFunctionCall = HAKCIRBuilder.CreateCall(Target, TransferredArguments);

    if (!Target->doesNotReturn()) {
        CreateBackwardArgumentTransfers(Target, TransferFunction);
        if (!Target->getReturnType()->isVoidTy()) {
            HAKCIRBuilder.CreateRet(TargetFunctionCall);
        } else {
            HAKCIRBuilder.CreateRetVoid();
        }
        Unreachable->eraseFromParent();
    }
    CommonHAKCAnalysis::VerifyFunction(TransferFunction);

    return TransferFunction;
}

Function *hakc::HAKCTransformer::CreateNonVariadicTransferFunction(Function *F) {
    if (F->isIntrinsic()) {
        CommonHAKCAnalysis::getWriter(true) << "Trying to create a HAKC Transfer function for " << F->getName() << "\n";
        throw std::exception();
    }

    auto *TransferFunction = GetTransferFunction(F);
    if (!TransferFunction->empty() || !ModuleAnalysis.TransferFunctionShouldBeCreated(F)) {
        return TransferFunction;
    }

    return PopulateTransferFunction(F, TransferFunction);
}

Value *hakc::HAKCTransformer::CreateBitCast(hakc::HAKCPointerBase &HAKCPointer, Type *TargetType, Instruction *I) {
    ValidateHAKCPointerAndLocation(HAKCPointer, I);
    auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

    if (TargetType->isPointerTy() && TargetType->getPointerAddressSpace() != AddrSpace) {
        CommonHAKCAnalysis::getWriter(true) << "TargetType " << *TargetType << " has AddrSpace when casting "
                << HAKCPointer
                << "\n" << *I->getFunction() << "\n";
        throw std::exception();
    }
    Value *BitCast;
    if (HAKCPointer.GetType()->IsIntegerType() && TargetType->isPointerTy()) {
        BitCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(), TargetType);
    } else if (HAKCPointer.GetType()->IsPointerType() && TargetType->isIntegerTy()) {
        BitCast = HAKCIRBuilder.CreatePtrToInt(HAKCPointer.GetBaseDefinition(), TargetType);
    } else {
        BitCast = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(), TargetType);
    }
    return BitCast;
}


ConstantInt *hakc::HAKCTransformer::GetObjectSizeInBytes(hakc::HAKCPointerBase &HAKCPointer) {
    CommonHAKCAnalysis::getWriter(DebugIsActive()) << "In Getobjectsizeinbytes, hakc pointer:: " << HAKCPointer << "\n";
    // temporary workaround which should result in the default size being used
    // this seems to be called on an uninitialized type (AKA HAKCPointer.GetType() is nullptr, I think), which causes a segfault... trying to fix this
    if (HAKCPointer.GetType() == nullptr) {
        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "HAKCPointer: " << HAKCPointer << " has GetType of null\n";
        return nullptr;
    } else if (!HAKCPointer.GetType()->GetPointeeType()) {
        return nullptr;
    }
    return GetObjectSizeInBytes(HAKCPointer.GetType()->GetPointeeType());
}

ConstantInt *hakc::HAKCTransformer::GetObjectSizeInBytes(hakc::HAKCTypeP HAKCType) {
    auto bit_size = HAKCType->GetSizeInBits();
    CommonHAKCAnalysis::getWriter(DebugIsActive()) << "In Getobjectsizeinbytes: bitsize: " << bit_size << "\n";
    return getInt64(bit_size / BITS_PER_BYTE);
}

Type *hakc::HAKCTransformer::HAKCAuthenticationRetType(unsigned AddrSpace) {
    auto *AuthCallType = CommonHAKCAnalysis::GetDataAuthenticationFunctionType(getModule(), AddrSpace);
    return AuthCallType->getReturnType();
}

ConstantInt *hakc::HAKCTransformer::getTrue() {
    return HAKCIRBuilder.getTrue();
}

ConstantInt *hakc::HAKCTransformer::getFalse() {
    return HAKCIRBuilder.getFalse();
}

ConstantInt *hakc::HAKCTransformer::getInt64(uint64_t Value) {
    return HAKCIRBuilder.getInt64(Value);
}

ConstantInt *hakc::HAKCTransformer::getInt32(uint32_t Value) {
    return HAKCIRBuilder.getInt32(Value);
}

ConstantInt *hakc::HAKCTransformer::GetDefaultObjectSize() {
    return getInt64(1);
}

bool hakc::HAKCTransformer::TargetIsKernel(GlobalValue *Target) {
    return CompartmentalizationPolicy.GetDivision(Target).GetHAKCCompartment().IsUncompartmentalized();
}

unsigned hakc::HAKCTransformer::GetPointerAddrSpace(hakc::HAKCPointerBase &HAKCPointer) {
    return GetPointerAddrSpace(HAKCPointer.GetBaseDefinition());
}

unsigned hakc::HAKCTransformer::GetPointerAddrSpace(Value *V) {
    unsigned AddrSpace = 0;
    if (V->getType()->isPointerTy()) {
        AddrSpace = V->getType()->getPointerAddressSpace();
    }
    return AddrSpace;
}

GlobalVariable *hakc::HAKCTransformer::AddCompartmentMetadataEntry(HAKCCompartment &Compartment) {
    return nullptr;
}

bool hakc::HAKCTransformer::DebugIsActive() {
    return ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().OutputDebugInfo(
        HAKCIRBuilder.GetInsertPoint()->getFunction());
}

hakc::HAKCPointerBaseP hakc::HAKCTransformer::CreateNewManagedPointer(Value *BaseDefinition) {
    auto ManagedPtr = std::make_shared<HAKCPointerBase>(BaseDefinition, 0);
    auto HAKCTy = ModuleAnalysis.GetTypeIdentifier().FindType(*ManagedPtr);
    if (!HAKCTy) {
        CommonHAKCAnalysis::getWriter(DebugIsActive()) << "Could not find valid HAKCTy for value: " << *BaseDefinition
                << "\n";
    } else {
        ManagedPtr->SetType(HAKCTy);
    }
    return ManagedPtr;
}
