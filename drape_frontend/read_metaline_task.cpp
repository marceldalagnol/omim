#include "drape_frontend/read_metaline_task.hpp"

#include "drape_frontend/drape_api.hpp"
#include "drape_frontend/map_data_provider.hpp"
#include "drape_frontend/message_subclasses.hpp"
#include "drape_frontend/metaline_manager.hpp"
#include "drape_frontend/threads_commutator.hpp"

#include "coding/file_container.hpp"
#include "coding/reader_wrapper.hpp"
#include "coding/varint.hpp"

#include "indexer/feature_decl.hpp"
#include "indexer/scales.hpp"

#include "defines.hpp"

#include <algorithm>

namespace
{
// Temporary!
df::MetalineModel ReadMetalinesFromFile(MwmSet::MwmId const & mwmId)
{
  ModelReaderPtr reader = FilesContainerR(mwmId.GetInfo()->GetLocalFile().GetPath(MapOptions::Map))
                                          .GetReader(METALINES_FILE_TAG);
  ReaderSrc src(reader.GetPtr());
  uint8_t const version = ReadPrimitiveFromSource<uint8_t>(src);
  CHECK_EQUAL(version, 1, ());
  df::MetalineModel model;
  for (auto metalineIndex = ReadVarUint<uint32_t>(src); metalineIndex > 0; --metalineIndex)
  {
    df::MetalineData data;
    for (auto i = ReadVarUint<uint32_t>(src); i > 0; --i)
    {
      int32_t const fid = ReadVarInt<int32_t>(src);
      data.m_features.push_back(FeatureID(mwmId, std::abs(fid)));
      data.m_directions.push_back(fid > 0);
    }
    model.push_back(std::move(data));
  }
  return model;
}

vector<dp::Color> colorList = { dp::Color(255, 0, 0, 255), dp::Color(0, 255, 0, 255), dp::Color(0, 0, 255, 255),
                                dp::Color(255, 255, 0, 255), dp::Color(0, 255, 255, 255), dp::Color(255, 0, 255, 255),
                                dp::Color(100, 0, 0, 255), dp::Color(0, 100, 0, 255), dp::Color(0, 0, 100, 255),
                                dp::Color(100, 100, 0, 255), dp::Color(0, 100, 100, 255), dp::Color(100, 0, 100, 255)
};

double const kPointEqualityEps = 1e-7;

std::vector<m2::PointD> MergePoints(std::vector<std::vector<m2::PointD>> const & points)
{
  size_t sz = 0;
  for (auto const & p : points)
    sz += p.size();

  std::vector<m2::PointD> result;
  result.reserve(sz);
  for (size_t i = 0; i < points.size(); i++)
  {
    for (auto const & pt : points[i])
    {
      if (result.empty() || !result.back().EqualDxDy(pt, kPointEqualityEps))
        result.push_back(pt);
    }
  }
  return result;
}
}  // namespace

namespace df
{
ReadMetalineTask::ReadMetalineTask(MapDataProvider & model)
  : m_model(model)
{}

void ReadMetalineTask::Init(ref_ptr<ThreadsCommutator> commutator)
{
  m_commutator = commutator;
}

void ReadMetalineTask::Reset()
{
}

bool ReadMetalineTask::IsCancelled() const
{
  return IRoutine::IsCancelled();
}

void ReadMetalineTask::Do()
{
  // Temporary! Load metaline data from file.
  MwmSet::MwmId mwmId;
  m_model.ReadFeaturesID([&mwmId](FeatureID const & fid)
  {
    if (!mwmId.IsAlive())
      mwmId = fid.m_mwmId;
  }, m2::RectD(m2::PointD(37.5380324, 67.5384520), m2::PointD(37.5380325, 67.5384521)), 17);
  if (!mwmId.IsAlive() || mwmId.GetInfo()->GetCountryName() != "Russia_Moscow")
    return;
  auto metalines = ReadMetalinesFromFile(mwmId);

  for (auto const & metaline : metalines)
  {
    bool failed = false;
    for (auto const & fid : metaline.m_features)
    {
      if (m_cache.find(fid) != m_cache.end())
      {
        failed = true;
        break;
      }
    }
    if (failed)
      continue;

    size_t curIndex = 0;
    std::vector<std::vector<m2::PointD>> points;
    points.reserve(5);
    m_model.ReadFeatures([&metaline, &failed, &curIndex, &points](FeatureType const & ft)
    {
      if (failed)
        return;
      if (ft.GetID() != metaline.m_features[curIndex])
      {
        failed = true;
        return;
      }
      std::vector<m2::PointD> featurePoints;
      featurePoints.reserve(5);
      ft.ForEachPoint([&featurePoints](m2::PointD const & pt)
      {
        if (!featurePoints.back().EqualDxDy(pt, kPointEqualityEps))
          featurePoints.push_back(pt);
      }, scales::GetUpperScale());
      if (featurePoints.size() < 2)
      {
        failed = true;
        return;
      }
      if (!metaline.m_directions[curIndex])
        std::reverse(featurePoints.begin(), featurePoints.end());

      points.push_back(std::move(featurePoints));
      curIndex++;
    }, metaline.m_features);

    if (failed || points.empty())
      continue;

    std::vector<m2::PointD> mergedPoints = MergePoints(points);
    if (mergedPoints.empty())
      continue;

//    if (!metaline.m_features.empty())
//    {
//      DrapeApi::TLines lines;
//      size_t const cc = colorIndex++ % colorList.size();
//      lines.insert(std::make_pair(DebugPrint(metaline.m_features.front()),
//                                  df::DrapeApiLineData(mergedPoints, colorList[cc]).Width(3.0f).ShowPoints(true)));
//      m_commutator->PostMessage(ThreadsCommutator::ResourceUploadThread,
//                                make_unique_dp<DrapeApiAddLinesMessage>(lines),
//                                MessagePriority::Normal);
//    }

    m2::SharedSpline spline(mergedPoints);
    for (auto const & fid : metaline.m_features)
      m_cache[fid] = spline;
  }
}

ReadMetalineTask * ReadMetalineTaskFactory::GetNew() const
{
  return new ReadMetalineTask(m_model);
}
}  // namespace df
