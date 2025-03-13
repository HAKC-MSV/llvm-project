//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERSTATE_H
#define HAKCTRANSFERSTATE_H

#include "llvm/Transforms/Compartmentalization/hakc/HAKCCompartmentalizationPolicy/HAKCCompartmentalizationPolicy.h"

#include "llvm/IR/Value.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPostTargetAction.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCTransformers/HAKCPreTransferAction.h"

using namespace llvm;

namespace llvm::hakc {
    class HAKCTransferState {
    public:
        HAKCTransferState(const HAKCCompartmentDivision &TargetDivision, HAKCPointerBaseP HAKCPointer);

        void AddTransferActionValue(const HAKCTransferAction &Action, Value *V);

        Value *GetTransferActionValue(const HAKCTransferAction &Action);

        Value *GetLabeledValue(StringRef Label) const;

        const HAKCCompartmentDivision &GetDivision() const;

        HAKCPointerBaseP GetManagedPointer();

        HAKC_Access_Token GetAccessToken();

    protected:
        std::map<HAKCTransferAction, Value *> ActionValues;
        const HAKCCompartmentDivision TargetDivision;
        HAKCPointerBaseP HAKCPointer;
        HAKC_Access_Token AccessToken;
    };
} // namespace llvm::hakc

#endif //HAKCTRANSFERSTATE_H
