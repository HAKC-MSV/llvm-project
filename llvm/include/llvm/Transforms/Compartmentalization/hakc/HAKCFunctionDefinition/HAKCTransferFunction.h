//
// Created by de29664 on 6/23/23.
//

#ifndef HAKC_HAKCTRANSFERFUNCTION_H
#define HAKC_HAKCTRANSFERFUNCTION_H

#include "llvm/IR/Instructions.h"
#include "HAKCFunctionDefinition.h"

namespace llvm::hakc {
    class HAKCTransferFunction : public HAKCFunctionDefinition {
    public:
        HAKCTransferFunction(Function *F, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, unsigned DivisionIdx,
                             unsigned SizeIdx);

        HAKCTransferFunction(Function *F, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, unsigned DivisionIdx);

        HAKCTransferFunction(Function *F, unsigned SignedPtrIdx, unsigned CompartmentIdIdx, unsigned DivisionIdx,
                             unsigned SizeIdx, unsigned IsCodePtrIdx);

        ConstantInt *GetSignedPtrIdx() const;

        ConstantInt *GetCompartmentIdIdx() const;

        ConstantInt *GetDivisionIdIdx() const;

        ConstantInt *GetSizeIdx() const;

        ConstantInt *GetIsCodePtrIdx() const;

        static constexpr unsigned MissingIdx = -1;

        // NB: This should be number of possible arguments - 1
        static constexpr unsigned MaxArgIndex = 4;

    protected:
        ConstantInt *SignedPtrIdx;
        ConstantInt *CompartmentIdIdx;
        ConstantInt *DivisionIdIdx;
        ConstantInt *SizeIdx;
        ConstantInt *IsCodeIdx;

        void CreateIndexes(unsigned SignedPtrIdx, unsigned CompartmentIdIdx, unsigned DivisionIdx,
                           unsigned SizeIdx, unsigned IsCodePtrIdx);
    };

    typedef std::shared_ptr<HAKCTransferFunction> hakc_transfer_def_t;
} // hakc

#endif //HAKC_HAKCTRANSFERFUNCTION_H
