#include "src/jsmalloc/blocks/free_block_allocator.h"

#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"
#include "src/jsmalloc/blocks/sentinel_block_allocator.h"
#include "src/jsmalloc/util/assert.h"

namespace jsmalloc {
namespace blocks {

FreeBlockAllocator::FreeBlockAllocator(SentinelBlockHeap& heap) : heap_(heap){};

constexpr size_t kPageSize = 4096;

void FreeBlockAllocator::Remove(FreeBlock* block) {
  if (block->BlockSize() == kPageSize) {
    if (FreeBlock::List::is_linked(*block)) {
      FreeBlock::List::unlink(*block);
    }
  } else {
    free_blocks_.Remove(block);
  }
}

void FreeBlockAllocator::Insert(FreeBlock* block) {
  if (block->BlockSize() == kPageSize) {
    page_size_blocks_.insert_front(*block);
  } else {
    free_blocks_.Insert(block);
  }
}

FreeBlock* FreeBlockAllocator::FindBestFit(size_t size) {
  DCHECK_EQ(size % 16, 0);
  if (size == kPageSize && !page_size_blocks_.empty()) {
    return page_size_blocks_.front();
  }
  return free_blocks_.LowerBound(
      [size](const FreeBlock& block) { return block.BlockSize() >= size; });
}

FreeBlock* FreeBlockAllocator::Allocate(size_t size) {
  DCHECK_EQ(size % 16, 0);

  FreeBlock* best_fit = FindBestFit(size);
  if (best_fit != nullptr) {
    Remove(best_fit);
    FreeBlock* remainder = best_fit->MarkUsed(size);
    // Don't bother storing small free blocks.
    // Small malloc sizes will be serviced by SmallBlockAllocator anyway.
    if (remainder != nullptr && remainder->BlockSize() > 256) {
      Insert(remainder);
    }
    return best_fit;
  }

  FreeBlock* block = FreeBlock::New(heap_, size);
  if (block == nullptr) {
    return nullptr;
  }
  block->MarkUsed();
  return block;
}

void FreeBlockAllocator::Free(BlockHeader* block) {
  FreeBlock* free_block = FreeBlock::MarkFree(block);

  FreeBlock* next_free_block = free_block->NextBlockIfFree();
  if (next_free_block != nullptr) {
    Remove(next_free_block);
    free_block->ConsumeNextBlock();
  }

  FreeBlock* prev_free_block = free_block->PrevBlockIfFree();
  if (prev_free_block != nullptr) {
    Remove(prev_free_block);
    prev_free_block->ConsumeNextBlock();
    free_block = prev_free_block;
  }

  Insert(free_block);
}

}  // namespace blocks
}  // namespace jsmalloc
