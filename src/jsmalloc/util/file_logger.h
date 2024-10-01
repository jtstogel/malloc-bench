#pragma once

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
  void Open(char const* file) {
    fd_ = open(file, O_CREAT | O_WRONLY | O_TRUNC,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  }

  void Log(const char* txt) const {
    write(fd_, txt, strlen(txt));
  }

 private:
  int fd_ = 0;
};

class GLogger {
 public:
  static void Open(char const* file) {
    std::lock_guard l(mu_);
    if (opened_) {
      return;
    }
    opened_ = true;
    logger_.Open(file);
  }

  static void Log(const char* txt) {
    Open("/tmp/glogger.txt");
    logger_.Log(txt);
  }

 private:
  static bool opened_;
  static std::mutex mu_;
  static FileLogger logger_;
};

}  // namespace jsmalloc
