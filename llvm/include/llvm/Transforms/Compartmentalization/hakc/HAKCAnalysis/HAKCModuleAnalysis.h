//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the module analysis subclass class of common analysis.
/// It contains specific functionality related to compartmentalization analysis
/// at the module level.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCMODULEANALYSIS_H
#define HAKC_HAKCMODULEANALYSIS_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

namespace llvm::hakc {
class HAKCModuleAnalysis {
public:
  SmallVector<HAKCCompartment, 8> UsedCompartments;
  CommonHAKCAnalysis &CommonAnalysis;
  FunctionList AnalysisFunctions;
  HAKCTypeIdentifier &TypeIdentifier;

  GlobalValue *ExtractGlobalFromKernelParam(GlobalVariable *GV) const;

  bool functionEscapes(Function *F) const;

  bool FunctionNeedsAnalysis(Function *F) const;

  static bool useEscapes(const Use &U);

  HAKCModuleAnalysis();

  HAKCModuleAnalysis(CommonHAKCAnalysis &CommonAnalysis);

  StructType *GetKernelParamType() const;

  Module &GetModule() const;

  HAKCSystemInformation &GetSystemInformation() const;

  bool FunctionDefinedInAssembly(Function *F) const;

  CommonHAKCAnalysis &GetCommonAnalysis() const;

  Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy) const;

  HAKCTypeIdentifier &GetTypeIdentifier() const;

  bool FunctionIsInAnalysisSet(Function *F);

  void OutputYAML(raw_ostream &out) const;

  void runAnalysis();

  void runEnforcement(bool UseSimulatedClient);

  std::unique_ptr<HAKCServerClientBase> ConstructClient(bool UseSimulatedClient);

};

} // namespace llvm::hakc

#endif // HAKC_HAKCMODULEANALYSIS_H
