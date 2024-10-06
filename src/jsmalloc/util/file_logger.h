#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace jsmalloc {

class FileLogger {
 public:
  void Open(char const* file);

  void Log(const char* fmt, ...) const;

  void LogVariadicArgs(const char* fmt, va_list args) const;

 private:
  int fd_ = 0;
};

class GLogger {
 public:
  static FileLogger& Instance();

 private:
  static void Open();

  static bool opened_;
  static std::mutex mu_;
  static FileLogger logger_;
};

#ifndef NDEBUG

#define DEBUG_LOG(...)                                \
  do {                                                \
    ::jsmalloc::GLogger::Instance().Log(__VA_ARGS__); \
  } while (false);

#define DEBUG_LOG_IF(cond, ...)                         \
  do {                                                  \
    if (cond) {                                         \
      ::jsmalloc::GLogger::Instance().Log(__VA_ARGS__); \
    }                                                   \
  } while (false);

#else

#define DEBUG_LOG(...)
#define DEBUG_LOG_IF(...)

#endif

}  // namespace jsmalloc
