//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the helper class that contains useful information for
/// outputting symbols to yaml (double check)
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 5/2/23.
//

#ifndef HAKC_HAKCINFO_H
#define HAKC_HAKCINFO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm::hakc {
class CommonHAKCAnalysis;

class HAKCInfo {
public:
  virtual ~HAKCInfo() = default;

  virtual std::string GetYaml(unsigned Indents) const = 0;

  virtual StringRef GetYamlIdentifier() const = 0;

  virtual std::string GetYamlHeader(unsigned Indents) const;

  virtual StringRef GetName() const __attribute__((noinline));

  raw_ostream &operator>>(raw_ostream &os) const;

  friend raw_ostream &operator<<(raw_ostream &os, const HAKCInfo &HAKCInfo);

  static unsigned int IndentSpaces();

  static unsigned int EntrySpaces();

  CommonHAKCAnalysis &GetCommonHAKCAnalysis() const;

protected:
  CommonHAKCAnalysis &Analysis;
  bool DebugActive;
  std::string Name;

  explicit HAKCInfo(CommonHAKCAnalysis &Analysis, StringRef Name,
                    bool DebugActive);
};
} // namespace llvm::hakc

#endif // HAKC_HAKCINFO_H
