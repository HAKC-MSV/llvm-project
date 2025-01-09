//
// Created by de29664 on 3/21/23.
//

#ifndef HAKC_HAKCTRANSFORMER_H
#define HAKC_HAKCTRANSFORMER_H

#include <map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCTypeIdentifier.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/ManagedHAKCPointer.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"

#include <sstream>


using namespace llvm;

namespace llvm::hakc {
    /**
     * A  class that defines the API for creating HAKC transformations.
     * Must be subclassed to provide architecture specific functionality.
     */
    class HAKCTransformer {
    public:
        HAKCTransformer(HAKCCompartmentalizationPolicy &Policy, HAKCModuleAnalysis &HAKCAnalysis);

        virtual ~HAKCTransformer() = default;

        /**
         * Create a pointer suitable for dereferencing
         * @param HAKCPointer
         * @param I
         * @return The last Instruction created, placed immediately prior to I
         */
        virtual Value *CreateSafePointer(HAKCPointerBase &HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateDataAuthentication(hakc::HAKCPointerBase &HAKCPointer, Instruction *I);

        /**
         * Create a HAKC Code Pointer check at I
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual Value *CreateCodeAuthentication(hakc::HAKCPointerBase &HAKCPointer, Instruction *I);

        /**
         * Computes the size of the transfer and then calls CreateSizedCompartmentTransfer
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsData
         * @return
         */
        virtual Instruction *
        CreateCompartmentTransfer(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target, bool IsData);

        /**
         * Creates a Compartment Transfer of ManagedHAKCPointer at I. The arguments to the transfer function are:
         *  0. ManagedHAKCPointer
         *  1. Size of transfer
         *  2. IsData
         *  3. Target CompartmentID
         *  4. OtherArgs
         *
         *  This order is to allow for additional architecture specific information to be passed if needed
         *
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsPerCPU
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *
        CreateSizedCompartmentTransfer(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target,
                                       bool IsData, ConstantInt *Size);

        /**
         * Creates a BitCastInst of Operand to TargetType at I
         * @param HAKCPointer
         * @param TargetType
         * @param I
         * @return
         */
        virtual Value *CreateBitCast(hakc::HAKCPointerBase &HAKCPointer, Type *TargetType, Instruction *I);


        /**
         * Create a signed pointer using the color of HAKCPointer
         * @param HAKCPointer
         * @param I
         * @param Target
         * @param IsData
         * @return
         */
        virtual Instruction *
        CreateSignWithDivision(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target, bool IsData);


        /**
         * Create a Outside Transfer Function
         * @param F
         * @return
         */
        Function *CreateTransferFunction(Function *F);

        /**
         * Create a transfer function to a variadic function in a different compartment
         * @param Call
         * @return
         */
        Function *CreateTransferToVariadic(CallInst *Call);

        /**
         * Create Architecture specific transformations for a new transfer function
         * @param Original
         * @param Transfer
         */
        //        void CreateTransferFunctionFinalize_Arch(Function *Original, Function *Transfer);

        /**
         * Perform architecture specific transformations prior to an argument transfer to a target compartment
         * @param F
         * @param TransferFunction
         * @param Arg
         */
        void CreateTransferFunctionArg_PreCall(Function *F, Function *TransferFunction, Value *Arg);

        /**
         * Perform architecture specific transformations after the cross compartment function call
         * @param F
         * @param TransformFunction
         * @param Arg
         */
        void CreateTransferFunctionArg_PostCall(Function *F, Function *TransformFunction, Value *Arg);

        Module &getModule();

        virtual Type *HAKCAuthenticationRetType(unsigned AddrSpace);

        ConstantInt *getTrue();

        ConstantInt *getFalse();

        ConstantInt *getInt64(uint64_t Value);

        ConstantInt *getInt32(uint32_t Value);

        ConstantInt *GetDefaultObjectSize();

        /**
        * Returns true if ManagedHAKCPointer is an appropriately sized integer for use as a pointer
        * @param HAKCPointer
        */
        //        virtual bool ValidateHAKCIntegerPointerSize(ManagedHAKCPointerP HAKCPointer);

        virtual unsigned GetPointerAddrSpace(hakc::HAKCPointerBase &HAKCPointer);

        static unsigned GetPointerAddrSpace(Value *V);

        /**
         * Creates metadata associated with a Compartment for proper loading by the kernel
         * @param CompartmentID
         * @return
         */
        virtual GlobalVariable *AddCompartmentMetadataEntry(HAKCCompartment &Compartment);

        virtual Function *PopulateGlobalTransfer(Function *GlobalTransfer, GlobalVariable *GlobalVar, bool Debug);

    protected:
        IRBuilder<> HAKCIRBuilder;
        HAKCCompartmentalizationPolicy &CompartmentalizationPolicy;
        HAKCModuleAnalysis &ModuleAnalysis;

        std::map<Function *, Function *> VariadicTransferFunctions;

        std::string getUniqueAddressable_Name(Function *F);

        std::string getKstrtab_entry_name(Function *F);

        std::string getKstrtabns_entry_name(Function *F);

        /**
         * Checks that ManagedHAKCPointer and I are valid, and sets the HAKCIRBuilder location to I
         * @param HAKCPointer
         * @param I
         */
        void ValidateHAKCPointerAndLocation(const HAKCPointerBase &HAKCPointer, Instruction *I);

        /**
         * Performs the transformations needed for creating a safe pointer
         * @param HAKCPointer
         * @param I
         * @return
         */
        //        virtual Value *CreateSafePointer_Arch(ManagedHAKCPointerP HAKCPointer, Instruction *I) = 0;

        /**
         * Creates a Call to the specified function
         * @param name
         * @param RetTy
         * @param Args
         * @return
         */
        CallInst *CreateCall(StringRef name, Type *RetTy, ArrayRef<Value *> Args);

        CallInst *CreateCall(Function *Callee, ArrayRef<Value *> Args);

        Instruction *CreateCallWithResultCast(StringRef Name, Type *RetTy, ArrayRef<Value *> Args,
                                              Value *ValueToTypeMatch);

        Instruction *CreateCallWithResultCast(Function *Callee, ArrayRef<Value *> Args, Value *ValueToTypeMatch);

        Instruction *CastCallToType(CallInst *Call, Value *ValueToTypeMatch);

        /**
         * Gets or inserts the GlobalVariable containing the list of valid targets from the Compartment F belongs to
         * @param F
         * @return
         */
        GlobalVariable *GetValidTargetCompartments(Function *F);

        /**
         * Return the type that HAKC Compartment Entry Tokens are in the source
         * @return
         */
        Type *GetEntryTokenType(unsigned AddrSpace);

        virtual ConstantInt *GetObjectSizeInBytes(hakc::HAKCPointerBase &HAKCPointer);

        virtual ConstantInt *GetObjectSizeInBytes(hakc::HAKCTypeP HAKCType);

        /**
         * Create the argument set for a HAKC data check
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual void
        CreateDataAuthArguments(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, SmallVectorImpl<Value *> &Result);

        /**
         * Create the argument set for a HAKC code check
         * @param HAKCPointer
         * @param I
         * @return
         */
        virtual void
        CreateCodeAuthArguments(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, SmallVectorImpl<Value *> &Result);

        /**
         * Create the argument set for a HAKC Compartment transfer
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual void
        CreateTransferArguments(hakc::HAKCPointerBase &HAKCPointer, GlobalValue *Target, bool IsData, ConstantInt *Size,
                                SmallVector<Value *> &Result);


        /**
         * Create a normal HAKC Compartment transfer for objects that do not have a custom transfer function
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *CreateDefaultTransfer(hakc::HAKCPointerBase &HAKCPointer,
                                                   GlobalValue *Target,
                                                   bool IsData,
                                                   ConstantInt *Size);

        /**
         *
         * @param HAKCPointer
         * @param Target
         * @param IsData
         * @param Size
         * @return
         */
        virtual Instruction *CreateCustomTransfer(hakc::HAKCPointerBase &HAKCPointer, GlobalValue *Target, bool IsData,
                                                  ConstantInt *Size);

        bool HAKCPointerHasCustomTransfer(hakc::HAKCPointerBase &HAKCPointer);

        Value *CreatePointerCast(hakc::HAKCPointerBase &HAKCPointer, PointerType *PointerTy);

        Value *CreateReturnCast(hakc::HAKCPointerBase &HAKCPointer, Value *V);

        /**
         * Return the custom transfer function if one exists
         * @param HAKCPointer
         * @return
         */
        virtual hakc_custom_transfer_def_t GetCustomTransferFunction(hakc::HAKCPointerBase &HAKCPointer);

        void ValidateLocation(Instruction *I);

        virtual void ValidateHAKCPointer(const HAKCPointerBase &HAKCPointer);

        Function *CreateNonVariadicTransferFunction(Function *F);

        Function *PopulateTransferFunction(Function *Target, Function *TransferFunction);

        Function *GetTransferFunction(Function *F);

        virtual void
        CreateForwardArgumentTransfers(Function *Target, Function *TransferFunction, SmallVector<Value *> &ArgsList);

        void CreateBackwardArgumentTransfers(Function *Target, Function *TransferFunction);

        virtual bool TargetIsKernel(GlobalValue *Target);

        virtual void
        TransferStructMembers(ConstantStruct *ConstStruct, Function *GlobalTransfer, GlobalValue *GlobalVar,
                              bool Debug);

        virtual bool TransferShouldBeCreated(Value *V, GlobalValue *Target);

        bool DebugIsActive();

        virtual HAKCTypeP FindEntryBitcast(hakc::HAKCPointerBase &HAKCPointerP, Instruction *I, Function *Target);

        virtual hakc_custom_transfer_def_t GetCustomTransferFunctionForType(HAKCTypeP HAKCType);

        virtual Instruction *
        CreateVoidCastCompartmentTransfer(hakc::HAKCPointerBase &HAKCPointer, Instruction *I, GlobalValue *Target,
                                          HAKCTypeP TypeToUse);

        virtual bool NoKernelTransfers(Function *Target);

        void InitNewFunction(Function *F, StringRef EntryBlockName);

        HAKCPointerBaseP CreateNewManagedPointer(Value *BaseDefinition);
    };
} // namespace hakc

#endif//HAKC_HAKCTRANSFORMER_H
