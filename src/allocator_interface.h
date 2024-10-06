#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "src/heap_factory.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/jsmalloc.h"
#include "src/jsmalloc/util/file_logger.h"

namespace bench {

static bool initialized = false;
static std::mutex mu;

inline void initialize_heap(HeapFactory& heap_factory) {
  std::lock_guard l(mu);
  static jsmalloc::HeapFactoryAdaptor adaptor(&heap_factory);
  new (&adaptor) jsmalloc::HeapFactoryAdaptor(&heap_factory);
  jsmalloc::initialize_heap(adaptor);
  initialized = true;
}

inline void initialize() {
  if (initialized) {
    return;
  }
  initialized = true;

  static jsmalloc::MMapMemRegionAllocator allocator;
  jsmalloc::initialize_heap(allocator);
}

inline void* malloc(size_t size, size_t alignment = 0) {
  std::lock_guard l(mu);
  initialize();

  void* ptr = jsmalloc::malloc(size, alignment);

#ifndef NLOG
  char msg[256];
  std::sprintf(msg, "malloc(%zu, %zu) = %p\n", size, alignment, ptr);
  jsmalloc::GLogger::Log(msg);
#endif

  return ptr;
}

inline void* calloc(size_t nmemb, size_t size) {
  std::lock_guard l(mu);
  initialize();

  void* ptr = jsmalloc::calloc(nmemb, size);

#ifndef NLOG
  char msg[256];
  std::sprintf(msg, "calloc(%zu, %zu) = %p\n", nmemb, size, ptr);
  jsmalloc::GLogger::Log(msg);
#endif

  return ptr;
}

inline void* realloc(void* ptr, size_t size) {
  std::lock_guard l(mu);
  initialize();

  void* new_ptr = jsmalloc::realloc(ptr, size);
#ifndef NLOG
  char msg[256];
  std::sprintf(msg, "realloc(%p, %zu) = %p\n", ptr, size, new_ptr);
  jsmalloc::GLogger::Log(msg);
#endif

  return new_ptr;
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  std::lock_guard l(mu);
  initialize();

#ifndef NLOG
  char msg[256];
  std::sprintf(msg, "free(%p)\n", ptr);
  jsmalloc::GLogger::Log(msg);
#endif

  return jsmalloc::free(ptr, size, alignment);
}

inline size_t get_size(void* ptr) {
  std::lock_guard l(mu);
  initialize();

  return jsmalloc::get_size(ptr);
}

}  // namespace bench
