//
// Created by al32163 on 02/27/25.
//

#ifndef HAKC_HAKCPOSTTARGETACTION_H
#define HAKC_HAKCPOSTTARGETACTION_H
#include "HAKCTransferAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <map>

namespace llvm::hakc {
    class HAKCPostTargetAction : public HAKCTransferAction {
    public:
      HAKCPostTargetAction(Function *PostTargetAction,
                           std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel);

      ~HAKCPostTargetAction() = default;

      void AddValue(hakc_label_ref_t Label, Value* val);

    protected:
      // get the label from the arg value (superclass), then query state object
      // TODO: might not need this, but could be helpful. might also rename
      std::map<hakc_label_ref_t, Value *> LabelToValue;
    };
    typedef std::shared_ptr<HAKCPostTargetAction> hakc_post_target_action_def_t;
}


#endif //HAKC_HAKCPOSTTARGETACTION_H
