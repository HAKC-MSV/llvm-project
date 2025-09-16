//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the information associated with a global variable that is
/// being analyzed for compartmentalization
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCGLOBALINFO_H
#define HAKC_HAKCGLOBALINFO_H

#include "llvm/IR/GlobalVariable.h"
#include "HAKCSymbolInfo.h"

namespace llvm::hakc {

    class HAKCGlobalInfo : public HAKCSymbolInfo {
    public:
        HAKCGlobalInfo(CommonHAKCAnalysis &Analysis, StringRef Name, bool DebugActive);

        void SetGlobalVariable(GlobalVariable *GV);

        GlobalVariable *GetGlobalVariable() const;

        StringRef GetYamlIdentifier() const override;
    };

} // hakc

#endif //HAKC_HAKCGLOBALINFO_H
