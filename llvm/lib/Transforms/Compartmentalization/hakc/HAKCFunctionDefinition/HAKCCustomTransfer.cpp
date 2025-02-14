//
// Created by de29664 on 6/21/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCCustomTransfer.h"

#include <utility>
#include "llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h"

hakc::HAKCCustomTransfer::HAKCCustomTransfer(Function *CustomFunction, hakc::HAKCTypeP TargetType,
                                             unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                             int DivisionIdx) : HAKCTransferFunction(CustomFunction, SignedPtrIdx,
                                                                    CompartmentIdIdx, DivisionIdx),
                                                                TargetType(std::move(TargetType)) {
}

hakc::HAKCCustomTransfer::HAKCCustomTransfer(Function *CustomFunction, hakc::HAKCTypeP TargetType,
                                             unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                             unsigned int DivisionIdx, unsigned int SizeIdx) : HAKCTransferFunction(
        CustomFunction, SignedPtrIdx, CompartmentIdIdx, DivisionIdx, SizeIdx), TargetType(std::move(TargetType)) {
}

hakc::HAKCTypeP hakc::HAKCCustomTransfer::GetTargetType() const {
    return TargetType;
}

Instruction *hakc::HAKCCustomTransfer::CreateTransfer(IRBuilder<> &HAKCIRBuilder,
                                                      HAKCCompartmentDivision &CompartmentDivision, Value *Pointer,
                                                      Value *Size, bool IsData) {
    SmallVector<Value *, hakc::HAKCTransferFunction::MaxArgIndex> Args;

    if (!SignedPtrIdx) {
        CommonHAKCAnalysis::getWriter(true) << "No signed pointer index for " << GetFunction()->getName() << "\n";
        throw std::exception();
    }
    Args[SignedPtrIdx->getZExtValue()] = Pointer;

    if (!DivisionIdIdx) {
        CommonHAKCAnalysis::getWriter(true) << "No Division index for " << GetFunction()->getName() << "\n";
        throw std::exception();
    }
    Args[DivisionIdIdx->getZExtValue()] = CompartmentDivision.GetDivisionID();

    if (!CompartmentIdIdx) {
        CommonHAKCAnalysis::getWriter(true) << "No Compartment index for " << GetFunction()->getName() << "\n";
        throw std::exception();
    }
    Args[CompartmentIdIdx->getZExtValue()] = CompartmentDivision.GetHAKCCompartment().GetCompartmentID();

    if (Size) {
        if (!SizeIdx) {
            CommonHAKCAnalysis::getWriter(true) << "No size index for " << GetFunction()->getName() << "\n";
            throw std::exception();
        }
        Args[SizeIdx->getZExtValue()] = Size;
    }

    return HAKCIRBuilder.CreateCall(GetFunction(), Args);
}

Instruction *hakc::HAKCCustomTransfer::CreateTransfer(IRBuilder<> &HAKCIRBuilder,
                                                      HAKCCompartmentDivision &CompartmentDivision,
                                                      hakc::HAKCPointerBase &HAKCPointer, Value *Size, bool IsData) {
    return CreateTransfer(HAKCIRBuilder, CompartmentDivision, HAKCPointer.GetBaseDefinition(), Size, IsData);
}

Instruction *hakc::HAKCCustomTransfer::CreateTransferWithCasts(IRBuilder<> &HAKCIRBuilder,
                                                               HAKCCompartmentDivision &CompartmentDivision,
                                                               hakc::HAKCPointerBase &HAKCPointer, Value *Size,
                                                               HAKCTypeP srcTy,
                                                               HAKCTypeP dstTy, bool IsData) {
    auto *BitcastArgForTransferCall = HAKCIRBuilder.
            CreateBitCast(HAKCPointer.GetBaseDefinition(), dstTy->GetLLVMType());
    auto *TransferCall = CreateTransfer(HAKCIRBuilder, CompartmentDivision, BitcastArgForTransferCall, Size, IsData);
    auto *BitcastArgForTargetCall = HAKCIRBuilder.CreateBitCast(TransferCall, srcTy->GetLLVMType());
    auto *Result = dyn_cast<Instruction>(BitcastArgForTargetCall);
    return Result;
}
