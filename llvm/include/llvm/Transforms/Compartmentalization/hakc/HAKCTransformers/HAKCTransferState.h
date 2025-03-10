//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERSTATE_H
#define HAKCTRANSFERSTATE_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCTransferState {
    public:
        HAKCTransferState();

        void AddTransferredArgument(Value *Arg);

        SmallVectorImpl<Value *> &GetTransferredArguments();

        void AddPreTransferAction(hakc_pre_transfer_action_def_t Action);

        void AddPostTargetAction(hakc_post_target_action_def_t Action);

        void AddPreTransferValue(hakc_label_ref_t Label, Value *Value);

        void AddPostTargetValue(hakc_label_ref_t Label, Value *Value);

        void AddActionArgumentValue(hakc_label_ref_t Label, Value *Value);

        hakc_label_ref_t AddActionArgumentLabel(hakc_label_t Label);

        Value *GetActionArgumentValue(hakc_label_ref_t Label);

        iterator_range<SmallVector<hakc_pre_transfer_action_def_t>::iterator>
        GetPreTransferActions();

        iterator_range<SmallVector<hakc_post_target_action_def_t>::iterator>
        GetPostTargetActions();

        SmallVector<llvm::hakc::hakc_label_t> GetActionArgumentLabels();

      protected:
        SmallVector<Value *> TransferredArgumentValues;
        // this will store the actual std::string values for all transfers; all
        // other labels will be StringRefs
        SmallVector<hakc_label_t> ActionArgumentLabels;
        // this will store a global copy of the labels to value mapping
        // TODO: do we want global label namespace?
        std::map<hakc_label_ref_t, Value *> ActionArgumentLabelToValue;
        // replacing map with vector since one action can map to multiple
        // values, e.g., save both input and output values as labels
        SmallVector<hakc_pre_transfer_action_def_t> PreTransferActionList;
        SmallVector<hakc_post_target_action_def_t> PostTargetActionList;
    };

} // namespace llvm::hakc

#endif //HAKCTRANSFERSTATE_H
