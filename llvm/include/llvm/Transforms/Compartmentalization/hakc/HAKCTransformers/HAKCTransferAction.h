//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERACTION_H
#define HAKCTRANSFERACTION_H

#include "llvm/IR/Function.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <map>
#include <llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h>

namespace llvm {
  namespace hakc {
    class HAKCTransferAction {
      // pretransfer: step0 = check_color
      // posttarget: color_address(step0)

      // transfer action should be executed, which creates a transfer state
      // the action itself consists of an argument number, an argument label, and a
      // function
    public:
      HAKCTransferAction(HAKCFunctionDefinition &HAKCActionFunction, StringRef Label);

      StringRef GetLabel() const;

      HAKCFunctionDefinition &GetHAKCActionFunction() const;

    protected:
      std::string Label;
      HAKCFunctionDefinition &HAKCActionFunction;
    };
  } // namespace hakc
} // namespace llvm

#endif // HAKCTRANSFERACTION_H
