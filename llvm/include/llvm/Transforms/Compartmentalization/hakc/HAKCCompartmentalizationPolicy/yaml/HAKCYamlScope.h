//
// Created by de29664 on 7/29/24.
//

#ifndef HAKC_HAKCYAMLSCOPE_H
#define HAKC_HAKCYAMLSCOPE_H

#include <string>
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"

namespace llvm::hakc {
    class HAKCYamlScope {
    public:
        HAKCYamlScope(hakc_scope_t Scope, std::string LocalScope);

        HAKCYamlScope() = default;

        std::string LocalScope;
        hakc_scope_t Scope;
    };
} // hakc

#endif //HAKC_HAKCYAMLSCOPE_H
