//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the information associated with identifying a llvm type
/// (debug types, llvm types, debug records, etc.)
///
//===----------------------------------------------------------------------===//
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

  HAKCTypeP GetVoidPointerPointeeType();

  HAKCTypeP GetVoidPointerType();

  void ProcessDebugInfo();

  Module &GetModule() const;

  ModuleAnalysisManager &GetMAM() const;

  void GetHAKCTypes(SmallVectorImpl<HAKCTypeP> &Results) const;

  Type *GetTypeFromString(StringRef TypeStr) const;

  void AddIgnoredType(StringRef TypeName) const;

  std::map<const DIGlobalVariable *, HAKCGlobalP> GetGlobals();

  std::set<HAKCGlobalP> GetUnmappedGlobals();

  void ModifyTypeUse(Function* F, const std::shared_ptr<HAKCTypeInfo> &HAKCTy, TypePerms perm);

  std::map<const DISubprogram *, HAKCFunctionP> GetFunctions();

  std::set<HAKCFunctionP> GetUnmappedFunctions();

  DebugInfoFinder GetDbgInfoFinder();

  HAKCTypeP FindType(Type *Ty) const;

  void FindAllTypes(Type *Ty, SmallVectorImpl<HAKCTypeP> &Results) const;

  HAKCTypeP FindPointeeType(HAKCPointerBase &HAKCPointer);

  HAKCTypeP FindPointeeType(HAKCTypeP BaseType);

  HAKCTypeP FindPointerType(const HAKCTypeInfo &BaseType);

  HAKCTypeP HandleType(const DIType *type);

  HAKCTypeP FindType(const DIType *type);

  void AddTypeMapping(const DIType *type, const HAKCTypeP &HAKCType);

  static std::string GetTypeName(const DIType *type);
  static std::string GetTypeName(Type *Ty);
  static std::string GetDbgName(const HAKCTypeInfo &HAKCTy);

  HAKCGlobalP HandleGlobal(const DIGlobalVariable *DIGV);

  void AddGlobalMapping(const DIGlobalVariable *DIGV,
                        const HAKCGlobalP &HAKCSymbol);

  GlobalVariable *FindGlobal(const DIGlobalVariable *DIGV) const;

  HAKCFunctionP HandleFunction(const DISubprogram *SubProg);

  // void FunctionTemporalAnalysis(const DISubprogram *SubProg);

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

  static FunctionType *GetIndirectCallFunctionType(const CallInst *CallI);

  bool IsStructTypeThatStartsWithPointerLikeType(const HAKCTypeInfo &HAKCTy);

  bool IsPointerLikeType(const DIType *DIType);

  static const DIType *
  GetFirstStructMemberType(const DICompositeType *DICompositeTy);

  HAKCFunctionP FindFunction(const Function *F, bool SearchUnmapped = false);

  HAKCGlobalP FindGlobal(const GlobalVariable *GV,
                         bool SearchUnmapped = false) const;

  HAKCTypeP FindCalledFunctionType(FunctionType *FunctionTy) const;

  HAKCTypeP CreateNoDebugType(Type *Ty);

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

  HAKCTypeP FindTypeFromDebug(const DbgVariableRecord &DVR, Value *V);

  HAKCTypeP CheckCallUses(Value *V);

  DIDerivedType *FindUnionMember(const DICompositeType *UnionDef,
                                 unsigned MemberOffset) const;

  CommonHAKCAnalysis &AnalysisHelper;
  DebugInfoFinder DbgInfoFinder;
  std::map<const DIType *, HAKCTypeP> TypesWithDebugInfo;
  std::map<const DIGlobalVariable *, HAKCGlobalP> globals;
  std::map<const DISubprogram *, HAKCFunctionP> functions;
  // std::map<Function *, HAKCFunctionP> FindHAKCFunctionMap;
  std::set<HAKCGlobalP> UnmappedGlobals;
  std::set<HAKCFunctionP> UnmappedFunctions;
  std::set<HAKCTypeP> TypesMissingDebugInfo;
  std::map<CallInst *, HAKCTypeP> IndirectCallsTypes;
  std::map<const DICompositeType *, Type *> AnonymousTypes;
  std::map<Value *, HAKCTypeP> FindHAKCTypeMap;
  const DIScope *DefinitionLocationScope;
  const std::vector<StructType *> IdentifiedStructTypes;
};
} // namespace llvm::hakc

#endif // PMC_HAKCTYPEIDENTIFIER_H
