#pragma once

#include "src/jsmalloc/allocator.h"
#include "src/jsmalloc/blocks/block.h"
#include "src/jsmalloc/blocks/free_block.h"

namespace jsmalloc {
namespace blocks {

/**
 * An allocator of free blocks.
 */
class FreeBlockAllocator {
 public:
  /**
   * Returns a FreeBlock allocator that depends
   * on the provided heap-based allocator.
   */
  explicit FreeBlockAllocator(Allocator& allocator);

  /**
   * Returns a pointer to some free space of exactly the given size.
   */
  FreeBlock* Allocate(size_t size);

  /**
   * Marks the block as free.
   */
  void Free(BlockHeader* block);

 private:
  Allocator& allocator_;
  FreeBlock::List free_blocks_;
};

namespace testing {

/** A FreeBlockAllocator that can be initialized on the stack. */
class StackFreeBlockAllocator : public FreeBlockAllocator {
 public:
  StackFreeBlockAllocator() : FreeBlockAllocator(stack_allocator_){};

 private:
  BigStackAllocator stack_allocator_;
};

}  // namespace testing

}  // namespace blocks
}  // namespace jsmalloc
