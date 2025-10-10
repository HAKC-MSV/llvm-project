//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains Temporal Compartmentalization code related to
/// 'compartments', also known as Epochs
///
//===----------------------------------------------------------------------===//
//#ifndef HAKC_TICTACEPOCH_H
//#define HAKC_TICTACEPOCH_H
//
//#include "HAKCPass.h"
//#include "HAKCSymbol.h"
//#include "HAKCTypeIdentifier/HAKCTypeInfo.h"
//#include "HAKCTypeIdentifier/HAKCFunctionInfo.h"
//#include "HAKCTypeIdentifier/HAKCSymbolInfo.h"
//
//namespace hakc {
//
//class TICTACEpoch {
//protected:
//  std::string typeString;
//  tictac_epoch_id_t epochID;
//  tictac_epoch_id_t nextEpochID;
//  Type* type;
//
//public:
//  TICTACEpoch(std::string typeString, tictac_epoch_id_t epochID, tictac_epoch_id_t nextEpoch);
//  std::string getTypeString();
//  void assignType(Type *ty);
//  tictac_epoch_id_t GetEpochID();
//  tictac_epoch_id_t GetNextEpochID();
//  Value *GetNextEpochIDAsValue(LLVMContext &context);
//  Type* getType();
//  friend raw_ostream &operator<<(raw_ostream &os, TICTACEpoch &Epoch);
//};
//
//} // hakc
//
//
//#endif //HAKC_TICTACEPOCH_H