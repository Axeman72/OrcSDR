#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>
#if !defined(__GLIBC__) || (__GLIBC__*100+__GLIBC_MINOR__<238)
inline size_t strlcpy(char* out,const char* in,size_t capacity) {
  const size_t size=std::strlen(in);
  if(capacity){const auto count=std::min(size,capacity-1);std::memcpy(out,in,count);out[count]=0;}
  return size;
}
#endif
using textdatum_t = int;
constexpr int middle_center = 0, middle_left = 1, middle_right = 2, top_left = 3, bottom_left = 4;
constexpr uint16_t TFT_WHITE = 0xffff, TFT_BLACK = 0, TFT_RED = 0xf800, TFT_DARKGREY = 0x7bef;
namespace fonts { inline int DejaVu24, DejaVu18; }
namespace lgfx {
inline namespace v1 {
struct rgb565_t {uint16_t raw;};
struct Paint { std::string text; int x, y; uint16_t color; int text_size=1; };
inline std::vector<Paint> paints;
inline bool sprite_fail=false;
inline int sampled_pixels=0, drawn_pixels=0;
inline int screen_clears=0;
class LovyanGFX {
  uint16_t color_ = 0;
  int text_size_ = 1;
public:
  template<class... T> void setTextDatum(T...) {}
  void setTextSize(int size) { text_size_=size; }
  template<class... T> void setFont(T...) {}
  void setTextColor(uint16_t c, uint16_t = 0) { color_ = c; }
  template<class... T> void fillRoundRect(T...) {}
  template<class... T> void drawRoundRect(T...) {}
  template<class... T> void fillRect(T...) {}
  template<class... T> void drawRect(T...) {}
  template<class... T> void fillScreen(T...) {++screen_clears;}
  template<class... T> void fillCircle(T...) {}
  template<class... T> void drawPixel(T...) { ++drawn_pixels; }
  template<class... T> void drawCircle(T...) {}
  template<class... T> void drawFastVLine(T...) {}
  template<class... T> void drawFastHLine(T...) {}
  template<class... T> void drawLine(T...) {}
  template<class... T> void drawArc(T...) {}
  template<class... T> void fillTriangle(T...) {}
  template<class... T> void pushImage(T...) {}
  template<class... T> void setScrollRect(T...) {}
  template<class... T> void scroll(T...) {}
  template<class... T> void setClipRect(T...) {}
  void clearClipRect() {}
  void clearScrollRect() {}
  void startWrite() {}
  void endWrite() {}
  void drawString(const char* s, int x, int y) { paints.push_back({s, x, y, color_, text_size_}); }
  int textWidth(const char* s) { return static_cast<int>(std::strlen(s)) * 12; }
};
}
}
class M5Canvas : public lgfx::LovyanGFX {
public:
  explicit M5Canvas(lgfx::LovyanGFX*) {}
  template<class... T> void setPsram(T...) {}
  template<class... T> void setColorDepth(T...) {}
  template<class... T> void fillSprite(T...) {}
  template<class... T> void pushSprite(T...) {}
  void deleteSprite() {}
  uint32_t readPixel(int,int) {++lgfx::sampled_pixels;return 0x1234;}
  void* createSprite(int, int) { return lgfx::sprite_fail ? nullptr : this; }
};
inline struct M5Stub {
  lgfx::LovyanGFX Display;
  struct {int getBatteryLevel(){return 100;}} Power;
} M5;
inline uint32_t test_millis=1000;
inline uint32_t millis() { return test_millis; }
