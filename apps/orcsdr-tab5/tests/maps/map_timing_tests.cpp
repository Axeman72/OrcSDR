#include "map_timing.hpp"
#include <cassert>
#include <cstdint>
int main() {
  orcsdr::maps::UpdateTiming timing;
  assert(timing.summary().count==0);
  timing.present(100,1);timing.present(200,1); // Repainting one frame is not a new update.
  assert(timing.summary().count==0);
  timing.present(133,2);timing.present(166,3);timing.present(266,4);
  auto summary=timing.summary();
  assert(summary.count==3&&summary.p50_us==33&&summary.p95_us==100&&summary.max_us==100);
  timing.reset();
  timing.present(1000,1);timing.present(900,2); // Clock discontinuity starts a new baseline.
  assert(timing.summary().count==0);
  for(uint32_t i=1;i<=300;++i)timing.present(900+i*10,2+i);
  summary=timing.summary();
  assert(summary.count==256&&summary.p50_us==10&&summary.p95_us==10&&summary.max_us==10);
}
