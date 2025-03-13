//
// Created by de29664 on 3/6/25.
//

#ifndef HAKCTRANSFERACTION_H
#define HAKCTRANSFERACTION_H

#include "llvm/IR/Function.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
#include <llvm/Transforms/Compartmentalization/hakc/HAKCFunctionDefinition/HAKCFunctionDefinition.h>

namespace llvm {
namespace hakc {
class HAKCActionArgument {
public:
  HAKCActionArgument(unsigned Idx, const std::string &Label)
      : Idx(Idx), Label(Label) {};

  StringRef GetLabel() const { return Label; }
  unsigned GetIdx() const { return Idx; }

protected:
  unsigned Idx;
  std::string Label;
};

class HAKCTransferAction {
public:
  HAKCTransferAction(HAKCFunctionDefinition &HAKCActionFunction,
                     StringRef Label,
                     SmallVector<HAKCActionArgument> Arguments);

  HAKCTransferAction(HAKCFunctionDefinition &HAKCActionFunction,
                     StringRef Label);

  virtual ~HAKCTransferAction() = default;

  StringRef GetLabel() const;

  iterator_range<SmallVector<HAKCActionArgument>::iterator> GetArguments();

  HAKCFunctionDefinition &GetHAKCActionFunction() const;

  bool operator<(const HAKCTransferAction &Rhs) const;

protected:
  std::string Label;
  HAKCFunctionDefinition &HAKCActionFunction;
  SmallVector<HAKCActionArgument> Arguments;
};

typedef std::shared_ptr<HAKCTransferAction> transfer_action_def_t;
} // namespace hakc
} // namespace llvm

#endif // HAKCTRANSFERACTION_H
