//
// Created by de29664 on 6/23/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"

#include <llvm/Transforms/Compartmentalization/hakc/HAKCAnalysis/CommonHAKCAnalysis.h>

namespace llvm::hakc {
    HAKCFunctionArgumentDefinition::HAKCFunctionArgumentDefinition(Type *ArgTy, unsigned Idx,
                                                                   HAKCFunctionArgumentUse Use) : ArgTy(ArgTy),
        Idx(Idx), ArgUse(Use) {
        if (!ArgTy) {
            CommonHAKCAnalysis::getWriter(true) << "ArgTy is null\n";
            throw std::exception();
        }
    }

    HAKCFunctionDefinition::HAKCFunctionDefinition(Function *F,
                                                   SmallVectorImpl<HAKCFunctionArgumentDefinition> &
                                                   Args) : F(F), ArgList(Args.begin(), Args.end()) {
        if (!F) {
            CommonHAKCAnalysis::getWriter(true) << "F is null\n";
            throw std::exception();
        }
    }

    StringRef HAKCFunctionDefinition::GetName() const {
        return F->getName();
    }

    Function *HAKCFunctionDefinition::GetFunction() const {
        return F;
    }

    ConstantInt *HAKCFunctionDefinition::GetSignedPtrIdx() const {
        return GetArgLLVMByUse(SignedPtr);
    }

    ConstantInt *HAKCFunctionDefinition::GetCompartmentIdIdx() const {
        return GetArgLLVMByUse(Comp);
    }

    ConstantInt *HAKCFunctionDefinition::GetDivisionIdIdx() const {
        return GetArgLLVMByUse(Div);
    }

    ConstantInt *HAKCFunctionDefinition::GetArgLLVMByUse(HAKCFunctionArgumentUse Use) const {
        unsigned Idx;
        if (GetArgIdxByUse(Use, &Idx)) {
            return ConstantInt::get(IntegerType::get(F->getContext(), 32), Idx);
        }
        return nullptr;
    }

    bool HAKCFunctionDefinition::GetArgIdxByUse(HAKCFunctionArgumentUse Use, unsigned *Idx) const {
        for (auto &Arg: ArgList) {
            if (Arg.ArgUse == Use) {
                *Idx = Arg.Idx;
                return true;
            }
        }
        return false;
    }

    iterator_range<SmallVector<HAKCFunctionArgumentDefinition>::iterator> HAKCFunctionDefinition::Args() {
        return make_range(ArgList.begin(), ArgList.end());
    }

    const std::map<HAKCFunctionArgumentUse, const char *> HAKCArgumentArgumentUseStringMap() {
        return {
            {hakc::Size, "size"},
            {hakc::SignedPtr, "signed-ptr"},
            {hakc::Comp, "compartment"},
            {hakc::Div, "division"},
            {hakc::IsCode, "is-code"},
            {hakc::AccessToken, "access-token"},
            {hakc::ValidTargets, "valid-targets"},
            {hakc::ValidTargetSize, "valid-target-size"},
            {hakc::Other, "other"}
        };
    }
} // namespace llvm::hakc
