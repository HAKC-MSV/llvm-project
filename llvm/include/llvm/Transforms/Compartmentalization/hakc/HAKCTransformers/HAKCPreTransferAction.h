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
      HAKCPreTransferAction(Function *PreTransferAction,
                            std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel);

      ~HAKCPreTransferAction() = default;

      void AddValue(hakc_label_ref_t Label, Value *val);

    protected:
      std::map<hakc_label_ref_t, Value *> LabelToValue;
    };

    typedef std::shared_ptr<HAKCPreTransferAction> hakc_pre_transfer_action_def_t;
}


#endif //HAKC_HAKCPRETRANSFERACTION_H
