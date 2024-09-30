#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/jsmalloc.h"

namespace bench {

static constexpr size_t kHeapSize = 512 * (1 << 20);

// Called before any allocations are made.
inline void initialize_heap(HeapFactory& heap_factory) {
  static jsmalloc::HeapFactoryAdaptor adaptor(&heap_factory);
  new (&adaptor) jsmalloc::HeapFactoryAdaptor(&heap_factory);
  jsmalloc::initialize_heap(adaptor);
}

inline void* malloc(size_t size, size_t alignment = 0) {
  return jsmalloc::malloc(size, alignment);
}

inline void* calloc(size_t nmemb, size_t size) {
  return jsmalloc::calloc(nmemb, size);
}

inline void* realloc(void* ptr, size_t size) {
  return jsmalloc::realloc(ptr, size);
}

inline void free(void* ptr, size_t size = 0, size_t alignment = 0) {
  return jsmalloc::free(ptr, size, alignment);
}

inline size_t get_size(void* ptr) {
  return 0;
}

}  // namespace bench
