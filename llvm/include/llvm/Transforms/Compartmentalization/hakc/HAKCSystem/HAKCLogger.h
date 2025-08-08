//
// Created by de29664 on 8/8/25.
//

#ifndef HAKC_HAKCLOGGER_H
#define HAKC_HAKCLOGGER_H
#include "HAKCWriter.h"
#include "yaml/HAKCYaml.h"

namespace llvm::hakc {

class HAKCLogger {
protected:
  SmallVector<std::shared_ptr<HAKCWriter>> HAKCStreams;
  HAKCLogLevel LogLevel;
  bool disabled;

public:
  explicit HAKCLogger(HAKCLogLevel log_level);
  ~HAKCLogger();
  void addStream(StringRef log_path, HAKCLogLevel log_level);
  HAKCLogLevel GetLogLevel() const;
  void SetLogLevel(HAKCLogLevel log_level);
  void SetConsoleConfiguredLogLevels(HAKCLogLevel log_level) const;
  void SetFileConfiguredLogLevel(HAKCLogLevel log_level) const;
  bool IsDisabled() const;
  iterator_range<SmallVector<std::shared_ptr<HAKCWriter>>::iterator> Streams();
};

template <typename T, std::enable_if_t<!std::is_integral_v<T>> * = nullptr>
HAKCLogger &operator<<(HAKCLogger &Logger, T &T_) {
  for (std::shared_ptr<HAKCWriter> &stream : Logger.Streams()) {
    if (!Logger.IsDisabled() &&
        (Logger.GetLogLevel() >= stream->GetConfiguredLogLevel())) {
      *stream << T_;
    }
  }
  return Logger;
}

template <typename T> HAKCLogger &operator<<(HAKCLogger &Logger, T *T_) {
  for (std::shared_ptr<HAKCWriter> &stream : Logger.Streams()) {
    if (!Logger.IsDisabled() &&
        (Logger.GetLogLevel() >= stream->GetConfiguredLogLevel())) {
      *stream << T_;
    }
  }
  return Logger;
}

template <typename T, std::enable_if_t<std::is_integral_v<T>> * = nullptr>
HAKCLogger &operator<<(HAKCLogger &Logger, T T_) {
  for (std::shared_ptr<HAKCWriter> &stream : Logger.Streams()) {
    if (!Logger.IsDisabled() &&
        (Logger.GetLogLevel() >= stream->GetConfiguredLogLevel())) {
      *stream << T_;
    }
  }
  return Logger;
}

inline HAKCLogger &operator<<(HAKCLogger &Logger, const StringRef S) {
  for (std::shared_ptr<HAKCWriter> &stream : Logger.Streams()) {
    if (!Logger.IsDisabled() &&
        (Logger.GetLogLevel() >= stream->GetConfiguredLogLevel())) {
      *stream << S;
    }
  }
  return Logger;
}

} // namespace llvm::hakc

#endif // HAKC_HAKCLOGGER_H
