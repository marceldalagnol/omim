#pragma once

#include "drape/pointers.hpp"

#include "indexer/feature_decl.hpp"

#include "geometry/spline.hpp"

#include "base/thread.hpp"
#include "base/thread_pool.hpp"

#include <map>

namespace df
{

class MapDataProvider;
class ThreadsCommutator;

using MetalineCache = std::map<FeatureID, m2::SharedSpline>;

class ReadMetalineTask : public threads::IRoutine
{
public:
  ReadMetalineTask(MapDataProvider & model);

  void Do() override;

  void Init(ref_ptr<ThreadsCommutator> commutator);
  void Reset() override;
  bool IsCancelled() const override;

  MetalineCache const & GetCache() const { return m_cache; }

private:
  MapDataProvider & m_model;
  ref_ptr<ThreadsCommutator> m_commutator;
  MetalineCache m_cache;
};

class ReadMetalineTaskFactory
{
public:
  ReadMetalineTaskFactory(MapDataProvider & model)
    : m_model(model) {}

  ReadMetalineTask * GetNew() const;

private:
  MapDataProvider & m_model;
};

}  // namespace df
