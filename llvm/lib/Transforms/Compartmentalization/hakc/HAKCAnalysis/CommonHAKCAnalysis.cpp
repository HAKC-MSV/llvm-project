//
// Created by derrick on 8/20/21.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"

#include "llvm/IR/DerivedTypes.h"

#include <llvm/IR/Verifier.h>

namespace llvm::hakc {
    std::error_code EC;
    auto HAKCLog = std::make_shared<HAKCLogger>(Verbose); // setting configured log level for errs(), which by default is the highest mode

    bool CommonHAKCAnalysis::IsNoTransferFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.NoTransferFunctions());
    }

    hakc_access_token_t CommonHAKCAnalysis::GetDefaultDivisionAccessToken(hakc_compartment_id_t CompartmentID,
                                                                          hakc_compartment_division_t DivisionID)
    const {
        return GetDefaultDivisionAccessToken(CompartmentID, DivisionID, SystemInfo.GetDivisionIDBitCount());
    }

    hakc_access_token_t CommonHAKCAnalysis::GetDefaultDivisionAccessToken(hakc_compartment_id_t CompartmentID,
                                                                      hakc_compartment_division_t DivisionID, unsigned DivisionIDBitCount){
      return CompartmentID << DivisionIDBitCount | DivisionID;
    }

    bool CommonHAKCAnalysis::IsFunctionInFunctionList(
        Function *F, iterator_range<HAKCFunctionList::iterator> Range) {
        if (!F) { return false; }

        auto Search = [F](function_def_t &Func) {
            return F == Func->GetFunction();
        };
        return llvm::any_of(Range, Search);
    }

    bool CommonHAKCAnalysis::IsFunctionInFunctionList(
        Function *F, iterator_range<FunctionList::iterator> Range) {
        if (!F) { return false; }

        auto Search = [F](Function *Func) { return F == Func; };
        return llvm::any_of(Range, Search);
    }

    bool CommonHAKCAnalysis::IsFunctionInHAKCTransferFunctionList(
        Function *F, iterator_range<HAKCTransferList::iterator> Range) {
        if (!F) { return false; }

        auto Search = [F](function_def_t &Func) { return F == Func->GetFunction(); };
        return llvm::any_of(Range, Search);
    }

    HAKCSystemInformation &CommonHAKCAnalysis::GetSystemInfo() {
        return SystemInfo;
    }

    bool CommonHAKCAnalysis::FunctionIsAnalysisCandidate(Function *F) {
        if (IsSafeTransitionFunction(F)) { return false; }
        if (IsHAKCFunction(F)) { return false; }
        if (IsOutsideTransferFunc(F)) { return false; }
        if (F->isIntrinsic()) { return false; }
        return true;
    }

    void CommonHAKCAnalysis::InitConfig(StringRef ConfigPath,
                                        StringRef ServerSocketPath,
                                        HAKCPassModeEnum PassMode) {
        if (!sys::fs::exists(ConfigPath)) {
            getLogger(Fatal) << "Could not find YAML file " << ConfigPath << "\n";
            throw std::exception();
        }
        if (!sys::fs::is_regular_file(ConfigPath)) {
            getLogger(Fatal) << ConfigPath << " is not a regular file\n";
            throw std::exception();
        }

        HAKCYAMLConfig SystemConfig;
        ErrorOr<std::unique_ptr<MemoryBuffer>> mb = MemoryBuffer::getFile(ConfigPath);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        // yaml parsed here
        yin >> SystemConfig;

        if (yin.error()) {
            getLogger(Fatal) << "Error parsing config file " << ConfigPath << "\n";
            throw std::exception();
        }
        // A bunch of work is done creating SystemInfo, so we want the log to be
        // created before this
        SystemInfo << SystemConfig;
        SystemInfo.SetSocketPath(ServerSocketPath);

        // create fd log
        getHAKCLoggerObject().addStream(
        createLogPath(SystemConfig.LogDir, PassMode),
          SystemConfig.ClientConfig
          .FileLogLevel); // setting the fd_ostream to configured log level
    }

CommonHAKCAnalysis::CommonHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM)
    : M(M), MAM(MAM), SystemInfo(*this), _HAKCLog(*HAKCLog) {
    }

    CommonHAKCAnalysis::CommonHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM,
                                           StringRef ConfigPath,
                                           StringRef ServerSocketPath,
                                           HAKCPassModeEnum PassMode)
        : M(M), MAM(MAM), SystemInfo(*this), _HAKCLog(*HAKCLog) {
      InitConfig(ConfigPath, ServerSocketPath, PassMode);
    }

    HAKCPassModeEnum CommonHAKCAnalysis::ParsePassMode(StringRef Mode) {
        auto It = PassModeToString.find(Mode);
        if (It != PassModeToString.end()) {
            return It->second;
        }

        throw std::invalid_argument("Invalid pass mode: " + Mode.str());
    }

    std::string CommonHAKCAnalysis::createLogPath(StringRef BuildPath,
                                                  HAKCPassModeEnum BuildMode) const {
        SmallString<256> Path;
        SmallString<256> ModulePath;
        GetModuleFullPath(M, ModulePath);
        sys::path::append(Path, BuildPath);
        sys::path::append(Path, ModulePath);

        std::string PassModeString = "";
        for (auto &it: PassModeToString) {
            if (it.second == BuildMode) {
                PassModeString = it.first.str();
            }
        }

        sys::path::replace_extension(Path, PassModeString + ".log");

        sys::path::make_preferred(Path);
        return std::string(Path);
    }

    Module &CommonHAKCAnalysis::GetModule() const { return M; }

    bool CommonHAKCAnalysis::IsHAKCTransferFunction(Function *F) {
        return IsFunctionInHAKCTransferFunctionList(
                   F, SystemInfo.CompartmentTransferFunctions()) ||
               IsHAKCCustomTransferFunction(F);
    }

    bool CommonHAKCAnalysis::IsHAKCCustomTransferFunction(Function *F) {
        SmallVector<Function *> CustomTransfers;
        for (const auto &CustomTransfer: SystemInfo.HAKCCustomTransfers()) {
            CustomTransfers.push_back(CustomTransfer->GetFunction());
        }
        return IsFunctionInFunctionList(F, CustomTransfers);
    }

    bool CommonHAKCAnalysis::IsHAKCCompartmentalizationSupportFunction(
        Function *F) {
        return IsFunctionInFunctionList(
            F, SystemInfo.CompartmentalizationSupportFunctions());
    }

    HAKCLogger &CommonHAKCAnalysis::getLogger(const HAKCLogLevel log_level,
                                              const bool suppress_output) {
      // must provide a log_level to print
      if (suppress_output) {
        // errs() << "SUPPRESSING OUTPUT!\n";
        HAKCLog->Disable();
      } else {
        HAKCLog->Enable();
        HAKCLog->SetLogLevel(log_level);
      }
      return *HAKCLog;
    }

    bool CommonHAKCAnalysis::IsPointerLikeType(const Type *Ty) {
        return Ty->isPointerTy() || Ty->isIntegerTy(64);
    }

    std::set<Intrinsic::ID> CommonHAKCAnalysis::GetBitshiftIntrinsics() {
        return {
            Intrinsic::fshl,
            Intrinsic::fshr,
        };
    }

    std::set<Instruction::BinaryOps>
    CommonHAKCAnalysis::GetPointerManipulatingBinaryOps() {
        return {
            Instruction::BinaryOps::Add, Instruction::BinaryOps::Xor,
            Instruction::BinaryOps::Sub, Instruction::BinaryOps::And,
            Instruction::BinaryOps::Or,
        };
    }

    bool CommonHAKCAnalysis::IsCallInIntrinsicSet(
        CallBase *Call, const std::set<Intrinsic::ID> &IntrinsicsSet) {
        bool result = false;
        if (auto *intrinsic = dyn_cast<IntrinsicInst>(Call)) {
            result = IntrinsicsSet.contains(intrinsic->getIntrinsicID());
            getLogger(Verbose) << "Intrinsic (" << intrinsic->getIntrinsicID()
                    << ") from " << Call->getFunction()->getName() << " "
                    << intrinsic;
            if (result) { getLogger(Verbose) << " is in { "; } else {
                getLogger(Verbose) << " is not in { ";
            }
            for (auto id: IntrinsicsSet) { getLogger(Verbose) << id << " "; }
            getLogger(Verbose) << "}\n";
        }

        return result;
    }

    bool CommonHAKCAnalysis::IsConstantUsedInGlobal(Value *V) {
        bool Result = false;
        if (auto *Const = dyn_cast<Constant>(V)) {
            auto Search = [](User *U) {
                return isa<GlobalVariable>(U) || IsConstantUsedInGlobal(U);
            };

            Result = llvm::any_of(Const->users(), Search);
        }
        return Result;
    }

    function_def_t CommonHAKCAnalysis::GetHAKCTransferDefinition(const Function *F) {
        SmallVector<function_def_t> HAKCFunctions;
        for (const auto &HAKCFunction: SystemInfo.CompartmentTransferFunctions()) {
            HAKCFunctions.push_back(HAKCFunction);
        }
        for (const auto &HAKCFunction: SystemInfo.HAKCCustomTransfers()) {
            HAKCFunctions.push_back(HAKCFunction);
        }

        for (auto HAKCFunction: HAKCFunctions) {
            if (HAKCFunction->GetFunction() == F) { return HAKCFunction; }
        }

        return nullptr;
    }

    /**
     * @brief Computes the definition chain from an arbitrary value to its source
     * definition
     * @param v
     * @param FollowLoad
     * @param Results
     * @return The chain of definitions starting from v to the source definition
     */
    void CommonHAKCAnalysis::findDefChain(Value *v, bool FollowLoad,
                                          SmallVectorImpl<Value *> &Results) {
        if (v == nullptr) {
            getLogger(Fatal) << "v is null\n";
            throw std::exception();
        }
        if (DefchainCache.contains(v)) {
            auto CachedChain = DefchainCache[v];
            Results.append(CachedChain);
            return;
        }

        getLogger(Verbose) << "Getting Def Chain for " << v << "\n";

        SmallVector<Value *> working_list = {v};
        while (!working_list.empty()) {
            auto *curr = working_list.back();
            working_list.pop_back();

            if (DefchainCache.contains(curr)) {
                auto CachedChain = DefchainCache[curr];
                getLogger(Verbose) << "Adding cached chain for " << curr << " containing "
                        << CachedChain.size() << " links\n";
                for (auto *Link: CachedChain) {
                    getLogger(Verbose) << "\t" << Link << "\n";
                    Results.push_back(Link);
                }
                continue;
            }

            if (auto *gep = dyn_cast<GetElementPtrInst>(curr)) {
                getLogger(Verbose) << "Adding GEP Operator pointer "
                        << gep->getPointerOperand() << "\n";
                working_list.push_back(gep->getPointerOperand());
            } else if (auto *BitCastI = dyn_cast<BitCastInst>(curr)) {
                working_list.push_back(BitCastI->getOperand(0));
            } else if (auto *call = dyn_cast<CallInst>(curr)) {
                if (call->getCalledFunction() &&
                    IsHAKCTransferFunction(call->getCalledFunction())) {
                    auto TransferDef = GetHAKCTransferDefinition(call->getCalledFunction());
                    getLogger(Verbose) << "Adding Arg " << TransferDef->GetSignedPtrIdx()
                            << " of HAKC Transfer " << call << "\n";
                    working_list.push_back(call->getArgOperand(
                        TransferDef->GetSignedPtrIdx()->getZExtValue()));
                } else if (call->getCalledFunction() &&
                           call->getCalledFunction()->isIntrinsic()) {
                    getLogger(Verbose) << "Call is intrinsic: "
                            << call->getCalledFunction()->getName() << "\n";

                    auto BitshiftIntrinsics = GetBitshiftIntrinsics();
                    if (IsCallInIntrinsicSet(call, BitshiftIntrinsics)) {
                        getLogger(Verbose) << "Adding argument 0 of " << call << "\n";
                        working_list.push_back(call->getArgOperand(0));
                    }
                }
            } else if (auto *GEPOp = dyn_cast<GEPOperator>(curr)) {
                working_list.push_back(GEPOp->getPointerOperand());
            } else if (auto *BitcastOp = dyn_cast<BitCastOperator>(curr)) {
                working_list.push_back(BitcastOp->getOperand(0));
            } else if (auto *PtrToIntI = dyn_cast<PtrToIntInst>(curr)) {
                working_list.push_back(PtrToIntI->getPointerOperand());
            } else if (auto *PtrToIntOp = dyn_cast<PtrToIntOperator>(curr)) {
                working_list.push_back(PtrToIntOp->getPointerOperand());
            } else if (FollowLoad && isa<LoadInst>(curr)) {
                auto *load = dyn_cast<LoadInst>(curr);
                working_list.push_back(load->getPointerOperand());
            } else if (auto *bitcast = dyn_cast<IntToPtrInst>(curr)) {
                working_list.push_back(bitcast->getOperand(0));
            } else if (auto *sext = dyn_cast<SExtInst>(curr)) {
                working_list.push_back(sext->getOperand(0));
            } else if (auto *binOp = dyn_cast<BinaryOperator>(curr)) {
              if (!GetPointerManipulatingBinaryOps().contains(binOp->getOpcode())) {
                    getLogger(Verbose)
                            << "BinaryOperator " << binOp
                            << " is not a pointer manipulating binary operation\n";
                    goto add_to_chain;
                }

                // instruction that seems to cause infinite loop:
                // %4 = load i32, ptr %0, align 4, !dbg !25, !tbaa !27
                // %5 = add nsw i32 %4, 1, !dbg !25
                auto *LHSDef = getDef(binOp->getOperand(0), false);
                auto *RHSDef = getDef(binOp->getOperand(1), false);
                if (!isa<Constant>(LHSDef) && ValueIsUsedAsPointer(LHSDef)) {
                    getLogger(Verbose) << "Adding LHS Binary Operand "
                            << binOp->getOperand(0) << "\n";
                    working_list.push_back(binOp->getOperand(0));
                } else if (!isa<Constant>(RHSDef) && ValueIsUsedAsPointer(RHSDef)) {
                    getLogger(Verbose) << "Adding RHS Binary Operand "
                            << binOp->getOperand(1) << "\n";
                    working_list.push_back(binOp->getOperand(1));
                } else if (!isa<Constant>(LHSDef) && !isa<Constant>(RHSDef)) {
                    getLogger(Verbose) << "Neither LHS nor RHS of " << binOp
                            << " are constants\n";
        /* We stop here */
        goto add_to_chain;
                } else if (isa<Constant>(LHSDef) && isa<Constant>(RHSDef)) {
                    getLogger(Verbose) << "Both LHS and RHS of " << binOp
                            << " are constants\n";
        /* We stop here */
        goto add_to_chain;
                }
            }
        add_to_chain:
            Results.push_back(curr);
        }

        getLogger(Verbose) << "Returning Def Chain of length " << Results.size()
                << " for " << v << "\n";
        DefchainCache[v].append(Results);
    }

    /**
     * @brief Returns the source definition of a Value
     * @param V
     * @param FollowLoad
     * @return
     */
    Value *CommonHAKCAnalysis::getDef(Value *V, bool FollowLoad) {
        SmallVector<Value *> Chain;
        findDefChain(V, FollowLoad, Chain);
        if (Chain.empty()) {
            getLogger(Fatal) << "Def Chain for " << V << " is empty!\n";
            throw std::exception();
        }
        return Chain.back();
    }

    /**
     * @brief Returns true if the called function is in the list of safe transition
     * calls defined above
     * @param call
     * @return
     */
    bool CommonHAKCAnalysis::IsSafeTransitionCall(const CallBase *call) {
        if (call->getCalledFunction()) {
            return IsSafeTransitionFunction(call->getCalledFunction());
        }

        return false;
    }

    /**
     * @brief
     * @param F
     * @return true if #F name is in #hakc_functions or
     * #hakc_transfer_funcs, false otherwise
     * */
    bool CommonHAKCAnalysis::IsHAKCFunction(Function *F) {
        return IsHAKCTransferFunction(F) ||
               IsHAKCCompartmentalizationSupportFunction(F);
    }

    /**
     * @brief
     * @param F
     * @return true if F->getName() is in #safe_transition_functions, false
     * otherwise
     */
    bool CommonHAKCAnalysis::IsSafeTransitionFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.SafeTransitionFunctions());
        /*        auto SafeTransitionFunctions = GetSafeTransitionFunctions();
                auto AllocationFunctions = GetKernelAllocationSizeMap();
                return (SafeTransitionFunctions.find(F->getName()) !=
                        SafeTransitionFunctions.end() ||
                        AllocationFunctions.find(F->getName()) !=
                        AllocationFunctions.end());*/
    }

    bool CommonHAKCAnalysis::IsMultiSSAUser(Value *V) {
        return isa<PHINode>(V) || isa<SelectInst>(V) || isa<BinaryOperator>(V);
    }

    bool CommonHAKCAnalysis::valueHasAttribute(Value *V, Attribute::AttrKind Kind) {
        bool result = false;
        auto attrName = Attribute::getNameFromAttrKind(Kind);
        if (attrName.empty()) {
            getLogger(Fatal) << "Invalid AttrKind name for value "
                    << std::to_string(Kind) << "\n";
            throw std::exception();
        }

        if (auto *gv = dyn_cast<GlobalVariable>(V)) {
            result = gv->hasAttribute(Kind);
        } else if (auto *arg = dyn_cast<Argument>(V)) {
            /* A pointer to a pointer is used by the kernel to allow setting
             * the value of a user pointer. See ___sys_recvmsg in net/socket.c
             */
            result = arg->hasAttribute(Kind);
        } else if (auto *I = dyn_cast<Instruction>(V)) {
    if (auto *Metadata = I->getMetadata(LLVMContext::MD_annotation)) {
      for (auto &operand : Metadata->operands()) {
                    if (auto *mdstring = dyn_cast<MDString>(operand.get())) {
                        auto MDStr = mdstring->getString();
                        if (MDStr == attrName) {
                            result = true;
                            break;
                        }
                    }
                }
            }
            if (!result && isa<CallInst>(V)) {
                auto *call = cast<CallInst>(V);
                if (call->getCalledFunction()) {
                    Function *func = call->getCalledFunction();
                    result = func->hasFnAttribute(Kind);
                }
            }
        }

        return result;
    }

    bool CommonHAKCAnalysis::IsIgnoredGlobal(Value *V) {
      if (auto *GV = dyn_cast<GlobalVariable>(V)) {
        auto Search = [GV](const GlobalVariable *G) { return GV == G; };
        return any_of(SystemInfo.IgnoredGlobals(), Search);
      }

      return false;
    }

    bool CommonHAKCAnalysis::IsPerCPUPointer(Value *V) {
        return valueHasAttribute(V, Attribute::PerCPUPtr);
    }

    bool CommonHAKCAnalysis::IsKernelUserPointer(Value *V) {
        return valueHasAttribute(V, Attribute::KernelUserPtr);
    }

    bool CommonHAKCAnalysis::FunctionIsStatic(const Function *F) {
        return Function::isLocalLinkage(F->getLinkage()) || F->isDeclaration();
    }

    bool CommonHAKCAnalysis::FunctionHasPointerArg(Function *F) {
        bool ArgumentsContainPointer = false;
        for (auto &Arg: F->args()) {
            if (Arg.getType()->isPointerTy()) {
                ArgumentsContainPointer = true;
                break;
            }
        }

        return ArgumentsContainPointer;
    }

    bool CommonHAKCAnalysis::ValueShouldBeReplacedWithTransfer(
        Value *V, HAKCServerClientBase &Client) {
        if (auto *F = dyn_cast<Function>(V)) {
            return functionIsTransferCandidate(F, Client);
        }
        if (auto *BCO = dyn_cast<BitCastOperator>(V)) {
            return ValueShouldBeReplacedWithTransfer(BCO->getOperand(0), Client);
        }
        return false;
    }

    bool CommonHAKCAnalysis::IsOutsideTransferFunc(const Function *F) {
        return F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX) ||
               F->getName().starts_with(VARIADIC_TRANSFER_PREFIX);
    }

    Function *
    CommonHAKCAnalysis::GetOriginalFunctionFromTransferFunction(Function *F) {
      if (IsOutsideTransferFunc(F)) {
        StringRef TransferPrefix = F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX) ? OUTSIDE_TRANSFER_PREFIX : VARIADIC_TRANSFER_PREFIX;
        const auto transferTargetName = F->getName().substr(TransferPrefix.size());
        auto *TransferTarget = F->getParent()->getFunction(transferTargetName);
        return TransferTarget;
      }

        return F;
    }

    bool CommonHAKCAnalysis::IsCapabilityReassignmentFunc(const Function *F) {
        return F->getName().starts_with(CAPABILITY_REASSIGNMENT_PREFIX);
    }

    void CommonHAKCAnalysis::VerifyFunction(Function *F) {
        std::string buf;
        auto tempOS = std::make_shared<raw_string_ostream>(buf);
        if (llvm::verifyFunction(*F, tempOS.get())) {
            getLogger(Fatal) << "Verification failed for function\n"
                    << F->getName() << "\n"
                    << F->getParent() << "\n"
                    << "With error: " << tempOS->str();
            tempOS.reset();
            throw std::exception();
        }
    }

    FunctionType *CommonHAKCAnalysis::GetDataAuthenticationFunctionType() {
        return GetSystemInfo().DataValidation()->GetFunction()->getFunctionType();
    }

    FunctionType *CommonHAKCAnalysis::GetTransferFunctionType() {
        return GetSystemInfo()
                .CompartmentTransfer(false)
                ->GetFunction()
                ->getFunctionType();
    }

    FunctionType *
    CommonHAKCAnalysis::GetCodeAuthenticationFunctionType(unsigned AddrSpace) {
        return GetSystemInfo().CodeValidation()->GetFunction()->getFunctionType();
    }

    bool CommonHAKCAnalysis::IsCompartmentalizedFunction(Function *F,
                                                         HAKCServerClientBase &Client) {
      getLogger(Debug) << "IsCompartmentalizedFunction returns " << (!IsNECSymbol(F, Client) && !IsOutsideTransferFunc(F)) << "\n";
        return !IsNECSymbol(F, Client) && !IsOutsideTransferFunc(F);
    }

    std::string CommonHAKCAnalysis::GetOutsideTransferName(Function *F) {
        if (F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX) ||
            IsNoTransferFunction(F)) {
          return F->getName().str();
        }
        return OUTSIDE_TRANSFER_PREFIX.str() + F->getName().str();
    }

    std::string CommonHAKCAnalysis::getVariadicTransferName(const Function *F) {
      return VARIADIC_TRANSFER_PREFIX.str() + F->getName().str();
    }

    std::string CommonHAKCAnalysis::getOriginalTransformedName(const Function *F) {
      return ORIGINAL_FUNCTION_PREFIX.str() + F->getName().str();
    }

    bool CommonHAKCAnalysis::FunctionIsModParamGetCtx(const Function *F) {
        return F->getName().starts_with(MODPARAM_GETCTX_PREFIX);
    }

    bool CommonHAKCAnalysis::functionIsTransferCandidate(Function *F,
                                                         HAKCServerClientBase &Client) {
        // auto Division = Client.GetDivision(F);
        return !IsNoTransferFunction(F) && !IsNECSymbol(F, Client) &&
               !F->isDeclaration() && !IsCapabilityReassignmentFunc(F) &&
               !FunctionIsComplexVariadic(F) && !FunctionIsModParamGetCtx(F) &&
               FunctionHasPointerArg(F) &&
               (!IsOutsideTransferFunc(F) ||
                !F->hasFnAttribute(Attribute::InlineHint));
    }

    bool CommonHAKCAnalysis::FunctionIsComplexVariadic(const Function *F) {
        return F->isVarArg();
    }

    bool CommonHAKCAnalysis::isRegisterRead(Value *v) {
        if (auto *call = dyn_cast<CallInst>(v)) {
            return call->isInlineAsm() ||
                   (call->getCalledFunction() &&
                    call->getCalledFunction()->isIntrinsic() &&
                    call->getCalledFunction()->getIntrinsicID() ==
                    Intrinsic::IndependentIntrinsics::read_register);
        }
        return false;
    }

    bool CommonHAKCAnalysis::argShouldTransfer(Value *V) {
        if (auto *Arg = dyn_cast<Argument>(V)) {
            if (Arg->hasAttribute(llvm::Attribute::ReadNone)) { return false; }
        }

        return V->getType()->isPointerTy() && !isa<FunctionType>(V->getType()) &&
               !isa<ConstantPointerNull>(V) && !IsKernelUserPointer(V);
    }

    void CommonHAKCAnalysis::SortGlobalList(
        std::vector<GlobalVariable *> &GlobalList) {
        llvm::sort(GlobalList.begin(), GlobalList.end(),
                   [](const GlobalVariable *LHS, const GlobalVariable *RHS) {
                       return LHS->getName().str() < RHS->getName().str();
                   });
    }

    void CommonHAKCAnalysis::SortFunctionList(FunctionList &FuncList) {
        llvm::sort(FuncList.begin(), FuncList.end(),
                   [](const Function *LHS, const Function *RHS) {
                       return LHS->getName().str() < RHS->getName().str();
                   });
    }

    bool CommonHAKCAnalysis::PointerShouldBeConsideredCode(
        const ManagedHAKCPointer &ManagedPointer) {
        auto HAKCType = ManagedPointer.GetType();
        if (HAKCType && HAKCType->IsPointerType() && HAKCType->GetPointeeType()) {
            return isa_and_nonnull<FunctionType>(HAKCType->GetPointeeType()->GetLLVMType());
        }
        return false;
    }

    bool CommonHAKCAnalysis::IsNECSymbol(GlobalValue *GV,
                                                         HAKCServerClientBase &Client) {
        auto Division = Client.GetDivision(GV);
      getLogger(Debug) << "Returning " << (Division.GetHAKCCompartment() == Client.GetDefaultDivision().GetHAKCCompartment()) << " for IsNECSymbol with Compartment " << Division.GetHAKCCompartment().GetCompartmentIDValue() << " and default compartment " << Client.GetDefaultDivision().GetHAKCCompartment().GetCompartmentIDValue() << "\n";
        return Division.GetHAKCCompartment() ==
               Client.GetDefaultDivision().GetHAKCCompartment();
    }

    bool CommonHAKCAnalysis::IsAllocationFunction(Function *F) {
        SmallVector<Function *> AllocationFunctions;
        for (const auto &Allocation: SystemInfo.AllocationFunctions()) {
            AllocationFunctions.push_back(Allocation->GetAllocationFunction());
        }
        return IsFunctionInFunctionList(F, AllocationFunctions);
    }

    HAKCCustomAllocation CommonHAKCAnalysis::GetAllocationDefinition(const Function *F) {
        for (auto Allocation: SystemInfo.AllocationFunctions()) {
            if (Allocation->GetAllocationFunction() == F) { return Allocation; }
        }

        return nullptr;
    }

    bool CommonHAKCAnalysis::FunctionsAreInSameCompartment(Function *F, Function *G, HAKCServerClientBase &Client) {
      return Client.GetDivision(F).GetHAKCCompartment() == Client.GetDivision(G).GetHAKCCompartment();
    }

    bool CommonHAKCAnalysis::IsAllocation(Value *V) {
        V = getDef(V, false);
        if (auto *call = dyn_cast<CallInst>(V)) {
            if (IsAllocationFunction(call->getCalledFunction())) { return true; }
        }
        return false;
    }

    StringRef CommonHAKCAnalysis::GetFunctionName(const Function *F) {
        /* The compiler will sometimes rename functions when directed to, especially
         * for kernel modules, in order to facilitate general functionality.  However,
         * the debug symbols maintain the original name, so use that name if it is
         * available */
        if (F->getSubprogram() && !F->getSubprogram()->getName().empty()) {
            return F->getSubprogram()->getName();
        }

        return F->getName();
    }

    bool CommonHAKCAnalysis::IsStringType(const Type *Ty) {
        return Ty->isArrayTy() && Ty->getArrayElementType()->isIntegerTy(8);
    }

    Instruction *CommonHAKCAnalysis::GetTargetTypeCast(Instruction *I,
                                                       const Type *TargetType) {
        if (I->getType() == TargetType) { return I; }

        for (auto *U: I->users()) {
            if (auto *BitCastI = dyn_cast<BitCastInst>(U)) {
                if (BitCastI->getDestTy() == TargetType) { return BitCastI; }
            }
        }

        return nullptr;
    }

    void CommonHAKCAnalysis::GetModuleFullPath(const Module &M,
                                               SmallVectorImpl<char> &Result) {
        const auto &SourceFileName = M.getSourceFileName();
        if (auto Err = sys::fs::real_path(SourceFileName, Result, true)) {
            getLogger(Fatal) << "Could not get real path to " << M.getSourceFileName()
                    << ": " << Err.message() << "\n";
            throw std::exception();
        }
    }

    bool CommonHAKCAnalysis::ValueIsUsedAsPointer(Value *V) {
        if (!IsPointerLikeType(V->getType())) { return false; }

        bool CallIsUsedAsPointer = V->getType()->isPointerTy();
        if (V->getType()->isIntegerTy()) {
            CallIsUsedAsPointer = false;
            /* Search for uses that determine if the call is considered a pointer or
             * integer */
            for (auto &U: V->uses()) {
                if (auto *IToPtrI = dyn_cast<IntToPtrInst>(U.getUser())) {
                    getLogger(
                                Debug, !GetSystemInfo().OutputDebugInfo(IToPtrI->getFunction()))
                            << "User of " << *V
                            << " is an inttoptr: " << *U.getUser() << "\n";

                    CallIsUsedAsPointer = true;
                } else if (auto *BinOp = dyn_cast<BinaryOperator>(U.getUser())) {
                    if (BinOp->getOpcode() == BinaryOperator::Add) {
                        unsigned OpNum = (U.getOperandNo() + 1) % 2;
                        auto *OtherOp = U.getUser()->getOperand(OpNum);

                        getLogger(Debug, !GetSystemInfo().OutputDebugInfo(BinOp->getFunction()))
                                << "Checking operator " << OpNum << " of "
                                << *BinOp << ": " << *OtherOp << "\n";

                        if (OtherOp->getType()->isPointerTy()) {
                            /* V is an integer (which could still be used as a pointer), but is
                             * used in an add operation that involves another pointer.  Adding
                             * two pointers together does not make sense, so V is a true integer
                             * and not a pointer.
                             */
                            break;
                        }
                    }
                }

                if (CallIsUsedAsPointer) { break; }
            }
        }

        return CallIsUsedAsPointer;
    }
    HAKCLogger &CommonHAKCAnalysis::getHAKCLoggerObject() const {
      return _HAKCLog;
    }
} // namespace llvm::hakc
