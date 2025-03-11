//
// Created by de29664 on 6/23/23.
//

#ifndef HAKC_HAKCFUNCTIONDEFINITION_H
#define HAKC_HAKCFUNCTIONDEFINITION_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace llvm::hakc {
    enum HAKCFunctionArgumentUse {
        Other,
        SignedPtr,
        Comp,
        Div,
        Size,
        IsCode,
        AccessToken,
        ValidTargets,
        ValidTargetSize,
        Void
    };

    struct HAKCFunctionArgumentDefinition {
        HAKCFunctionArgumentDefinition(Type *ArgTy, unsigned Idx, StringRef Label, HAKCFunctionArgumentUse Use);

        Type *ArgTy;
        unsigned Idx;
        std::string Label;
        HAKCFunctionArgumentUse ArgUse;
    };

    class HAKCFunctionDefinition {
    public:
        virtual ~HAKCFunctionDefinition() = default;

        explicit HAKCFunctionDefinition(Function *F, SmallVectorImpl<HAKCFunctionArgumentDefinition> &Args);

        StringRef GetName() const;

        Function *GetFunction() const;

        ConstantInt *GetSignedPtrIdx() const;

        ConstantInt *GetCompartmentIdIdx() const;

        ConstantInt *GetDivisionIdIdx() const;

    protected:
        Function *F;
        SmallVector<HAKCFunctionArgumentDefinition> Args;

        bool GetArgIdxByUse(HAKCFunctionArgumentUse Use, unsigned *Idx) const;

        ConstantInt *GetArgLLVMByUse(HAKCFunctionArgumentUse Use) const;
    };

    typedef std::shared_ptr<HAKCFunctionDefinition> function_def_t;
} // hakc

#endif //HAKC_HAKCFUNCTIONDEFINITION_H
