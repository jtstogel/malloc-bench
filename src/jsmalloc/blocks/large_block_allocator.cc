#include "src/jsmalloc/blocks/large_block_allocator.h"

#include <cstddef>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/free_block_allocator.h"
#include "src/jsmalloc/blocks/large_block.h"
#include "src/jsmalloc/util/math.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {
namespace blocks {

LargeBlockAllocator::LargeBlockAllocator(FreeBlockAllocator& allocator)
    : allocator_(allocator){};

int32_t* DataPrefix(void* data_ptr) {
  return twiddle::AddPtrOffset<int32_t>(
      data_ptr, -static_cast<int32_t>(sizeof(uint32_t)));
  ;
}

/** Allocates a chunk of user data from a `LargeBlock`. */
void* LargeBlockAllocator::Allocate(size_t size, size_t alignment) {
  size_t required_size = size + alignment - 1;

  FreeBlock* free_block =
      allocator_.Allocate(LargeBlock::BlockSize(required_size));
  if (free_block == nullptr) {
    return nullptr;
  }
  LargeBlock* block = LargeBlock::Init(free_block);
  if (block == nullptr) {
    return nullptr;
  }

  void* data_ptr = twiddle::Align(block->Data(), alignment);
  *DataPrefix(data_ptr) =
      twiddle::PtrValue(data_ptr) - twiddle::PtrValue(block);

  return data_ptr;
}

/** Frees a chunk of user data from its `LargeBlock`. */
void LargeBlockAllocator::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  auto* block = twiddle::AddPtrOffset<LargeBlock>(ptr, -(*DataPrefix(ptr)));
  allocator_.Free(block->Header());
}

}  // namespace blocks
}  // namespace jsmalloc
