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

class CommonHAKCAnalysis final {
protected:
  Module &M;

  ModuleAnalysisManager &MAM;

  std::map<Value *, SmallVector<Value *>> DefchainCache;

  HAKCSystemInformation SystemInfo;

  std::shared_ptr<HAKCLogger> _HAKCLog;

  static bool IsFunctionInHAKCTransferFunctionList(
      Function *F, iterator_range<HAKCTransferList::iterator> Range);

  void InitConfig(StringRef ConfigPath);

public:

  ~CommonHAKCAnalysis() = default;

  explicit CommonHAKCAnalysis(Module &M, ModuleAnalysisManager &MAM,
                              StringRef ConfigPath);

  HAKCSystemInformation &GetSystemInfo();

  Module &GetModule() const;

  Value *getDef(Value *V, bool followLoad);

  void findDefChain(Value *v, bool followLoad,
                    SmallVectorImpl<Value *> &Results);

  static bool argShouldTransfer(Value *V);

  static bool IsPerCPUPointer(Value *V);

  static bool IsKernelUserPointer(Value *V);

  bool IsNoTransferFunction(Function *F);

  static bool FunctionIsStatic(const Function *F);

  static bool FunctionHasPointerArg(Function *F);

  static bool IsOutsideTransferFunc(const Function *F);

  static bool IsCapabilityReassignmentFunc(const Function *F);

  static bool IsPointerLikeType(const Type *Ty);

  std::string GetOutsideTransferName(Function *F);

  static bool FunctionIsModParamGetCtx(const Function *F);

  bool
  ValueShouldBeReplacedWithTransfer(Value *V,
                                    HAKCServerClient &Client);

  bool IsSafeTransitionFunction(Function *F);

  static std::string getVariadicTransferName(const Function *F);

  static std::string getOriginalTransformedName(const Function *F);

  bool IsHAKCTransferFunction(Function *F);

  bool IsHAKCCustomTransferFunction(Function *F);

  bool IsHAKCCompartmentalizationSupportFunction(Function *F);

  bool IsHAKCFunction(Function *F);

  bool IsAllocationFunction(Function *F);

  bool functionIsTransferCandidate(Function *F,
                                   HAKCServerClient &Client);

  HAKCLogger &getHAKCLoggerObject() const;

  static HAKCLogger &getLogger(HAKCLogLevel log_level,
                               bool suppress_output = false);

  FunctionType *GetDataAuthenticationFunctionType();

  FunctionType *GetCodeAuthenticationFunctionType(unsigned AddrSpace = 0);

  FunctionType *GetTransferFunctionType();

  static bool FunctionIsComplexVariadic(const Function *F);

  static StringRef GetFunctionName(const Function *F);

  static bool isRegisterRead(Value *v);

  bool IsIgnoredGlobal(Value *V);

  static bool
  FunctionsAreInSameCompartment(Function *F, Function *G,
                                HAKCServerClient &Client);

  bool IsSafeTransitionCall(const CallBase *call);

  bool IsAllocation(Value *V);

  static bool
  IsCompartmentalizedFunction(Function *F,
                              HAKCServerClient &Client);

  static bool IsStringType(const Type *Ty);

  static Instruction *GetTargetTypeCast(Instruction *I, const Type *TargetType);

  static std::set<Intrinsic::ID> GetBitshiftIntrinsics();

  static std::set<Instruction::BinaryOps> GetPointerManipulatingBinaryOps();

  static bool IsCallInIntrinsicSet(CallBase *Call,
                       const std::set<Intrinsic::ID> &IntrinsicsSet);

  static void GetModuleFullPath(const Module &M, SmallVectorImpl<char> &Result);

  static bool IsMultiSSAUser(Value *V);

  static bool IsConstantUsedInGlobal(Value *V);

  static void SortGlobalList(std::vector<GlobalVariable *> &GlobalList);

  static void SortFunctionList(FunctionList &FuncList);

  static bool
  IsUncompartmentalizedSymbol(GlobalValue *GV,
                              HAKCServerClient &Client);

  static void VerifyFunction(Function *F);

  bool ValueIsUsedAsPointer(Value *V);

  function_def_t GetHAKCTransferDefinition(const Function *F);

  HAKCCustomAllocation GetAllocationDefinition(const Function *F);

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

  std::string createLogPath(StringRef BuildPath, HAKCBuildModeTypeEnum BuildMode) const;

private:
  static bool valueHasAttribute(Value *V, Attribute::AttrKind Kind);
};
} // namespace llvm::hakc

#endif // HAKC_COMMONHAKCANALYSIS_H
