//
// Created by derrick on 9/8/21.
//

#ifndef PMC_HAKCTYPEIDENTIFIER_H
#define PMC_HAKCTYPEIDENTIFIER_H

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Local.h"

#include "HAKCFunctionInfo.h"
#include "HAKCGlobalInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
// #include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/HAKCModuleAnalysis.h"

#include <map>
#include <set>

using namespace llvm;

namespace llvm::hakc {
typedef std::shared_ptr<HAKCFunctionInfo> HAKCFunctionP;
typedef std::shared_ptr<HAKCSymbolInfo> HAKCSymbolP;
typedef std::shared_ptr<HAKCGlobalInfo> HAKCGlobalP;

class CommonHAKCAnalysis;
class HAKCPointerBase;

class HAKCTypeIdentifier {
public:
  explicit HAKCTypeIdentifier(CommonHAKCAnalysis &Analysis);

  void OutputYAML(raw_ostream &out) const;

  HAKCSymbolP FindSymbol(Value *V, bool SearchUnmapped = false);

  HAKCSymbolP FindYamlSymbol(const HAKCYamlSymbol &YamlSymbol);

  HAKCTypeP FindType(HAKCPointerBase &HAKCPointer);

  HAKCTypeP FindHAKCType(Value *V);

  HAKCTypeP FindHAKCTypeForUse(Use &U);

  void ProcessDebugInfo();

  Module &GetModule() const;

  ModuleAnalysisManager &GetMAM() const;

  void GetHAKCTypes(SmallVectorImpl<HAKCTypeP> &Results) const;

  Type *GetTypeFromString(StringRef TypeStr) const;

  void AddIgnoredType(StringRef TypeName) const;

  std::map<const DIGlobalVariable *, HAKCGlobalP> GetGlobals();

  std::set<HAKCGlobalP> GetUnmappedGlobals();

  std::map<const DISubprogram *, HAKCFunctionP> GetFunctions();

  std::set<HAKCFunctionP> GetUnmappedFunctions();

  DebugInfoFinder GetDbgInfoFinder();

  HAKCTypeP FindCalledFunctionType(FunctionType *FunctionTy);

  static FunctionType *GetIndirectCallFunctionType(const CallInst *CallI);

  HAKCTypeP FindType(Type *Ty) const;

  HAKCTypeP FindType(const DIType *type);

  HAKCTypeP FindPointeeType(HAKCPointerBase &HAKCPointer);

  HAKCTypeP FindPointeeType(HAKCTypeP BaseType);

  HAKCTypeP FindPointerType(const HAKCTypeInfo &BaseType);

  HAKCTypeP HandleType(const DIType *type);


  void AddTypeMapping(const DIType *type, const HAKCTypeP &HAKCType);

  std::string GetTypeName(const DIType *type) const;

  HAKCGlobalP HandleGlobal(const DIGlobalVariable *DIGV);

  void AddGlobalMapping(const DIGlobalVariable *DIGV,
                        const HAKCGlobalP &HAKCSymbol);

  GlobalVariable *FindGlobal(const DIGlobalVariable *DIGV) const;

  HAKCFunctionP HandleFunction(const DISubprogram *SubProg);

  void FunctionTemporalAnalysis(const DISubprogram *SubProg);

  // HAKCFunctionP HandleFunctionTemporalAnalysis(const DISubprogram *SubProg);

  void AddFunctionMapping(const DISubprogram *SubProg,
                          const HAKCFunctionP &HAKCFunction);

  void FindAllGlobalsUsed(Value *V, std::set<GlobalObject *> &GlobalSet);

  void FindUsesInGlobals();

  void FindUsesInFunctions();

  void FindTypesInFunctions();

  HAKCSymbolP AddNoDebugGlobal(GlobalObject *GlobalObj);

  HAKCFunctionP AddNoDebugFunction(Function *F);

  void AddUsedGlobals(const std::set<GlobalObject *> &GlobalObjects,
                      const HAKCSymbolP &UserSymbol);

  HAKCTypeP HandleIndirectCall(CallInst *CallI);

  HAKCFunctionP FindFunction(const Function *F, bool SearchUnmapped = false);

  HAKCGlobalP FindGlobal(const GlobalVariable *GV,
                         bool SearchUnmapped = false) const;

  HAKCTypeP CreateNoDebugType(Type *Ty) const;

  void FindIndirectCallSource(
      CallInst *CallI,
      std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path);

  void CreateIndirectCallSourceLink(
      Value *V, std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &Path);

  HAKCTypeP GetArgumentHAKCType(Argument *Arg);

  HAKCTypeP GetArgumentHAKCType(const DISubroutineType *FunctionTy,
                                unsigned ArgNo);

  FunctionType *GetLLVMFunctionTy(const DISubroutineType *FunctionTy);

  Type *GetLLVMType(const DIType *);

  HAKCTypeP AddMissingPointerType(const HAKCTypeP &BaseType);

  Type *FindNamedType(StringRef TypeName) const;

  Type *FindAnonymousType(const DICompositeType *CompositeTy);

  void TemporalAnalysisHandleCall(CallInst* Call, HAKCFunctionP FP);
  void TemporalAnalysisHandleLoad(LoadInst* Load, HAKCFunctionP FP);
  void TemporalAnalysisHandleStore(StoreInst* Store, HAKCFunctionP FP);
  // void TemporalAnalysis(HAKCModuleAnalysis &ModuleAnalysis);
  void FindHAKCTypeMapDebug(Value*V, bool printall);

  CommonHAKCAnalysis &AnalysisHelper;
  DebugInfoFinder DbgInfoFinder;
  std::map<const DIType *, HAKCTypeP> types;
  std::map<const DIGlobalVariable *, HAKCGlobalP> globals;
  std::map<const DISubprogram *, HAKCFunctionP> functions;
  std::set<HAKCGlobalP> UnmappedGlobals;
  std::set<HAKCFunctionP> UnmappedFunctions;
  std::set<HAKCTypeP> MissingPointerTypes;
  std::map<CallInst *, HAKCTypeP> IndirectCallsTypes;
  std::map<const DICompositeType *, Type *> AnonymousTypes;
  std::map<Value*, HAKCTypeP> FindHAKCTypeMap;
  const DIScope *CompilationUnitScope;
  int recursion_limit = 2;
  int recursion_depth_di;
  int recursion_depth_llvm;
  int recursion_depth_other;
};
} // namespace llvm::hakc

#endif // PMC_HAKCTYPEIDENTIFIER_H
