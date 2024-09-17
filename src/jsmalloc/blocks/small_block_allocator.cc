#include "src/jsmalloc/blocks/small_block_allocator.h"

#include <cstdint>

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/fixed_size_free_block_allocator.h"
#include "src/jsmalloc/blocks/small_block.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {

namespace small {

/**
 * Returns the number of bins to use in a SmallBlock for a given size_class.
 */
constexpr uint32_t BinCountForSizeClass(uint32_t size_class) {
  uint32_t data_size =
      SmallBlockAllocator::kMaxDataSizePerSizeClass[size_class];
  size_t best_bin_count =
      (SmallBlockAllocator::kBlockSize + data_size - 1) / data_size;
  for (size_t c = best_bin_count; c > 0; c--) {
    if (SmallBlock::RequiredSize(data_size, c) <=
        SmallBlockAllocator::kBlockSize) {
      return c;
    }
  }
  return 0;
}

/**
 * Returns the data size allocable by a SmallBlock with the given size class.
 */
constexpr uint32_t DataSizeForSizeClass(uint32_t size_class) {
  return SmallBlockAllocator::kMaxDataSizePerSizeClass[size_class];
}

}  // namespace small

SmallBlock::List& SmallBlockAllocator::GetSmallBlockList(size_t data_size) {
  uint32_t size_class = small::SizeClass(data_size);
  return small_block_lists_[size_class];
}

SmallBlock* SmallBlockAllocator::NewSmallBlock(size_t data_size) {
  uint32_t size_class = small::SizeClass(data_size);
  DCHECK_LE(SmallBlock::RequiredSize(small::DataSizeForSizeClass(size_class),
                                     small::BinCountForSizeClass(size_class)),
            SmallBlockAllocator::kBlockSize);

  void* free_block = allocator_.Allocate();
  if (free_block == nullptr) {
    return nullptr;
  }
  return SmallBlock::Init(free_block, small::DataSizeForSizeClass(size_class),
                          small::BinCountForSizeClass(size_class));
}

void* SmallBlockAllocator::Allocate(size_t size) {
  SmallBlock::List& allocable_block_list = GetSmallBlockList(size);

  SmallBlock* block = allocable_block_list.front();

  if (block != nullptr) {
    void* ptr = block->Alloc();
    if (block->IsFull()) {
      SmallBlock::List::unlink(*block);
    }
    return ptr;
  }

  block = NewSmallBlock(size);
  if (block == nullptr) {
    return nullptr;
  }

  void* ptr = block->Alloc();
  DCHECK_FALSE(block->IsFull());
  allocable_block_list.insert_front(*block);
  return ptr;
}

void SmallBlockAllocator::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  int32_t offset = twiddle::PtrValue(ptr) % SmallBlockAllocator::kBlockSize;
  auto* hdr = twiddle::AddPtrOffset<BlockHeader>(ptr, -offset);
  auto* block = reinterpret_cast<SmallBlock*>(hdr);
  // If the block was full, then we would have removed it from its
  // freelist. Add it back now.
  if (block->IsFull()) {
    SmallBlock::List& allocable_block_list =
        GetSmallBlockList(block->DataSize());
    allocable_block_list.insert_back(*block);
  }

  block->Free(ptr);

  // There's no more data left in the block.
  // Remove it from our datastructures and
  // return it to the free block allocator.
  if (block->IsEmpty()) {
    SmallBlock::List::unlink(*block);
    allocator_.Free(hdr);
  }
}

}  // namespace blocks
}  // namespace jsmalloc
