//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERACTION_H
#define HAKCTRANSFERACTION_H

#include "llvm/IR/Function.h"

namespace llvm {
namespace hakc {

class HAKCTransferAction {
    public:
      HAKCTransferAction(Function *F);

      Function *GetActionFunction() const;

    protected:
      Function *ActionFunction;
};

} // hakc
} // llvm

#endif //HAKCTRANSFERACTION_H
