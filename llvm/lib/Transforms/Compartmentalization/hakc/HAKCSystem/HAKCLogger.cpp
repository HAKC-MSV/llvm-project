//
// Created by de29664 on 8/8/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCLogger.h"

#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

using namespace llvm;

llvm::hakc::HAKCLogger::HAKCLogger(hakc::HAKCLogLevel log_level)
  : HAKCStreams(), LogLevel(Verbose), disabled(log_level == Disabled) {
  auto writer = std::make_shared<HAKCWriter>(log_level);
  HAKCStreams.push_back(writer);
}

hakc::HAKCLogger::~HAKCLogger() { HAKCStreams.clear(); }

void hakc::HAKCLogger::addStream(StringRef log_path,
                                 hakc::HAKCLogLevel log_level) {
  if (log_level == Disabled || log_path.empty()) { disabled = true; } else {
    auto log_dir = sys::path::parent_path(log_path).str();
    // create directory if it does not exist
    if (!sys::fs::exists(log_dir)) {
      if (sys::fs::create_directories(log_dir)) {
        errs() << "Failed to create " << sys::path::parent_path(log_path) <<
            "\n";
        // should we crash here?
        // throw std::exception();
      }
    }

    auto writer = std::make_shared<HAKCWriter>(log_path, log_level);
    if (*writer) { HAKCStreams.push_back(writer); }
  }
}

hakc::HAKCLogLevel hakc::HAKCLogger::GetLogLevel() const { return LogLevel; }

void hakc::HAKCLogger::SetLogLevel(hakc::HAKCLogLevel log_level) {
  LogLevel = log_level;
  disabled = (log_level == Disabled);
}

void hakc::HAKCLogger::SetConsoleConfiguredLogLevels(
    hakc::HAKCLogLevel log_level) const {
  HAKCStreams[0]->SetConfiguredLogLevel(log_level);
}

void hakc::HAKCLogger::SetFileConfiguredLogLevel(
    hakc::HAKCLogLevel log_level) const {
  for (unsigned long i = 1; i < HAKCStreams.size(); ++i) {
    HAKCStreams[i]->SetConfiguredLogLevel(log_level);
  }
}

bool hakc::HAKCLogger::IsDisabled() const { return disabled; }

iterator_range<SmallVector<std::shared_ptr<hakc::HAKCWriter> >::iterator>
hakc::HAKCLogger::Streams() {
  return make_range(HAKCStreams.begin(), HAKCStreams.end());
}
