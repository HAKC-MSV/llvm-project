//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCTransformer.h"

namespace llvm::hakc {
class HAKCModuleAnalysis {
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

  CommonHAKCAnalysis &GetCommonAnalysis() const;

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

} // namespace llvm::hakc

#endif // HAKC_HAKCMODULEANALYSIS_H
