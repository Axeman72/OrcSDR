#pragma once
#include <cstdint>
namespace orcsdr::display_writeback {
struct Snapshot {
  uint32_t calls=0,chunks=0,errors=0,max_chunk_bytes=0,max_chunk_us=0,max_call_us=0;
};
Snapshot snapshot();
}
