//===----------------------------------------------------------------------===//
//
// Part of the MIT Lincoln Laboratory HAKC Compartmentalization Project.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains; TODO
///
//===----------------------------------------------------------------------===//
//
// Created by de29664 on 6/18/24.
//

#ifndef HAKC_HAKCINDIRECTCALLSOURCE_H
#define HAKC_HAKCINDIRECTCALLSOURCE_H

#include "HAKCGlobalInfo.h"
#include "HAKCTypeInfo.h"

using namespace llvm;

namespace llvm::hakc {
class HAKCIndirectCallSourceLink : public HAKCInfo {
public:
  HAKCIndirectCallSourceLink(Argument *Arg,
                             const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                             bool Debug);

  HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                             bool Debug);

  HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCSymbolInfo> &HAKCSymbol,
                             bool Debug);

  HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCGlobalInfo> &GlobalInfo,
                             int OffsetInBits, bool Debug);

  HAKCIndirectCallSourceLink(const std::shared_ptr<HAKCTypeInfo> &HAKCType,
                             int OffsetInBits, bool Debug);

  ~HAKCIndirectCallSourceLink() override = default;

  std::string GetYaml(unsigned Indents) const override;

  StringRef GetYamlIdentifier() const override;

protected:
  std::vector<std::string> LinkYamlTokens;

  void SplitTypeYaml(const std::shared_ptr<HAKCTypeInfo> &HAKCType);

  void SplitString(StringRef S, unsigned Indents);

  void InputHAKCSymbol(const std::shared_ptr<HAKCSymbolInfo> &HAKCSymbol);

  void InputLinkType(StringRef LinkType);

  void InputType(const std::shared_ptr<HAKCTypeInfo> &HAKCType);

  void InputGlobalObject(GlobalObject *GlobalObj);

  void InputYamlHeader();

  void InputBitoffset(unsigned BitOffset);

  void InputArgument(Argument *Arg);
};

class HAKCIndirectCallSource : public HAKCInfo {
public:
  HAKCIndirectCallSource(
      const std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> &SourcePath,
      const std::shared_ptr<HAKCTypeInfo> &HAKCType, bool debug);

  ~HAKCIndirectCallSource() override = default;

  std::string GetYaml(unsigned Indents) const override;

  StringRef GetYamlIdentifier() const override;

protected:
  std::shared_ptr<HAKCTypeInfo> HAKCType;
  std::vector<std::shared_ptr<HAKCIndirectCallSourceLink>> SourcePath;
};
} // namespace llvm::hakc

#endif // HAKC_HAKCINDIRECTCALLSOURCE_H
