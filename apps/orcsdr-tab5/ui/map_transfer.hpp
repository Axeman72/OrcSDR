#pragma once
#include "map_catalog.hpp"
#include "map_download_response.hpp"
#include "orcsdr_storage.hpp"

namespace orcsdr::maps::transfer {
enum class Error {none,input,space,resume,network,response,cancelled,storage,hash,archive,allocation};
struct VerificationProfile {
  // bytes/blocks count successfully hashed data. read_us measures block reads;
  // hash_us measures block SHA updates. elapsed_us also includes other work,
  // including cancellation/yields and PMTiles validation (before RAII cleanup).
  uint64_t bytes=0,blocks=0,read_us=0,hash_us=0,elapsed_us=0;
  Error result=Error::none;
};
struct Headers {
  int status=0;bool has_length=false;uint64_t length=0;
  std::string range,etag,encoding;
};
class Stream {
 public:
  virtual ~Stream()=default;
  virtual bool open(std::string_view url,const download::Request&,Headers&)=0;
  virtual int read(uint8_t*,size_t)=0;
  virtual void close()=0; // Must be idempotent, including after failed open.
};
struct Hooks {
  void* context=nullptr;
  bool (*cancelled)(void*)=nullptr;
  void (*progress)(void*,uint64_t,uint64_t)=nullptr;
  void (*verifying)(void*)=nullptr; // Worker callback; never draw UI here.
  // Per full-file check: starts at zero, advances after hashing each block.
  void (*verify_progress)(void*,uint64_t,uint64_t)=nullptr;
  // One observation per normally returned file check, including cancellation
  // and failure. Allocation exceptions retain the caller's existing handling.
  // Durations include scheduling/preemption; they are not isolated CPU costs.
  void (*verify_profile)(void*,const VerificationProfile&)=nullptr;
};
struct Paths {std::string directory,partial,archive,manifest,record[2];};
Paths paths(const catalog::Pack&);
// Synchronous worker-only operation; caller serializes jobs/owns FS lifetime.
// Requires a pack from read_signed. Uses a caller-owned 4–64 KiB buffer
// (allocate in PSRAM on P4). This initial path accepts archives up to 600 MiB.
// Success prepares a hidden directory, never publishes into map discovery.
// Activation/version retirement is a separate transaction. No existing map,
// location, NVS setting or catalog revision is changed here.
Error stage(storage::FileSystem&,const catalog::Pack&,size_t mirror,Stream&,
            uint64_t free_bytes,uint8_t* buffer,size_t buffer_bytes,const Hooks&) noexcept;
// Read-only full hash and actual PMTiles checks for activation/revalidation.
Error verify_archive(storage::FileSystem&,const catalog::Pack&,const std::string& path,
                     uint8_t*,size_t,const Hooks&) noexcept;
} // namespace orcsdr::maps::transfer
