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

  void GetHAKCTypes(SmallVectorImpl<HAKCTypeP> &Results) const;

  Type *GetTypeFromString(StringRef TypeStr) const;

protected:
  HAKCTypeP FindType(Type *Ty);

  HAKCTypeP FindPointeeType(HAKCPointerBase &HAKCPointer);

  HAKCTypeP FindPointeeType(const HAKCTypeInfo &BaseType);

  HAKCTypeP FindPointerType(const HAKCTypeP &BaseType);

  HAKCTypeP HandleType(const DIType *type);

  HAKCTypeP FindType(const DIType *type);

  void AddTypeMapping(const DIType *type, const HAKCTypeP &HAKCType);

  std::string GetTypeName(const DIType *type);

  HAKCGlobalP HandleGlobal(const DIGlobalVariable *DIGV);

  void AddGlobalMapping(const DIGlobalVariable *DIGV,
                        const HAKCGlobalP &HAKCSymbol);

  GlobalVariable *FindGlobal(const DIGlobalVariable *DIGV) const;

  HAKCFunctionP HandleFunction(const DISubprogram *SubProg);

  void AddFunctionMapping(const DISubprogram *SubProg,
                          const HAKCFunctionP &HAKCFunction);

  void FindAllGlobalsUsed(Value *V, std::set<GlobalObject *> &GlobalSet);

  void FindUsesInGlobals();

  void FindUsesInFunctions();

  void FindTypesInFunctions();

  unsigned GetAnonymousID(const DIType *type);

  HAKCSymbolP AddUnmappedGlobal(GlobalObject *GlobalObj);

  HAKCFunctionP AddUnmappedFunction(Function *F);

  void AddUsedGlobals(const std::set<GlobalObject *> &GlobalObjects,
                      const HAKCSymbolP &UserSymbol);

  HAKCTypeP HandleIndirectCall(CallInst *CallI);

  static FunctionType *GetIndirectCallFunctionType(const CallInst *CallI);

  HAKCFunctionP FindFunction(const Function *F, bool SearchUnmapped = false);

  HAKCGlobalP FindGlobal(const GlobalVariable *GV, bool SearchUnmapped = false);

  HAKCTypeP FindCalledFunctionType(FunctionType *FunctionTy);

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

  CommonHAKCAnalysis &AnalysisHelper;
  DebugInfoFinder DbgInfoFinder;
  std::map<const DIType *, HAKCTypeP> types;
  std::map<const DIGlobalVariable *, HAKCGlobalP> globals;
  std::map<const DISubprogram *, HAKCFunctionP> functions;
  std::map<const DIType *, unsigned> AnonymousNumberMapping;
  std::set<HAKCGlobalP> UnmappedGlobals;
  std::set<HAKCFunctionP> UnmappedFunctions;
  std::map<CallInst *, HAKCTypeP> IndirectCallsTypes;
  unsigned CurrentAnonID;
};
} // namespace llvm::hakc

#endif // PMC_HAKCTYPEIDENTIFIER_H
