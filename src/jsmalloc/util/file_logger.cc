#include "src/jsmalloc/util/file_logger.h"

namespace jsmalloc {

bool GLogger::opened_ = false;
std::mutex GLogger::mu_;
FileLogger GLogger::logger_;

}
