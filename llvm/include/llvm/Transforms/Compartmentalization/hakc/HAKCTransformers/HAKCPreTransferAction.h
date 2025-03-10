//
// Created by al32163 on 02/27/25.
//

#ifndef HAKC_HAKCPRETRANSFERACTION_H
#define HAKC_HAKCPRETRANSFERACTION_H
#include "HAKCTransferAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <map>

namespace llvm::hakc {
    class HAKCPreTransferAction : public HAKCTransferAction {
    public:
        HAKCPreTransferAction(HAKCFunctionDefinition &HAKCActionFunction, StringRef Label);
    };

    typedef std::shared_ptr<HAKCPreTransferAction> pre_transfer_action_def_t;
}


#endif //HAKC_HAKCPRETRANSFERACTION_H
