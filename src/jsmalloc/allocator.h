#pragma once

#include <cstdint>
#include <map>

#include "util/absl_util.h"

#include "src/heap_factory.h"
#include "src/heap_interface.h"
#include "src/jsmalloc/util/assert.h"
#include "src/jsmalloc/util/twiddle.h"

namespace jsmalloc {

class MemRegionAllocator;
class HeapFactoryAdaptor;

class MemRegion {
  friend MemRegionAllocator;
  friend HeapFactoryAdaptor;

 public:
  MemRegion() : MemRegion(nullptr) {}

  explicit MemRegion(void* start) : start_(start), end_(start) {}

  void* Start() const {
    return start_;
  }

  void* End() const {
    return end_;
  }

  bool Contains(void* ptr) const {
    uint64_t val = twiddle::PtrValue(ptr);
    return twiddle::PtrValue(Start()) <= val && val < twiddle::PtrValue(End());
  }

 private:
  void SetEnd(void* end) {
    end_ = end;
  }

  void* start_;
  void* end_;
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
    return MemRegion(heap->Start());
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
