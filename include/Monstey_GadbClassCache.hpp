#pragma once
#include "Monstey_Core.hpp"
#include <memory>
namespace Monstey
{
// Immutable prepared native declarations, reusable for category-sized database pages.
class Monstey_GadbClassCache
{
  public:
    explicit Monstey_GadbClassCache(const Monstey_Json &reflection);
    ~Monstey_GadbClassCache();
    Monstey_GadbClassCache(const Monstey_GadbClassCache &) = delete;
    Monstey_GadbClassCache &operator=(const Monstey_GadbClassCache &) = delete;
    Monstey_Json Check(const Monstey_Json &layout, std::size_t diagnosticLimit = 256) const;

  private:
    struct Monstey_Impl;
    std::unique_ptr<const Monstey_Impl> impl_;
};
// Exact selected native binder/getter layout predicates, not a game execution proof.
// reflection: existing Monstey_FxReflectCategories categories/entries/source_sha256 shape.
// layout: final native category/record order with raw descriptor metadata; see docs/input.md.
// Every category and every supplied descriptor is visited. Diagnostics alone are bounded.
Monstey_Json Monstey_GadbClassCacheCheck(const Monstey_Json &reflection, const Monstey_Json &layout,
                                         std::size_t diagnosticLimit = 256);
// Conditional trace of explicit owned-input success/fallback helper calls only.
// No constructor, compaction, registry-order or live-state inference. Input is immutable.
Monstey_Json Monstey_GadbClassCacheTrace(const Monstey_Json &input,
                                       std::size_t stepLimit = 100000);
} // namespace Monstey
