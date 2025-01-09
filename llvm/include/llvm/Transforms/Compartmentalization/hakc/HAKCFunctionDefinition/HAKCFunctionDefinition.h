//
// Created by de29664 on 6/23/23.
//

#ifndef HAKC_HAKCFUNCTIONDEFINITION_H
#define HAKC_HAKCFUNCTIONDEFINITION_H

#include "llvm/IR/Function.h"
#include <memory>

using namespace llvm;

namespace llvm::hakc {
    class HAKCFunctionDefinition {
    public:
        virtual ~HAKCFunctionDefinition() = default;

        explicit HAKCFunctionDefinition(Function *F);

        StringRef GetName() const;

        Function *GetFunction() const;

    protected:
        Function *F;
    };

    typedef std::shared_ptr<HAKCFunctionDefinition> hakc_function_def_t;
} // hakc

#endif //HAKC_HAKCFUNCTIONDEFINITION_H
