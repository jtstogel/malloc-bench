#include "src/jsmalloc/util/file_logger.h"

namespace jsmalloc {

bool GLogger::opened_ = false;
std::mutex GLogger::mu_;
FileLogger GLogger::logger_;

void FileLogger::Open(char const* file) {
  fd_ = open(file, O_CREAT | O_WRONLY | O_TRUNC,
             S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

void FileLogger::Log(const char* txt) const {
  write(fd_, txt, strlen(txt));
}

void GLogger::Open(char const* file) {
  std::lock_guard l(mu_);
  if (opened_) {
    return;
  }
  opened_ = true;
  logger_.Open(file);
}

void GLogger::Log(const char* txt) {
  char fname[256];
  std::sprintf(fname, "/tmp/glogger-%d.txt", ::getpid());
  Open(fname);

  logger_.Log(txt);
}

}  // namespace jsmalloc
