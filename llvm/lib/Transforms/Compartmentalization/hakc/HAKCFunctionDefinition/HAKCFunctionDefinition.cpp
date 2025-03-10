//
// Created by de29664 on 6/23/23.
//

#include "llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h"
// should call parses type, then do get or insertm odule or get function
// pre ref to ir builder, and value, and use that ir builder to create the call to the pre transfer action function, and supply the pointer arg
namespace llvm::hakc {
    HAKCFunctionDefinition::HAKCFunctionDefinition(Function *F) : F(F) {
    }

    StringRef HAKCFunctionDefinition::GetName() const {
        return F->getName();
    }

    Function *HAKCFunctionDefinition::GetFunction() const {
        return F;
    }
} // hakc
