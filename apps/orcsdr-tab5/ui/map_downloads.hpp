#pragma once
#include <cstdint>
namespace orcsdr::storage {class FileSystem;}
namespace orcsdr::maps::downloads {
enum class Phase:uint8_t {idle,verifying,complete,cancelled,failed};
struct Snapshot {
  Phase phase=Phase::idle;
  bool busy=false,pending=false;
  uint32_t revision=0;
  char message[96]{};
};
void begin();
bool snapshot(Snapshot*);
bool retry();
void cancel();
// Called only by the existing map worker, without its rendering mutex held.
// Returns true after a recovery attempt so its caller can refresh inventory.
bool service(storage::FileSystem*);
}
