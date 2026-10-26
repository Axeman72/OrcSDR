#include "display_writeback.hpp"
#include <esp_cache.h>
#include <esp_timer.h>
#include <algorithm>
#include <atomic>
#include <limits>

namespace {
constexpr uint32_t kAlignment=128,kMaxChunk=4096;
std::atomic<uint32_t> calls{0},chunks{0},errors{0},max_chunk_bytes{0},max_chunk_us{0},max_call_us{0};
void maximum(std::atomic<uint32_t>& value,uint32_t candidate) {
  auto old=value.load(std::memory_order_relaxed);
  while(old<candidate&&!value.compare_exchange_weak(old,candidate,std::memory_order_relaxed)){}
}
}

// M5GFX deliberately supplies a weak hook with this signature. Keep each
// esp_cache_msync invocation bounded: IDF holds its cache mutex for the entire
// invocation, even when its optional inner critical-section chunking is enabled.
// Releasing both between chunks lets USB/DSP run without changing image pixels,
// receiver rate, driver buffers or the SDK. Never modify managed components.
int Cache_WriteBack_Addr(uint32_t address,uint32_t size) {
  calls.fetch_add(1,std::memory_order_relaxed);
  if(!size)return ESP_OK;
  const uint64_t end=(uint64_t{address}+size+kAlignment-1)&~uint64_t{kAlignment-1};
  if(end>std::numeric_limits<uint32_t>::max()) {
    errors.fetch_add(1,std::memory_order_relaxed);return ESP_ERR_INVALID_SIZE;
  }
  const auto started=esp_timer_get_time();
  int result=ESP_OK;
  for(uint64_t cursor=address&~uint32_t{kAlignment-1};cursor<end;) {
    const auto count=static_cast<uint32_t>(std::min<uint64_t>(kMaxChunk,end-cursor));
    const auto chunk_started=esp_timer_get_time();
    result=esp_cache_msync(reinterpret_cast<void*>(static_cast<uintptr_t>(cursor)),count,
                         ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    chunks.fetch_add(1,std::memory_order_relaxed);
    maximum(max_chunk_bytes,count);
    maximum(max_chunk_us,static_cast<uint32_t>(esp_timer_get_time()-chunk_started));
    if(result!=ESP_OK){errors.fetch_add(1,std::memory_order_relaxed);break;}
    cursor+=count;
  }
  maximum(max_call_us,static_cast<uint32_t>(esp_timer_get_time()-started));
  return result;
}

namespace orcsdr::display_writeback {
Snapshot snapshot() {
  return {calls.load(),chunks.load(),errors.load(),max_chunk_bytes.load(),max_chunk_us.load(),max_call_us.load()};
}
}
