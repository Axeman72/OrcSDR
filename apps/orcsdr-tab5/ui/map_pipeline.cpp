#include "map_pipeline.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <new>
#ifndef __cpp_exceptions
#error "OrcMaps allocation recovery requires C++ exception support"
#endif

namespace orcsdr::maps {
namespace {
bool coordinates(int32_t lat, int32_t lon) {
  return lat >= -900000000 && lat <= 900000000 && lon >= -1800000000 && lon <= 1800000000;
}
}

Pipeline::Pipeline(orcmap::ByteSource& source, WorkHooks hooks)
    : hooks_(hooks), source_(source, stats_, hooks_), reader_(&source_, 1) {}

size_t Pipeline::Source::Read(uint64_t offset, void* output, size_t size) {
  const uint64_t start = hooks_.now_us ? hooks_.now_us() : 0;
  const size_t bytes = source_.Read(offset, output, size);
  ++stats_.reads;
  stats_.bytes += bytes;
  if (hooks_.now_us) stats_.io_us += hooks_.now_us() - start;
  return bytes;
}

bool Pipeline::open(std::string_view profile) {
  try {return open_impl(profile);}
  catch(const std::bad_alloc&) {ready_=false;stats_.out_of_memory=true;++stats_.failed;return false;}
}
bool Pipeline::open_impl(std::string_view profile) {
  stats_={};
  ready_ = false;
  if (profile != "openmaptiles-3.16" && profile != "orcmaps-overview-1") return false;
  if (!reader_.Open() || reader_.Header().tile_type != orcmap::TileType::kMvt) return false;
  // Scratch is reused, including the inflater and bounded one-feature buffer.
  if (!orcmap::ReserveMvtStreamScratch(&scratch_, 48 * 1024)) return false;
  ready_ = true;
  return true;
}

struct Pipeline::Context {
  Pipeline& pipeline;
  const orcmap::Viewport& viewport;
  orcmap::RenderTarget& target;
  const std::vector<orcmap::TilePlacement>& placements;
  orcmap::TileId tile;
};

bool Pipeline::feature(const orcmap::Feature& decoded, void* opaque) {
  auto& context = *static_cast<Context*>(opaque);
  auto& pipeline = context.pipeline;
  if (pipeline.hooks_.service && !pipeline.hooks_.service(pipeline.hooks_.context)) {
    pipeline.stats_.cancelled = true;
    return false;
  }
  ++pipeline.stats_.features;
  // The streaming callback owns this reused feature until it returns. The
  // classifier is the engine's public no-text classifier, not dashboard logic.
  auto& feature = const_cast<orcmap::Feature&>(decoded);
  feature.kind_assigned = orcmap::experimental::TryClassifyFeature(feature, &feature.kind);
  const uint64_t start = pipeline.now();
  for (const auto& placement : context.placements) {
    if (placement.tile == context.tile &&
        !orcmap::RenderFeatureAt(feature, placement, context.viewport,
                                orcmap::styles::OrcSdrDark(), &context.target)) return false;
  }
  pipeline.stats_.render_us += pipeline.now() - start;
  return true;
}

bool Pipeline::render(const orcmap::Viewport& viewport, orcmap::RenderTarget& target) {
  try {return render_impl(viewport,target);}
  catch(const std::bad_alloc&) {ready_=false;stats_.out_of_memory=true;++stats_.failed;return false;}
}
bool Pipeline::render_impl(const orcmap::Viewport& viewport,orcmap::RenderTarget& target) {
  stats_ = {};
  const uint64_t start = now();
  if (!ready_ || viewport.width_px != target.Width() || viewport.height_px != target.Height() ||
      viewport.zoom < reader_.Header().min_zoom || viewport.zoom > reader_.Header().max_zoom) return false;
  std::vector<orcmap::TilePlacement> placements;
  if (!orcmap::EnumerateVisibleTilePlacements(viewport, &placements) || placements.size() > 64) return false;
  if (!orcmap::ClearMapBackground(viewport, orcmap::styles::OrcSdrDark(), &target)) return false;
  for (size_t i = 0; i < placements.size(); ++i) {
    const auto tile = placements[i].tile;
    bool duplicate = false;
    for (size_t j = 0; j < i; ++j) duplicate |= placements[j].tile == tile;
    if (duplicate) continue; // decode once, draw every wrapped-world placement
    const uint64_t lookup = now();
    const bool present = reader_.TileExists(tile.z, tile.x, tile.y);
    stats_.lookup_us += now() - lookup;
    if (!present) { ++stats_.missing; continue; }
    Context context{*this, viewport, target, placements, tile};
    orcmap::MvtStreamOptions options;
    options.include_layer = &orcmap::experimental::IncludeNoTextBasemapLayer;
    options.feature_sink = &Pipeline::feature;
    options.feature_sink_ctx = &context;
    const uint64_t stream = now(), render_before = stats_.render_us;
    const bool ok = reader_.StreamTile(tile.z, tile.x, tile.y, options, &scratch_);
    stats_.stream_us += now() - stream - (stats_.render_us - render_before);
    if (ok) ++stats_.tiles;
    else ++stats_.failed;
    if (stats_.cancelled) break;
  }
  stats_.total_us = now() - start;
  return stats_.tiles > 0 && stats_.failed == 0 && !stats_.cancelled;
}

bool regional_view(int32_t latitude_e7, int32_t longitude_e7, double range_nm,
                   int width, int height, orcmap::Viewport* output) {
  if (!output || !coordinates(latitude_e7, longitude_e7) || std::abs(static_cast<int64_t>(latitude_e7))>850511287 || width <= 0 || height <= 0 ||
      width > 1280 || height > 720 || !std::isfinite(range_nm) || range_nm <= 0 || range_nm > 10000) return false;
  orcmap::Viewport view;
  view.width_px = width; view.height_px = height;
  if (!orcmap::SetCenter(&view, latitude_e7 / 1e7, longitude_e7 / 1e7)) return false;
  // Choose source zoom and screen tile size so range has a continuous scale,
  // rather than changing the receiver's requested range to an integer zoom.
  const double radius = std::min(width, height) / 2.0;
  const double cosine = std::max(.05, std::cos(view.center_lat_deg * .017453292519943295));
  const double world_pixels = radius * 21600.0 * cosine / range_nm;
  const int zoom = std::clamp(static_cast<int>(std::floor(std::log2(std::max(1.0, world_pixels / 256.0)))), 0, 20);
  view.zoom = static_cast<uint8_t>(zoom);
  view.tile_size_px = std::max(1, static_cast<int>(std::round(world_pixels / std::ldexp(1.0, zoom))));
  *output = view;
  return true;
}

bool world_compat_project(double latitude,double longitude,int left,int top,int width,int height,int* x,int* y) {
  if(!x||!y||width<=0||height<=0||width>1280||height>720||!std::isfinite(latitude)||
      !std::isfinite(longitude)||latitude<-90||latitude>90||longitude<-180||longitude>180)return false;
  // Preserve the existing full-world float geometry, including unclipped edges.
  *x=left+static_cast<int>((static_cast<float>(longitude)+180.0f)/360.0f*width);
  *y=top+static_cast<int>((90.0f-static_cast<float>(latitude))/180.0f*height);
  return true;
}

bool project(const orcmap::Viewport& view, int32_t latitude_e7, int32_t longitude_e7, int* x, int* y) {
  if (!x || !y || !coordinates(latitude_e7, longitude_e7) || std::abs(static_cast<int64_t>(latitude_e7))>850511287) return false;
  double sx = 0, sy = 0;
  if (!orcmap::ProjectLatLon(view, {latitude_e7 / 1e7, longitude_e7 / 1e7}, &sx, &sy) ||
      !std::isfinite(sx) || !std::isfinite(sy) || sx < 0 || sy < 0 || sx >= view.width_px || sy >= view.height_px) return false;
  *x = static_cast<int>(std::lround(sx)); *y = static_cast<int>(std::lround(sy));
  return *x < view.width_px && *y < view.height_px;
}
} // namespace orcsdr::maps
