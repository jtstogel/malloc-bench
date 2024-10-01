#include "src/jsmalloc/jsmalloc.h"

#include <bit>
#include <cassert>
#include <cstddef>

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block_allocator.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/blocks/small_block_allocator.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

namespace {

constexpr size_t kHeapSize = 512 << 20;

class FreeBlockHeap {
 public:
  explicit FreeBlockHeap(MemRegionAllocator* allocator, MemRegion* region)
      : sentinel_block_heap_(region, allocator),
        free_block_allocator_(sentinel_block_heap_) {}

  blocks::FreeBlockAllocator& FreeBlockAllocator() {
    return free_block_allocator_;
  }

  void Init() {
    sentinel_block_heap_.Init();
  }

 private:
  blocks::SentinelBlockHeap sentinel_block_heap_;
  blocks::FreeBlockAllocator free_block_allocator_;
};

class HeapGlobals {
 public:
  explicit HeapGlobals(MemRegionAllocator* allocator,
                       MemRegion* small_block_heap, MemRegion* large_block_heap)
      : large_block_heap_(allocator, large_block_heap),
        large_block_allocator_(large_block_heap_.FreeBlockAllocator()),
        small_block_region_(small_block_heap),
        small_block_allocator_(allocator, small_block_heap) {}

  void Init() {
    large_block_heap_.Init();
  }

  FreeBlockHeap large_block_heap_;
  blocks::LargeBlockAllocator large_block_allocator_;
  MemRegion* small_block_region_;
  blocks::SmallBlockAllocator small_block_allocator_;
};

MemRegion g_small_block_heap;
MemRegion g_large_block_heap;
uint8_t globals_data[sizeof(HeapGlobals)];
HeapGlobals* heap_globals = reinterpret_cast<HeapGlobals*>(&globals_data);

}  // namespace

// Called before any allocations are made.
void initialize_heap(MemRegionAllocator& allocator) {
  absl::StatusOr<MemRegion> large_block_heap = allocator.New(kHeapSize);
  if (!large_block_heap.ok()) {
    std::cerr << "Failed to initialize large block heap" << std::endl;
    std::exit(-1);
  }
  new (&large_block_heap) MemRegion(std::move(*large_block_heap));

  absl::StatusOr<MemRegion> small_block_heap = allocator.New(kHeapSize);
  if (!small_block_heap.ok()) {
    std::cerr << "Failed to initialize small block heap" << std::endl;
    std::exit(-1);
  }
  new (&g_small_block_heap) MemRegion(std::move(*small_block_heap));

  heap_globals = new (globals_data)
      HeapGlobals(&allocator, &g_small_block_heap, &g_small_block_heap);
  heap_globals->Init();
}

void* malloc(size_t size, size_t alignment) {
  if (size == 0) {
    return nullptr;
  }

  alignment = alignment == 0 ? 1 : alignment;
  DCHECK_EQ(std::popcount(alignment), 1);

  size_t required_size = size + alignment - 1;
  if (required_size <= blocks::SmallBlockAllocator::kMaxDataSize) {
    void* ptr = heap_globals->small_block_allocator_.Allocate(required_size);
    return twiddle::Align(ptr, alignment);
  }

  return heap_globals->large_block_allocator_.Allocate(size, alignment);
}

void* calloc(size_t nmemb, size_t size) {
  void* ptr = malloc(nmemb * size);
  if (ptr == nullptr) {
    return nullptr;
  }
  memset(ptr, 0, nmemb * size);
  return ptr;
}

void* default_realloc(void* ptr, size_t size) {
  void* new_ptr = malloc(size);
  if (new_ptr == nullptr) {
    return nullptr;
  }
  if (ptr != nullptr) {
    memcpy(new_ptr, ptr, size);
  }
  free(ptr);
  return new_ptr;
}

void* realloc(void* ptr, size_t size) {
  if (heap_globals->small_block_region_->Contains(ptr)) {
    void* new_ptr = heap_globals->small_block_allocator_.Realloc(ptr, size);
    if (new_ptr != nullptr) {
      return new_ptr;
    }
  }

  return default_realloc(ptr, size);
}

void free(void* ptr, size_t size, size_t alignment) {
  if (ptr == nullptr) {
    return;
  }

  if (heap_globals->small_block_region_->Contains(ptr)) {
    heap_globals->small_block_allocator_.Free(ptr);
    return;
  }
  heap_globals->large_block_allocator_.Free(ptr);
}

size_t get_size(void* ptr) {
  return 0;
}

}  // namespace jsmalloc
