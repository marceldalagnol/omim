#pragma once

#include "drape_frontend/read_metaline_task.hpp"

#include "drape/object_pool.hpp"
#include "drape/pointers.hpp"

#include "indexer/feature_decl.hpp"

#include <mutex>
#include <vector>
#include <utility>

namespace df
{
struct MetalineData
{
  std::vector<FeatureID> m_features;
  std::vector<bool> m_directions;
};

using MetalineModel = std::vector<MetalineData>;

class ThreadsCommutator;

class MetalineManager
{
public:
  MetalineManager(ref_ptr<ThreadsCommutator> commutator, MapDataProvider & model);
  ~MetalineManager();

  MetalineCache GetMetalines(std::vector<FeatureID> const & features) const;

private:
  void OnTaskFinished(threads::IRoutine * task);

  using TasksPool = ObjectPool<ReadMetalineTask, ReadMetalineTaskFactory>;

  MapDataProvider & m_model;

  mutable std::mutex m_metalineCacheMutex;
  MetalineCache m_metalineCache;

  TasksPool m_tasksPool;
  drape_ptr<threads::ThreadPool> m_threadsPool;
  ref_ptr<ThreadsCommutator> m_commutator;
};
}  // namespace df
