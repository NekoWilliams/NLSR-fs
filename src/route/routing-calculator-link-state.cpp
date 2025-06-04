/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2024,  The University of Memphis,
 *                           Regents of the University of California,
 *                           Arizona Board of Regents.
 *
 * This file is part of NLSR (Named-data Link State Routing).
 * See AUTHORS.md for complete list of NLSR authors and contributors.
 *
 * NLSR is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NLSR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NLSR, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "routing-calculator.hpp"
#include "name-map.hpp"
#include "nexthop.hpp"

#include "adjacent.hpp"
#include "logger.hpp"
#include "nlsr.hpp"

#include <boost/multi_array.hpp>

namespace nlsr {
namespace {

INIT_LOGGER(route.RoutingCalculatorLinkState);

constexpr int EMPTY_PARENT = -12345;
constexpr double INF_DISTANCE = 2147483647;
constexpr int NO_NEXT_HOP = -12345;

/**
 * @brief Adjacency matrix.
 *
 * The matrix shall be a 2D array with N rows and N columns, where N is the number of routers.
 * Element i,j is the cost from router i to router j.
 */
using AdjMatrix = boost::multi_array<double, 2>;

struct PrintAdjMatrix
{
  const AdjMatrix& matrix;
  const NameMap& map;
};

/**
 * @brief Print adjacency matrix.
 */
std::ostream&
operator<<(std::ostream& os, const PrintAdjMatrix& p)
{
  size_t nRouters = p.map.size();

  os << "-----------Legend (routerName -> index)------\n";
  for (size_t i = 0; i < nRouters; ++i) {
    os << "Router:" << *p.map.getRouterNameByMappingNo(i)
       << " Index:" << i << "\n";
  }
  os << " |";
  for (size_t i = 0; i < nRouters; ++i) {
    os << i << " ";
  }
  os << "\n";
  os << "--";
  for (size_t i = 0; i < nRouters; ++i) {
    os << "--";
  }
  os << "\n";

  for (size_t i = 0; i < nRouters; i++) {
    os << i << "|";
    for (size_t j = 0; j < nRouters; j++) {
      double cost = p.matrix[i][j];
      if (cost == NO_NEXT_HOP) {
        os << "0 ";
      }
      else {
        os << cost << " ";
      }
    }
    os << "\n";
  }

  return os;
}

/**
 * @brief Allocate and populate adjacency matrix.
 *
 * The adjacency matrix is resized to match the number of routers in @p map .
 * Costs from Adjacency LSAs are filled into the matrix; in case of a mismatch in bidirectional
 * costs, the higher cost is assigned for both directions.
 * All other elements are set to @c NON_ADJACENT_COST .
 */
AdjMatrix
makeAdjMatrix(const Lsdb& lsdb, NameMap& map)
{
  // Create the matrix to have N rows and N columns, where N is number of routers.
  size_t nRouters = map.size();
  AdjMatrix matrix(boost::extents[nRouters][nRouters]);

  // Initialize all elements to NON_ADJACENT_COST.
  std::fill_n(matrix.origin(), matrix.num_elements(), Adjacent::NON_ADJACENT_COST);

  // For each LSA represented in the map
  auto lsaRange = lsdb.getLsdbIterator<AdjLsa>();
  for (auto lsaIt = lsaRange.first; lsaIt != lsaRange.second; ++lsaIt) {
    auto adjLsa = std::static_pointer_cast<AdjLsa>(*lsaIt);
    auto row = map.getMappingNoByRouterName(adjLsa->getOriginRouter());

    std::list<Adjacent> adl = adjLsa->getAdl().getAdjList();
    // For each adjacency represented in the LSA
    for (const auto& adjacent : adl) {
      auto col = map.getMappingNoByRouterName(adjacent.getName());
      double cost = adjacent.getLinkCost();

      if (row && col && *row < static_cast<int32_t>(nRouters)
          && *col < static_cast<int32_t>(nRouters)) {
        matrix[*row][*col] = cost;
      }
    }
  }

  // Links that do not have the same cost for both directions should
  // have their costs corrected:
  //
  //   If the cost of one side of the link is NON_ADJACENT_COST (i.e. broken) or negative,
  //   both direction of the link should have their cost corrected to NON_ADJACENT_COST.
  //
  //   Otherwise, both sides of the link should use the larger of the two costs.
  //
  // Additionally, this means that we can halve the amount of space
  // that the matrix uses by only maintaining a triangle.
  // - But that is not yet implemented.
  for (size_t row = 0; row < nRouters; ++row) {
    for (size_t col = 0; col < nRouters; ++col) {
      double& toCost = matrix[row][col];
      double& fromCost = matrix[col][row];

      if (fromCost == toCost) {
        continue;
      }

      // If both sides of the link are up, use the larger cost else break the link
      double correctedCost = Adjacent::NON_ADJACENT_COST;
      if (toCost >= 0 && fromCost >= 0) {
        correctedCost = std::max(toCost, fromCost);
      }

      NLSR_LOG_WARN("Cost between [" << row << "][" << col << "] and [" << col << "][" << row <<
                "] are not the same (" << toCost << " != " << fromCost << "). " <<
                "Correcting to cost: " << correctedCost);

      toCost = correctedCost;
      fromCost = correctedCost;
    }
  }

  return matrix;
}

void
sortQueueByDistance(std::vector<int>& q, const std::vector<PathCost>& costs, size_t start)
{
  for (size_t i = start; i < q.size(); ++i) {
    for (size_t j = i + 1; j < q.size(); ++j) {
      if (costs[q[j]].totalCost < costs[q[i]].totalCost) {
        std::swap(q[i], q[j]);
      }
    }
  }
}

bool
isNotExplored(std::vector<int>& q, int u, size_t start)
{
  for (size_t i = start; i < q.size(); i++) {
    if (q[i] == u) {
      return true;
    }
  }
  return false;
}

struct Link
{
  size_t index;
  double cost;
};

/**
 * @brief List adjacencies and link costs from a source router.
 */
std::vector<Link>
gatherLinks(const AdjMatrix& matrix, int sourceRouter)
{
  size_t nRouters = matrix.size();
  std::vector<Link> result;
  result.reserve(nRouters);
  for (size_t i = 0; i < nRouters; ++i) {
    if (i == static_cast<size_t>(sourceRouter)) {
      continue;
    }
    double cost = matrix[sourceRouter][i];
    if (cost >= 0.0) {
      result.emplace_back(Link{i, cost});
    }
  }
  return result;
}

/**
 * @brief Adjust link costs to simulate having only one accessible neighbor.
 */
void
simulateOneNeighbor(AdjMatrix& matrix, int sourceRouter, const Link& accessibleNeighbor)
{
  size_t nRouters = matrix.size();
  for (size_t i = 0; i < nRouters; ++i) {
    if (i == accessibleNeighbor.index) {
      matrix[sourceRouter][i] = accessibleNeighbor.cost;
    }
    else {
      // if "i" is not a link to the source, set its cost to a non adjacent value.
      matrix[sourceRouter][i] = Adjacent::NON_ADJACENT_COST;
    }
  }
}

struct PathCost {
  double linkCost;
  double functionCost;
  double totalCost;
  
  PathCost(double lc = INF_DISTANCE, double fc = INF_DISTANCE)
    : linkCost(lc)
    , functionCost(fc)
    , totalCost(lc + fc)
  {}
};

class DijkstraResult {
public:
  std::vector<int> parent;
  std::vector<PathCost> costs;

  explicit
  DijkstraResult(size_t size)
    : parent(size, EMPTY_PARENT)
    , costs(size, PathCost())
  {}

  DijkstraResult(std::vector<int> p, std::vector<PathCost> c)
    : parent(std::move(p))
    , costs(std::move(c))
  {}

  int
  getNextHop(int dest, int source) const
  {
    int u = dest;
    while (parent[u] != source) {
      if (parent[u] == EMPTY_PARENT) {
        return NO_NEXT_HOP;
      }
      u = parent[u];
    }
    return u;
  }
};

static PathCost
calculateCombinedCost(double linkCost, const ServiceFunctionInfo& sfInfo,
                     const ConfParameter& confParam)
{
  if (linkCost < 0) {
    return PathCost();  // INF_DISTANCEを返す
  }

  double functionCost = sfInfo.processingTime * confParam.getProcessingWeight() +
                       sfInfo.load * confParam.getLoadWeight() +
                       (sfInfo.usageCount / 100.0) * confParam.getUsageWeight();

  return PathCost(linkCost, functionCost);
}

/**
 * @brief Compute the shortest path from a source router to every other router.
 */
DijkstraResult
calculateDijkstraPath(const AdjMatrix& matrix, int sourceRouter)
{
  size_t nRouters = matrix.size();
  std::vector<int> parent(nRouters, EMPTY_PARENT);
  std::vector<PathCost> costs(nRouters, PathCost());
  std::vector<int> q(nRouters);

  // 初期化
  costs[sourceRouter] = PathCost(0, 0);  // 始点のコストを0に設定
  for (size_t i = 0; i < nRouters; ++i) {
    q[i] = i;
  }

  // メインループ
  size_t start = 0;
  while (start < nRouters) {
    sortQueueByDistance(q, costs, start);
    int u = q[start];
    if (costs[u].totalCost == INF_DISTANCE) {
      break;
    }

    for (size_t v = 0; v < nRouters; ++v) {
      if (matrix[u][v] >= 0 && isNotExplored(q, v, start + 1)) {
        PathCost newCost(matrix[u][v], 0);  // Service Function情報は後で追加
        if (newCost.totalCost + costs[u].totalCost < costs[v].totalCost) {
          costs[v] = PathCost(newCost.linkCost + costs[u].linkCost,
                             newCost.functionCost + costs[u].functionCost);
          parent[v] = u;
        }
      }
    }
    ++start;
  }

  return DijkstraResult(std::move(parent), std::move(costs));
}

/**
 * @brief Insert shortest paths into the routing table.
 */
void
addNextHopsToRoutingTable(RoutingTable& rt, const NameMap& map, int sourceRouter,
                         const AdjacencyList& adjacencies, const DijkstraResult& dr)
{
  size_t nRouters = map.size();

  for (size_t i = 0; i < nRouters; i++) {
    if (i == static_cast<size_t>(sourceRouter)) {
      continue;
    }

    int nextHopRouter = dr.getNextHop(i, sourceRouter);
    if (nextHopRouter == NO_NEXT_HOP) {
      continue;
    }

    double routeCost = dr.costs[i].totalCost;
    auto nextHopRouterName = map.getRouterNameByMappingNo(nextHopRouter);
    if (!nextHopRouterName) {
      continue;
    }

    auto destRouterName = map.getRouterNameByMappingNo(i);
    if (!destRouterName) {
      continue;
    }

    // Find the adjacent router
    auto adjIt = adjacencies.findAdjacent(*nextHopRouterName);
    if (adjIt == adjacencies.getAdjList().end()) {
      continue;
    }

    NextHop nh(adjIt->getFaceUri(), routeCost);
    rt.addNextHop(*destRouterName, nh);
  }
}

} // anonymous namespace

void
calculateLinkStateRoutingPath(NameMap& map, RoutingTable& rt, ConfParameter& confParam,
                              const Lsdb& lsdb)
{
  NLSR_LOG_DEBUG("calculateLinkStateRoutingPath called");

  auto sourceRouter = map.getMappingNoByRouterName(confParam.getRouterPrefix());
  if (!sourceRouter) {
    NLSR_LOG_DEBUG("Source router is absent, nothing to do");
    return;
  }

  AdjMatrix matrix = makeAdjMatrix(lsdb, map);
  NLSR_LOG_DEBUG((PrintAdjMatrix{matrix, map}));

  if (confParam.getMaxFacesPerPrefix() == 1) {
    // In the single path case we can simply run Dijkstra's algorithm.
    auto dr = calculateDijkstraPath(matrix, *sourceRouter);
    // Inform the routing table of the new next hops.
    addNextHopsToRoutingTable(rt, map, *sourceRouter, confParam.getAdjacencyList(), dr);
  }
  else {
    // Multi Path
    // Gets a sparse listing of adjacencies for path calculation
    auto links = gatherLinks(matrix, *sourceRouter);
    for (const auto& link : links) {
      // Simulate that only the current neighbor is accessible
      simulateOneNeighbor(matrix, *sourceRouter, link);
      NLSR_LOG_DEBUG((PrintAdjMatrix{matrix, map}));
      // Do Dijkstra's algorithm using the current neighbor as your start.
      auto dr = calculateDijkstraPath(matrix, *sourceRouter);
      // Update the routing table with the calculations.
      addNextHopsToRoutingTable(rt, map, *sourceRouter, confParam.getAdjacencyList(), dr);
    }
  }
}

} // namespace nlsr
