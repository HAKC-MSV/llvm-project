//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_COMMONHAKCANALYSIS_H
#define HAKC_COMMONHAKCANALYSIS_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCWriter.h"
#include <map>

namespace llvm::hakc {
class HAKCTransformer;

typedef std::function<llvm::Value *(llvm::Value *)> hakc_allocation_size_map_t;

class CommonHAKCAnalysis {
protected:
  Module &M;

  std::map<Value *, SmallVector<Value *>> DefchainCache;

  HAKCSystemInformation SystemInfo;

  static bool IsFunctionInHAKCTransferFunctionList(
      Function *F, iterator_range<HAKCTransferList::iterator> Range);

  void InitConfig(StringRef ConfigPath);

public:
  virtual ~CommonHAKCAnalysis() = default;

  explicit CommonHAKCAnalysis(Module &M, StringRef ConfigPath);

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
                                    HAKCCompartmentalizationPolicy &Policy);

  bool IsSafeTransitionFunction(Function *F);

  static std::string getVariadicTransferName(Function *F);

  static std::string getOriginalTransformedName(Function *F);

  bool IsHAKCTransferFunction(Function *F);

  bool IsHAKCCustomTransferFunction(Function *F);

  bool IsHAKCCompartmentalizationSupportFunction(Function *F);

  bool IsHAKCFunction(Function *F);

  bool IsAllocationFunction(Function *F);

  bool functionIsTransferCandidate(Function *F,
                                   HAKCCompartmentalizationPolicy &Policy);

  static hakc::HAKCWriter &getWriter(bool DebugActive);

  FunctionType *GetDataAuthenticationFunctionType(Module &M,
                                                  unsigned AddrSpace = 0);

  FunctionType *GetCodeAuthenticationFunctionType(Module &M,
                                                  unsigned AddrSpace = 0);

  FunctionType *GetTransferFunctionType(Module &M, unsigned AddrSpace = 0);

  static bool FunctionIsComplexVariadic(Function *F);

  static StringRef GetFunctionName(Function *F);

  static bool isRegisterRead(Value *v);

  bool IsIgnoredGlobal(Value *V);

  static bool
  FunctionsAreInSameCompartment(Function *F, Function *G,
                                HAKCCompartmentalizationPolicy &Policy);

  bool IsSafeTransitionCall(CallBase *call);

  bool IsAllocation(Value *V);

  static bool
  IsCompartmentalizedFunction(Function *F,
                              HAKCCompartmentalizationPolicy &Policy);

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
                              HAKCCompartmentalizationPolicy &Policy);

  static void VerifyFunction(Function *F);

  bool ValueIsUsedAsPointer(Value *V);

  hakc::function_def_t GetHAKCTransferDefinition(Function *F);

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

private:
  static bool valueHasAttribute(Value *v, Attribute::AttrKind Kind);
};
} // namespace llvm::hakc

#endif // HAKC_COMMONHAKCANALYSIS_H
