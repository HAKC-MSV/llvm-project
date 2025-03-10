//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERACTION_H
#define HAKCTRANSFERACTION_H

#include "llvm/IR/Function.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <map>

namespace llvm {
namespace hakc {

class HAKCTransferAction {
  // pretransfer: step0 = check_color
  // posttarget: color_address(step0)

  // transfer action should be executed, which creates a transfer state
  // the action itself consists of an argument number, an argument label, and a
  // function
public:
  HAKCTransferAction(Function *F,
                     std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel);

  Function *GetActionFunction() const;

  hakc_label_ref_t GetActionLabel(hakc_arg_t arg) const;

  const std::map<llvm::hakc::hakc_arg_t, llvm::hakc::hakc_label_ref_t> &
  GetArgToLabel();

protected:
  // Should be instantiated from yaml data and never changed; store Value *
  // in state
  Function *ActionFunction;
  const std::map<hakc_arg_t, hakc_label_ref_t> ArgToLabel;
};

} // namespace hakc
} // namespace llvm

#endif // HAKCTRANSFERACTION_H
