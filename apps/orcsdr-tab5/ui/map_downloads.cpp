#include "map_downloads.hpp"
#include "map_install.hpp"
#include "map_delivery_nvs.hpp"
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <cstdio>
#include <memory>

namespace orcsdr::maps::downloads {
namespace {
EXT_RAM_BSS_ATTR Snapshot g_snapshot;
portMUX_TYPE g_lock=portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> g_requested{false},g_busy{false},g_cancel{false};
uint64_t g_last_yield=0;
void publish(Phase phase,bool pending,const char* message) {
  portENTER_CRITICAL(&g_lock);
  g_snapshot.phase=phase;g_snapshot.pending=pending;g_snapshot.busy=g_busy.load();
  std::snprintf(g_snapshot.message,sizeof(g_snapshot.message),"%s",message);
  ++g_snapshot.revision;
  portEXIT_CRITICAL(&g_lock);
}
bool cancelled(void*) {
  const auto now=static_cast<uint64_t>(esp_timer_get_time());
  if(now-g_last_yield>=3000){vTaskDelay(1);g_last_yield=now;}
  return g_cancel.load();
}
}
void begin(){g_cancel.store(false);g_requested.store(true);}
bool snapshot(Snapshot* output) {
  if(!output)return false;
  portENTER_CRITICAL(&g_lock);*output=g_snapshot;portEXIT_CRITICAL(&g_lock);return true;
}
bool retry() {
  Snapshot current;snapshot(&current);
  if(g_busy.load()||!(current.phase==Phase::failed||current.phase==Phase::cancelled))return false;
  g_cancel.store(false);g_requested.store(true);return true;
}
void cancel(){g_cancel.store(true);}
bool service(storage::FileSystem* fs) {
  if(!g_requested.exchange(false))return false;
  g_busy.store(true);
  struct Busy {~Busy(){g_busy.store(false);}} busy;
  delivery::NvsStateStore store;delivery::State state;
  const auto loaded=delivery::load(store,&state);
  if(loaded!=delivery::Error::none) {
    g_busy.store(false);publish(Phase::failed,false,"Map download state unreadable; files retained.");
    std::printf("ORCMAPS_INSTALL state_error=%u\n",static_cast<unsigned>(loaded));return true;
  }
  if(!fs) {
    g_busy.store(false);publish(state.pending.product.empty()?Phase::idle:Phase::failed,
        !state.pending.product.empty(),state.pending.product.empty()?"":"Insert the original SD card and restart to finish installation.");return true;
  }
  const bool pending=!state.pending.product.empty();
  if(pending)publish(Phase::verifying,true,"Verifying saved map installation...");
  std::unique_ptr<uint8_t,decltype(&heap_caps_free)> buffer(nullptr,heap_caps_free);
  if(pending) {
    buffer.reset(static_cast<uint8_t*>(heap_caps_malloc(16384,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
    if(!buffer) {
      g_busy.store(false);publish(Phase::failed,true,"Not enough map memory; files retained. Rescan to retry.");return true;
    }
  }
  g_last_yield=static_cast<uint64_t>(esp_timer_get_time());
  const auto result=install::resume(*fs,store,buffer.get(),pending?16384:0,{nullptr,cancelled});
  g_busy.store(false);
  if(result.error==install::Error::none)publish(pending?Phase::complete:Phase::idle,false,
      pending?"Map installation recovered. Choose it from Local Maps.":"");
  else {
    const bool stopped=result.transfer==transfer::Error::cancelled||result.activation==activation::Error::cancelled;
    publish(stopped?Phase::cancelled:Phase::failed,pending,stopped?
        "Installation paused; Rescan to resume.":"Map recovery needs attention; files retained. Rescan to retry.");
    std::printf("ORCMAPS_INSTALL error=%u state=%u transfer=%u activation=%u\n",static_cast<unsigned>(result.error),
        static_cast<unsigned>(result.state),static_cast<unsigned>(result.transfer),static_cast<unsigned>(result.activation));
  }
  return true;
}
}
