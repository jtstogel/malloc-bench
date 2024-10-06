#include "src/jsmalloc/util/file_logger.h"

#include <cstdarg>
#include <cstdio>

namespace jsmalloc {

bool GLogger::opened_ = false;
std::mutex GLogger::mu_;
FileLogger GLogger::logger_;

void FileLogger::Open(char const* file) {
  fd_ = open(file, O_CREAT | O_WRONLY | O_TRUNC,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

void FileLogger::Log(Level level, const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  char buf[256];
  std::vsprintf(buf, fmt, args);
  va_end(args);

  write(fd_, buf, strlen(buf));

  if (level == Level::kError) {
    fsync(fd_);
  }
}

void GLogger::Open() {
  std::lock_guard l(mu_);
  if (opened_) {
    return;
  }
  opened_ = true;

  char file[256];
  std::sprintf(file, "/tmp/glogger-%d.txt", ::getpid());
  logger_.Open(file);
}

FileLogger& GLogger::Instance() {
  Open();
  return logger_;
}

}  // namespace jsmalloc
