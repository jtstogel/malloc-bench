#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "src/heap_factory.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/jsmalloc.h"

namespace bench {

static constexpr size_t kHeapSize = 512 * (1 << 20);
static bool initialized = false;
static jsmalloc::MMapMemRegionAllocator allocator;

inline void initialize_heap(HeapFactory& heap_factory) {
  static jsmalloc::HeapFactoryAdaptor adaptor(&heap_factory);
  new (&adaptor) jsmalloc::HeapFactoryAdaptor(&heap_factory);
  jsmalloc::initialize_heap(adaptor);
  initialized = true;
}

inline void initialize() {
  jsmalloc::initialize_heap(allocator);
  initialized = true;
}

inline void* malloc(size_t size, size_t alignment = 0) {
  if (!initialized) {
    initialize();
  }
  return jsmalloc::malloc(size, alignment);
}

inline void* calloc(size_t nmemb, size_t size) {
  if (!initialized) {
    initialize();
  }
  return jsmalloc::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  if (!initialized) {
    initialize();
  }
  return jsmalloc::realloc(ptr, size);
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  if (!initialized) {
    initialize();
  }
  return jsmalloc::free(ptr, size, alignment);
}

inline size_t get_size(void* ptr) {
  if (!initialized) {
    initialize();
  }
  return jsmalloc::get_size(ptr);
}

}  // namespace bench
