//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

namespace llvm::hakc {
class HAKCModuleAnalysis {
protected:
  SmallVector<HAKCCompartment, 8> UsedCompartments;
  CommonHAKCAnalysis &CommonAnalysis;
  FunctionList AnalysisFunctions;
  HAKCTypeIdentifier &TypeIdentifier;
  HAKCCompartmentalizationPolicy &Policy;
  HAKCTransformer Transformer;

  void InitAnalysis();

  GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV);

  void emitModParamGetCtx(GlobalValue *kernparam);

  bool functionEscapes(Function *F);

  void RegisterUsedCompartment(HAKCCompartment &compartment);

  std::string getGlobalHAKCSectionName(GlobalVariable *GV) const;

  void TransformModule();

  void TransformFunctions();

  bool FunctionNeedsAnalysis(Function *F) const;

  Function *CreateInitTransfer(GlobalVariable *GlobalVar);

  static StringRef GlobalInitTransferPrefix();

  StringRef GlobalInitTransferSectionName() const;

  StringRef GlobalInitTransferPointerSectionName() const;

  std::string GlobalVariableROSectionName(GlobalVariable *GlobalVar);

  void PopulateGlobalInitTransferFunc(Function *GlobTransfer,
                                      GlobalVariable *GlobalVar);

  bool TransferIsNeeded(GlobalVariable *GlobalVar);

  bool ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct);

  bool AliasShouldBeCreated(Function *F);

  bool isModuleCompartmentalized();

  void MoveGlobalsToHAKCSection();

  void AddTransferFunctions();

public:
  HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis,
                     HAKCCompartmentalizationPolicy &Policy);

  void performTransformations();

  void AddCompartmentMetadata();

  bool TransferFunctionShouldBeCreated(Function *F);

  StructType *GetKernelParamType();

  void CreateInitGlobalMemberTransfers();

  Module &GetModule() const;

  bool FunctionDefinedInAssembly(Function *F);

  CommonHAKCAnalysis &GetCommonAnalysis();

  Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy);

  HAKCTypeIdentifier &GetTypeIdentifier() const;

  HAKCTransformer &GetTransformer();

  bool FunctionIsInAnalysisSet(Function *F);
};
} // namespace llvm::hakc

#endif // HAKC_HAKCMODULEANALYSIS_H
