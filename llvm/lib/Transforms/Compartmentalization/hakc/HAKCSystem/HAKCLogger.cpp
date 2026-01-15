//
// Created by de29664 on 8/8/25.
//
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/HAKCLogger.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Transforms/Compartmentalization/hakc/HAKCSystem/yaml/HAKCYaml.h"

using namespace llvm;

hakc::HAKCLogger::HAKCLogger(HAKCLogLevel log_level)
    : LogLevel(Verbose), disabled(false) {
  const auto writer = std::make_shared<HAKCWriter>(log_level);
  HAKCStreams.push_back(writer);
}

hakc::HAKCLogger::~HAKCLogger() { HAKCStreams.clear(); }

void hakc::HAKCLogger::Disable() { disabled = true; }

void hakc::HAKCLogger::Enable() { disabled = false; }

bool hakc::HAKCLogger::IsDisabled() const { return disabled; }

void hakc::HAKCLogger::addStream(StringRef log_path, HAKCLogLevel log_level) {
  if (log_path.empty()) {
    disabled = true;
    throw std::exception();
  } else {
    auto log_dir = sys::path::parent_path(log_path).str();
    // create directory if it does not exist
    if (!sys::fs::exists(log_dir)) {
      if (sys::fs::create_directories(log_dir)) {
        errs() << "Failed to create " << sys::path::parent_path(log_path)
               << "\n";
        // should we crash here?
        // throw std::exception();
      }
    }

    auto writer = std::make_shared<HAKCWriter>(log_path, log_level);
    if (*writer) {
      HAKCStreams.push_back(writer);
    }
  }
}

hakc::HAKCLogLevel hakc::HAKCLogger::GetLogLevel() const { return LogLevel; }

void hakc::HAKCLogger::SetLogLevel(const HAKCLogLevel log_level) {
  LogLevel = log_level;
}

void hakc::HAKCLogger::SetConsoleConfiguredLogLevel(
    const HAKCLogLevel log_level) const {
  HAKCStreams[0]->SetConfiguredLogLevel(log_level);
}

void hakc::HAKCLogger::SetFileConfiguredLogLevel(
    const HAKCLogLevel log_level) const {
  for (unsigned long i = 1; i < HAKCStreams.size(); ++i) {
    HAKCStreams[i]->SetConfiguredLogLevel(log_level);
  }
}

iterator_range<SmallVector<std::shared_ptr<hakc::HAKCWriter>>::iterator>
hakc::HAKCLogger::Streams() {
  return make_range(HAKCStreams.begin(), HAKCStreams.end());
}
