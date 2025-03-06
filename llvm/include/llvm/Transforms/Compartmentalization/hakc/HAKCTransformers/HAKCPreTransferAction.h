//
// Created by al32163 on 02/27/25.
//

#ifndef HAKC_HAKCPRETRANSFERACTION_H
#define HAKC_HAKCPRETRANSFERACTION_H
#include "HAKCTransferAction.h"

namespace llvm::hakc {
    class HAKCPreTransferAction : public HAKCTransferAction {
    public:
        HAKCPreTransferAction(Function *PreTransferAction, StringRef Label);

        ~HAKCPreTransferAction() = default;

        StringRef GetLabel() const;

    protected:
        std::string Label;
    };

    typedef std::shared_ptr<HAKCPreTransferAction> hakc_pre_transfer_action_def_t;
}


#endif //HAKC_HAKCPRETRANSFERACTION_H
