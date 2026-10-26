#include "display_writeback.hpp"
#include <esp_cache.h>
#include <cassert>
#include <cstdio>
int Cache_WriteBack_Addr(uint32_t,uint32_t);
int main(){
  assert(Cache_WriteBack_Addr(0x48000001,0)==ESP_OK&&sync_calls.empty());
  assert(Cache_WriteBack_Addr(0xfffffff0,1024)==ESP_ERR_INVALID_SIZE&&sync_calls.empty());
  assert(Cache_WriteBack_Addr(0x48000013,1500000)==ESP_OK);
  uintptr_t end=0x48000000;
  for(const auto& call:sync_calls){assert(call.address==end&&call.bytes&&call.bytes<=4096);assert(call.address%128==0&&call.bytes%128==0);end+=call.bytes;}
  assert(end==((uint64_t{0x48000013}+1500000+127)&~uint64_t{127}));
  auto stats=orcsdr::display_writeback::snapshot();assert(stats.chunks==sync_calls.size()&&stats.max_chunk_bytes<=4096);
  sync_calls.clear();fail_sync_at=2;
  assert(Cache_WriteBack_Addr(0x48000000,16384)==17&&sync_calls.size()==2);
  assert(orcsdr::display_writeback::snapshot().errors==2);
  std::puts("PASS: complete aligned writeback, <=4 KiB per cache mutex/critical interval, overflow/no-op/error handling and observable counters");
}
