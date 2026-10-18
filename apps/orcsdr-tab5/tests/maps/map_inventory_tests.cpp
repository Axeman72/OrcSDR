#include "map_inventory.hpp"
#include "orcmap/pack_json.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <new>

namespace {
struct Files final : orcmap::PackFileSystem {
  std::map<std::string,std::string> manifests;
  std::map<std::string,uint64_t> archives;
  bool listed=true, fail=false;
  bool ListManifestNames(const std::string&,std::vector<std::string>* out) override {
    out->clear();for(const auto& item:manifests)out->push_back(item.first);return listed;
  }
  bool ReadTextFile(const std::string& path,size_t limit,std::string* out) override {
    if(fail)throw std::bad_alloc();
    auto it=manifests.find(path.substr(path.find_last_of('/')+1));
    if(it==manifests.end() || it->second.size()>limit)return false;
    *out=it->second;return true;
  }
  bool FileSize(const std::string& path,uint64_t* size) override {
    auto it=archives.find(path);if(it==archives.end())return false;
    *size=it->second;return true;
  }
};
void replace(std::string& text,const std::string& old,const std::string& value) {
  auto at=text.find(old);assert(at!=std::string::npos);text.replace(at,old.size(),value);
}
}
void inventory_tests(const char* path) {
  std::ifstream input(path);assert(input);
  std::string original((std::istreambuf_iterator<char>(input)),{});
  orcmap::PackManifest base;
  assert(orcmap::ParsePackManifestJson(original,&base)==orcmap::PackJsonError::kNone);
  using orcsdr::maps::Inventory;
  Inventory inventory;
  Files files;
  assert(!inventory.scan(files,"/maps",false) && !inventory.sd_mounted());
  for (size_t count:{0u,1u,5u,16u}) {
    files={};
    for(size_t i=0;i<count;++i) {
      auto pack=base;
      pack.region_id="test-region-"+std::to_string(i);
      std::string json=original;
      replace(json,base.pack_id,orcmap::MakePackId(pack));
      replace(json,"\"region_id\": \""+base.region_id+"\"","\"region_id\": \""+pack.region_id+"\"");
      const auto name="test"+std::to_string(i);
      files.manifests[name+".manifest.json"]=json;
      files.archives["/maps/"+name+".pmtiles"]=base.size_bytes;
    }
    assert(inventory.scan(files,"/maps",true));
    assert(inventory.catalog().Packs().size()==count);
    if(count) {
      auto& pack=inventory.catalog().Packs()[0];
      assert(inventory.find(pack.pack_id.c_str())==&pack);
      assert(inventory.resolve(pack.bounds,pack.min_zoom,pack.pack_id.c_str())==&pack);
    }
  }
  files.manifests["overflow.manifest.json"]=original;
  assert(!inventory.scan(files,"/maps",true) && inventory.limit_exceeded());
  assert(inventory.catalog().Packs().empty());
  files={};files.manifests["broken.manifest.json"]="{";
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected.size()==1);
  files={};files.manifests["missing.manifest.json"]=original;
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected[0].rejection==orcmap::PackRejection::kArchiveMissing);
  files.archives["/maps/missing.pmtiles"]=1;
  assert(!inventory.scan(files,"/maps",true));
  assert(inventory.report().rejected[0].rejection==orcmap::PackRejection::kArchiveSizeMismatch);
  files.listed=false;assert(!inventory.scan(files,"/maps",true));
  assert(!inventory.report().directory_listed);
  files.listed=true;files.archives["/maps/missing.pmtiles"]=base.size_bytes;files.fail=true;
  assert(!inventory.scan(files,"/maps",true)&&inventory.out_of_memory());
  assert(inventory.catalog().Packs().empty());
  files.fail=false;assert(inventory.scan(files,"/maps",true));
  assert(!inventory.out_of_memory()&&inventory.catalog().Packs().size()==1);
}
