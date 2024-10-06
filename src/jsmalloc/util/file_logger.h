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

  void Log(const char* txt) const;

 private:
  int fd_ = 0;
};

class GLogger {
 public:
  static void Log(const char* txt);

 private:
  static void Open(char const* file);

  static bool opened_;
  static std::mutex mu_;
  static FileLogger logger_;
};

}  // namespace jsmalloc
