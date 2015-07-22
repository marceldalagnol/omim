#pragma once

#include "routing/road_graph.hpp"
#include "routing/route.hpp"

namespace routing
{

class IDirectionsEngine
{
public:
  virtual ~IDirectionsEngine() = default;

  virtual void Generate(IRoadGraph const & graph, vector<Junction> const & path,
                        Route::TTimes & times,
                        Route::TTurns & turnsDir,
                        turns::TTurnsGeom & turnsGeom) = 0;
};

}  // namespace routing
