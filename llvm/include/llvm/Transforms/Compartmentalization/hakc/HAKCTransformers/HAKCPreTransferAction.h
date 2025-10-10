//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the pre transfer action class, e.g., what needs to be saved
/// before a transfer is called, e.g., what was the previous compartment.
///
//===----------------------------------------------------------------------===//
//
// Created by al32163 on 02/27/25.
//

#ifndef HAKC_HAKCPRETRANSFERACTION_H
#define HAKC_HAKCPRETRANSFERACTION_H
#include "HAKCTransferAction.h"

namespace llvm::hakc {
class HAKCPreTransferAction : public HAKCTransferAction {
public:
  HAKCPreTransferAction(HAKCFunctionDefinition &HAKCActionFunction,
                        StringRef Label);
};

typedef std::shared_ptr<HAKCPreTransferAction> pre_transfer_action_def_t;
} // namespace llvm::hakc

#endif // HAKC_HAKCPRETRANSFERACTION_H
