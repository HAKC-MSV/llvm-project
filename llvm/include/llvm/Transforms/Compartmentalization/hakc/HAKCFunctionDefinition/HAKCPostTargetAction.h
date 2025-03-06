//
// Created by al32163 on 02/27/25.
//

#ifndef HAKC_HAKCPOSTTARGETACTION_H
#define HAKC_HAKCPOSTTARGETACTION_H

#include "llvm/IR/IRBuilder.h"
#include "HAKCFunctionDefinition.h"

namespace llvm::hakc {
    class HAKCPostTargetAction : public HAKCFunctionDefinition {
    public:
        // TODO: for now, assuming one arg, but expand in the future
        HAKCPostTargetAction(Function *PostTargetAction, unsigned Idx, StringRef Val);

        ~HAKCPostTargetAction() = default;

        unsigned GetIdx() const;

        StringRef GetVal() const;

    protected:
      unsigned Idx;
      StringRef Val;
    };

    typedef std::shared_ptr<HAKCPostTargetAction> hakc_post_target_action_def_t;
}


#endif //HAKC_HAKCPOSTTARGETACTION_H
