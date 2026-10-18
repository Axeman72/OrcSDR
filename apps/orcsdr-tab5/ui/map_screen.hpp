#pragma once
#include "nvs_store.hpp"
#include "keyboard_input.hpp"
#include "screen_controller.hpp"

namespace orcsdr::map_screen {
enum class Mode { picker, inventory, setup };
struct Hooks {
  void (*restore)(screens::Id) = nullptr;
  bool (*location)(int32_t latitude_e7,int32_t longitude_e7) = nullptr;
  bool (*pack)(const char* identity) = nullptr;
  void (*location_method)(int method) = nullptr;
};
// Call immediately after opening preferences, before legacy defaults are saved.
void load(NvsStore&);
bool first_run_pending();
void configure(const Hooks&);
void open(Mode,bool automatic = false);
void draw();
void tick();
void touch(int x,int y,bool pressed,bool new_press);
void key(const keyboard_input::Key&);
} // namespace orcsdr::map_screen
