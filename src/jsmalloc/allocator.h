#pragma once

#include <cstdint>
#include <map>
#include <sys/mman.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "util/absl_util.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

class MemRegionAllocator;
class MMapMemRegionAllocator;
class HeapFactoryAdaptor;

class MemRegion {
  friend MemRegionAllocator;
  friend MMapMemRegionAllocator;
  friend HeapFactoryAdaptor;

 public:
  MemRegion() : MemRegion(nullptr, 0) {}

  MemRegion(MemRegion&& region) : MemRegion(region.Start(), region.MaxSize()) {
    SetEnd(region.End());
  }

  void* Start() const {
    return start_;
  }

  void* End() const {
    return end_;
  }

  size_t MaxSize() const {
    return max_size_;
  }

  bool Contains(void* ptr) const {
    uint64_t val = twiddle::PtrValue(ptr);
    return twiddle::PtrValue(Start()) <= val && val < twiddle::PtrValue(End());
  }

 private:
  explicit MemRegion(void* start, size_t max_size)
      : start_(start), end_(start), max_size_(max_size) {}

  void SetEnd(void* end) {
    end_ = end;
  }

  void* start_;
  void* end_;
  size_t max_size_;
};

class MemRegionAllocator {
 public:
  /** Returns a pointer to a new memory region. */
  virtual absl::StatusOr<MemRegion> New(size_t max_size) = 0;

  /** Extends the memory region. */
  virtual void* Extend(MemRegion* region, intptr_t increment) = 0;

  /** Releases the region back to main memory. */
  virtual absl::Status Delete(MemRegion* region) = 0;
};

class MMapMemRegionAllocator : public MemRegionAllocator {
 public:
  /** Returns a pointer to a new memory region. */
  absl::StatusOr<MemRegion> New(size_t max_size) override {
    void* heap_start = mmap(nullptr, max_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (heap_start == MAP_FAILED) {
      // std::cerr << "Failed to unmap heap: " << strerror(errno) << std::endl;
      return absl::InternalError(absl::StrFormat(
          "Failed to mmap size %zu region: %s", max_size, strerror(errno)));
    }
    return MemRegion(heap_start, max_size);
  }

  /** Extends the memory region. */
  void* Extend(MemRegion* region, intptr_t increment) override {
    if ((static_cast<uint8_t*>(region->End()) -
         static_cast<uint8_t*>(region->Start())) +
            increment >
        region->MaxSize()) {
      return nullptr;
    }
    void* previous_end = region->End();
    region->SetEnd(twiddle::AddPtrOffset<void>(region->End(), increment));
    return previous_end;
  }

  /** Releases the region back to main memory. */
  absl::Status Delete(MemRegion* region) override {
    if (region->Start() != nullptr) {
      int result = munmap(region->Start(), region->MaxSize());
      if (result == -1) {
        std::cerr << "Failed to unmap heap: " << strerror(errno) << std::endl;
        return absl::InternalError(
            absl::StrCat("Failed to unmap heap: ", strerror(errno)));
      }
    }
    return absl::OkStatus();
  }
};

/** A MemRegionAllocator that interfaces with a HeapFactory. */
class HeapFactoryAdaptor : public MemRegionAllocator {
 public:
  explicit HeapFactoryAdaptor(bench::HeapFactory* heap_factory)
      : heap_factory_(heap_factory){};

  absl::StatusOr<MemRegion> New(size_t size) override {
    auto s = heap_factory_->NewInstance(size);
    if (!s.ok()) {
      return s.status();
    }
    bench::Heap* heap = *s;
    heaps_by_start_.emplace(heap->Start(), heap);
    return MemRegion(heap->Start(), size);
  }

  absl::Status Delete(MemRegion* region) override {
    auto it = heaps_by_start_.find(region->Start());
    if (it == heaps_by_start_.end()) {
      return absl::InternalError(
          "HeapFactoryAdaptor::Delete called with invalid region");
    }

    RETURN_IF_ERROR(heap_factory_->DeleteInstance(it->second));
    heaps_by_start_.erase(it);
    return absl::OkStatus();
  }

  void* Extend(MemRegion* region, intptr_t increment) override {
    bench::Heap* heap = heaps_by_start_.at(region->Start());
    if (heap == nullptr) {
      return nullptr;
    }

    void* ptr = heap->sbrk(increment);
    if (ptr == nullptr) {
      return nullptr;
    }

    region->SetEnd(twiddle::AddPtrOffset<void>(ptr, increment));
    return ptr;
  }

 private:
  bench::HeapFactory* heap_factory_;
  std::map<void*, bench::Heap*> heaps_by_start_;
};

namespace testing {

/**
 * Allocator for testing.
 */
template <size_t N>
class FixedSizeTestRegion : public MemRegion, public MemRegionAllocator {
 public:
  void* Extend(MemRegion* region, intptr_t increment) override {
    DCHECK_TRUE(region == this);
    DCHECK_EQ(increment % 16, 0);
    DCHECK_LE(increment, N);
    if (end_ + increment >= N) {
      return nullptr;
    }
    void* ptr = End();
    end_ += increment;
    return ptr;
  }

  absl::StatusOr<MemRegion> New(size_t) override {
    return absl::UnimplementedError("FixedSizeTestRegion::New");
  }

  absl::Status Delete(MemRegion*) override {
    return absl::UnimplementedError("FixedSizeTestRegion::New");
  };

  void* Start() {
    // Ensure we give out 16-byte aligned addresses.
    return &data_[Offset()];
  }

  void* End() {
    return &data_[Offset() + end_];
  }

 private:
  intptr_t Offset() {
    return 16 - (twiddle::PtrValue(this) % 16);
  }

  size_t end_ = 0;
  uint8_t data_[N];
};

using TestRegionAllocator = FixedSizeTestRegion<1 << 20>;

}  // namespace testing

}  // namespace jsmalloc
