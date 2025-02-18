//
// Created by derrick on 8/20/21.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Path.h"

namespace llvm::hakc {
    HAKCWriter HAKC_Writer;

    bool CommonHAKCAnalysis::IsNoTransferFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.NoTransferFunctions());
    }

    bool CommonHAKCAnalysis::IsFunctionInFunctionList(Function *F, iterator_range<FunctionList::iterator> Range) {
        if (!F) {
            return false;
        }

        auto Search = [F](Function *Func) {
            return F == Func;
        };
        return llvm::any_of(Range, Search);
    }

    bool CommonHAKCAnalysis::IsFunctionInHAKCTransferFunctionList(Function *F,
                                                                  iterator_range<HAKCTransferList::iterator> Range) {
        if (!F) {
            return false;
        }

        auto Search = [F](hakc_transfer_def_t &Func) {
            return F == Func->GetFunction();
        };
        return llvm::any_of(Range, Search);
    }

    HAKCSystemInformation &CommonHAKCAnalysis::GetSystemInfo() {
        return SystemInfo;
    }

    bool CommonHAKCAnalysis::FunctionIsAnalysisCandidate(Function *F) {
        if (IsSafeTransitionFunction(F)) {
            return false;
        }
        if (IsHAKCFunction(F)) {
            return false;
        }
        if (IsOutsideTransferFunc(F)) {
            return false;
        }
        if (F->isIntrinsic()) {
            return false;
        }
        return true;
    }

    void CommonHAKCAnalysis::InitConfig(StringRef ConfigPath) {
        if (!sys::fs::exists(ConfigPath)) {
            errs() << "Could not find YAML file " << ConfigPath << "\n";
            throw std::exception();
        } else if (!sys::fs::is_regular_file(ConfigPath)) {
            errs() << ConfigPath << " is not a regular file\n";
            throw std::exception();
        }

        HAKCYamlConfig SystemConfig;
        ErrorOr<std::unique_ptr<MemoryBuffer> > mb = MemoryBuffer::getFile(ConfigPath);
        yaml::Input yin(mb.get()->getMemBufferRef().getBuffer());

        // yaml parsed here
        yin >> SystemConfig;
        if (yin.error()) {
            errs() << "Error parsing config file " << ConfigPath << "\n";
            throw std::exception();
        }

        SystemInfo << SystemConfig;
    }

    CommonHAKCAnalysis::CommonHAKCAnalysis(Module &M, StringRef ConfigPath) : M(M), SystemInfo(*this) {
        InitConfig(ConfigPath);
    }

    Module &CommonHAKCAnalysis::GetModule() {
        return M;
    }

    bool CommonHAKCAnalysis::IsHAKCTransferFunction(Function *F) {
        return IsFunctionInHAKCTransferFunctionList(F, SystemInfo.CompartmentTransferFunctions()) ||
               IsHAKCCustomTransferFunction(F);
    }

    bool CommonHAKCAnalysis::IsHAKCCustomTransferFunction(Function *F) {
        SmallVector<Function *> CustomTransfers;
        for (const auto &CustomTransfer: SystemInfo.HAKCCustomTransfers()) {
            CustomTransfers.push_back(CustomTransfer->GetFunction());
        }
        return IsFunctionInFunctionList(F, CustomTransfers);
    }

    bool CommonHAKCAnalysis::IsHAKCCompartmentalizationSupportFunction(Function *F) {
        return IsFunctionInFunctionList(F, SystemInfo.CompartmentalizationSupportFunctions());
    }

    hakc::HAKCWriter &CommonHAKCAnalysis::getWriter(bool DebugActive) {
        HAKC_Writer.SetDebug(DebugActive);
        return HAKC_Writer;
    }

    bool CommonHAKCAnalysis::IsPointerLikeType(Type *Ty) {
        return Ty->isPointerTy() || Ty->isIntegerTy(64);
    }

    std::set<Intrinsic::ID> CommonHAKCAnalysis::GetBitshiftIntrinsics() {
        return {
            Intrinsic::fshl,
            Intrinsic::fshr,
        };
    }

    std::set<Instruction::BinaryOps> CommonHAKCAnalysis::GetPointerManipulatingBinaryOps() {
        return {
            Instruction::BinaryOps::Add,
            Instruction::BinaryOps::Xor,
            Instruction::BinaryOps::Sub,
            Instruction::BinaryOps::And,
            Instruction::BinaryOps::Or,
        };
    }

    bool CommonHAKCAnalysis::IsCallInIntrinsicSet(CallBase *Call, std::set<Intrinsic::ID> &IntrinsicsSet) const {
        bool result = false;
        if (auto *intrinsic = dyn_cast<IntrinsicInst>(Call)) {
            result = (IntrinsicsSet.find(intrinsic->getIntrinsicID()) != IntrinsicsSet.end());
            auto OutputDebug = SystemInfo.OutputDebugInfo(Call->getFunction());
            if (OutputDebug) {
                CommonHAKCAnalysis::getWriter(OutputDebug) << "Intrinsic (" << intrinsic->getIntrinsicID() << ") from "
                        <<
                        Call->getFunction()->getName() << " " << intrinsic;
                if (result) {
                    CommonHAKCAnalysis::getWriter(OutputDebug) << " is in { ";
                } else {
                    CommonHAKCAnalysis::getWriter(OutputDebug) << " is not in { ";
                }
                for (auto id: IntrinsicsSet) {
                    CommonHAKCAnalysis::getWriter(OutputDebug) << id << " ";
                }
                CommonHAKCAnalysis::getWriter(OutputDebug) << "}\n";
            }
        }
        return result;
    }

    bool CommonHAKCAnalysis::IsConstantUsedInGlobal(Value *V) {
        bool Result = false;
        if (auto *Const = dyn_cast<Constant>(V)) {
            auto Search = [](User *U) {
                return isa<GlobalVariable>(U) || CommonHAKCAnalysis::IsConstantUsedInGlobal(U);
            };

            Result = llvm::any_of(Const->users(), Search);
        }
        return Result;
    }

    hakc_transfer_def_t CommonHAKCAnalysis::GetHAKCTransferDefinition(Function *F) {
        SmallVector<hakc_transfer_def_t> HAKCFunctions;
        for (const auto &HAKCFunction: SystemInfo.CompartmentTransferFunctions()) {
            HAKCFunctions.push_back(HAKCFunction);
        }
        for (const auto &HAKCFunction: SystemInfo.HAKCCustomTransfers()) {
            HAKCFunctions.push_back(HAKCFunction);
        }

        for (auto HAKCFunction: HAKCFunctions) {
            if (HAKCFunction->GetFunction() == F) {
                return HAKCFunction;
            }
        }

        return nullptr;
    }

    /**
     * @brief Computes the definition chain from an arbitrary value to its source definition
     * @param v
     * @return The chain of definitions starting from v to the source definition
     */
    void
    CommonHAKCAnalysis::findDefChain(Value *v, bool followLoad, SmallVectorImpl<Value *> &Results) {
        auto debug = GetSystemInfo().OutputDebugInfo();
        if (v == nullptr) {
            CommonHAKCAnalysis::getWriter(true) << "v is null\n";
            throw std::exception();
        }
        if (DefchainCache.contains(v)) {
            auto CachedChain = DefchainCache[v];
            Results.append(CachedChain);
            return;
        }

        CommonHAKCAnalysis::getWriter(debug) << "Getting Def Chain for " << v << "\n";

        SmallVector<Value *> working_list = {v};
        while (!working_list.empty()) {
            auto *curr = working_list.back();
            working_list.pop_back();

            if (DefchainCache.contains(curr)) {
                auto CachedChain = DefchainCache[curr];
                CommonHAKCAnalysis::getWriter(debug) << "Adding cached chain for " << curr << " containing "
                        << CachedChain.size() << " links\n";
                for (auto *Link: CachedChain) {
                    CommonHAKCAnalysis::getWriter(debug) << "\t" << Link << "\n";
                    Results.push_back(Link);
                }
                continue;
            }

            if (auto *gep = dyn_cast<GetElementPtrInst>(curr)) {
                CommonHAKCAnalysis::getWriter(debug) << "Adding GEP Operator pointer " << gep->getPointerOperand()
                        << "\n";
                working_list.push_back(gep->getPointerOperand());
            } else if (auto *BitCastI = dyn_cast<BitCastInst>(curr)) {
                working_list.push_back(BitCastI->getOperand(0));
            } else if (auto *call = dyn_cast<CallInst>(curr)) {
                if (call->getCalledFunction() &&
                    IsHAKCTransferFunction(call->getCalledFunction())) {
                    auto TransferDef = GetHAKCTransferDefinition(call->getCalledFunction());
                    CommonHAKCAnalysis::getWriter(debug) << "Adding Arg " << TransferDef->GetSignedPtrIdx()
                            << " of HAKC Transfer " << call << "\n";
                    working_list.push_back(call->getArgOperand(TransferDef->GetSignedPtrIdx()->getZExtValue()));
                } else if (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic()) {
                    CommonHAKCAnalysis::getWriter(debug) << "Call is intrinsic: "
                            << call->getCalledFunction()->getName() << "\n";

                    auto BitshiftIntrinsics = GetBitshiftIntrinsics();
                    if (IsCallInIntrinsicSet(call, BitshiftIntrinsics)) {
                        CommonHAKCAnalysis::getWriter(debug) << "Adding argument 0 of " << call << "\n";
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
            } else if (followLoad && isa<LoadInst>(curr)) {
                auto *load = dyn_cast<LoadInst>(curr);
                working_list.push_back(load->getPointerOperand());
            } else if (auto *bitcast = dyn_cast<IntToPtrInst>(curr)) {
                working_list.push_back(bitcast->getOperand(0));
            } else if (auto *sext = dyn_cast<SExtInst>(curr)) {
                working_list.push_back(sext->getOperand(0));
            } else if (auto *binOp = dyn_cast<BinaryOperator>(curr)) {
                auto PointerBinOps = GetPointerManipulatingBinaryOps();
                if (PointerBinOps.find(binOp->getOpcode()) == PointerBinOps.end()) {
                    CommonHAKCAnalysis::getWriter(debug) << "BinaryOperator " << binOp
                            << " is not a pointer manipulating binary operation\n";
                    goto add_to_chain;
                }

                // instruction that seems to cause infinite loop:
                // %4 = load i32, ptr %0, align 4, !dbg !25, !tbaa !27
                // %5 = add nsw i32 %4, 1, !dbg !25
                auto *LHSDef = getDef(binOp->getOperand(0), false);
                auto *RHSDef = getDef(binOp->getOperand(1), false);
                if (!isa<Constant>(LHSDef) && ValueIsUsedAsPointer(LHSDef)) {
                    CommonHAKCAnalysis::getWriter(debug) << "Adding LHS Binary Operand " << binOp->getOperand(0) <<
                            "\n";
                    working_list.push_back(binOp->getOperand(0));
                } else if (!isa<Constant>(RHSDef) && ValueIsUsedAsPointer(RHSDef)) {
                    CommonHAKCAnalysis::getWriter(debug) << "Adding RHS Binary Operand " << binOp->getOperand(1) <<
                            "\n";
                    working_list.push_back(binOp->getOperand(1));
                } else if (!isa<Constant>(LHSDef) && !isa<Constant>(RHSDef)) {
                    CommonHAKCAnalysis::getWriter(debug) << "Neither LHS nor RHS of " << binOp <<
                            " are constants\n";
                    /* We stop here */
                    goto add_to_chain;
                } else if (isa<Constant>(LHSDef) && isa<Constant>(RHSDef)) {
                    CommonHAKCAnalysis::getWriter(debug) << "Both LHS and RHS of " << binOp << " are constants\n";
                    /* We stop here */
                    goto add_to_chain;
                }
            }
        add_to_chain:
            Results.push_back(curr);
        }

        CommonHAKCAnalysis::getWriter(debug) << "Returning Def Chain of length " << Results.size() << " for " << v
                << "\n";
        DefchainCache[v].append(Results);
    }

    /**
     * @brief Returns the source definition of a Value
     * @param V
     * @return
     */
    Value *CommonHAKCAnalysis::getDef(Value *V, bool followLoad) {
        SmallVector<Value *> Chain;
        findDefChain(V, followLoad, Chain);
        if (Chain.empty()) {
            errs() << "Def Chain for " << V << " is empty!\n";
            throw std::exception();
        }
        return Chain.back();
    }

    /**
     * @brief Returns true if the called function is in the list of safe transition calls defined above
     * @param call
     * @return
     */
    bool CommonHAKCAnalysis::IsSafeTransitionCall(CallBase *call) {
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
        return IsHAKCTransferFunction(F) || IsHAKCCompartmentalizationSupportFunction(F);
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
        if (auto *gv = dyn_cast<GlobalVariable>(V)) {
            result = gv->hasAttribute(Kind);
        } else if (auto *arg = dyn_cast<Argument>(V)) {
            /* A pointer to a pointer is used by the kernel to allow setting
             * the value of a user pointer. See ___sys_recvmsg in net/socket.c
             */
            result = arg->hasAttribute(Kind);
        } else if (auto *I = dyn_cast<Instruction>(V)) {
            auto *metadata = I->getMetadata(LLVMContext::MD_annotation);
            if (metadata) {
                auto attrName = Attribute::getNameFromAttrKind(Kind);
                if (attrName.empty()) {
                    errs() << "Invalid AttrKind name for value " << std::to_string(Kind)
                            << "\n";
                    throw std::exception();
                }
                for (auto &operand: metadata->operands()) {
                    if (auto *mdstring = dyn_cast<MDString>(operand.get())) {
                        if (mdstring->getString() == attrName) {
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

    bool CommonHAKCAnalysis::IsIgnoredType(Type *Ty) {
        if (!Ty) {
            return false;
        }

        auto Search = [Ty](Type *T) {
            return Ty == T;
        };
        return llvm::any_of(SystemInfo.IgnoredTypes(), Search);
    }

    bool CommonHAKCAnalysis::IsIgnoredGlobal(Value *V) {
        bool Result = false;
        if (auto *GV = dyn_cast<GlobalVariable>(V)) {
            auto Search = [GV](GlobalVariable *G) {
                return GV == G;
            };
            return llvm::any_of(SystemInfo.IgnoredGlobals(), Search);
        }

        return Result;
    }

    bool CommonHAKCAnalysis::IsPerCPUPointer(Value *V) {
        //        return valueHasAttribute(V, Attribute::PerCPUPtr);
        // TODO: Fix this when attributes are added in again
        return false;
    }

    bool CommonHAKCAnalysis::IsKernelUserPointer(Value *V) {
        //        return valueHasAttribute(V, Attribute::KernelUserPtr);
        // TODO: Fix this when attributes are added in again
        return false;
    }

    bool CommonHAKCAnalysis::FunctionIsStatic(Function *F) {
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

    bool CommonHAKCAnalysis::ValueShouldBeReplacedWithTransfer(Value *V, HAKCCompartmentalizationPolicy &Policy) {
        if (auto *F = dyn_cast<Function>(V)) {
            return functionIsTransferCandidate(F, Policy);
        } else if (auto *BCO = dyn_cast<BitCastOperator>(V)) {
            return ValueShouldBeReplacedWithTransfer(BCO->getOperand(0), Policy);
        }
        return false;
    }

    bool CommonHAKCAnalysis::IsOutsideTransferFunc(Function *F) {
        return (F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX));
    }

    bool CommonHAKCAnalysis::IsCapabilityReassignmentFunc(Function *F) {
        return F->getName().starts_with(CAPABILITY_REASSIGNMENT_PREFIX);
    }

    void CommonHAKCAnalysis::VerifyFunction(Function *F) {
        if (llvm::verifyFunction(*F, &CommonHAKCAnalysis::getWriter(false).ostream())) {
            CommonHAKCAnalysis::getWriter(false) << "Verification failed for function\n" << F << "\n";
            errs() << *F << "\n";
            throw std::exception();
        }
    }

    FunctionType *CommonHAKCAnalysis::GetDataAuthenticationFunctionType(Module &M, unsigned AddrSpace) {
        auto *RetTy = PointerType::get(M.getContext(), AddrSpace);
        Type *ArgTy[] = {
            PointerType::get(M.getContext(), AddrSpace),
            IntegerType::get(M.getContext(), 64),
            IntegerType::get(M.getContext(), 64)
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    FunctionType *CommonHAKCAnalysis::GetTransferFunctionType(Module &M, unsigned int AddrSpace) {
        auto *RetTy = PointerType::get(M.getContext(), AddrSpace);
        Type *ArgTy[] = {
            PointerType::get(M.getContext(), AddrSpace),
            IntegerType::get(M.getContext(), 64),
            IntegerType::get(M.getContext(), 64)
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    FunctionType *CommonHAKCAnalysis::GetCodeAuthenticationFunctionType(Module &M, unsigned AddrSpace) {
        auto *RetTy = PointerType::get(M.getContext(), AddrSpace);
        Type *ArgTy[] = {
            PointerType::get(M.getContext(), AddrSpace),
            IntegerType::get(M.getContext(), 64),
            IntegerType::get(M.getContext(), 64)
        };

        return FunctionType::get(RetTy, ArgTy, false);
    }

    bool CommonHAKCAnalysis::IsCompartmentalizedFunction(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        return !IsUncompartmentalizedSymbol(F, Policy) && !IsOutsideTransferFunc(F);
    }

    std::string CommonHAKCAnalysis::GetOutsideTransferName(Function *F) {
        if (F->getName().starts_with(OUTSIDE_TRANSFER_PREFIX) || IsNoTransferFunction(F)) {
            return F->getName().str();
        }
        std::string name = OUTSIDE_TRANSFER_PREFIX.str();
        name += F->getName().str();
        return name;
    }

    std::string CommonHAKCAnalysis::getVariadicTransferName(Function *F) {
        std::string VariadicTransferName = VARIADIC_TRANSFER_PREFIX.str();
        VariadicTransferName += F->getName();
        return VariadicTransferName;
    }

    std::string CommonHAKCAnalysis::getOriginalTransformedName(Function *F) {
        std::string TransformedName = ORIGINAL_FUNCTION_PREFIX.str();
        TransformedName += F->getName();
        return TransformedName;
    }

    bool CommonHAKCAnalysis::FunctionIsModParamGetCtx(Function *F) {
        return F->getName().starts_with(MODPARAM_GETCTX_PREFIX);
    }

    bool CommonHAKCAnalysis::functionIsTransferCandidate(Function *F, HAKCCompartmentalizationPolicy &Policy) {
        auto Division = Policy.GetDivision(F);
        return !IsNoTransferFunction(F) &&
               !Division.GetHAKCCompartment().IsUncompartmentalized() &&
               !F->isDeclaration() &&
               !IsCapabilityReassignmentFunc(F) &&
               !FunctionIsComplexVariadic(F) &&
               !FunctionIsModParamGetCtx(F) &&
               FunctionHasPointerArg(F) &&
               (!IsOutsideTransferFunc(F) ||
                !F->hasFnAttribute(Attribute::InlineHint));
    }

    bool CommonHAKCAnalysis::FunctionIsComplexVariadic(Function *F) {
        return F->isVarArg();
    }

    bool CommonHAKCAnalysis::isRegisterRead(Value *v) {
        if (auto *call = dyn_cast<CallInst>(v)) {
            return call->isInlineAsm() || (call->getCalledFunction() && call->getCalledFunction()->isIntrinsic() &&
                                           call->getCalledFunction()->getIntrinsicID() ==
                                           Intrinsic::IndependentIntrinsics::read_register);
        }
        return false;
    }

    bool CommonHAKCAnalysis::argShouldTransfer(Value *V) {
        if (auto *Arg = dyn_cast<Argument>(V)) {
            if (Arg->hasAttribute(llvm::Attribute::ReadNone)) {
                return false;
            }
        }
        // this should return true for int*, I think, but it's not (foo, kuzu test 11)
        return V->getType()->isPointerTy() && !isa<FunctionType>(V->getType()) &&
               !isa<ConstantPointerNull>(V) && !IsKernelUserPointer(V);
    }

    bool CommonHAKCAnalysis::valueIsReadonlyPtr(Value *value) {
        Type *Ty = value->getType();
        if (auto *Call = dyn_cast<CallInst>(value)) {
            Ty = Call->getFunctionType()->getReturnType();
        }
        bool result = isa<FunctionType>(Ty);
        return result;
    }

    void CommonHAKCAnalysis::SortGlobalList(std::vector<GlobalVariable *> &GlobalList) {
        llvm::sort(GlobalList.begin(), GlobalList.end(),
                   [](GlobalVariable *LHS, GlobalVariable *RHS) {
                       return LHS->getName().str() < RHS->getName().str();
                   });
    }

    void CommonHAKCAnalysis::SortFunctionList(FunctionList &FuncList) {
        llvm::sort(FuncList.begin(), FuncList.end(),
                   [](Function *LHS, Function *RHS) { return LHS->getName().str() < RHS->getName().str(); });
    }

    bool CommonHAKCAnalysis::PointerShouldBeConsideredCode(Value *Pointer) {
        if (Pointer->getType()->isPointerTy()) {
            /*return Pointer->getType()->getPointerElementType()->isFunctionTy();*/
            return Pointer->getType()->isFunctionTy();
        }
        return false;
    }

    bool CommonHAKCAnalysis::IsUncompartmentalizedSymbol(GlobalValue *GV, HAKCCompartmentalizationPolicy &Policy) {
        auto Division = Policy.GetDivision(GV);
        return Division.GetHAKCCompartment().IsUncompartmentalized();
    }

    bool CommonHAKCAnalysis::IsAllocationFunction(Function *F) {
        SmallVector<Function *> AllocationFunctions;
        for (const auto &Allocation: SystemInfo.AllocationFunctions()) {
            AllocationFunctions.push_back(Allocation->GetAllocationFunction());
        }
        return IsFunctionInFunctionList(F, AllocationFunctions);
    }

    HAKCCustomAllocation CommonHAKCAnalysis::GetAllocationDefinition(Function *F) {
        for (auto Allocation: SystemInfo.AllocationFunctions()) {
            if (Allocation->GetAllocationFunction() == F) {
                return Allocation;
            }
        }

        return nullptr;
    }

    bool CommonHAKCAnalysis::FunctionsAreInSameCompartment(Function *F, Function *G,
                                                           HAKCCompartmentalizationPolicy &Policy) {
        auto FCompartment = Policy.GetDivision(F).GetHAKCCompartment();
        auto GCompartment = Policy.GetDivision(G).GetHAKCCompartment();
        return FCompartment == GCompartment;
    }

    bool CommonHAKCAnalysis::IsAllocation(Value *V) {
        V = getDef(V, false);
        if (auto *call = dyn_cast<CallInst>(V)) {
            if (IsAllocationFunction(call->getCalledFunction())) {
                return true;
            }
        }
        return false;
    }

    StringRef CommonHAKCAnalysis::GetFunctionName(Function *F) {
        /* The compiler will sometimes rename functions when directed to, especially for
         * kernel modules, in order to facilitate general functionality.  However,
         * the debug symbols maintain the original name, so use that name if it is available */
        if (F->getSubprogram() && !F->getSubprogram()->getName().empty()) {
            return F->getSubprogram()->getName();
        }

        return F->getName();
    }

    bool CommonHAKCAnalysis::IsStringType(Type *Ty) {
        return Ty->isArrayTy() && Ty->getArrayElementType()->isIntegerTy(8);
    }

    Instruction *CommonHAKCAnalysis::GetTargetTypeCast(Instruction *I, Type *TargetType) {
        if (I->getType() == TargetType) {
            return I;
        }

        for (auto *U: I->users()) {
            if (auto *BitCastI = dyn_cast<BitCastInst>(U)) {
                if (BitCastI->getDestTy() == TargetType) {
                    return BitCastI;
                }
            }
        }

        return nullptr;
    }

    std::string CommonHAKCAnalysis::GetModuleFullPath(Module &M) {
        const auto &SourceFileName = M.getSourceFileName();
        SmallString<256> FilenameVec = StringRef(SourceFileName);
        SmallString<256> RealPath;

        auto err = sys::fs::real_path(FilenameVec, RealPath, true);
        if (err) {
            errs() << "Could not get real path to " << M.getSourceFileName() << "\n";
            throw std::exception();
        }
        return RealPath.str().str();
    }

    bool CommonHAKCAnalysis::ValueIsUsedAsPointer(Value *V) {
        if (!IsPointerLikeType(V->getType())) {
            return false;
        }

        bool CallIsUsedAsPointer = V->getType()->isPointerTy();
        if (V->getType()->isIntegerTy()) {
            CallIsUsedAsPointer = false;
            /* Search for uses that determine if the call is considered a pointer or integer */
            for (auto &U: V->uses()) {
                if (auto *IToPtrI = dyn_cast<IntToPtrInst>(U.getUser())) {
                    CommonHAKCAnalysis::getWriter(GetSystemInfo().OutputDebugInfo(IToPtrI->getFunction())) << "User of "
                            << *V << " is an inttoptr: " << *U.getUser()
                            << "\n";
                    CallIsUsedAsPointer = true;
                } else if (auto *BinOp = dyn_cast<BinaryOperator>(U.getUser())) {
                    if (BinOp->getOpcode() == BinaryOperator::Add) {
                        unsigned OpNum = (U.getOperandNo() + 1) % 2;
                        auto *OtherOp = U.getUser()->getOperand(OpNum);
                        CommonHAKCAnalysis::getWriter(GetSystemInfo().OutputDebugInfo(BinOp->getFunction())) <<
                                "Checking operator " << OpNum << " of " << *BinOp <<
                                ": "
                                << *OtherOp << "\n";
                        if (OtherOp->getType()->isPointerTy()) {
                            /* V is an integer (which could still be used as a pointer), but is used in an add operation
                             * that involves another pointer.  Adding two pointers together does not make sense, so V
                             * is a true integer and not a pointer.
                             */
                            break;
                        }
                    }
                }

                if (CallIsUsedAsPointer) {
                    break;
                }
            }
        }

        return CallIsUsedAsPointer;
    }

    std::string CommonHAKCAnalysis::GetTransformedPath(StringRef Path) const {
        if (Path.empty()) {
            return Path.str();
        }

        auto SourcePath = SystemInfo.GetSourcePath();
        if (SourcePath.size() == 0) {
            errs() << "Invalid " << SourcePath << "!\n";
            throw std::exception();
        }

        auto BuildPath = SystemInfo.GetBuildPath();
        if (BuildPath.size() == 0) {
            errs() << "Invalid " << BuildPath << "!\n";
            throw std::exception();
        }

        unsigned length;
        std::string Replacement;
        if (Path.starts_with(BuildPath)) {
            length = BuildPath.size();
            Replacement = HAKC_BUILD_PATH_REPLACEMENT.str();
        } else if (Path.starts_with(SourcePath)) {
            length = SourcePath.size();
            Replacement = HAKC_SOURCE_PATH_REPLACEMENT.str();
        } else {
            errs() << "Path " << Path << " does not start with either "
                    << BuildPath << " or " << SourcePath << "!\n";
            throw std::exception();
        }

        if (!sys::path::is_separator(Path[length])) {
            Replacement += sys::path::get_separator();
        }

        std::string Result = Path.str();
        Result.replace(0, length, Replacement);
        return Result;
    }
} // namespace hakc
