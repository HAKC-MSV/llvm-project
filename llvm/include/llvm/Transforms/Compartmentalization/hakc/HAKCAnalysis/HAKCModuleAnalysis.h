//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
// #include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"
// #include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h"

namespace llvm::hakc {
class HAKCModuleAnalysis {
// protected:
public:
  SmallVector<HAKCCompartment, 8> UsedCompartments;
  CommonHAKCAnalysis &CommonAnalysis;
  FunctionList AnalysisFunctions;
  HAKCTypeIdentifier &TypeIdentifier;

  GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

  bool functionEscapes(Function *F);

  bool FunctionNeedsAnalysis(Function *F) const;

  bool ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct);

  bool AliasShouldBeCreated(Function *F);

  bool useEscapes(Use &U);

  StringRef GlobalInitTransferSectionName() const;

  StringRef GlobalInitTransferPointerSectionName() const;

  static StringRef GlobalInitTransferPrefix();

  HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis);

  StructType *GetKernelParamType();

  Module &GetModule() const;

  bool FunctionDefinedInAssembly(Function *F);

  CommonHAKCAnalysis &GetCommonAnalysis();

  Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);

  HAKCTypeIdentifier &GetTypeIdentifier() const;

  bool FunctionIsInAnalysisSet(Function *F);

  void OutputYAML(raw_ostream &out) const;

  void TemporalAnalysisHandleCall(CallInst *Call, HAKCFunctionP FP);
  void TemporalAnalysisHandleLoad(LoadInst *Load, HAKCFunctionP FP);
  void TemporalAnalysisHandleStore(StoreInst *Store, HAKCFunctionP FP);
  void FunctionTemporalAnalysis(const DISubprogram *SubProg);
  void TemporalAnalysis(HAKCModuleAnalysis &ModuleAnalysis);

};

// class HAKCModuleTransform : public HAKCModuleAnalysis {
// protected:
//
//   HAKCCompartmentalizationPolicy &Policy;
//   HAKCTransformer Transformer;
//
//   void InitAnalysis();
//
//   void TransformModule();
//
//   void TransformFunctions();
//
//   void emitModParamGetCtx(GlobalValue *kernparam);
//
//   std::string getGlobalHAKCSectionName(GlobalVariable *GV) const;
//
//   Function *CreateInitTransfer(GlobalVariable *GlobalVar);
//
//   void RegisterUsedCompartment(HAKCCompartment &compartment);
//
//   std::string GlobalVariableROSectionName(GlobalVariable *GlobalVar);
//
//   void PopulateGlobalInitTransferFunc(Function *GlobTransfer,
//                                       GlobalVariable *GlobalVar);
//
//   bool TransferIsNeeded(GlobalVariable *GlobalVar);
//
//   bool ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct);
//
//   bool AliasShouldBeCreated(Function *F);
//
//   bool isModuleCompartmentalized();
//
//   void MoveGlobalsToHAKCSection();
//
//   void AddTransferFunctions();
//
// public:
//   HAKCModuleTransform(CommonHAKCAnalysis &CommonAnalysis, HAKCCompartmentalizationPolicy &Policy);
//
//   void performTransformations();
//
//   void AddCompartmentMetadata();
//
//   bool TransferFunctionShouldBeCreated(Function *F);
//
//   StructType *GetKernelParamType();
//
//   void CreateInitGlobalMemberTransfers();
//
//   bool FunctionDefinedInAssembly(Function *F);
//
//   Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);
//
//   HAKCTransformer &GetTransformer();
//
//   bool FunctionIsInAnalysisSet(Function *F);
// };
} // namespace llvm::hakc

#endif // HAKC_HAKCMODULEANALYSIS_H
