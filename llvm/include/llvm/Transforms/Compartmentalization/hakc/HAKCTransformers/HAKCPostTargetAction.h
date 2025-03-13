//
// Created by al32163 on 02/27/25.
//

#ifndef HAKC_HAKCPOSTTARGETACTION_H
#define HAKC_HAKCPOSTTARGETACTION_H

#include "HAKCTransferAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"

namespace llvm::hakc {
class HAKCPostTargetAction : public HAKCTransferAction {
public:
  HAKCPostTargetAction(HAKCFunctionDefinition &HAKCActionFunction,
                       StringRef Label,
                       SmallVector<HAKCActionArgument> Arguments);
};

typedef std::shared_ptr<HAKCPostTargetAction> post_target_action_def_t;
} // namespace llvm::hakc

#endif // HAKC_HAKCPOSTTARGETACTION_H
