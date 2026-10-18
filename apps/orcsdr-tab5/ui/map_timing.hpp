#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace orcsdr::maps {
// Rolling raw intervals between distinct presented frames, including user dwell.
// Not an FPS or touch-latency metric; sample during a controlled gesture run.
class UpdateTiming {
 public:
  struct Summary {size_t count=0;uint64_t p50_us=0,p95_us=0,max_us=0;};
  void reset() {*this=UpdateTiming{};}
  void present(uint64_t now,uint32_t revision) {
    if(have_baseline_&&revision==revision_)return;
    if(have_baseline_&&now>=last_) {
      samples_[next_]=now-last_;next_=(next_+1)%samples_.size();
      count_=std::min(count_+1,samples_.size());
    }
    have_baseline_=true;last_=now;revision_=revision;
  }
  Summary summary() const {
    if(!count_)return {};
    auto sorted=samples_;
    std::sort(sorted.begin(),sorted.begin()+count_);
    return {count_,sorted[(count_*50+99)/100-1],
            sorted[(count_*95+99)/100-1],sorted[count_-1]};
  }
 private:
  std::array<uint64_t,256> samples_{};
  size_t next_=0,count_=0;
  uint64_t last_=0;
  uint32_t revision_=0;
  bool have_baseline_=false;
};
}
