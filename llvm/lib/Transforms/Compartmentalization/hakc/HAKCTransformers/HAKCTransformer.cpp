//
// Created by de29664 on 3/21/23.
//

#include "llvm/IR/DIBuilder.h"
#include "llvm/Support/Regex.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCPointerManager.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentDivision.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransferState.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

namespace llvm::hakc {
HAKCTransformer::HAKCTransformer(HAKCModuleAnalysis &ModuleAnalysis,
                                 HAKCCompartmentalizationPolicy &Policy)
    : ModuleAnalysis(ModuleAnalysis), Policy(Policy),
      HAKCIRBuilder(ModuleAnalysis.GetModule().getContext()),
      CompartmentalizationPolicy(Policy), VariadicTransferFunctions() {
    InitAnalysis();
}

// if (!ModuleAnalysis.GetCommonAnalysis().GetSystemInfo().GetTemporalAnalysisEnabled()) {
HAKCTransformer::HAKCTransformer(HAKCModuleAnalysis &ModuleAnalysis,
                                 HAKCCompartmentalizationPolicyDAG &Policy)
    : ModuleAnalysis(ModuleAnalysis), Policy(Policy),
      HAKCIRBuilder(ModuleAnalysis.GetModule().getContext()),
      CompartmentalizationPolicy(Policy), VariadicTransferFunctions() {
  InitAnalysis();
}

void HAKCTransformer::InitAnalysis() {
  // TODO: fix this error here
  for (auto &F : getModule().functions()) {
    if (ModuleAnalysis.FunctionNeedsAnalysis(&F)) {
      auto &Division = Policy.GetDivision(&F);
      auto Compartment = Division.GetHAKCCompartment();
      RegisterUsedCompartment(Compartment);
      ModuleAnalysis.AnalysisFunctions.push_back(&F);
    }
  }
  CommonHAKCAnalysis::SortFunctionList(ModuleAnalysis.AnalysisFunctions);
}

void HAKCTransformer::RegisterUsedCompartment(HAKCCompartment &compartment) {
  if (compartment != Policy.GetDefaultDivision().GetHAKCCompartment()) {
    ModuleAnalysis.UsedCompartments.push_back(compartment);
  }
}

/**
 * @brief Moves all global values to the specified HAKC ELF section
 */
void HAKCTransformer::MoveGlobalsToHAKCSection() {
  std::set<GlobalVariable *> globalsToChange;

  for (auto *pGlobal : globalsToChange) {
    auto finalName = getGlobalHAKCSectionName(pGlobal);
    auto compartment = Policy.GetDivision(pGlobal).GetHAKCCompartment();
    RegisterUsedCompartment(compartment);

    if (finalName != pGlobal->getSection()) {
      CommonHAKCAnalysis::getWriter(
          getCommonAnalysis().GetSystemInfo().OutputDebugInfo(pGlobal))
          << "Changing section of global " << *pGlobal << " to section "
          << finalName << " from " << pGlobal->getSection() << "\n";
      pGlobal->setSection(finalName);
    }
  }
}

Function *HAKCTransformer::GetFunctionByName(StringRef Name,
                                             FunctionType *FuncTy) {
  auto Callee = getModule().getOrInsertFunction(Name, FuncTy);
  return dyn_cast<Function>(Callee.getCallee());
}

bool HAKCTransformer::isModuleCompartmentalized() {
  auto &DefaultCompartment = Policy.GetDefaultDivision().GetHAKCCompartment();
  auto Search = [DefaultCompartment](HAKCCompartment &Compartment) {
    return Compartment != DefaultCompartment;
  };

  return llvm::any_of(ModuleAnalysis.UsedCompartments, Search);
}

bool HAKCTransformer::AliasShouldBeCreated(Function *F) {
  //         /* See note in
  //         HAKCTransformerLinux::TransferFunctionShouldBeCreated */ if
  //         (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
  //             return false;
  //         }
  //         return HAKCTransformerLinux::AliasShouldBeCreated(F);
  return TransferFunctionShouldBeCreated(F);
}

bool HAKCTransformer::FunctionDefinedInAssembly(Function *F) {
  StringRef ModuleAsm = getModule().getModuleInlineAsm();
  std::string SearchTerm = "[[:space:];]+";
  SearchTerm += F->getName().str();
  SearchTerm += ":";
  Regex NameRegex(SearchTerm);
  SmallVector<StringRef, 2> Matches;

  auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
  if (NameInAssembly) {
    CommonHAKCAnalysis::getWriter(getSystemInfo().OutputDebugInfo(F))
        << F->getName()
        << " was found in the Module inline assembly: " << Matches[0] << "\n";
  } else {
    CommonHAKCAnalysis::getWriter(getSystemInfo().OutputDebugInfo(F))
        << "Could not find " << SearchTerm << " in\n"
        << ModuleAsm << "\n";
  }
  return NameInAssembly;
}

void HAKCTransformer::TransformModule() {
  MoveGlobalsToHAKCSection();
  TransformFunctions();
  AddCompartmentMetadata();

  CreateInitGlobalMemberTransfers();
  AddTransferFunctions();
}

void HAKCTransformer::TransformFunctions() {
  for (auto *F : ModuleAnalysis.AnalysisFunctions) {
    HAKCFunctionAnalysis FunctionTransformation(F, ModuleAnalysis, (*this),
                                                Policy);
    FunctionTransformation.InstrumentCode();
  }
}

void HAKCTransformer::performTransformations() {
  TransformModule();
  CommonHAKCAnalysis::getWriter(Info)
      << "Final Module After Transformations:\n"
      << getModule() << "\n";
}

bool HAKCTransformer::TransferFunctionShouldBeCreated(Function *F) {
  if (F->isDeclaration()) {
    return false;
  }

  return CommonHAKCAnalysis::FunctionHasPointerArg(F);
}

std::string
HAKCTransformer::getGlobalHAKCSectionName(GlobalVariable *GV) const {
  if (CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GV, Policy)) {
    return GV->getSection().str();
  }

  auto Compartment = Policy.GetDivision(GV).GetHAKCCompartment();
  std::string finalName = HAKC_SECTION_PREFIX.str();
  finalName += std::to_string(Compartment.GetCompartmentID()->getSExtValue());

  finalName += GV->getSection().str();
  if (GV->getSection().empty()) {
    if (GV->isConstant()) {
      finalName += ".rodata";
    } else {
      finalName += ".data";
    }
  }
  return finalName;
}

void HAKCTransformer::AddTransferFunctions() {
  FunctionList FuncsNeedingTransfers;
  for (auto &F : getModule().functions()) {
    if (!CommonHAKCAnalysis::IsUncompartmentalizedSymbol(&F, Policy) &&
        getCommonAnalysis().functionIsTransferCandidate(&F, Policy) &&
        !CommonHAKCAnalysis::IsOutsideTransferFunc(&F) &&
        ModuleAnalysis.functionEscapes(&F)) {
      FuncsNeedingTransfers.push_back(&F);
    }
  }
  CommonHAKCAnalysis::SortFunctionList(FuncsNeedingTransfers);
  for (auto *Funcp : FuncsNeedingTransfers) {
    Function &F = *Funcp;

    Function *transferFunc = nullptr;

    if (getCommonAnalysis().functionIsTransferCandidate(&F, Policy)) {
      auto Compartment = Policy.GetDivision(&F).GetHAKCCompartment();
      transferFunc = CreateTransferFunction(&F);
      if (!transferFunc) {
        CommonHAKCAnalysis::getWriter(Error)
            << "Could not create transfer for " << F.getName() << "\n";
        throw std::exception();
      }
      bool TransferAlreadyExisted =
          (transferFunc->getInstructionCount() > 0 && !F.isDeclaration());
      if (TransferAlreadyExisted) {
        CommonHAKCAnalysis::getWriter(Debug)
            << "Retrieved transfer function " << transferFunc->getName();
      } else {
        CommonHAKCAnalysis::getWriter(Debug)
            << "Created transfer function " << transferFunc->getName();
      }
      CommonHAKCAnalysis::getWriter(Debug)
          << " in compartment "
          << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
      if (!TransferAlreadyExisted) {
        CommonHAKCAnalysis::getWriter(Debug) << *transferFunc << "\n";
      }

      if (F.isDeclaration()) {
        CommonHAKCAnalysis::getWriter(Debug)
            << F.getName() << " is a declaration\n";
      }
    } else {
      CommonHAKCAnalysis::getWriter(Debug)
          << "No transfer created for " << F.getName() << "\n";
    }
    if (!transferFunc) {
      transferFunc = GetFunctionByName(
          getCommonAnalysis().GetOutsideTransferName(&F), F.getFunctionType());
      /* Ensure that transfer functions not defined here are treated
       * the same as the function we are replacing */
      transferFunc->setLinkage(F.getLinkage());
      transferFunc->copyAttributesFrom(&F);
    }

    if (getCommonAnalysis().ValueShouldBeReplacedWithTransfer(&F, Policy)) {
      if (!getCommonAnalysis().IsNoTransferFunction(&F)) {
        CommonHAKCAnalysis::getWriter(Debug)
            << "Replacing uses of " << F.getName() << " with "
            << transferFunc->getName() << "\n";
        // in_debug = debug_output;
        std::vector<std::pair<User *, unsigned>> UsesToReplace;
        auto TargetDivision = Policy.GetDivision(&F);

        for (auto &FUse : F.uses()) {
          if (auto *I = dyn_cast<Instruction>(FUse.getUser())) {
            if (CommonHAKCAnalysis::IsOutsideTransferFunc(I->getFunction())) {
              continue;
            }
          }
          if (auto *CallI = dyn_cast<CallInst>(FUse.getUser())) {
            if (CallI->getCalledFunction() == &F) {
              auto HeadDivision =
                  Policy.GetDivision(CallI->getParent()->getParent());
              if (HeadDivision.GetHAKCCompartment().GetCompartmentID() !=
                  TargetDivision.GetHAKCCompartment().GetCompartmentID()) {
                UsesToReplace.push_back(
                    std::make_pair(CallI, FUse.getOperandNo()));
                continue;
              }
            }
          }
          if (ModuleAnalysis.useEscapes(FUse)) {
            UsesToReplace.push_back(
                std::make_pair(FUse.getUser(), FUse.getOperandNo()));
          }
        }
        for (auto &U : UsesToReplace) {
          U.first->setOperand(U.second, transferFunc);
        }

        CommonHAKCAnalysis::getWriter(Debug) << "Done\n"
                                                    << getModule() << "\n";
      }
    }
    if (AliasShouldBeCreated(&F)) {
      auto OrigName = F.getName().str();
      auto NewName = CommonHAKCAnalysis::getOriginalTransformedName(&F);
      CommonHAKCAnalysis::getWriter(Debug)
          << "Changing name from " << F.getName() << " to " << NewName << "\n";
      F.setName(NewName);

      auto *alias = GlobalAlias::create(OrigName, transferFunc);
      CommonHAKCAnalysis::getWriter(Debug)
          << "Final Transfer:\n"
          << *transferFunc << "\nAlias: " << *alias << "\n";
    }
  }
}

bool HAKCTransformer::ConstantStructTransferIsNeeded(
    ConstantStruct *ConstStruct) {
  bool Result = false;
  CommonHAKCAnalysis::getWriter(Info)
      << "TransferIsNeeded Checking " << *ConstStruct << "\n";
  for (auto &Member : ConstStruct->operands()) {
    auto *Def = getCommonAnalysis().getDef(Member.get(), false);
    CommonHAKCAnalysis::getWriter(Info)
        << "Checking struct member " << *Member << " with Def " << *Def << "\n";
    if (isa<ConstantPointerNull>(Def)) {
      continue;
    }
    if (auto *GlobalVal = dyn_cast<GlobalValue>(Def)) {
      Result =
          !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalVal, Policy);
    } else if (auto *StructMember = dyn_cast<ConstantStruct>(Def)) {
      Result = ConstantStructTransferIsNeeded(StructMember);
    }

    if (Result) {
      break;
    }
  }

  CommonHAKCAnalysis::getWriter(Info)
      << __FUNCTION__ << " Result: " << std::to_string(Result) << "\n";
  return Result;
}

bool HAKCTransformer::TransferIsNeeded(GlobalVariable *GlobalVar) {
  bool IsKernelSym =
      CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalVar, Policy);
  bool Result = GlobalVar->hasInitializer() && !IsKernelSym;
  if (Result) {
    if (auto *ConstStruct =
            dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
      Result = ConstantStructTransferIsNeeded(ConstStruct);
    } else {
      if (isa<GlobalValue>(GlobalVar->getInitializer())) {
        Result = !IsKernelSym;
      } else if (isa<ConstantPointerNull>(GlobalVar->getInitializer())) {
        Result = false;
      } else {
        Result = CommonHAKCAnalysis::IsPointerLikeType(
            GlobalVar->getInitializer()->getType());
      }
    }
  }
  return Result;
}

void HAKCTransformer::CreateInitGlobalMemberTransfers() {
  std::vector<GlobalVariable *> GlobalsToModifyDuringInit;
  for (auto &GV : getModule().globals()) {
    if (TransferIsNeeded(&GV)) {
      GlobalsToModifyDuringInit.push_back(&GV);
    }
  }
  CommonHAKCAnalysis::SortGlobalList(GlobalsToModifyDuringInit);

  CommonHAKCAnalysis::getWriter(Info)
      << "Creating Init transfer functions for "
      << std::to_string(GlobalsToModifyDuringInit.size()) << " globals:\n";
  for (auto *GlobToTransfer : GlobalsToModifyDuringInit) {
    CommonHAKCAnalysis::getWriter(Info) << GlobToTransfer->getName() << "\n";
  }

  for (auto *GlobToTransfer : GlobalsToModifyDuringInit) {
    auto *InitTransfer = CreateInitTransfer(GlobToTransfer);
    CommonHAKCAnalysis::getWriter(
        getSystemInfo().OutputDebugInfo(GlobToTransfer))
        << "Created InitTransfer " << InitTransfer->getName() << "\n";
  }
}

Function *HAKCTransformer::CreateInitTransfer(GlobalVariable *GlobalVar) {
  if (!GlobalVar->hasInitializer()) {
    CommonHAKCAnalysis::getWriter(Fatal) << GlobalVar << " has no initializer\n";
    throw std::exception();
  }

  SmallString<128> FunctionName;
  FunctionName.append(ModuleAnalysis.GlobalInitTransferPrefix());

  for (auto letter : GlobalVar->getName()) {
    if (letter != '@') {
      FunctionName.push_back(letter);
    }
  }

  auto *GlobalTransferTy =
      FunctionType::get(Type::getVoidTy(getModule().getContext()), {}, false);
  auto *GlobalInitFunc =
      GetFunctionByName(FunctionName.str(), GlobalTransferTy);
  if (!GlobalInitFunc) {
    CommonHAKCAnalysis::getWriter(Fatal)
        << "Could not get Global Transfer function " << FunctionName << "\n";
    throw std::exception();
  }

  if (GlobalInitFunc->empty()) {
    PopulateGlobalInitTransferFunc(GlobalInitFunc, GlobalVar);
    CommonHAKCAnalysis::getWriter(getSystemInfo().OutputDebugInfo(GlobalVar))
        << "Finished Populating Global Init Transfer\n"
        << *GlobalInitFunc << "\n";
  }

  return GlobalInitFunc;
}

std::string
HAKCTransformer::GlobalVariableROSectionName(GlobalVariable *GlobalVar) {
  auto Compartment = Policy.GetDivision(GlobalVar).GetHAKCCompartment();
  std::string SectionName = ".hakc.";
  SectionName += std::to_string(Compartment.GetCompartmentIDValue());
  SectionName += ".ro_data";

  return SectionName;
}

void HAKCTransformer::PopulateGlobalInitTransferFunc(
    Function *GlobTransfer, GlobalVariable *GlobalVar) {

  CommonHAKCAnalysis::getWriter(Debug)
      << "Populating Global Init Transfer Function " << GlobTransfer->getName()
      << "\n";

  GlobTransfer->setSection(ModuleAnalysis.GlobalInitTransferSectionName());
  if (!GlobTransfer->empty()) {
    return;
  }

  if (GlobalVar->isConstant()) {
    GlobalVar->setConstant(false);
    GlobalVar->setSection(GlobalVariableROSectionName(GlobalVar));
  }

  CommonHAKCAnalysis::getWriter(Debug)
      << "Starting Global Init Population\n";
  PopulateGlobalTransfer(GlobTransfer, GlobalVar);
  CommonHAKCAnalysis::VerifyFunction(GlobTransfer);

  auto GlobalTrackerName = GlobTransfer->getName() + "_loc";
  auto *TransferPointer =
      dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(
          GlobalTrackerName.str(), GlobTransfer->getType()));
  TransferPointer->setConstant(true);
  TransferPointer->setInitializer(GlobTransfer);
  TransferPointer->setSection(
      ModuleAnalysis.GlobalInitTransferPointerSectionName());
}

void HAKCTransformer::AddCompartmentMetadata() {
  auto &DefaultCompartment = Policy.GetDefaultDivision().GetHAKCCompartment();
  for (auto Compartment : ModuleAnalysis.UsedCompartments) {
    if (Compartment != DefaultCompartment) {
      AddCompartmentMetadataEntry(Compartment);
    }
  }
}

// Get the StructType representing a kernel (module) parameter
StructType *HAKCTransformer::GetKernelParamType() {
  // linux
  return llvm::StructType::getTypeByName(
      getModule().getContext(), llvm::StringRef("struct.kernel_param"));
}

void HAKCTransformer::emitModParamGetCtx(GlobalValue *kernparam) {
  // linux
  // type of void*
  PointerType *PointerTy =
      PointerType::get(IntegerType::get(getModule().getContext(), 8), 0);

  // two args
  std::vector<Type *> FuncTy_args;
  // first arg points to param
  FuncTy_args.push_back(PointerTy);
  // second arg is int64_t flag (0 to return param's access token, 1 to return
  // param's color)
  FuncTy_args.push_back(IntegerType::get(getModule().getContext(), 64));

  // type of function that returns int64_t, takes (void *, int64_t)
  FunctionType *FuncTy = FunctionType::get(
      IntegerType::get(getModule().getContext(), 64), FuncTy_args, false);

  // create a function named "hakc_modparam_getctx_paramname"
  auto c = getModule().getOrInsertFunction(
      MODPARAM_GETCTX_PREFIX.str() + kernparam->getName().str(), FuncTy);

  auto *constc = dyn_cast<Constant>(c.getCallee());
  auto *getctx = cast<Function>(constc);
  getctx->setCallingConv(CallingConv::C);

  // put "hakc_modparam_getctx_paramname" in a special text section in the
  // module
  getctx->setSection(HAKC_MODPARAM_TEXT_SECTION);

  Function::arg_iterator args = getctx->arg_begin();
  // param pointer
  Value *pointerArg = args++;
  // 0 to return context, 1 to return color
  Value *returnTypeArg = args++;

  // create entry basic block in our new function
  BasicBlock *block =
      BasicBlock::Create(getModule().getContext(), "entry", getctx);
  //
  IRBuilder<> builder(block);
  // constant zero for compare/select
  Value *czero =
      ConstantInt::get(IntegerType::get(getModule().getContext(), 64), 0);

  // get HAKC symbol for the kernel parameter Value
  auto &Division = Policy.GetDivision(kernparam);

  CommonHAKCAnalysis::getWriter(Debug)
      << kernparam << " " << Division << "\n";

  // cast kernparam to a void*
  Value *voidCast;
  auto AddrSpace = HAKCTransformer::GetPointerAddrSpace(kernparam);

  if (kernparam->getType()->isIntegerTy()) {
    voidCast = builder.CreateIntToPtr(kernparam, builder.getPtrTy(AddrSpace));
  } else {
    voidCast = builder.CreateBitCast(kernparam, builder.getPtrTy(AddrSpace));
  }

  // if returnTypeArg == 0, next step will use access token for return value
  // else, use color for return value
  Value *tokEqZero = builder.CreateICmpEQ(returnTypeArg, czero);
  Value *tokColSelect = builder.CreateSelect(
      tokEqZero, Division.GetAccessToken(), Division.GetDivisionID());

  // check if the address passed in matches address of kernparam
  Value *pointerArgEq = builder.CreateICmpEQ(pointerArg, voidCast);
  // if it does, return the previously selected token/color
  // it it isn't a match, return zero
  Value *ctxSelect = builder.CreateSelect(pointerArgEq, tokColSelect, czero);
  // function is done
  builder.CreateRet(ctxSelect);

  CommonHAKCAnalysis::getWriter(Debug)
      << *getctx << "\n";

  CommonHAKCAnalysis::VerifyFunction(getctx);

  // generate function pointer and place in modparam fp section
  auto CtxFPName = getctx->getName() + "_fp";
  auto *gcfp = dyn_cast<GlobalVariable>(getModule().getOrInsertGlobal(
      CtxFPName.getSingleStringRef(), getctx->getType()));
  gcfp->setSection(HAKC_MODPARAM_FUNCP_SECTION);
  gcfp->setLinkage(GlobalValue::ExternalLinkage);
  gcfp->setConstant(true);
  gcfp->setInitializer(getctx);
}

bool HAKCTransformer::FunctionIsInAnalysisSet(Function *F) {
  return CommonHAKCAnalysis::IsFunctionInFunctionList(
      F, make_range(ModuleAnalysis.AnalysisFunctions.begin(),
                    ModuleAnalysis.AnalysisFunctions.end()));
}

// HAKCTransformer::HAKCTransformer(HAKCCompartmentalizationPolicy
// &Policy,
//                                        HAKCTransformer &ModuleAnalysis)
//     : HAKCIRBuilder(ModuleAnalysis.getModule().getContext()),
//       CompartmentalizationPolicy(Policy), ModuleAnalysis(ModuleAnalysis),
//       VariadicTransferFunctions() {}

Type *HAKCTransformer::GetEntryTokenType(unsigned AddrSpace) const {
  return HAKCCompartment::GetEntryTokenType(HAKCIRBuilder.getContext());
}

void HAKCTransformer::CreateDataAuthArguments(
    HAKCPointerBase &HAKCPointer, Instruction *I,
    SmallVectorImpl<Value *> &Result) {
  Function *F = I->getFunction();
  Value *HAKCPointerBitCast;
  auto Division = CompartmentalizationPolicy.GetDivision(F);
  auto *AccessToken = Division.GetAccessToken();
  unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
  auto *DataAuthFuncTy = getCommonAnalysis().GetDataAuthenticationFunctionType(
      getModule(), AddrSpace);

  if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
    HAKCPointerBitCast = HAKCIRBuilder.CreateIntToPtr(
        HAKCPointer.GetBaseDefinition(), DataAuthFuncTy->getParamType(0));
  } else {
    HAKCPointerBitCast = HAKCIRBuilder.CreateBitCast(
        HAKCPointer.GetBaseDefinition(), DataAuthFuncTy->getParamType(0));
  }

  SmallVector<Value *> Args = {HAKCPointerBitCast,
                               Division.GetHAKCCompartment().GetCompartmentID(),
                               AccessToken};
  Result.append(Args);
}

void HAKCTransformer::CreateCodeAuthArguments(
    HAKCPointerBase &HAKCPointer, Instruction *I,
    SmallVectorImpl<Value *> &Results) {
  Function *F = I->getFunction();
  auto *ExitTokens = GetValidTargetCompartments(F);
  auto Division = CompartmentalizationPolicy.GetDivision(F);
  auto AccessToken = Division.GetAccessToken();

  if (!ExitTokens->getValueType()->isArrayTy()) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Invalid ExitToken Type (" << *ExitTokens->getValueType() << ") for "
        << *ExitTokens << "\n";
    throw std::exception();
  }
  Value *FirstExitToken = HAKCIRBuilder.CreateGEP(
      ExitTokens->getValueType(), ExitTokens,
      {HAKCIRBuilder.getInt64(0), HAKCIRBuilder.getInt64(0)});
  unsigned AddrSpace = GetPointerAddrSpace(FirstExitToken);
  Value *IndirectCallTarget = HAKCIRBuilder.CreateBitCast(
      HAKCPointer.GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));

  SmallVector<Value *> Args = {
      IndirectCallTarget, Division.GetHAKCCompartment().GetCompartmentID(),
      AccessToken, FirstExitToken,
      HAKCIRBuilder.getInt64(
          ExitTokens->getValueType()->getArrayNumElements())};

  Results.append(Args);
}

void HAKCTransformer::CreateTransferArguments(
    HAKCPointerBase &HAKCPointer, GlobalValue *Target, bool IsData,
    ConstantInt *Size, SmallVector<Value *> &Result) {
  Value *OperandCast;
  auto AddrSpace = GetPointerAddrSpace(HAKCPointer);
  bool IsPerCPU =
      CommonHAKCAnalysis::IsPerCPUPointer(HAKCPointer.GetBaseDefinition());
  auto Division = CompartmentalizationPolicy.GetDivision(Target);

  OperandCast = HAKCIRBuilder.CreateBitOrPointerCast(
      HAKCPointer.GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));

  SmallVector<Value *> FullArgSet = {
      OperandCast, Size, Division.GetHAKCCompartment().GetCompartmentID(),
      Division.GetDivisionID()};
  if (!IsPerCPU) {
    /* Function signature uses is_code which is !isData */
    FullArgSet.push_back(IsData ? getFalse() : getTrue());
  }

  Result.append(FullArgSet);
}

Module &HAKCTransformer::getModule() const {
  return ModuleAnalysis.GetModule();
}

CommonHAKCAnalysis &HAKCTransformer::getCommonAnalysis() const {
  return ModuleAnalysis.GetCommonAnalysis();
}

HAKCSystemInformation &HAKCTransformer::getSystemInfo() const {
  return ModuleAnalysis.GetCommonAnalysis().GetSystemInfo();
}

void HAKCTransformer::ValidateLocation(Instruction *I) {
  if (I == nullptr) {
    CommonHAKCAnalysis::getWriter(Error) << "I is null\n";
    throw std::exception();
  }
  HAKCIRBuilder.SetInsertPoint(I);
}

void HAKCTransformer::ValidateHAKCPointer(
    const HAKCPointerBase &HAKCPointer) {
  if (HAKCPointer.GetType() == nullptr) {
    CommonHAKCAnalysis::getWriter(Error)
        << "HAKCPointer " << HAKCPointer << " has no HAKCType\n";
    throw std::exception();
  } else if (HAKCPointer.GetType()->GetPointeeType() == nullptr) {
    CommonHAKCAnalysis::getWriter(Error)
        << "HAKCPointer " << HAKCPointer << " Type " << *HAKCPointer.GetType()
        << " has no PointeeType\n";
    throw std::exception();
  }
}

void HAKCTransformer::ValidateHAKCPointerAndLocation(
    const HAKCPointerBase &HAKCPointer, Instruction *I) {
  ValidateHAKCPointer(HAKCPointer);
  ValidateLocation(I);
}

Value *HAKCTransformer::CreateSafePointer(HAKCPointerBase &HAKCPointer,
                                                Instruction *I) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);

  if (isa<PHINode>(I)) {
    CommonHAKCAnalysis::getWriter(Error) << "exception in CreateSafePointer\n";
    CommonHAKCAnalysis::getWriter(Error)
        << "Trying to insert data auth check at " << *I << " for "
        << HAKCPointer << "\n"
        << *I->getFunction() << "\n";
    throw std::exception();
  } else if (isa<ConstantPointerNull>(HAKCPointer.GetBaseDefinition())) {
    CommonHAKCAnalysis::getWriter(Error) << "exception in CreateSafePointer\n";
    CommonHAKCAnalysis::getWriter(Error)
        << "HAKCPointerBase is a ConstantPointerNull: " << HAKCPointer << "\n";
    throw std::exception();
  }

  if (HAKCPointer.GetAuthenticatedPointer()) {
    return HAKCPointer.GetAuthenticatedPointer();
  }

  Value *voidCast;

  unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);

  if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
    voidCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(),
                                            HAKCIRBuilder.getPtrTy(AddrSpace));
  } else {
    voidCast = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(),
                                           HAKCIRBuilder.getPtrTy(AddrSpace));
  }

  Value *maxUserAddr = HAKCIRBuilder.CreateIntToPtr(
      ConstantInt::get(HAKCIRBuilder.getInt64Ty(), 0x0000ffffffffffff),
      voidCast->getType());
  Value *addrCheck = HAKCIRBuilder.CreateICmpUGT(voidCast, maxUserAddr);
  Value *ptrToInt =
      HAKCIRBuilder.CreatePtrToInt(voidCast, HAKCIRBuilder.getInt64Ty());
  Value *orValue = HAKCIRBuilder.CreateOr(ptrToInt, 0xFFFF000000000000);
  Value *orCast = HAKCIRBuilder.CreateIntToPtr(
      orValue, HAKCPointer.GetBaseDefinition()->getType());
  auto SafePtr = HAKCIRBuilder.CreateSelect(addrCheck, orCast,
                                            HAKCPointer.GetBaseDefinition());

  if (SafePtr->getType() != HAKCPointer.GetBaseDefinition()->getType()) {
    CommonHAKCAnalysis::getWriter(Error)
        << "SafePtr and HAKCPointerBase are not the same Type!\n"
        << "SafePtr: " << SafePtr->getType()
        << "\nHAKCPointerBase: " << HAKCPointer.GetBaseDefinition()->getType()
        << "\n";
    throw std::exception();
  }
  HAKCPointer.SetAuthenticatedPointer(SafePtr);
  return SafePtr;
}

Value *HAKCTransformer::CreateDataAuthentication(
    HAKCPointerBase &HAKCPointer, Instruction *I) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);

  if (HAKCPointer.GetAuthenticatedPointer()) {
    return HAKCPointer.GetAuthenticatedPointer();
  }

  if (isa<PHINode>(I)) {
    CommonHAKCAnalysis::getWriter(Error)
        << "exception in CreateDataAuthentication\n";
    CommonHAKCAnalysis::getWriter(Error)
        << "Trying to insert data auth check at " << *I << " for "
        << HAKCPointer << "\n"
        << *I->getFunction();
    throw std::exception();
  }

  SmallVector<Value *> Args;
  unsigned AddrSpace = GetPointerAddrSpace(HAKCPointer);
  auto *DataAuthFuncTy = getCommonAnalysis().GetDataAuthenticationFunctionType(
      getModule(), AddrSpace);
  CreateDataAuthArguments(HAKCPointer, I, Args);
  for (unsigned i = 0; i < DataAuthFuncTy->getNumParams(); i++) {
    if (Args[i]->getType() != DataAuthFuncTy->getParamType(i)) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Types do not match at index " << std::to_string(i) << "\n"
          << *DataAuthFuncTy << "\n"
          << *Args[i] << "\n";
      throw std::exception();
    }
  }

  auto *DataAuthCall = CreateCall(getSystemInfo().DataValidation(), Args);
  Value *HAKCPointerBitCast = CreateReturnCast(HAKCPointer, DataAuthCall);
  HAKCPointer.SetAuthenticatedPointer(HAKCPointerBitCast);

  return HAKCPointerBitCast;
}

Value *
HAKCTransformer::CreateReturnCast(HAKCPointerBase &HAKCPointer,
                                        Value *V) {
  if (!V) {
    CommonHAKCAnalysis::getWriter(Error) << "NULL V\n";
    throw std::exception();
  }
  if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
    return HAKCIRBuilder.CreatePtrToInt(
        V, HAKCPointer.GetBaseDefinition()->getType());
  } else {
    return HAKCIRBuilder.CreateBitCast(
        V, HAKCPointer.GetBaseDefinition()->getType());
  }
}

Value *
HAKCTransformer::CreatePointerCast(HAKCPointerBase &HAKCPointer,
                                         PointerType *PointerTy) {
  if (!PointerTy) {
    CommonHAKCAnalysis::getWriter(Error) << "NULL PointerTy\n";
    throw std::exception();
  }

  if (HAKCPointer.GetBaseDefinition()->getType()->isIntegerTy()) {
    return HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(),
                                        PointerTy);
  } else {
    return HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(),
                                       PointerTy);
  }
}

Value *HAKCTransformer::CreateCodeAuthentication(
    HAKCPointerBase &HAKCPointer, Instruction *I) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);

  SmallVector<Value *> Args;
  CreateCodeAuthArguments(HAKCPointer, I, Args);
  auto *AuthResult = CreateCall(getSystemInfo().CodeValidation(), Args);
  auto *BitCast = HAKCIRBuilder.CreateBitCast(
      AuthResult, HAKCPointer.GetBaseDefinition()->getType());
  return BitCast;
}

GlobalVariable *HAKCTransformer::GetValidTargetCompartments(
    const HAKCCompartmentDivision &Division) const {
  const auto CompartmentID = Division.GetHAKCCompartment().GetCompartmentID();
  std::string name =
      "entry_tokens_" + std::to_string(CompartmentID->getZExtValue());
  GlobalVariable *EntryTokenArray = getModule().getNamedGlobal(name);
  if (EntryTokenArray) {
    if (!EntryTokenArray->getValueType()->isArrayTy()) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Invalid type for " << *EntryTokenArray << "\n";
      throw std::exception();
    }
    return EntryTokenArray;
  }

  SmallVector<Constant *> EntryTokenValues;
  SmallVector<hakc_compartment_id_t> IDs;
  IDs.push_back(CompartmentID->getZExtValue());
  for (auto &t : Division.GetHAKCCompartment().GetValidTargets()) {
    IDs.push_back(t->getZExtValue());
  }
  llvm::sort(IDs.begin(), IDs.end(),
             [](const hakc_compartment_id_t LHS,
                const hakc_compartment_id_t RHS) { return LHS < RHS; });

  for (auto ID : IDs) {
    auto TargetCompartment = CompartmentalizationPolicy.GetCompartment(ID);
    EntryTokenValues.push_back(TargetCompartment->GetEntryToken());
  }

  Type *EntryTokenTy =
      GetEntryTokenType(GetPointerAddrSpace(*EntryTokenValues.begin()));

  for (auto *Token : EntryTokenValues) {
    if (Token->getType() != EntryTokenTy) {
      CommonHAKCAnalysis::getWriter(Fatal)
          << "Token Type of " << *Token << " (" << *Token->getType()
          << ") does not match " << *EntryTokenTy << "\n";
      throw std::exception();
    }
  }

  auto *Initializer = ConstantArray::get(
      ArrayType::get(EntryTokenTy, EntryTokenValues.size()), EntryTokenValues);

  EntryTokenArray = dyn_cast<GlobalVariable>(
      getModule().getOrInsertGlobal(name, Initializer->getType()));
  EntryTokenArray->setConstant(true);
  EntryTokenArray->setLinkage(GlobalValue::InternalLinkage);
  
  CommonHAKCAnalysis::getWriter(Debug)
      << "Setting initializer for " << EntryTokenArray->getName() << " to be "
      << Initializer << " from token values ";
  for (auto *TokenValue : EntryTokenValues) {
    CommonHAKCAnalysis::getWriter(Debug) << "\n\t" << TokenValue;
  }
  CommonHAKCAnalysis::getWriter(Debug) << "\n";

  EntryTokenArray->setInitializer(Initializer);

  return EntryTokenArray;
}

GlobalVariable *
HAKCTransformer::GetValidTargetCompartments(Function *F) const {
  auto Division = CompartmentalizationPolicy.GetDivision(F);
  return GetValidTargetCompartments(Division);
}

CallInst *HAKCTransformer::CreateCall(Function *Callee,
                                            ArrayRef<Value *> Args) {
  auto *Call = HAKCIRBuilder.CreateCall(Callee, Args);

  /* The LLVM function checker throws an error when an inline-able function with
   * debug info contains a function call with no debug information.  So try to
   * set the appropriate debug info for this transfer */
  if (!Call->getDebugLoc()) {
    auto *I = &*HAKCIRBuilder.GetInsertPoint();
    if (I->getDebugLoc()) {
      Call->setDebugLoc(I->getDebugLoc());
    } else {
      /* Use the closest debug info to I */
      bool PastI = false;
      for (auto BBI = I->getParent()->begin(), BBE = I->getParent()->end();
           BBI != BBE; ++BBI) {
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

CallInst *HAKCTransformer::CreateCall(const function_def_t &Callee,
                                            ArrayRef<Value *> Args) {
  return CreateCall(Callee->GetFunction(), Args);
}

CallInst *HAKCTransformer::CreateCall(StringRef name, Type *RetTy,
                                            ArrayRef<Value *> Args) {
  std::vector<Type *> FunctionParamTypes;
  for (auto *Arg : Args) {
    FunctionParamTypes.push_back(Arg->getType());
  }

  FunctionType *FunctionCallTy =
      FunctionType::get(RetTy, FunctionParamTypes, false);

  auto *Func = ModuleAnalysis.GetFunctionByName(name, FunctionCallTy);
  if (!Func) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Could not find function " << name << " of type " << FunctionCallTy
        << " to be inserted into\n"
        << HAKCIRBuilder.GetInsertBlock()->getParent() << "\n";
    throw std::exception();
  }
  return CreateCall(Func, Args);
}

Instruction *HAKCTransformer::CreateSizedCompartmentTransfer(
    HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target,
    bool IsData, ConstantInt *Size) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);
  Instruction *Transfer;
  if (TargetIsKernel(Target)) {
    auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
    auto *SafePtr = dyn_cast<Instruction>(V);
    if (!SafePtr) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Unexpected Safe Pointer Type: " << *V << "\n";
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
HAKCTransformer::CreateCustomTransfer(HAKCPointerBase &HAKCPointer,
                                            GlobalValue *Target, bool IsData,
                                            ConstantInt *Size) {
  auto CustomTransfer = GetCustomTransferFunction(HAKCPointer);
  if (!CustomTransfer) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Could not find Transfer Function for "
        << HAKCPointer.GetBaseDefinition()->getType() << "\n";
    throw std::exception();
  }

  auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);
  return CustomTransfer->CreateTransfer(HAKCIRBuilder, TargetDivision,
                                        HAKCPointer, Size, IsData);
}

Instruction *HAKCTransformer::CreateSignWithDivision(
    HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target,
    bool IsData) {
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
  auto *OperandCast = HAKCIRBuilder.CreateBitCast(
      HAKCPointer.GetBaseDefinition(), HAKCIRBuilder.getPtrTy(AddrSpace));
  SmallVector<Value *> Args = {OperandCast, CompartmentIDValue, IsCodeValue};

  return CreateCallWithResultCast(ModuleAnalysis.GetCommonAnalysis()
                                      .GetSystemInfo()
                                      .SignWithDivision()
                                      ->GetName(),
                                  HAKCAuthenticationRetType(AddrSpace), Args,
                                  HAKCPointer.GetBaseDefinition());
}

bool HAKCTransformer::HAKCPointerHasCustomTransfer(
    HAKCPointerBase &HAKCPointer) {
  return GetCustomTransferFunction(HAKCPointer) != nullptr;
}

custom_transfer_def_t
HAKCTransformer::GetCustomTransferFunctionForType(
    HAKCTypeP HAKCTy) {
  CommonHAKCAnalysis::getWriter(Debug)
      << "Attempting to find Custom Transfer Function for Type " << *HAKCTy
      << "\n";
  for (auto &it : ModuleAnalysis.GetCommonAnalysis()
                      .GetSystemInfo()
                      .HAKCCustomTransfers()) {
    CommonHAKCAnalysis::getWriter(Debug)
        << "Custom Transfer Type " << *it->GetTargetType() << "\n";
    if (HAKCTy == it->GetTargetType()) {
      return it;
    }
  }
  return nullptr;
}

custom_transfer_def_t HAKCTransformer::GetCustomTransferFunction(
    HAKCPointerBase &HAKCPointer) {
  CommonHAKCAnalysis::getWriter(Debug)
      << "Attempting to find custom transfer for " << HAKCPointer << "\n";
  return GetCustomTransferFunctionForType(HAKCPointer.GetType());
}

Instruction *
HAKCTransformer::CreateDefaultTransfer(HAKCPointerBase &HAKCPointer,
                                             GlobalValue *Target, bool IsData,
                                             ConstantInt *Size) {
  SmallVector<Value *> TransferOperations;
  CreateTransferArguments(HAKCPointer, Target, IsData, Size,
                          TransferOperations);
  bool IsPerCPU =
      CommonHAKCAnalysis::IsPerCPUPointer(HAKCPointer.GetBaseDefinition());

  auto CompartmentTransfer = getSystemInfo().CompartmentTransfer(IsPerCPU);

  return CreateCallWithResultCast(CompartmentTransfer->GetFunction(),
                                  TransferOperations,
                                  HAKCPointer.GetBaseDefinition());
}

Instruction *
HAKCTransformer::CreateCallWithResultCast(StringRef Name, Type *RetTy,
                                                ArrayRef<Value *> Args,
                                                Value *ValueToTypeMatch) {
  auto *Call = CreateCall(Name, RetTy, Args);
  return CastCallToType(Call, ValueToTypeMatch);
}

Instruction *HAKCTransformer::CreateCallWithResultCast(
    Function *Callee, ArrayRef<Value *> Args, Value *ValueToTypeMatch) {
  auto *Call = CreateCall(Callee, Args);
  return CastCallToType(Call, ValueToTypeMatch);
}

Instruction *HAKCTransformer::CastCallToType(CallInst *Call,
                                                   Value *ValueToTypeMatch) {
  Value *ResultCast;
  if (isa<PtrToIntInst>(ValueToTypeMatch) ||
      ValueToTypeMatch->getType()->isIntegerTy()) {
    ResultCast =
        HAKCIRBuilder.CreatePtrToInt(Call, ValueToTypeMatch->getType());
  } else {
    ResultCast = HAKCIRBuilder.CreateBitCast(Call, ValueToTypeMatch->getType());
  }

  auto *Result = dyn_cast<Instruction>(ResultCast);
  if (!Result) {
    Result = Call;
  }

  return Result;
}

/**
 * Sometimes, a function will take a "void *" ("i8*") parameter and immediately
 * cast it to some destination type (struct). Sometimes, the destination type
 * (struct) has a custom transfer function.
 *
 * If we want to make this work, we need to do a little extra work.
 *
 * This function will try to find a custom transfer function by Type instead of
 * from the Value (which is of type "i8*") and generate a call to the custom
 * function instead of "hakc_transfer_to_clique".
 */
Instruction *HAKCTransformer::CreateVoidCastCompartmentTransfer(
    HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target,
    HAKCTypeP TypeToUse) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);

  /* just give safe pointer to kernel targets */
  if (TargetIsKernel(Target)) {
    auto *V = CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
    auto *SafePtr = dyn_cast<Instruction>(V);
    if (!SafePtr) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Unexpected Safe Pointer Type: " << *V << "\n";
      throw std::exception();
    }
    return SafePtr;
  }

  if (TypeToUse->IsPointerToPointer()) {
    CommonHAKCAnalysis::getWriter(Debug)
        << "TypeToUse " << *TypeToUse
        << " is a pointer to a pointer.\nAdding transfer starting at "
        << *HAKCIRBuilder.GetInsertPoint() << " in function "
        << *HAKCIRBuilder.GetInsertBlock()->getParent() << "\n";
    auto *FinalLocation = &*HAKCIRBuilder.GetInsertPoint();
    auto *SafePtr =
        CreateSafePointer(HAKCPointer, &*HAKCIRBuilder.GetInsertPoint());
    auto *Load = HAKCIRBuilder.CreateLoad(
        PointerType::get(getModule().getContext(),
                         GetPointerAddrSpace(HAKCPointer)),
        SafePtr);
    auto ManagedPointer = CreateNewManagedPointer(Load);
    CreateVoidCastCompartmentTransfer(*ManagedPointer,
                                      Load->getNextNonDebugInstruction(),
                                      Target, TypeToUse->GetPointeeType());
    auto *FinalTransfer = CreateSizedCompartmentTransfer(
        HAKCPointer, FinalLocation, Target, true, HAKCIRBuilder.getInt64(64));
    return FinalTransfer;
  }

  auto *size = TypeToUse->GetPointeeType()->GetSizeInBytes();

  if (size->equalsInt(0)) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Zero size for HAKCType " << *TypeToUse->GetPointeeType() << "\n";
    throw std::exception();
  }

  auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);

  /*
   * at this point, we know the dest type is a struct* and we know the actual
   * size
   *
   * even if a custom transfer function doesn't exist for the struct type, we
   * can do a more accurate transfer than the previous void* single-byte
   * transfer
   */
  Instruction *Transfer;
  CommonHAKCAnalysis::getWriter(Debug)
      << "LLVM type: " << *TypeToUse << "\nsize of type: " << *size << "\n";

  if (auto CustomTransfer = GetCustomTransferFunctionForType(TypeToUse)) {
    /* custom transfer exists, give the most specific transfer possible */
    Transfer = CustomTransfer->CreateTransferWithCasts(
        HAKCIRBuilder, TargetDivision, HAKCPointer, size, HAKCPointer.GetType(),
        TypeToUse, HAKCPointer.GetBaseDefinition()->getType()->isFunctionTy());

    CommonHAKCAnalysis::getWriter(Debug) << "custom xfer result:\n";
  } else {
    /* no custom transfer exists, give the next-most specific transfer possible,
     * correctly-sized generic transfer */
    Transfer =
        CreateSizedCompartmentTransfer(HAKCPointer, I, Target, true, size);

    CommonHAKCAnalysis::getWriter(Debug) << "sized xfer result:\n";
  }
  CommonHAKCAnalysis::getWriter(Debug) << *Transfer << "\n";
  return Transfer;
}

Instruction *HAKCTransformer::CreateCompartmentTransfer(
    HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target,
    bool IsData) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);

  auto ObjectSize = HAKCPointer.GetType()->GetPointeeType()->GetSizeInBytes();

  return CreateSizedCompartmentTransfer(HAKCPointer, I, Target, IsData,
                                        ObjectSize);
}

Function *HAKCTransformer::GetTransferFunction(Function *F) const {
  auto TransferFunctionName =
      ModuleAnalysis.GetCommonAnalysis().GetOutsideTransferName(F);
  CommonHAKCAnalysis::getWriter(Debug)
      << "Getting Transfer Function " << TransferFunctionName
      << " for Target Function " << *F << "\n";
  auto *TransferFunction = ModuleAnalysis.GetFunctionByName(
      TransferFunctionName, F->getFunctionType());
  if (TransferFunction == nullptr) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Could not create HAKC transfer function " << TransferFunctionName
        << "\n";
    throw std::exception();
  }

  TransferFunction->setCallingConv(F->getCallingConv());
  TransferFunction->setLinkage(F->getLinkage());
  TransferFunction->copyAttributesFrom(F);
  TransferFunction->setSection(F->getSection());

  return TransferFunction;
}

bool HAKCTransformer::NoKernelTransfers(Function *Target) {
  return CommonHAKCAnalysis::IsUncompartmentalizedSymbol(
      Target, CompartmentalizationPolicy);
}

Value *
HAKCTransformer::CreateActionCall(HAKCTransferAction &TransferAction,
                                        HAKCTransferState &TransferState) {
  auto ActionFunction = TransferAction.GetHAKCActionFunction();
  CommonHAKCAnalysis::getWriter(Debug)
      << "Creating Action for: " << TransferAction << "\n";
  SmallVector<Value *> ActionArgs;
  for (auto &ActionArg : ActionFunction.Args()) {
    switch (ActionArg.ArgUse) {
    case SignedPtr:
      ActionArgs.push_back(
          TransferState.GetManagedPointer().GetBaseDefinition());
      break;
    case Comp:
      ActionArgs.push_back(
          TransferState.GetDivision().GetHAKCCompartment().GetCompartmentID());
      break;
    case Div:
      ActionArgs.push_back(TransferState.GetDivision().GetDivisionID());
      break;
    case Size: {
      ActionArgs.push_back(TransferState.GetManagedPointer()
                               .GetType()
                               ->GetPointeeType()
                               ->GetSizeInBytes());
      break;
    }
    case IsCode:
      ActionArgs.push_back(TransferState.GetManagedPointer()
                                   .GetType()
                                   ->GetPointeeType()
                                   ->IsFunctionType()
                               ? getTrue()
                               : getFalse());
      break;
    case AccessToken:
      ActionArgs.push_back(TransferState.GetAccessToken());
      break;
    case ValidTargets:
      ActionArgs.push_back(
          GetValidTargetCompartments(TransferState.GetDivision()));
      break;
    case ValidTargetSize:
      ActionArgs.push_back(getInt64(TransferState.GetDivision()
                                        .GetHAKCCompartment()
                                        .GetValidTargetsSize()));
      break;
    default:
      CommonHAKCAnalysis::getWriter(Error)
          << "Unsupported argument use: " << ActionArg.ArgUse << "\n";
      throw std::exception();
    }
  }

  for (auto &LabeledArg : TransferAction.GetArguments()) {
    auto ArgLabel = LabeledArg.GetLabel();
    auto *ArgValue = TransferState.GetLabeledValue(ArgLabel);
    if (!ArgValue) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Could not find action value with label " << ArgLabel << "\n";
      throw std::exception();
    } else if (LabeledArg.GetIdx() >= ActionArgs.size()) {
      CommonHAKCAnalysis::getWriter(Error)
          << "Label " << ArgLabel << " has an invalid index "
          << LabeledArg.GetIdx() << " for calling " << ActionFunction << "\n";
      throw std::exception();
    }
    ActionArgs[LabeledArg.GetIdx()] = ArgValue;
  }
  auto *CallI = CreateCall(ActionFunction.GetFunction(), ActionArgs);
  TransferState.AddTransferActionValue(TransferAction, CallI);
  CommonHAKCAnalysis::getWriter(Debug)
      << "Created CallI: " << *CallI << "\n";
  return CallI;
}

Function *HAKCTransformer::CreateTransferFunction(Function *F) {
  if (F->isIntrinsic()) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Trying to create a HAKC Transfer function for " << F->getName()
        << "\n";
    throw std::exception();
  } else if (F->isVarArg()) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Trying to create HAKC Transfer function for variadic function "
        << F->getName() << "\n";
    throw std::exception();
  }

  Function *TransferFunction = CreateNonVariadicTransferFunction(F);

  return TransferFunction;
}

Function *HAKCTransformer::CreateTransferToVariadic(
    CallInst *Call, HAKCPointerManager *PointerManager) {
  auto *Target = Call->getCalledFunction();
  if (!Target) {
    CommonHAKCAnalysis::getWriter(Error) << "Null Call target\n";
    throw std::exception();
  }
  if (Target->isIntrinsic()) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Trying to create a HAKC Transfer function for " << Target->getName()
        << "\n";
    throw std::exception();
  }

  std::vector<Type *> ArgTypes;
  for (auto &Arg : Call->args()) {
    ArgTypes.push_back(Arg->getType());
  }

  FunctionType *TransferType =
      FunctionType::get(Target->getReturnType(), ArgTypes, false);
  Function *TransferFunction = nullptr;
  unsigned TargetTransferCount = 0;

  for (auto &it : VariadicTransferFunctions) {
    Function *Transfer = it.first;
    Function *TransferTarget = it.second;
    if (TransferTarget == Target) {
      TargetTransferCount += 1;
    }

    if (Transfer->getFunctionType() == TransferType &&
        TransferTarget == Target) {
      TransferFunction = Transfer;
    }
  }

  if (!TransferFunction) {
    auto TransferName = CommonHAKCAnalysis::getVariadicTransferName(Target);

    TransferFunction =
        ModuleAnalysis.GetFunctionByName(TransferName, TransferType);
    PopulateTransferFunction(Target, TransferFunction, Call, PointerManager);
    TransferFunction->setLinkage(GlobalValue::PrivateLinkage);

    /* For every variadic call site, we create a transfer function since we
     * cannot create a "traditional" transfer function since we don't know
     * how many pointers we need to transfer.  Therefore, we can't just call the
     * transfer function HAKC_XFER_foo like we do with traditional transfer
     * functions because of the risk of symbol name collision. So tack on
     * a unique identifier to every variadic transfer function, but do it after
     * we create the transfer function to keep the transfer function creation
     * code simple.
     */
    auto FinalName = TransferName + "_" + std::to_string(TargetTransferCount);
    TransferFunction->setName(FinalName);

    VariadicTransferFunctions[TransferFunction] = Target;
  }

  return TransferFunction;
}

void HAKCTransformer::TransferStructMembers(ConstantStruct *ConstStruct,
                                                  Function *GlobalTransfer,
                                                  GlobalValue *GlobalVar) {
  CommonHAKCAnalysis::getWriter(Debug)
      << "Transferring " << *ConstStruct << "\n";

  for (auto &Member : ConstStruct->operands()) {
    GlobalValue *Target = GlobalVar;
    if (auto *GlobalMember = dyn_cast<GlobalValue>(Member.get())) {
      Target = GlobalMember;
    }
    if (!TransferShouldBeCreated(Member.get(), Target)) {
      CommonHAKCAnalysis::getWriter(Debug)
          << "No transfer of member " << std::to_string(Member.getOperandNo())
          << " to " << Target << "\n";
      continue;
    }

    if (auto *StructMember = dyn_cast<ConstantStruct>(Member.get())) {
      TransferStructMembers(StructMember, GlobalTransfer, GlobalVar);
      continue;
    }

    if (CommonHAKCAnalysis::IsPointerLikeType(Member->getType())) {
      Value *Transfer, *GEP, *Load;

      CommonHAKCAnalysis::getWriter(Debug)
          << "Creating Transfer of Member "
          << std::to_string(Member.getOperandNo()) << " " << Member.get()
          << "\n";
      GEP = HAKCIRBuilder.CreateStructGEP(GlobalVar->getValueType(), GlobalVar,
                                          Member.getOperandNo());
      Load = HAKCIRBuilder.CreateLoad(Member->getType(), GEP);
      auto ManagedPointer = CreateNewManagedPointer(Load);
      Transfer = CreateCompartmentTransfer(
          *ManagedPointer, GlobalTransfer->getEntryBlock().getTerminator(),
          Target, !isa<Function>(Member.get()));
      HAKCIRBuilder.CreateStore(Transfer, GEP);
    }
  }
}

bool HAKCTransformer::TransferShouldBeCreated(Value *V,
                                                    GlobalValue *Target) {
  bool CreateTransfer = !TargetIsKernel(Target) &&
                        !isa<ConstantPointerNull>(V) &&
                        CommonHAKCAnalysis::IsPointerLikeType(V->getType());
  if (auto *I = dyn_cast<ConstantInt>(V)) {
    CreateTransfer = !I->equalsInt(0) && !I->isMinusOne();
  }

  return CreateTransfer;
}

Function *HAKCTransformer::PopulateGlobalTransfer(
    Function *GlobalTransfer, GlobalVariable *GlobalVar) {
  if (!GlobalTransfer->empty()) {
    return GlobalTransfer;
  }

  CommonHAKCAnalysis::getWriter(Debug)
      << "Initializing New Function " << GlobalTransfer->getName() << "\n";
  InitNewFunction(GlobalTransfer, "HAKCGlobalTransferEntry");
  auto *VoidRet = HAKCIRBuilder.CreateRetVoid();
  HAKCIRBuilder.SetInsertPoint(VoidRet);

  if (GlobalVar->hasInitializer()) {
    CommonHAKCAnalysis::getWriter(Debug)
        << "Creating Init Transfer of " << GlobalVar << "\n";
    if (auto *InitStruct =
            dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
      CommonHAKCAnalysis::getWriter(Debug) << "Transferring struct members\n";
      TransferStructMembers(InitStruct, GlobalTransfer, GlobalVar);
    } else if (CommonHAKCAnalysis::IsPointerLikeType(
                   GlobalVar->getInitializer()->getType())) {
      GlobalValue *Target = GlobalVar;
      if (auto *FuncPtr = dyn_cast<Function>(GlobalVar->getInitializer())) {
        Target = FuncPtr;
      }

      if (TransferShouldBeCreated(GlobalVar->getInitializer(), Target)) {
        CommonHAKCAnalysis::getWriter(Debug)
            << "Creating Transfer of " << Target << "\n";
        auto ManagedPointer =
            CreateNewManagedPointer(GlobalVar->getInitializer());
        auto *Transfer = CreateCompartmentTransfer(
            *ManagedPointer, VoidRet, Target,
            !isa<Function>(GlobalVar->getInitializer()));
        HAKCIRBuilder.CreateStore(Transfer, GlobalVar);
      }
    }
  }

  CommonHAKCAnalysis::getWriter(Debug)
      << "Finished initializing " << GlobalTransfer->getName() << "\n";
  return GlobalTransfer;
}

void HAKCTransformer::InitNewFunction(Function *F,
                                            StringRef EntryBlockName) {
  if (!F->empty()) {
    return;
  }

  auto *EntryBB =
      BasicBlock::Create(getModule().getContext(), EntryBlockName, F);
  auto *TestEntryBB = &F->getEntryBlock();
  if (EntryBB != TestEntryBB) {
    CommonHAKCAnalysis::getWriter(Error) << "Invalid Entry BasicBlock created\n";
    throw std::exception();
  }

  HAKCIRBuilder.SetInsertPoint(EntryBB);
}

HAKCTypeP
HAKCTransformer::InferHAKCType(Argument &Arg, CallInst *CallSite,
                                     HAKCPointerManager *PointerManager) const {

  auto *CallSitePtr = CallSite->getOperand(Arg.getArgNo());
  auto HAKCTy = ModuleAnalysis.GetTypeIdentifier().FindHAKCType(CallSitePtr);
  if (HAKCTy) {
    return HAKCTy;
  }
  auto ManagedCallSitePointer = PointerManager->GetManagedPointer(CallSitePtr);
  if (ManagedCallSitePointer && ManagedCallSitePointer->GetType()) {
    return ManagedCallSitePointer->GetType();
  }

  if (auto *LoadI = dyn_cast<LoadInst>(CallSitePtr)) {
    if (isa<GlobalVariable>(LoadI->getPointerOperand()) ||
        isa<AllocaInst>(LoadI->getPointerOperand())) {
      HAKCTy = ModuleAnalysis.GetTypeIdentifier().FindHAKCType(
          LoadI->getPointerOperand());
      return HAKCTy;
    }
  }

  return nullptr;
}

Function *HAKCTransformer::PopulateTransferFunction(
    Function *Target, Function *TransferFunction, CallInst *CallSite,
    HAKCPointerManager *PointerManager) {
  if (!TransferFunction->empty()) {
    return TransferFunction;
  }

  InitNewFunction(TransferFunction, "HAKCTransferEntry");
  auto *Unreachable = HAKCIRBuilder.CreateUnreachable();
  HAKCIRBuilder.SetInsertPoint(Unreachable);
  CommonHAKCAnalysis::getWriter(Debug)
      << "Populating " << TransferFunction->getName() << "\n";

  bool NoKernelXfers = NoKernelTransfers(Target);
  auto TargetDivision = CompartmentalizationPolicy.GetDivision(Target);
  SmallVector<Value *> Args;
  for (unsigned i = 0; i < TransferFunction->arg_size(); i++) {
    auto *ArgP = TransferFunction->getArg(i);
    CommonHAKCAnalysis::getWriter(Debug)
        << "Adding Argument " << *ArgP << "\n";
    Args.push_back(ArgP);
  }
  CommonHAKCAnalysis::getWriter(Debug)
      << "Creating Target call in Transfer function "
      << TransferFunction->getName() << " to " << Target->getName()
      << " with arguments\n";
  for (auto *A : Args) {
    CommonHAKCAnalysis::getWriter(Debug) << "\t" << A << "\n";
  }

  CallInst *TargetFunctionCall = HAKCIRBuilder.CreateCall(Target, Args);
  if (Target->doesNotReturn()) {
    HAKCIRBuilder.CreateUnreachable();
  } else {
    if (Target->getReturnType()->isVoidTy()) {
      HAKCIRBuilder.CreateRetVoid();
    } else {
      HAKCIRBuilder.CreateRet(TargetFunctionCall);
    }
  }
  Unreachable->removeFromParent();

  for (auto &Arg : TransferFunction->args()) {
    if (!CommonHAKCAnalysis::argShouldTransfer(&Arg) || NoKernelXfers) {
      continue;
    }
    auto ManagedPointer = CreateNewManagedPointer(&Arg);
    if (!ManagedPointer->GetType() && CallSite && PointerManager) {
      auto HAKCTy = InferHAKCType(Arg, CallSite, PointerManager);
      if (HAKCTy) {
        ManagedPointer->SetType(HAKCTy);
      }
    }
    HAKCTransferState TransferState(TargetDivision, *ManagedPointer);
    if (!TransferState) {
      continue;
    }

    HAKCIRBuilder.SetInsertPoint(TargetFunctionCall);
    CommonHAKCAnalysis::getWriter(Debug)
        << "Forward Argument Transfer with Arg: " << Arg << "\n";
    bool IsData = !Arg.getType()->isFunctionTy();
    for (auto &Preaction : ModuleAnalysis.GetCommonAnalysis()
                               .GetSystemInfo()
                               .PreTransferActions()) {
      CreateActionCall(*Preaction, TransferState);
    }
    auto *Transfer = CreateCompartmentTransfer(
        *ManagedPointer, &*HAKCIRBuilder.GetInsertPoint(), Target, IsData);
    TargetFunctionCall->setArgOperand(Arg.getArgNo(), Transfer);

    if (!Target->doesNotReturn()) {
      HAKCIRBuilder.SetInsertPoint(
          TargetFunctionCall->getNextNonDebugInstruction());
      CommonHAKCAnalysis::getWriter(Debug)
          << "Backward Argument Transfer with Arg: " << Arg << "\n";
      for (auto &Postaction : ModuleAnalysis.GetCommonAnalysis()
                                  .GetSystemInfo()
                                  .PostTargetActions()) {
        CreateActionCall(*Postaction, TransferState);
      }
    }
  }

  CommonHAKCAnalysis::VerifyFunction(TransferFunction);

  return TransferFunction;
}

Function *
HAKCTransformer::CreateNonVariadicTransferFunction(Function *F) {
  if (F->isIntrinsic()) {
    CommonHAKCAnalysis::getWriter(Error)
        << "Trying to create a HAKC Transfer function for " << F->getName()
        << "\n";
    throw std::exception();
  }

  auto *TransferFunction = GetTransferFunction(F);
  if (!TransferFunction->empty() || !TransferFunctionShouldBeCreated(F)) {
    return TransferFunction;
  }

  return PopulateTransferFunction(F, TransferFunction);
}

Value *HAKCTransformer::CreateBitCast(HAKCPointerBase &HAKCPointer,
                                            Type *TargetType, Instruction *I) {
  ValidateHAKCPointerAndLocation(HAKCPointer, I);
  auto AddrSpace = GetPointerAddrSpace(HAKCPointer);

  if (TargetType->isPointerTy() &&
      TargetType->getPointerAddressSpace() != AddrSpace) {
    CommonHAKCAnalysis::getWriter(Error)
        << "TargetType " << *TargetType << " has AddrSpace when casting "
        << HAKCPointer << "\n"
        << *I->getFunction() << "\n";
    throw std::exception();
  }
  Value *BitCast;
  if (HAKCPointer.GetType()->IsIntegerType() && TargetType->isPointerTy()) {
    BitCast = HAKCIRBuilder.CreateIntToPtr(HAKCPointer.GetBaseDefinition(),
                                           TargetType);
  } else if (HAKCPointer.GetType()->IsPointerType() &&
             TargetType->isIntegerTy()) {
    BitCast = HAKCIRBuilder.CreatePtrToInt(HAKCPointer.GetBaseDefinition(),
                                           TargetType);
  } else {
    BitCast = HAKCIRBuilder.CreateBitCast(HAKCPointer.GetBaseDefinition(),
                                          TargetType);
  }
  return BitCast;
}

Type *HAKCTransformer::HAKCAuthenticationRetType(unsigned AddrSpace) {
  auto *AuthCallType = ModuleAnalysis.GetCommonAnalysis()
                           .GetSystemInfo()
                           .DataValidation()
                           ->GetFunction()
                           ->getFunctionType();
  return AuthCallType->getReturnType();
}

ConstantInt *HAKCTransformer::getTrue() {
  return HAKCIRBuilder.getTrue();
}

ConstantInt *HAKCTransformer::getFalse() {
  return HAKCIRBuilder.getFalse();
}

ConstantInt *HAKCTransformer::getInt64(uint64_t Value) {
  return HAKCIRBuilder.getInt64(Value);
}

ConstantInt *HAKCTransformer::getInt32(uint32_t Value) {
  return HAKCIRBuilder.getInt32(Value);
}

ConstantInt *HAKCTransformer::GetDefaultObjectSize() {
  return getInt64(1);
}

bool HAKCTransformer::TargetIsKernel(GlobalValue *Target) {
  return CommonHAKCAnalysis::IsUncompartmentalizedSymbol(
      Target, CompartmentalizationPolicy);
}

unsigned
HAKCTransformer::GetPointerAddrSpace(HAKCPointerBase &HAKCPointer) {
  return GetPointerAddrSpace(HAKCPointer.GetBaseDefinition());
}

unsigned HAKCTransformer::GetPointerAddrSpace(Value *V) {
  unsigned AddrSpace = 0;
  if (V->getType()->isPointerTy()) {
    AddrSpace = V->getType()->getPointerAddressSpace();
  }
  return AddrSpace;
}

GlobalVariable *HAKCTransformer::AddCompartmentMetadataEntry(
    HAKCCompartment &Compartment) {
  return nullptr;
}

HAKCPointerBaseP
HAKCTransformer::CreateNewManagedPointer(Value *BaseDefinition) const {
  CommonHAKCAnalysis::getWriter(Debug)
      << "Creating new managed pointer for " << *BaseDefinition << "\n";
  auto ManagedPtr = std::make_shared<HAKCPointerBase>(BaseDefinition, 0);
  auto HAKCTy = ModuleAnalysis.GetTypeIdentifier().FindType(*ManagedPtr);
  if (!HAKCTy) {
    CommonHAKCAnalysis::getWriter(Debug) << "Could not find valid HAKCTy for value: " << *BaseDefinition << "\n";
    throw std::exception();
  } else {
    ManagedPtr->SetType(HAKCTy);
  }
  return ManagedPtr;
}
} // namespace llvm::hakc
