//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the common analysis base class. It contains various references
/// to useful structs that are relevant to compartmentalization.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_COMMONHAKCANALYSIS_H
#define HAKC_COMMONHAKCANALYSIS_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCLogger.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCWriter.h"
#include <map>

namespace llvm::hakc {
    class HAKCTransformer;

    typedef std::function<Value *(Value *)> hakc_allocation_size_map_t;

    class CommonHAKCAnalysis {
    protected:
        Module &M;

        ModuleAnalysisManager &MAM;

        std::map<Value *, SmallVector<Value *> > DefchainCache;

        HAKCSystemInformation SystemInfo;

        std::shared_ptr<HAKCLogger> _HAKCLog;

        static bool IsFunctionInHAKCTransferFunctionList(
            Function *F, iterator_range<HAKCTransferList::iterator> Range);

        void InitConfig(StringRef ConfigPath, StringRef ServerSocketPath);

    public:
        virtual ~CommonHAKCAnalysis() = default;

        explicit CommonHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM,
                                    StringRef ConfigPath, StringRef ServerSocketPath);

        HAKCSystemInformation &GetSystemInfo();

        Module &GetModule() const;

        Value *getDef(Value *V, bool followLoad);

        void findDefChain(Value *v, bool followLoad,
                          SmallVectorImpl<Value *> &Results);

        static bool argShouldTransfer(Value *V);

        static bool IsPerCPUPointer(Value *V);

        static bool IsKernelUserPointer(Value *V);

        bool IsNoTransferFunction(Function *F);

        static bool FunctionIsStatic(Function *F);

        static bool FunctionHasPointerArg(Function *F);

        static bool IsOutsideTransferFunc(Function *F);

        static bool IsCapabilityReassignmentFunc(Function *F);

        static bool IsPointerLikeType(Type *Ty);

        std::string GetOutsideTransferName(Function *F);

        static bool FunctionIsModParamGetCtx(Function *F);

        bool
        ValueShouldBeReplacedWithTransfer(Value *V,
                                          HAKCServerClient &Client);

        bool IsSafeTransitionFunction(Function *F);

        static std::string getVariadicTransferName(Function *F);

        static std::string getOriginalTransformedName(Function *F);

        bool IsHAKCTransferFunction(Function *F);

        bool IsHAKCCustomTransferFunction(Function *F);

        bool IsHAKCCompartmentalizationSupportFunction(Function *F);

        bool IsHAKCFunction(Function *F);

        bool IsAllocationFunction(Function *F);

        bool functionIsTransferCandidate(Function *F,
                                         HAKCServerClient &Client);

        static HAKCLogger &getLogger(HAKCLogLevel log_level,
                                     bool suppress_output = false);

        FunctionType *GetDataAuthenticationFunctionType(unsigned AddrSpace = 0);

        FunctionType *GetCodeAuthenticationFunctionType(unsigned AddrSpace = 0);

        FunctionType *GetTransferFunctionType(unsigned AddrSpace = 0);

        static bool FunctionIsComplexVariadic(Function *F);

        static StringRef GetFunctionName(Function *F);

        static bool isRegisterRead(Value *v);

        bool IsIgnoredGlobal(Value *V);

        static bool
        FunctionsAreInSameCompartment(Function *F, Function *G,
                                      HAKCServerClient &Client);

        bool IsSafeTransitionCall(CallBase *call);

        bool IsAllocation(Value *V);

        static bool
        IsCompartmentalizedFunction(Function *F,
                                    HAKCServerClient &Client);

        static bool IsStringType(Type *Ty);

        static Instruction *GetTargetTypeCast(Instruction *I, Type *TargetType);

        virtual std::set<Intrinsic::ID> GetBitshiftIntrinsics();

        virtual std::set<Instruction::BinaryOps> GetPointerManipulatingBinaryOps();

        bool IsCallInIntrinsicSet(CallBase *Call,
                                  std::set<Intrinsic::ID> &IntrinsicsSet) const;

        static void GetModuleFullPath(Module &M, SmallVectorImpl<char> &Result);

        static bool IsMultiSSAUser(Value *V);

        static bool IsConstantUsedInGlobal(Value *V);

        static void SortGlobalList(std::vector<GlobalVariable *> &GlobalList);

        static void SortFunctionList(FunctionList &FuncList);

        static bool
        IsUncompartmentalizedSymbol(GlobalValue *GV,
                                    HAKCServerClient &Client);

        static void VerifyFunction(Function *F);

        bool ValueIsUsedAsPointer(Value *V);

        function_def_t GetHAKCTransferDefinition(Function *F);

        HAKCCustomAllocation GetAllocationDefinition(Function *F);

        bool FunctionIsAnalysisCandidate(Function *F);

        static bool
        IsFunctionInFunctionList(Function *F,
                                 iterator_range<FunctionList::iterator> Range);

        static bool functionIsEpochTransferCandidate(Function *F);

        static bool
        IsFunctionInFunctionList(Function *F,
                                 iterator_range<HAKCFunctionList::iterator> Range);

        static bool
        PointerShouldBeConsideredCode(const ManagedHAKCPointer &ManagedPointer);

        static Function *GetOriginalFunctionFromTransferFunction(Function *F);

        std::string createLogPath(StringRef BuildPath, HAKCBuildModeTypeEnum BuildMode);

    private:
        static bool valueHasAttribute(Value *v, Attribute::AttrKind Kind);
    };
} // namespace llvm::hakc

#endif // HAKC_COMMONHAKCANALYSIS_H
