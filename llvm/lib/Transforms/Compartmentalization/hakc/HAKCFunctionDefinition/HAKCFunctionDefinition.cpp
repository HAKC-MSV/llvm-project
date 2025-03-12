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
                                                   Args) : F(F), Args(Args.begin(), Args.end()) {
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
        for (auto &Arg: Args) {
            if (Arg.ArgUse == Use) {
                *Idx = Arg.Idx;
                return true;
            }
        }
        return false;
    }

    HAKCFunctionArgumentUse HAKCFunctionDefinition::GetArgUseByIdx(unsigned Idx) {
      for (auto Arg: Args) {
        if (Arg.Idx == Idx) {
          return Arg.ArgUse;
        }
      }
      CommonHAKCAnalysis::getWriter(true) << "Error, failed to find TransferredArgument Use by Idx!\n";
      std::exception();
    }
} // namespace llvm::hakc
