#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "orcmap/byte_source.hpp"
#include "orcmap/mvt_stream.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/render_target.hpp"
#include "orcmap/viewport.hpp"

namespace orcsdr::maps {

struct WorkHooks {
  uint64_t (*now_us)() = nullptr;
  bool (*service)(void*) = nullptr; // false cancels; device worker yields here
  void* context = nullptr;
};

struct RenderStats {
  uint32_t tiles = 0, missing = 0, failed = 0, features = 0;
  uint32_t reads = 0;
  uint64_t bytes = 0, io_us = 0, lookup_us = 0, stream_us = 0, render_us = 0, total_us = 0;
  bool cancelled = false;
};

// One source/reader/scratch per active archive. No display, radio, or network
// ownership lives here; the device adapter supplies an off-screen target.
class Pipeline {
 public:
  explicit Pipeline(orcmap::ByteSource& source, WorkHooks hooks = {});
  bool open(std::string_view profile);
  bool render(const orcmap::Viewport& viewport, orcmap::RenderTarget& target);
  const orcmap::PmTilesHeader& header() const { return reader_.Header(); }
  const RenderStats& stats() const { return stats_; }

 private:
  class Source final : public orcmap::ByteSource {
   public:
    Source(orcmap::ByteSource& source, RenderStats& stats, WorkHooks& hooks)
        : source_(source), stats_(stats), hooks_(hooks) {}
    size_t Read(uint64_t offset, void* output, size_t size) override;
    uint64_t Size() const override { return source_.Size(); }
    bool Valid() const override { return source_.Valid(); }
   private:
    orcmap::ByteSource& source_;
    RenderStats& stats_;
    WorkHooks& hooks_;
  };
  struct Context;
  static bool feature(const orcmap::Feature&, void*);
  uint64_t now() const { return hooks_.now_us ? hooks_.now_us() : 0; }
  WorkHooks hooks_;
  RenderStats stats_{};
  Source source_;
  orcmap::PmTilesReader reader_;
  orcmap::MvtStreamScratch scratch_;
  bool ready_ = false;
};

// Application geometry goes through OrcMaps too; the coordinates remain E7 at
// the boundary. Invalid dimensions/coordinates fail rather than plotting zero.
bool regional_view(int32_t latitude_e7, int32_t longitude_e7, double range_nm,
                   int width, int height, orcmap::Viewport* output);
bool project(const orcmap::Viewport&, int32_t latitude_e7, int32_t longitude_e7,
             int* x, int* y);

} // namespace orcsdr::maps
