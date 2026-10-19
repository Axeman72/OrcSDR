#include "map_catalog.hpp"
#include <cJSON.h>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <string>

std::string load(const char* path) {
  std::ifstream file(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int main(int argc,char** argv) {
  if(argc!=11)return 2;
  const auto payload=load(argv[1]),signature=load(argv[2]),key=load(argv[3]);
  namespace c=orcsdr::maps::catalog;
  c::Policy policy{};
  policy.utc_now=std::stoull(argv[5]);policy.accepted_revision=std::stoull(argv[6]);
  policy.accepted_sha256=std::string_view(argv[7])=="-"?std::string_view{}:argv[7];
  policy.clock_trusted=std::string_view(argv[8])=="1";
  c::Catalog output;output.revision=999;
  output.packs.resize(1);output.packs[0].manifest_json="unchanged";
  if(std::string_view(argv[10])=="1") {
    cJSON_Hooks hooks{};hooks.malloc_fn=[](size_t)->void*{return nullptr;};
    hooks.free_fn=std::free;cJSON_InitHooks(&hooks);
  }
  const auto error=c::read_signed(payload,signature,key,policy,&output);
  if(std::string_view(c::error_name(error))!=argv[4]) {
    std::printf("expected %s, got %s\n",argv[4],c::error_name(error));return 1;
  }
  if(error!=c::Error::none) {
    if(output.revision!=999||output.packs.size()!=1||output.packs[0].manifest_json!="unchanged")return 3;
  } else if(output.revision!=1||output.packs.size()!=std::stoull(argv[9])||output.sha256.size()!=64)return 4;
  return 0;
}
