#ifndef HAKC_HAKCEPOCH_H
#define HAKC_HAKCEPOCH_H

//#include "HAKCSymbol.h"
//#include "HAKCTypeIdentifier/HAKCTypeInfo.h"
//#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
//#include "HAKCTypeIdentifier/HAKCSymbolInfo.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKC-defs.h"
//#include "llvm/Transforms/Compartmentalization/hakc/HAKCTypeIdentifier/HAKCInfo.h"
//#include <stdio.h>

namespace llvm::hakc {

class TICTACEpoch {
protected:
  tictac_epoch_id_t epochID;
  tictac_epoch_id_t nextEpochID;
  Type* type;

public:
  TICTACEpoch(tictac_epoch_id_t epochID, tictac_epoch_id_t nextEpoch, Type* type);
  tictac_epoch_id_t GetEpochID();
  tictac_epoch_id_t GetNextEpochID();
  Value *GetNextEpochIDAsValue(LLVMContext &context);
  Type* getType();
  friend raw_ostream &operator<<(raw_ostream &os, TICTACEpoch &Epoch);
};

} // hakc


#endif //HAKC_HAKCEPOCH_H