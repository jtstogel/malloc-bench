#include "src/concurrent_id_map.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "folly/concurrency/ConcurrentHashMap.h"

#include "src/perfetto.h"  // IWYU pragma: keep

namespace bench {

/* static */
uint64_t ConcurrentIdMap::UniqueId(uint64_t id, uint64_t iteration,
                                   const Tracefile& tracefile) {
  return id + iteration * tracefile.lines_size();
}

absl::Status ConcurrentIdMap::AddAllocation(uint64_t id, void* allocated_ptr) {
  TRACE_EVENT("test_infrastructure", "ConcurrentIdMap::AddAllocation");
  auto [it, inserted] =
      id_map_.insert(id, MapVal{ .allocated_ptr = allocated_ptr });
  if (!inserted) {
    TRACE_EVENT("test_infrastructure", "ConcurrentIdMap::Queue");
    // If the insertion failed, that means there was a pending allocation
    // marker in this slot.
    auto pending_idx = it->second.idx;

    // Replace the slot with the allocated pointer...
    auto result = id_map_.assign(id, MapVal{ .allocated_ptr = allocated_ptr });
    if (!result.has_value()) {
      return absl::InternalError(absl::StrFormat(
          "Failed to insert %v after queuing pending operation.", id));
    }

    // and then push the pending index to the queue.
    {
      absl::MutexLock lock(&queue_lock_);
      queued_ops_.push_back(std::move(pending_idx));
    }
  }

  return absl::OkStatus();
}

absl::Status ConcurrentIdMap::AddFree(uint64_t id) {
  size_t erased_elems = id_map_.erase(id);
  if (erased_elems != 1) {
    return absl::InternalError(
        absl::StrFormat("Failed to erase ID %v from the map, not found", id));
  }
  return absl::OkStatus();
}

std::optional<void*> ConcurrentIdMap::LookupAllocation(uint64_t id) {
  auto it = id_map_.find(id);
  if (it == id_map_.end()) {
    return std::nullopt;
  }
  return it->second.allocated_ptr;
}

bool ConcurrentIdMap::MaybeSuspendAllocation(
    uint64_t id, std::pair<const TraceLine*, uint64_t> idx) {
  auto [it, inserted] = id_map_.insert(id, MapVal{ .idx = idx });
  return inserted;
}

size_t ConcurrentIdMap::TakeFromQueue(
    std::pair<const TraceLine*, uint64_t> (&array)[], size_t array_len) {
  TRACE_EVENT("test_infrastructure", "ConcurrentIdMap::TakeFromQueue");
  absl::MutexLock lock(&queue_lock_);
  uint32_t n_taken_elements =
      static_cast<uint32_t>(std::min(queued_ops_.size(), array_len));
  for (uint32_t i = 0; i < n_taken_elements; i++) {
    array[i] = queued_ops_.front();
    queued_ops_.pop_front();
  }
  return n_taken_elements;
}

}  // namespace bench
