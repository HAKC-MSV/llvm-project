//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the post target action class, e.g., what needs to be set
/// after a transfer function is called.
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCTRANSFORMER_H
#define HAKC_HAKCTRANSFORMER_H

#include <map>

#include "HAKCTransferState.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCDatabase/HAKCServerClient.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"

#include <llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCSystemInformation.h>

using namespace llvm;

namespace llvm::hakc {
/**
 * A  class that defines the API for creating HAKC transformations.
 * Must be subclassed to provide architecture specific functionality.
 */
class HAKCTransformer {
public:
  HAKCTransformer(HAKCModuleAnalysis &ModuleAnalysis,
                  HAKCServerClientBase &Client);

  virtual ~HAKCTransformer() = default;

  void runEnforcement();

  /**
   * Create a pointer suitable for dereferencing
   * @param HAKCPointer
   * @param I
   * @return The last Instruction created, placed immediately prior to I
   */
  virtual Value *CreateSafePointer(HAKCPointerBase &HAKCPointer,
                                   Instruction *I);

  /**
   * Create a HAKC Pointer check at I
   * @param HAKCPointer
   * @param I
   * @return
   */
  virtual Value *CreateDataAuthentication(HAKCPointerBase &HAKCPointer,
                                          Instruction *I);

  /**
   * Create a HAKC Code Pointer check at I
   * @param HAKCPointer
   * @param I
   * @return
   */
  virtual Value *CreateCodeAuthentication(HAKCPointerBase &HAKCPointer,
                                          Instruction *I);

  /**
   * Computes the size of the transfer and then calls
   * CreateSizedCompartmentTransfer
   * @param HAKCPointer
   * @param I
   * @param Target
   * @param IsData
   * @return
   */
  virtual Instruction *CreateCompartmentTransfer(HAKCPointerBase &HAKCPointer,
                                                 Instruction *I,
                                                 GlobalValue *Target,
                                                 bool IsData);

  /**
   * Creates a Compartment Transfer of ManagedHAKCPointer at I. The arguments to
   * the transfer function are: 0. ManagedHAKCPointer
   *  1. Size of transfer
   *  2. IsData
   *  3. Target CompartmentID
   *  4. OtherArgs
   *
   *  This order is to allow for additional architecture specific information to
   * be passed if needed
   *
   * @param HAKCPointer
   * @param I
   * @param Target
   * @param IsData
   * @param Size
   * @return
   */
  virtual Instruction *
  CreateSizedCompartmentTransfer(HAKCPointerBase &HAKCPointer, Instruction *I,
                                 GlobalValue *Target, bool IsData,
                                 ConstantInt *Size);

  /**
   * Creates a BitCastInst of Operand to TargetType at I
   * @param HAKCPointer
   * @param TargetType
   * @param I
   * @return
   */
  virtual Value *CreateBitCast(HAKCPointerBase &HAKCPointer, Type *TargetType,
                               Instruction *I);

  /**
   * Create a signed pointer using the color of HAKCPointer
   * @param HAKCPointer
   * @param I
   * @param Target
   * @param IsData
   * @return
   */
  virtual Instruction *CreateSignWithDivision(HAKCPointerBase &HAKCPointer,
                                              Instruction *I,
                                              GlobalValue *Target, bool IsData);

  /**
   * Create a Outside Transfer Function
   * @param F
   * @return
   */
  Function *CreateTransferFunction(Function *F);

  /**
   * Create a transfer function to a variadic function in a different
   * compartment
   * @param Call
   * @param PointerManager
   * @return
   */
  Function *
  CreateTransferToVariadic(CallInst *Call,
                           HAKCPointerManager *PointerManager = nullptr);

  Module &getModule() const;

  CommonHAKCAnalysis &getCommonAnalysis() const;

  HAKCSystemInformation &getSystemInfo() const;

  virtual Type *HAKCAuthenticationRetType(unsigned AddrSpace);

  ConstantInt *getTrue();

  ConstantInt *getFalse();

  ConstantInt *getInt64(uint64_t Value);

  ConstantInt *getInt32(uint32_t Value);

  ConstantInt *GetDefaultObjectSize();

  /**
   * Returns true if ManagedHAKCPointer is an appropriately sized integer for
   * use as a pointer
   * @param HAKCPointer
   */
  //        virtual bool ValidateHAKCIntegerPointerSize(ManagedHAKCPointerP
  //        HAKCPointer);

  virtual unsigned GetPointerAddrSpace(HAKCPointerBase &HAKCPointer);

  static unsigned GetPointerAddrSpace(Value *V);

  /**
   * Creates metadata associated with a Compartment for proper loading by the
   * kernel
   * @param Compartment
   * @return
   */
  virtual GlobalVariable *
  AddCompartmentMetadataEntry(HAKCCompartment &Compartment);

  virtual Function *PopulateGlobalTransfer(Function *GlobalTransfer,
                                           GlobalVariable *GlobalVar);

  void AddCompartmentMetadata();

  static bool TransferFunctionShouldBeCreated(Function *F);

  StructType *GetKernelParamType() const;

  bool FunctionDefinedInAssembly(Function *F) const;

  Function *GetFunctionByName(StringRef Name, FunctionType *FuncTy) const;

  HAKCModuleAnalysis &GetModuleAnalysis() const;

  bool FunctionIsInAnalysisSet(Function *F) const;

protected:
  HAKCModuleAnalysis &ModuleAnalysis;
  HAKCServerClientBase &Client;
  IRBuilder<> HAKCIRBuilder;

  std::map<Function *, Function *> VariadicTransferFunctions;

  static HAKCLogger &getLogger(HAKCLogLevel log_level);

  static HAKCLogger &GetLogger(HAKCLogLevel log_level, bool suppress_output);

  void InitAnalysis();

  void TransformModule();

  void TransformFunctions();

  void emitModParamGetCtx(GlobalValue *kernparam);

  std::string getGlobalHAKCSectionName(GlobalVariable *GV) const;

  void RegisterUsedCompartment(const HAKCCompartment &compartment) const;

  bool TransferIsNeeded(GlobalVariable *GlobalVar);

  bool ConstantStructTransferIsNeeded(ConstantStruct *ConstStruct);

  bool AliasShouldBeCreated(Function *F);

  bool isModuleCompartmentalized() const;

  void MoveGlobalsToHAKCSection() const;

  void AddTransferFunctions();

  /**
   * Checks that ManagedHAKCPointer and I are valid, and sets the HAKCIRBuilder
   * location to I
   * @param HAKCPointer
   * @param I
   */
  void ValidateHAKCPointerAndLocation(const HAKCPointerBase &HAKCPointer,
                                      Instruction *I);

  /**
   * Performs the transformations needed for creating a safe pointer
   * @param HAKCPointer
   * @param I
   * @return
   */
  //        virtual Value *CreateSafePointer_Arch(ManagedHAKCPointerP
  //        HAKCPointer, Instruction *I) = 0;

  /**
   * Creates a Call to the specified function
   * @param name
   * @param RetTy
   * @param Args
   * @return
   */
  CallInst *CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args);

  CallInst *CreateCall(const function_def_t &Callee, ArrayRef<Value *> Args);

  CallInst *CreateCall(Function *Callee, ArrayRef<Value *> Args);

  Instruction *CreateCallWithResultCast(StringRef Name, Type *RetTy,
                                        ArrayRef<Value *> Args,
                                        Value *ValueToTypeMatch);

  Instruction *CreateCallWithResultCast(Function *Callee,
                                        ArrayRef<Value *> Args,
                                        Value *ValueToTypeMatch);

  Instruction *CastCallToType(CallInst *Call, Value *ValueToTypeMatch);

  /**
   * Gets or inserts the GlobalVariable containing the list of valid targets
   * from the Compartment F belongs to
   * @param F
   * @return
   */
  GlobalVariable *GetValidTargetCompartments(Function *F) const;

  GlobalVariable *
  GetValidTargetCompartments(const HAKCCompartmentDivision &Division) const;

  /**
   * Return the type that HAKC Compartment Entry Tokens are in the source
   * @return
   */
  Type *GetEntryTokenType(unsigned AddrSpace) const;

  /**
   * Create the argument set for a HAKC data check
   * @param HAKCPointer
   * @param I
   * @return
   */
  virtual void CreateDataAuthArguments(HAKCPointerBase &HAKCPointer,
                                       Instruction *I,
                                       SmallVectorImpl<Value *> &Result);

  /**
   * Create the argument set for a HAKC code check
   * @param HAKCPointer
   * @param I
   * @return
   */
  virtual void CreateCodeAuthArguments(HAKCPointerBase &HAKCPointer,
                                       Instruction *I,
                                       SmallVectorImpl<Value *> &Result);

  /**
   * Create the argument set for a HAKC Compartment transfer
   * @param HAKCPointer
   * @param Target
   * @param IsData
   * @param Size
   * @return
   */
  virtual void CreateTransferArguments(HAKCPointerBase &HAKCPointer,
                                       GlobalValue *Target, bool IsData,
                                       ConstantInt *Size,
                                       SmallVector<Value *> &Result);

  /**
   * Create a normal HAKC Compartment transfer for objects that do not have a
   * custom transfer function
   * @param HAKCPointer
   * @param Target
   * @param IsData
   * @param Size
   * @return
   */
  virtual Instruction *CreateDefaultTransfer(HAKCPointerBase &HAKCPointer,
                                             GlobalValue *Target, bool IsData,
                                             ConstantInt *Size);

  /**
   *
   * @param HAKCPointer
   * @param Target
   * @param IsData
   * @param Size
   * @return
   */
  virtual Instruction *CreateCustomTransfer(HAKCPointerBase &HAKCPointer,
                                            GlobalValue *Target, bool IsData,
                                            ConstantInt *Size);

  bool HAKCPointerHasCustomTransfer(HAKCPointerBase &HAKCPointer);

  Value *CreatePointerCast(HAKCPointerBase &HAKCPointer,
                           PointerType *PointerTy);

  Value *CreateReturnCast(HAKCPointerBase &HAKCPointer, Value *V);

  /**
   * Return the custom transfer function if one exists
   * @param HAKCPointer
   * @return
   */
  virtual custom_transfer_def_t
  GetCustomTransferFunction(HAKCPointerBase &HAKCPointer);

  void ValidateLocation(Instruction *I);

  virtual void ValidateHAKCPointer(const HAKCPointerBase &HAKCPointer);

  Function *CreateNonVariadicTransferFunction(Function *F);

  Function *
  PopulateTransferFunction(Function *Target, Function *TransferFunction,
                           CallInst *CallSite = nullptr,
                           HAKCPointerManager *PointerManager = nullptr);

  Function *GetTransferFunction(Function *F) const;

  virtual bool TargetIsKernel(GlobalValue *Target);

  virtual void TransferStructMembers(ConstantStruct *ConstStruct,
                                     Function *GlobalTransfer,
                                     GlobalValue *GlobalVar);

  virtual bool TransferShouldBeCreated(Value *V, GlobalValue *Target);

  virtual custom_transfer_def_t
  GetCustomTransferFunctionForType(HAKCTypeP HAKCType);

  virtual Instruction *
  CreateVoidCastCompartmentTransfer(HAKCPointerBase &HAKCPointer,
                                    Instruction *I, GlobalValue *Target,
                                    HAKCTypeP TypeToUse);

  virtual bool NoKernelTransfers(Function *Target);

  void InitNewFunction(Function *F, StringRef EntryBlockName);

  HAKCPointerBaseP CreateNewManagedPointer(Value *BaseDefinition) const;

  Value *CreateActionCall(HAKCTransferAction &TransferAction,
                          HAKCTransferState &TransferState);

  HAKCTypeP InferHAKCType(Argument &Arg, CallInst *CallSite,
                          HAKCPointerManager *PointerManager) const;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCTRANSFORMER_H
