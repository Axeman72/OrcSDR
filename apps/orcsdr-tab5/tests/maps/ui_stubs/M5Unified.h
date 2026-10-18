#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
enum textdatum_t {top_left,middle_left,middle_center,middle_right};
enum {TFT_BLACK,TFT_WHITE,TFT_NAVY,TFT_LIGHTGREY,TFT_DARKGREY,TFT_MAROON,
      TFT_DARKCYAN,TFT_DARKGREEN,TFT_ORANGE,TFT_YELLOW};
inline size_t strlcpy(char* out,const char* in,size_t capacity) {
  const auto size=std::strlen(in);
  if(capacity) {const auto copied=std::min(size,capacity-1);std::memcpy(out,in,copied);out[copied]=0;}
  return size;
}
inline uint32_t millis() {return 1000;}
namespace lgfx {inline namespace v1 {
class LovyanGFX {
 public:
  std::vector<std::string> strings;
  template<class... T> void setFont(T...) {}
  template<class... T> void setTextDatum(T...) {}
  template<class... T> void setTextSize(T...) {}
  template<class... T> void setTextColor(T...) {}
  template<class... T> void fillScreen(T...) {strings.clear();}
  template<class... T> void fillRect(T...) {}
  template<class... T> void fillRoundRect(T...) {}
  template<class... T> void drawRoundRect(T...) {}
  template<class... T> void drawRect(T...) {}
  template<class... T> void drawLine(T...) {}
  template<class... T> void drawFastHLine(T...) {}
  void drawString(const char* value,int,int) {strings.emplace_back(value);}
};
}}
inline struct {lgfx::LovyanGFX Display;} M5;
