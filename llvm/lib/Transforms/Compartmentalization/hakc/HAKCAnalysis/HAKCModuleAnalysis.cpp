//
// Created by derrick on 8/20/21.
//
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/DIBuilder.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCTransferFunction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCFunctionAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

namespace llvm::hakc {
    HAKCModuleAnalysis::HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis, HAKCCompartmentalizationPolicy &Policy)
        : UsedCompartments(), CommonAnalysis(CommonAnalysis), AnalysisFunctions(),
          TypeIdentifier(CommonAnalysis.GetSystemInfo().GetTypeIdentifier()),
          Policy(Policy), Transformer(Policy, *this) {
        InitAnalysis();
    }

    void HAKCModuleAnalysis::InitAnalysis() {
        for (auto &F: GetModule().functions()) {
            if (FunctionNeedsAnalysis(&F)) {
                auto &Division = Policy.GetDivision(&F);
                auto Compartment = Division.GetHAKCCompartment();
                RegisterUsedCompartment(Compartment);
                AnalysisFunctions.push_back(&F);
            }
        }
        CommonHAKCAnalysis::SortFunctionList(AnalysisFunctions);
    }

    Module &HAKCModuleAnalysis::GetModule() {
        return CommonAnalysis.GetSystemInfo().GetModule();
    }

    HAKCTransformer &HAKCModuleAnalysis::GetTransformer() {
        return Transformer;
    }

    HAKCTypeIdentifier &HAKCModuleAnalysis::GetTypeIdentifier() {
        return TypeIdentifier;
    }

    std::string
    HAKCModuleAnalysis::getGlobalHAKCSectionName(GlobalVariable *GV) {
        auto Compartment = Policy.GetDivision(GV).GetHAKCCompartment();
        if (Compartment.IsUncompartmentalized()) {
            return GV->getSection().str();
        }

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

    void HAKCModuleAnalysis::RegisterUsedCompartment(HAKCCompartment &compartment) {
        if (!(compartment.IsUncompartmentalized())) {
            UsedCompartments.push_back(compartment);
        }
    }

    /**
        * @brief Moves all global values to the specified HAKC ELF section
        */
    void HAKCModuleAnalysis::MoveGlobalsToHAKCSection() {
        std::set<GlobalVariable *> globalsToChange;

        for (auto *pGlobal: globalsToChange) {
            auto finalName = getGlobalHAKCSectionName(pGlobal);
            auto compartment = Policy.GetDivision(pGlobal).GetHAKCCompartment();
            RegisterUsedCompartment(compartment);

            if (finalName != pGlobal->getSection()) {
                if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(pGlobal)) {
                    CommonHAKCAnalysis::getWriter() << "Changing section of global " << *pGlobal << " to section "
                            << finalName << " from " << pGlobal->getSection() << "\n";
                }
                pGlobal->setSection(finalName);
            }
        }
    }

    bool HAKCModuleAnalysis::FunctionNeedsAnalysis(Function *F) {
        bool needsAnalysis = !F->isIntrinsic() && !F->isDeclaration() && F->getSubprogram() != nullptr &&
                             !CommonHAKCAnalysis::IsOutsideTransferFunc(F) && !CommonAnalysis.IsHAKCFunction(F);


        // TODO: Add this to configuration file
        // linux
        // if (F->getName().contains("static_branch_")) {
        //        if (F->getName().contains(StringRef(*(*GetSysInfo()->GetMethods())["FunctionNeedsAnalysis"].begin()))) {
        //            /* These functions call inline assembly that needs to be
        //                * constant at compile time, so we can't analyze them.
        //                * We ensure that any pointer passed to these functions have
        //                * no signature in argNeedsAnalysis.
        //                */
        //            if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(F)) {
        //                CommonHAKCAnalysis::getWriter() << F->getName() << " does not need analysis\n";
        //            }
        //            needsAnalysis = false;
        //        }

        if (!needsAnalysis) {
            goto out;
        }
        for (auto *user: F->users()) {
            if (!isa<CallInst>(user)) {
                /* Function is passed into a global variable */
                needsAnalysis = true;
            }
        }

    out:
        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(F)) {
            CommonHAKCAnalysis::getWriter() << F->getName();
            if (!needsAnalysis) {
                CommonHAKCAnalysis::getWriter() << " does not need ";
            } else {
                CommonHAKCAnalysis::getWriter() << " needs ";
            }
            CommonHAKCAnalysis::getWriter() << "analysis\n";
        }

        return needsAnalysis;
    }

    Function *HAKCModuleAnalysis::GetFunctionByName(StringRef Name, FunctionType *FuncTy) {
        auto Callee = GetModule().getOrInsertFunction(Name, FuncTy);
        return dyn_cast<Function>(Callee.getCallee());
    }

    bool HAKCModuleAnalysis::isModuleCompartmentalized() {
        auto Search = [](HAKCCompartment &Compartment) {
            return !Compartment.IsUncompartmentalized();
        };

        return llvm::any_of(UsedCompartments, Search);
    }

    bool HAKCModuleAnalysis::AliasShouldBeCreated(Function *F) {
        //         /* See note in HAKCModuleAnalysisLinux::TransferFunctionShouldBeCreated */
        //         if (F->isDeclaration() && FunctionDefinedInAssembly(F)) {
        //             return false;
        //         }
        //         return HAKCModuleAnalysisLinux::AliasShouldBeCreated(F);
        return TransferFunctionShouldBeCreated(F);
    }

    bool HAKCModuleAnalysis::FunctionDefinedInAssembly(Function *F) {
        StringRef ModuleAsm = GetModule().getModuleInlineAsm();
        std::string SearchTerm = "[[:space:];]+";
        SearchTerm += F->getName().str();
        SearchTerm += ":";
        Regex NameRegex(SearchTerm);
        SmallVector<StringRef, 2> Matches;

        auto NameInAssembly = NameRegex.match(ModuleAsm, &Matches, nullptr);
        if (NameInAssembly && CommonAnalysis.GetSystemInfo().OutputDebugInfo(F)) {
            CommonHAKCAnalysis::getWriter() << F->getName() << " was found in the Module inline assembly: " <<
                    Matches[0] << "\n";
        } else if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(F)) {
            CommonHAKCAnalysis::getWriter() << "Could not find " << SearchTerm << " in\n" << ModuleAsm << "\n";
        }
        return NameInAssembly;
    }

    bool _useEscapes(Use &U, std::set<Value *> &expected) {
        /* If F is used in a global variable */
        if (auto *gv = dyn_cast<GlobalVariable>(U.getUser())) {
            return gv->getSection() != ".discard.addressable";
        } else if (isa<ConstantStruct>(U.getUser()) || isa<SelectInst>(U.getUser())) {
            return true;
        } else if (auto *call = dyn_cast<CallInst>(U.getUser())) {
            for (auto &arg: call->args()) {
                if (arg.get() == U.get()) {
                    return true;
                }
            }
        } else if (auto *bc = dyn_cast<BitCastOperator>(U.getUser())) {
            for (Use &u: bc->uses()) {
                if (expected.find(u.get()) != expected.end()) {
                    continue;
                }
                expected.insert(u.get());
                if (_useEscapes(u, expected)) {
                    return true;
                }
            }
        } else if (isa<ICmpInst>(U.getUser())) {
            return false;
        } else if (auto *store = dyn_cast<StoreInst>(U.getUser())) {
            if (store->getValueOperand() == U.get()) {
                return true;
            }
        } else if (auto *phi = dyn_cast<PHINode>(U.getUser())) {
            for (Use &u: phi->uses()) {
                if (expected.find(u.get()) != expected.end()) {
                    continue;
                }
                expected.insert(u.get());
                if (_useEscapes(u, expected)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool in_debug = false;

    bool useEscapes(Use &U) {
        std::set<Value *> examined;
        bool escapes = _useEscapes(U, examined);
        if (in_debug) {
            CommonHAKCAnalysis::getWriter() << "Use " << U.get() << " in " << U.getUser();
            if (escapes) {
                CommonHAKCAnalysis::getWriter() << " escapes\n";
            } else {
                CommonHAKCAnalysis::getWriter() << " does not escape\n";
            }
        }
        return escapes;
    }

    void HAKCModuleAnalysis::TransformModule() {
        MoveGlobalsToHAKCSection();
        TransformFunctions();
        AddCompartmentMetadata();

        CreateInitGlobalMemberTransfers();
        AddTransferFunctions();
    }

    void HAKCModuleAnalysis::TransformFunctions() {
        for (auto *F: AnalysisFunctions) {
            HAKCFunctionAnalysis FunctionTransformation(F, *this, Policy);
            FunctionTransformation.InstrumentCode();
        }
    }

    void HAKCModuleAnalysis::performTransformations() {
        TransformModule();
        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
            CommonHAKCAnalysis::getWriter() << "Final Module After Transformations:\n" << GetModule() << "\n";
        }
    }

    bool HAKCModuleAnalysis::functionEscapes(Function *F) {
        if (F->isIntrinsic()) {
            return false;
        }
        for (auto &U: F->uses()) {
            if (useEscapes(U)) {
                return true;
            }
            if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(F)) {
                CommonHAKCAnalysis::getWriter() << "Use " << U.getUser() << " does not escape\n";
            }
        }
        Function *transfer = GetModule().getFunction(CommonAnalysis.GetOutsideTransferName(F));
        if (transfer) {
            /* A transfer function reference has been made, so it escapes */
            return true;
        }

        return !CommonHAKCAnalysis::FunctionIsStatic(F);
    }

    CommonHAKCAnalysis &HAKCModuleAnalysis::GetCommonAnalysis() {
        return CommonAnalysis;
    }

    bool HAKCModuleAnalysis::TransferFunctionShouldBeCreated(Function *F) {
        if (F->isDeclaration()) {
            return false;
        }
        /*if (CommonHAKCAnalysis::NoKernelTransferFunctionsSet()) {
            auto Compartment = Policy.GetDivision(F).GetHAKCCompartment();
            if (Compartment.IsUncompartmentalized()) {
                return false;
            }
        }*/

        return CommonHAKCAnalysis::FunctionHasPointerArg(F);
    }

    void HAKCModuleAnalysis::AddTransferFunctions() {
        FunctionList FuncsNeedingTransfers;
        for (auto &F: GetModule().functions()) {
            auto Compartment = Policy.GetDivision(&F).GetHAKCCompartment();

            if (!Compartment.IsUncompartmentalized() && CommonAnalysis.functionIsTransferCandidate(&F, Policy) &&
                !hakc::CommonHAKCAnalysis::IsOutsideTransferFunc(&F) &&
                functionEscapes(&F)) {
                FuncsNeedingTransfers.push_back(&F);
            }
        }
        CommonHAKCAnalysis::SortFunctionList(FuncsNeedingTransfers);
        for (auto *Funcp: FuncsNeedingTransfers) {
            Function &F = *Funcp;
            auto debug_output = CommonAnalysis.GetSystemInfo().OutputDebugInfo(Funcp);
            Function *transferFunc = nullptr;

            if (CommonAnalysis.functionIsTransferCandidate(&F, Policy)) {
                auto Compartment = Policy.GetDivision(&F).GetHAKCCompartment();
                transferFunc = GetTransformer().CreateTransferFunction(&F);
                if (!transferFunc) {
                    CommonHAKCAnalysis::getWriter() << "Could not create transfer for " << F.getName() << "\n";
                    throw std::exception();
                }
                bool TransferAlreadyExisted = (transferFunc->getInstructionCount() > 0 && !F.isDeclaration());
                if (debug_output) {
                    if (TransferAlreadyExisted) {
                        CommonHAKCAnalysis::getWriter() << "Retrieved transfer function " <<
                                transferFunc->getName();
                    } else {
                        CommonHAKCAnalysis::getWriter() << "Created transfer function " << transferFunc->getName();
                    }
                    CommonHAKCAnalysis::getWriter() << " in compartment "
                            << std::to_string(Compartment.GetCompartmentIDValue()) << "\n";
                    if (!TransferAlreadyExisted) {
                        CommonHAKCAnalysis::getWriter() << *transferFunc << "\n";
                    }
                }

                if (F.isDeclaration() && debug_output) {
                    CommonHAKCAnalysis::getWriter() << F.getName() << " is a declaration\n";
                }
            } else if (debug_output) {
                CommonHAKCAnalysis::getWriter() << "No transfer created for " << F.getName() << "\n";
            }
            if (!transferFunc) {
                transferFunc = GetFunctionByName(CommonAnalysis.GetOutsideTransferName(&F), F.getFunctionType());
                /* Ensure that transfer functions not defined here are treated
                * the same as the function we are replacing */
                transferFunc->setLinkage(F.getLinkage());
                transferFunc->copyAttributesFrom(&F);
            }

            if (CommonAnalysis.ValueShouldBeReplacedWithTransfer(&F, Policy)) {
                if (!CommonAnalysis.IsNoTransferFunction(&F)) {
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Replacing uses of " << F.getName()
                                << " with " << transferFunc->getName() << "\n";
                    }
                    in_debug = debug_output;
                    F.replaceUsesWithIf(transferFunc, useEscapes);
                    if (debug_output) {
                        CommonHAKCAnalysis::getWriter() << "Done\n" << GetModule() << "\n";
                    }
                }
            }
            if (AliasShouldBeCreated(&F)) {
                auto OrigName = F.getName().str();
                auto NewName = CommonHAKCAnalysis::getOriginalTransformedName(&F);
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Changing name from " << F.getName()
                            << " to " << NewName << "\n";
                }
                F.setName(NewName);

                auto *alias = GlobalAlias::create(OrigName, transferFunc);
                if (debug_output) {
                    CommonHAKCAnalysis::getWriter() << "Final Transfer:\n" << *transferFunc << "\nAlias: " << *alias
                            << "\n";
                }
                auto *OrigSP = F.getSubprogram();
                if (OrigSP) {
                    DIBuilder DIB(*F.getParent(), false, OrigSP->getUnit());
                    DISubprogram::DISPFlags SPFlags = DISubprogram::SPFlagDefinition |
                                                      DISubprogram::SPFlagOptimized |
                                                      DISubprogram::SPFlagLocalToUnit;
                    auto NewSP = DIB.createFunction(OrigSP->getScope(), transferFunc->getName(),
                                                    transferFunc->getName(),
                                                    OrigSP->getFile(), 0, OrigSP->getType(), 0, DINode::FlagZero,
                                                    SPFlags);
                    transferFunc->setSubprogram(NewSP);
                }
            }
        }
    }

    bool HAKCModuleAnalysis::ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct) {
        bool Result = false;
        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
            CommonHAKCAnalysis::getWriter() << "TransferIsNeeded Checking " << *ConstStruct << "\n";
        }
        for (auto &Member: ConstStruct->operands()) {
            auto *Def = CommonAnalysis.getDef(Member.get(), false);
            if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
                CommonHAKCAnalysis::getWriter() << "Checking struct member " << *Member << " with Def " << *Def
                        << "\n";
            }
            if (isa<ConstantPointerNull>(Def)) {
                continue;
            }
            if (auto *GlobalVal = dyn_cast<GlobalValue>(Def)) {
                Result = !CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalVal, Policy);
            } else if (auto *StructMember = dyn_cast<ConstantStruct>(Def)) {
                Result = ConstantStructTransferIsNeeded(StructMember);
            }

            if (Result) {
                break;
            }
        }

        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
            CommonHAKCAnalysis::getWriter() << __FUNCTION__ << " Result: " << std::to_string(Result) << "\n";
        }
        return Result;
    }

    bool HAKCModuleAnalysis::TransferIsNeeded(GlobalVariable *GlobalVar) {
        bool IsKernelSym = CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalVar, Policy);
        bool Result = GlobalVar->hasInitializer() && !IsKernelSym;
        if (Result) {
            if (auto *ConstStruct = dyn_cast<ConstantStruct>(GlobalVar->getInitializer())) {
                Result = ConstantStructTransferIsNeeded(ConstStruct);
            } else {
                if (isa<GlobalValue>(GlobalVar->getInitializer())) {
                    Result = !IsKernelSym;
                } else if (isa<ConstantPointerNull>(GlobalVar->getInitializer())) {
                    Result = false;
                } else {
                    Result = CommonHAKCAnalysis::IsPointerLikeType(GlobalVar->getInitializer()->getType());
                }
            }
        }

        return Result;
    }

    void HAKCModuleAnalysis::CreateInitGlobalMemberTransfers() {
        std::vector<GlobalVariable *> GlobalsToModifyDuringInit;
        for (auto &GV: GetModule().globals()) {
            if (TransferIsNeeded(&GV)) {
                GlobalsToModifyDuringInit.push_back(&GV);
            }
        }
        CommonHAKCAnalysis::SortGlobalList(GlobalsToModifyDuringInit);

        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
            CommonHAKCAnalysis::getWriter() << "Creating Init transfer functions for "
                    << std::to_string(GlobalsToModifyDuringInit.size()) << " globals:\n";
            for (auto *GlobToTransfer: GlobalsToModifyDuringInit) {
                CommonHAKCAnalysis::getWriter() << GlobToTransfer->getName() << "\n";
            }
        }

        for (auto *GlobToTransfer: GlobalsToModifyDuringInit) {
            auto *InitTransfer = CreateInitTransfer(GlobToTransfer);
            if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(GlobToTransfer)) {
                CommonHAKCAnalysis::getWriter() << "Created InitTransfer " << InitTransfer->getName() << "\n";
            }
        }
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferPrefix() const {
        return "hakc_glob_init_xfer_";
    }


    Function *
    HAKCModuleAnalysis::CreateInitTransfer(GlobalVariable *GlobalVar) {
        if (!GlobalVar->hasInitializer()) {
            CommonHAKCAnalysis::getWriter() << GlobalVar << " has no initializer\n";
            throw std::exception();
        }

        SmallString<128> FunctionName = GlobalInitTransferPrefix();

        for (auto letter: GlobalVar->getName()) {
            if (letter != '@') {
                FunctionName += letter;
            }
        }

        auto *GlobalTransferTy = FunctionType::get(Type::getVoidTy(GetModule().getContext()), {});
        auto *GlobalInitFunc = GetFunctionByName(FunctionName, GlobalTransferTy);
        if (!GlobalInitFunc) {
            CommonHAKCAnalysis::getWriter() << "Could not get Global Transfer function " << FunctionName << "\n";
            throw std::exception();
        }

        if (GlobalInitFunc->empty()) {
            PopulateGlobalInitTransferFunc(GlobalInitFunc, GlobalVar);
            if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(GlobalVar)) {
                CommonHAKCAnalysis::getWriter() << "Finished Populating Global Init Transfer\n" << *GlobalInitFunc
                        << "\n";
            }
        }

        return GlobalInitFunc;
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferSectionName() const {
        return ".hakc.glob_init.text";
    }

    StringRef HAKCModuleAnalysis::GlobalInitTransferPointerSectionName() const {
        return ".hakc.global_init.data";
    }

    std::string
    HAKCModuleAnalysis::GlobalVariableROSectionName(GlobalVariable *GlobalVar) {
        auto Compartment = Policy.GetDivision(GlobalVar).GetHAKCCompartment();
        std::string SectionName = ".hakc.";
        SectionName += std::to_string(Compartment.GetCompartmentIDValue());
        SectionName += ".ro_data";

        return SectionName;
    }

    void HAKCModuleAnalysis::PopulateGlobalInitTransferFunc(Function *GlobTransfer, GlobalVariable *GlobalVar) {
        auto debug_output = CommonAnalysis.GetSystemInfo().OutputDebugInfo(GlobalVar);
        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Populating Global Init Transfer Function "
                    << GlobTransfer->getName()
                    << "\n";
        }

        GlobTransfer->setSection(GlobalInitTransferSectionName());
        if (!GlobTransfer->empty()) {
            return;
        }

        if (GlobalVar->isConstant()) {
            GlobalVar->setConstant(false);
            GlobalVar->setSection(GlobalVariableROSectionName(GlobalVar));
        }

        if (debug_output) {
            CommonHAKCAnalysis::getWriter() << "Starting Global Init Population\n";
        }
        GetTransformer().PopulateGlobalTransfer(GlobTransfer, GlobalVar, debug_output);
        CommonHAKCAnalysis::VerifyFunction(GlobTransfer);

        auto GlobalTrackerName = GlobTransfer->getName() + "_loc";
        auto *TransferPointer = dyn_cast<GlobalVariable>(
            GetModule().getOrInsertGlobal(GlobalTrackerName.getSingleStringRef(), GlobTransfer->getType()));
        TransferPointer->setConstant(true);
        TransferPointer->setInitializer(GlobTransfer);
        TransferPointer->setSection(GlobalInitTransferPointerSectionName());
    }

    void HAKCModuleAnalysis::AddCompartmentMetadata() {
        for (auto Compartment: UsedCompartments) {
            if (!Compartment.IsUncompartmentalized()) {
                GetTransformer().AddCompartmentMetadataEntry(Compartment);
            }
        }
    }

    // Get the StructType representing a kernel (module) parameter
    StructType *HAKCModuleAnalysis::GetKernelParamType() {
        // linux
        return llvm::StructType::getTypeByName(GetModule().getContext(), llvm::StringRef("struct.kernel_param"));
    }

    GlobalValue *HAKCModuleAnalysis::ExtractGlobalFromKernelParam(GlobalVariable *GV) {
        // the result of walking through the kernel param struct
        // until we get to the actual global value backing the parameter
        GlobalValue *kernparam;

        auto *KernelParamType = GetKernelParamType();
        // type not found, just do nothing
        if (!KernelParamType) {
            return nullptr;
        }

        // trying to find globals of type GetKernelParamType()
        if (auto *StructTy = dyn_cast<StructType>(GV->getValueType())) {
            if (!(StructTy->getName() == KernelParamType->getName())) {
                return nullptr; // someone passed us a struct that wasn't a kernel param struct
            }
        } else {
            return nullptr; // this is not good, don't give non-structs to this function
        }

        // we know it is a kernel param now, moving on

        // cast the value into a ConstantStruct so we can pick it apart
        auto *kp_struct = dyn_cast<ConstantStruct>(GV->getInitializer());

        // do we have struct kernel_param kp now
        if (kp_struct) {
            // the anonymous union that holds the Value we actually want
            // is the last element of the struct
            auto num_ops = kp_struct->getNumOperands();
            Constant *last_op = kp_struct->getOperand(num_ops - 1);

            // this holds kp->arg
            if (last_op) {
                // cast the union into a ConstantStruct so we can pick it apart
                auto *kparg_union = dyn_cast<ConstantStruct>(last_op);

                if (kparg_union) {
                    // get the only thing in the struct, that's how unions work?
                    // this constant is kp->arg, sort of
                    Constant *kparg_val = kparg_union->getOperand(0);
                    // check that the value in there is a BitCastOperator
                    // it is bit-casting the global that backs the parameter
                    if (auto *kparg_val_bco = dyn_cast<BitCastOperator>(kparg_val)) {
                        // extract the pointer from the BitCastOperator
                        Value *gv_from_bco = kparg_val_bco->getOperand(0);

                        if (!(kernparam = dyn_cast<GlobalValue>(gv_from_bco))) {
                            // if it isn't a global value, that's bad
                            return nullptr;
                        }

                        // now we have kp->arg
                    }
                    // the thing in the union isn't a BitCastOperator, that's bad
                    else {
                        return nullptr;
                    }
                }
                // we couldn't get the union out of the union struct, that's bad
                else {
                    return nullptr;
                }
            }
            // we couldn't get the union struct at all out of the param struct, that's bad
            else {
                return nullptr;
            }
        }
        // we couldn't even get the kernel param struct as a struct, that's bad
        else {
            return nullptr;
        }

        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(GV)) {
            CommonHAKCAnalysis::getWriter() << "processing kernel param\n" << *kernparam << "\n";
        }

        return kernparam;
    }

    // we generate these for all kernel params, some may go unused by the actual module loader
    // (non-pointer params are ignored by the loader when it comes to transferring)
    //    void HAKCModuleAnalysis::transferModuleParams() {
    //
    //        if (!isModuleCompartmentalized()) {
    //            return;
    //        }
    //
    //        auto *KernelParamType = GetKernelParamType();
    //        // type not found, just do nothing
    //        if (!KernelParamType) {
    //            return;
    //        }
    //
    //        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo()) {
    //            CommonHAKCAnalysis::getWriter() << "kernel param type is: " << *KernelParamType << "\n";
    //        }
    //
    //        // inspect all globals
    //        for (auto &Global: GetModule().globals()) {
    //            // trying to find globals of type GetKernelParamType()
    //            if (auto *StructTy = dyn_cast<StructType>(Global.getValueType())) {
    //                // if true, the type of Global matches GetKernelParamType
    //                if (StructTy == KernelParamType) {
    //                    if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(&Global)) {
    //                        CommonHAKCAnalysis::getWriter() << "found kernel param: " << Global << "\n";
    //                    }
    //
    //                    // generate a GetCtx function for the parameter and update
    //                    // function pointer array
    //                    generateModuleParamGetCtxFunction(&Global);
    //                }
    //            }
    //        }
    //    }

    void HAKCModuleAnalysis::emitModParamGetCtx(GlobalValue *kernparam) {
        // linux
        // type of void*
        PointerType *PointerTy = PointerType::get(IntegerType::get(GetModule().getContext(), 8), 0);

        // two args
        std::vector<Type *> FuncTy_args;
        // first arg points to param
        FuncTy_args.push_back(PointerTy);
        // second arg is int64_t flag (0 to return param's access token, 1 to return param's color)
        FuncTy_args.push_back(IntegerType::get(GetModule().getContext(), 64));

        // type of function that returns int64_t, takes (void *, int64_t)
        FunctionType *FuncTy = FunctionType::get(IntegerType::get(GetModule().getContext(), 64),
                                                 FuncTy_args,
                                                 false);

        // create a function named "hakc_modparam_getctx_paramname"
        auto c = GetModule().getOrInsertFunction(MODPARAM_GETCTX_PREFIX.str() + kernparam->getName().str(),
                                                 FuncTy);

        auto *constc = dyn_cast<Constant>(c.getCallee());
        auto *getctx = cast<Function>(constc);
        getctx->setCallingConv(CallingConv::C);

        // put "hakc_modparam_getctx_paramname" in a special text section in the module
        getctx->setSection(HAKC_MODPARAM_TEXT_SECTION);

        Function::arg_iterator args = getctx->arg_begin();
        // param pointer
        Value *pointerArg = args++;
        // 0 to return context, 1 to return color
        Value *returnTypeArg = args++;

        // create entry basic block in our new function
        BasicBlock *block = BasicBlock::Create(GetModule().getContext(), "entry", getctx);
        //
        IRBuilder<> builder(block);
        // constant zero for compare/select
        Value *czero = ConstantInt::get(IntegerType::get(GetModule().getContext(), 64), 0);

        // get HAKC symbol for the kernel parameter Value
        auto &Division = Policy.GetDivision(kernparam);

        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(kernparam)) {
            CommonHAKCAnalysis::getWriter() << kernparam << " " << Division << "\n";
        }

        // cast kernparam to a void*
        Value *voidCast;
        auto AddrSpace = hakc::HAKCTransformer::GetPointerAddrSpace(kernparam);

        if (kernparam->getType()->isIntegerTy()) {
            voidCast = builder.CreateIntToPtr(kernparam, builder.getPtrTy(AddrSpace));
        } else {
            voidCast = builder.CreateBitCast(kernparam, builder.getPtrTy(AddrSpace));
        }


        // if returnTypeArg == 0, next step will use access token for return value
        // else, use color for return value
        Value *tokEqZero = builder.CreateICmpEQ(returnTypeArg, czero);
        Value *tokColSelect = builder.CreateSelect(tokEqZero, Division.GetAccessToken(), Division.GetDivisionID());

        // check if the address passed in matches address of kernparam
        Value *pointerArgEq = builder.CreateICmpEQ(pointerArg, voidCast);
        // if it does, return the previously selected token/color
        // it it isn't a match, return zero
        Value *ctxSelect = builder.CreateSelect(pointerArgEq, tokColSelect, czero);
        // function is done
        builder.CreateRet(ctxSelect);

        if (CommonAnalysis.GetSystemInfo().OutputDebugInfo(kernparam)) {
            CommonHAKCAnalysis::getWriter() << *getctx << "\n";
        }

        CommonHAKCAnalysis::VerifyFunction(getctx);

        // generate function pointer and place in modparam fp section
        auto CtxFPName = getctx->getName() + "_fp";
        auto *gcfp = dyn_cast<GlobalVariable>(
            GetModule().getOrInsertGlobal(CtxFPName.getSingleStringRef(), getctx->getType()));
        gcfp->setSection(HAKC_MODPARAM_FUNCP_SECTION);
        gcfp->setLinkage(GlobalValue::ExternalLinkage);
        gcfp->setConstant(true);
        gcfp->setInitializer(getctx);
    }

    bool HAKCModuleAnalysis::FunctionIsInAnalysisSet(Function *F) {
        return CommonHAKCAnalysis::IsFunctionInFunctionList(
            F, make_range(AnalysisFunctions.begin(), AnalysisFunctions.end()));
    }

    // TODO: Add this to config definition
    // takes a KernelParam and generate a function to get the HAKC signing context
    // for the actual backing global variable
    // used to correctly transfer charp parameters
    //    void HAKCModuleAnalysis::generateModuleParamGetCtxFunction(GlobalVariable *GV) {
    //        // linux
    //
    //        GlobalValue *kernparam = ExtractGlobalFromKernelParam(GV);
    //
    //        if (!kernparam) {
    //            CommonHAKCAnalysis::getWriter() << "Could not extract global from kernel param " << *GV << "\n";
    //            throw std::exception();
    //        }
    //
    //        emitModParamGetCtx(kernparam);
    //    }

    //    void HAKCModuleAnalysis::updateCallParameters(const std::map<Function *, std::set<CallInst *>> &calls_map) {
    //        // linux
    //        for (auto &pair: calls_map) {
    //            Function *F = pair.first;
    //            auto debug_output = CommonAnalysis.GetSystemInfo().OutputDebugInfo(F);
    //
    //            for (auto &call: pair.second) {
    //                auto HAKCTransferFunction = GetHAKCTransferDef(call->getCalledFunction()->getName());
    //                if (HAKCTransferFunction) {
    //                    if (debug_output) {
    //                        CommonHAKCAnalysis::getWriter() << "Updating HAKC call parameters for " << *call << "\n";
    //                    }
    //                    hakc_compartment_id_t id;
    //                    StringRef transferTargetName = F->getName();
    //                    if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
    //                        transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
    //                        Function *TransferTarget = GetModule().getFunction(transferTargetName);
    //                        id = GetTransformer().getFunctionCompartmentID(TransferTarget);
    //                    } else {
    //                        id = GetTransformer().getFunctionCompartmentID(F);
    //                    }
    //                    if (id < 0) {
    //                        CommonHAKCAnalysis::getWriter() << "Could not find Compartment ID for function "
    //                                                        << transferTargetName << "\n";
    //                        throw std::exception();
    //                    }
    //                    if (debug_output) {
    //                        CommonHAKCAnalysis::getWriter() << "Updating index " << std::to_string(
    //                                HAKCTransferFunction->GetCompartmentIdIdx()) << " (";
    //                        call->getArgOperand(HAKCTransferFunction->GetCompartmentIdIdx())->print(
    //                                CommonHAKCAnalysis::getWriter());
    //                        CommonHAKCAnalysis::getWriter() << ") to " << std::to_string(id) << "\n";
    //                    }
    //                    call->setArgOperand(HAKCTransferFunction->GetCompartmentIdIdx(),
    //                                        GetTransformer().GetHAKCCompartmentValue(id));
    //
    //                    if (HAKCTransferFunction->HasColorIdx()) {
    //                        ConstantInt *color;
    //                        if (CommonHAKCAnalysis::IsOutsideTransferFunc(F)) {
    //                            transferTargetName = F->getName().substr(OUTSIDE_TRANSFER_PREFIX.size());
    //                            auto *TransferTarget = GetModule().getFunction(transferTargetName);
    //                            color = getSymbolColor(TransferTarget);
    //                        } else {
    //                            color = getFunctionColor(F);
    //                        }
    //
    //                        if (!color) {
    //                            CommonHAKCAnalysis::getWriter() << "Could not find Color for function " << F->getName()
    //                                                            << "\n";
    //                            throw std::exception();
    //                        }
    //                        call->setArgOperand(HAKCTransferFunction->GetColorIdx(), color);
    //                    }
    //                    if (debug_output) {
    //                        CommonHAKCAnalysis::getWriter() << "After update call is " << *call << "\n";
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    bool HAKCModuleAnalysis::valueIsReadonlyPtr(Value *value) {
    //        // linux
    //        bool result = CommonAnalysis.valueIsReadonlyPtr(value);
    //        if (!result) {
    //            if (auto *callInst = dyn_cast<CallInst>(value)) {
    //                /* We may have done some global transfer beforehand, so check for that */
    //                if (callInst->getCalledFunction() &&
    //                    callInst->getCalledFunction()->getName() == "hakc_sign_pointer_with_color") {
    //                    auto *isCode = dyn_cast<ConstantInt>(
    //                            callInst->getArgOperand(callInst->arg_size() - 1));
    //                    result = isCode->isOne();
    //                }
    //            }
    //        }
    //
    //        return result;
    //    }
} // namespace hakc
