//
// Created by de29664 on 6/23/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCTransferFunction.h"

#include "llvm/IR/Constants.h"

using namespace llvm;

namespace llvm::hakc {
    HAKCTransferFunction::HAKCTransferFunction(Function *F, unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                               unsigned int DivisionIdx,
                                               unsigned int SizeIdx) : HAKCFunctionDefinition(F), SignedPtrIdx(nullptr),
                                                                       CompartmentIdIdx(nullptr),
                                                                       DivisionIdIdx(nullptr),
                                                                       SizeIdx(nullptr), IsCodeIdx(nullptr) {
        CreateIndexes(SignedPtrIdx, CompartmentIdIdx, DivisionIdx, SizeIdx, hakc::HAKCTransferFunction::MissingIdx);
    }

    HAKCTransferFunction::HAKCTransferFunction(Function *F, unsigned int SignedPtrIdx, unsigned int CompartmentIdIdx,
                                               unsigned int DivisionIdx) : HAKCFunctionDefinition(F),
                                                                           SignedPtrIdx(nullptr),
                                                                           CompartmentIdIdx(nullptr),
                                                                           DivisionIdIdx(nullptr), SizeIdx(nullptr),
                                                                           IsCodeIdx(nullptr) {
        CreateIndexes(SignedPtrIdx, CompartmentIdIdx, DivisionIdx, MissingIdx, hakc::HAKCTransferFunction::MissingIdx);
    }

    HAKCTransferFunction::HAKCTransferFunction(Function *F, unsigned SignedPtrIdx, unsigned CompartmentIdIdx,
                                               unsigned DivisionIdx, unsigned SizeIdx,
                                               unsigned IsCodePtrIdx) : HAKCFunctionDefinition(F),
                                                                        SignedPtrIdx(nullptr),
                                                                        CompartmentIdIdx(nullptr),
                                                                        DivisionIdIdx(nullptr), SizeIdx(nullptr),
                                                                        IsCodeIdx(nullptr) {
        CreateIndexes(SignedPtrIdx, CompartmentIdIdx, DivisionIdx, SizeIdx, IsCodePtrIdx);
    }

    void HAKCTransferFunction::CreateIndexes(unsigned int PtrIdx, unsigned int CompartmentIdx,
                                             unsigned int DivisionIdx, unsigned int SzIdx, unsigned IsCodePtrIdx) {
        unsigned IndexBitSize = 64;
        if (PtrIdx != MissingIdx) {
            this->SignedPtrIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), PtrIdx);
        }
        if (CompartmentIdx != MissingIdx) {
            this->CompartmentIdIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), CompartmentIdx);
        }
        if (DivisionIdx != MissingIdx) {
            this->DivisionIdIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), DivisionIdx);
        }
        if (SzIdx != MissingIdx) {
            this->SizeIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), SzIdx);
        }
        if (IsCodePtrIdx != MissingIdx) {
            this->IsCodeIdx = ConstantInt::get(IntegerType::get(F->getContext(), IndexBitSize), IsCodePtrIdx);
        }
    }

    ConstantInt *HAKCTransferFunction::GetSignedPtrIdx() const {
        return SignedPtrIdx;
    }

    ConstantInt *HAKCTransferFunction::GetCompartmentIdIdx() const {
        return CompartmentIdIdx;
    }

    ConstantInt *HAKCTransferFunction::GetDivisionIdIdx() const {
        return DivisionIdIdx;
    }

    ConstantInt *HAKCTransferFunction::GetSizeIdx() const {
        return SizeIdx;
    }

    ConstantInt *HAKCTransferFunction::GetIsCodePtrIdx() const {
        return IsCodeIdx;
    }
} // hakc
