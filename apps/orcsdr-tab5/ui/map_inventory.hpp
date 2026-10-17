#pragma once
#include "orcmap/pack_discovery.hpp"

namespace orcsdr::maps {
// This is separate from the signed download catalog. Discovery performs
// metadata/size pairing only; it must never imply a verified archive hash.
class Inventory {
 public:
  static constexpr size_t kPackLimit = 16;
  bool scan(orcmap::PackFileSystem&, const std::string& directory, bool sd_mounted);
  const orcmap::PackCatalog& catalog() const { return catalog_; }
  const orcmap::DiscoveryReport& report() const { return report_; }
  const orcmap::PackManifest* find(const char* identity) const;
  const orcmap::PackManifest* resolve(const orcmap::GeoBounds&, uint8_t zoom,
                                     const char* preferred = nullptr) const;
  bool sd_mounted() const { return sd_mounted_; }
  bool limit_exceeded() const { return limit_exceeded_; }
 private:
  orcmap::PackCatalog catalog_;
  orcmap::DiscoveryReport report_;
  bool sd_mounted_ = false, limit_exceeded_ = false;
};
} // namespace orcsdr::maps
