"""Fault-inject real map preview publication/reclaim; no device or archive access."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'apps/orcsdr-tab5/ui/map_service.cpp').read_text(encoding='utf-8')
def extract(signature):
    start=source.index(signature)
    pos=source.index('{',start)+1
    depth=1
    while depth:
        depth+=(source[pos]=='{')-(source[pos]=='}');pos+=1
    return source[start:pos]

harness=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,pdTRUE=1,portMAX_DELAY=-1;
constexpr size_t kPreviewStripePixels=1280*16;
namespace lgfx {constexpr int rgb565_nonswapped=272;}
void* g_lock=reinterpret_cast<void*>(1);
void* g_preview_lock=reinterpret_cast<void*>(2);
bool worker_available=true,preview_available=true,allocation_fails=false;
int releases=0,allocations=0,frees=0,pushes=0;
size_t largest=4*1024*1024;
uint16_t storage[kPreviewStripePixels];
int xSemaphoreTake(void* handle,int) {return (handle==g_lock?worker_available:preview_available)?pdTRUE:0;}
void xSemaphoreGive(void*){++releases;}
size_t heap_caps_get_largest_free_block(int caps){assert(caps==MALLOC_CAP_SPIRAM);return largest;}
void* heap_caps_malloc(size_t bytes,int caps) {
  assert(bytes==sizeof(storage)&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  ++allocations;return allocation_fails?nullptr:storage;
}
void heap_caps_free(void* data){if(data){assert(data==storage);++frees;}}
struct Canvas {
  int w=0,h=0;
  int width(){return w;} int height(){return h;}
  void setPsram(bool value){assert(value);}
  void setColorDepth(int value){assert(value==lgfx::rgb565_nonswapped);}
  void deleteSprite(){w=h=0;}
  void* createSprite(int width,int height){w=width;h=height;return this;}
  void pushSprite(Canvas*,int,int){++pushes;}
} g_canvas,g_preview;
struct Request {struct {int width_px=842,height_px=330;} view;} g_preview_request;
int g_preview_info=0,g_frame=17;
bool g_preview_valid=false,g_cached=true;
uint16_t* g_preview_stripe=nullptr;
std::atomic<size_t> g_canvas_bytes{555720},g_preview_bytes{0},g_preview_stripe_bytes{0};
std::atomic<uint32_t> g_preview_lifetime{0},g_serial{0};
std::unique_ptr<int> g_pipeline(new int),g_source(new int);
char g_archive[8]="loaded";
'''
harness+='\n'.join(extract(s) for s in ('void publish_preview(const Request& request)', 'size_t held()', 'size_t release()'))
harness+=r'''
int main() {
  Request request;
  allocation_fails=true;publish_preview(request);
  assert(g_preview_valid&&g_preview_info==17&&pushes==1);
  assert(!g_preview_stripe&&g_preview_stripe_bytes==0&&held()==1111440);
  allocation_fails=false;publish_preview(request);
  assert(g_preview_valid&&g_preview_stripe&&g_preview_stripe_bytes==40960);
  assert(held()==1152400&&allocations==2);
  publish_preview(request);assert(allocations==2);
  worker_available=false;assert(release()==0&&frees==0&&held()==1152400);
  worker_available=true;preview_available=false;
  const int before=releases;assert(release()==0&&releases==before+1&&held()==1152400);
  preview_available=true;
  assert(release()==1152400&&frees==1&&held()==0&&!g_preview_stripe);
  assert(!g_preview_valid&&!g_cached&&!g_pipeline&&!g_source&&!g_archive[0]);
  assert(g_preview_lifetime==1&&g_serial==1);
  assert(release()==0&&frees==1);
  publish_preview(request);assert(g_preview_valid&&g_preview_stripe&&allocations==3);
  assert(release()==596680&&frees==2&&held()==0);
  largest=128*1024;publish_preview(request);
  assert(!g_preview_valid&&!g_preview_stripe&&allocations==3&&held()==0);
  std::puts("MAP_PREVIEW_MEMORY_PASS allocation failure preserves preview, retry, accounting, locks, reclaim and low-memory fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='orcsdr-preview-memory-') as temp:
    cpp=Path(temp)/'test.cpp';exe=Path(temp)/'test'
    cpp.write_text(harness,encoding='utf-8')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
