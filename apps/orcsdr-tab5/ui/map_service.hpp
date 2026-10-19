#pragma once
#include "map_pipeline.hpp"
#include <cstddef>
#include <cstdint>

namespace lgfx { inline namespace v1 { class LovyanGFX; } }
namespace orcsdr::storage { class FileSystem; }

namespace orcsdr::maps {
enum class Kind : uint8_t { firmware, pmtiles, legacy, rejected };
enum class Frame : uint8_t { pending, ready, unavailable };
struct PackInfo {
  Kind kind = Kind::rejected;
  char identity[256]{}, title[64]{}, region[80]{}, version[32]{}, status[64]{}, credit[192]{};
  char coverage[96]{}, provenance[160]{};
  uint64_t bytes = 0;
  uint8_t min_zoom = 0, max_zoom = 0;
  bool usable = false, size_checked = false;
};
struct MapSnapshot {
  bool loaded = false, sd = false, world = false, legacy = false, overflow = false, out_of_memory = false;
  uint8_t count = 0;
  uint32_t revision = 0;
  PackInfo packs[18]{};
};
struct FrameInfo {
  Frame state = Frame::unavailable;
  char title[64]{}, credit[192]{}, status[64]{};
  uint32_t revision = 0, cache_hits = 0;
  uint32_t push_us = 0;
  RenderStats stats{};
};

// Starts one low-priority worker. It renders only into its private PSRAM
// sprite; drawing/pushing that sprite remains on the owning UI task.
bool begin(storage::FileSystem* sd);
bool running();
bool snapshot(MapSnapshot*);
void rescan();
void cancel();
uint32_t revision();
bool installed(); // discovered availability, not a coverage or integrity claim
Frame draw(const orcmap::Viewport&, lgfx::v1::LovyanGFX& target, int x, int y,
           FrameInfo*, const char* preferred = nullptr, bool labels = true);
bool frame_info(FrameInfo*);
void report(void (*write)(const char*)); // read-only diagnostics through the caller's console
} // namespace orcsdr::maps
