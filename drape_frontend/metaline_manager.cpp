#include "drape_frontend/metaline_manager.hpp"

#include "drape_frontend/map_data_provider.hpp"
#include "drape_frontend/threads_commutator.hpp"

#include <functional>
#include <unordered_set>

namespace df
{

MetalineManager::MetalineManager(ref_ptr<ThreadsCommutator> commutator, MapDataProvider & model)
  : m_model(model)
  , m_tasksPool(4, ReadMetalineTaskFactory(m_model))
  , m_threadsPool(make_unique_dp<threads::ThreadPool>(2, std::bind(&MetalineManager::OnTaskFinished,
                                                                   this, std::placeholders::_1)))
  , m_commutator(commutator)
{
  ReadMetalineTask * task = m_tasksPool.Get();
  task->Init(m_commutator);
  m_threadsPool->PushBack(task);
}

MetalineManager::~MetalineManager()
{
  m_threadsPool->Stop();
  m_threadsPool.reset();
}

MetalineCache MetalineManager::GetMetalines(std::vector<FeatureID> const & features) const
{
  std::lock_guard<std::mutex> lock(m_metalineCacheMutex);
  MetalineCache result;
  std::unordered_set<m2::Spline const *> splines;
  for (FeatureID const & fid : features)
  {
    auto const metalineIt = m_metalineCache.find(fid);
    if (metalineIt == m_metalineCache.end())
      continue;

    auto const it = splines.find(metalineIt->second.Get());
    if (it == splines.end())
    {
      result.insert(*metalineIt);
      splines.insert(metalineIt->second.Get());
    }
    else
    {
      // Mark by means of empty shared spline.
      result.insert(std::make_pair(metalineIt->first, m2::SharedSpline()));
    }
  }
  return result;
}

void MetalineManager::OnTaskFinished(threads::IRoutine * task)
{
  ASSERT(dynamic_cast<ReadMetalineTask *>(task) != nullptr, ());
  ReadMetalineTask * t = static_cast<ReadMetalineTask *>(task);

  // Update metaline cache.
  {
    std::lock_guard<std::mutex> lock(m_metalineCacheMutex);
    for (auto const & metaline : t->GetCache())
      m_metalineCache[metaline.first] = metaline.second;
  }

  t->Reset();
  m_tasksPool.Return(t);
}
}  // namespace df
