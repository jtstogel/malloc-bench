#pragma once

#include <cstdint>

namespace jsmalloc {
namespace twiddle {

inline uint64_t PtrValue(void* ptr) {
  return reinterpret_cast<uint8_t*>(ptr) - static_cast<uint8_t*>(nullptr);
}

template <typename T>
inline T* AddPtrOffset(void* ptr, int32_t offset_bytes) {
  return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(ptr) + offset_bytes);
}

}  // namespace twiddle
}  // namespace jsmalloc
