//
// Created by de29664 on 6/23/23.
//

#ifndef HAKC_HAKCFUNCTIONDEFINITION_H
#define HAKC_HAKCFUNCTIONDEFINITION_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include <map>

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
        ValidTargetSize
    };

    const std::map<HAKCFunctionArgumentUse, const char *> HAKCArgumentArgumentUseStringMap();

    struct HAKCFunctionArgumentDefinition {
        HAKCFunctionArgumentDefinition(Type *ArgTy, unsigned Idx, HAKCFunctionArgumentUse Use);

        Type *ArgTy;
        unsigned Idx;
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

        iterator_range<SmallVector<HAKCFunctionArgumentDefinition>::iterator> Args();

    protected:
        Function *F;
        SmallVector<HAKCFunctionArgumentDefinition> ArgList;

        bool GetArgIdxByUse(HAKCFunctionArgumentUse Use, unsigned *Idx) const;

        ConstantInt *GetArgLLVMByUse(HAKCFunctionArgumentUse Use) const;
    };

    typedef std::shared_ptr<HAKCFunctionDefinition> function_def_t;
    typedef HAKCFunctionDefinition arg_def_t;
} // hakc

#endif //HAKC_HAKCFUNCTIONDEFINITION_H
