#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
constexpr int ESP_OK=0,ESP_ERR_INVALID_SIZE=1;
constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1,ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2;
struct SyncCall {uintptr_t address;size_t bytes;};
inline std::vector<SyncCall> sync_calls;
inline int fail_sync_at=-1;
inline int esp_cache_msync(void* address,size_t bytes,int flags) {
  if(flags!=3)return 99;
  sync_calls.push_back({reinterpret_cast<uintptr_t>(address),bytes});
  return static_cast<int>(sync_calls.size())==fail_sync_at?17:ESP_OK;
}
