//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the post target action class, e.g., what needs to be set
/// after a transfer function is called.
///
//===----------------------------------------------------------------------===//
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
                       const SmallVector<HAKCActionArgument> &Arguments);
};

typedef std::shared_ptr<HAKCPostTargetAction> post_target_action_def_t;
} // namespace llvm::hakc

#endif // HAKC_HAKCPOSTTARGETACTION_H
