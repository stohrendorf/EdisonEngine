#pragma once

#include "engine/world/box.h"
#include "loader/file/datatypes.h"
#include "serialization/serialization_fwd.h"
#include "util/helpers.h"

#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace engine::world
{
class World;
}

namespace engine
{
namespace objects
{
struct ObjectState;
}

namespace ai
{
struct PathFinderNode
{
  const world::Box* next = nullptr;
  bool reachable = true;

  void serialize(const serialization::Serializer<world::World>& ser);
  static PathFinderNode create(const serialization::Serializer<world::World>& ser);
};

struct PathFinder
{
  bool cannotVisitBlocked = true;
  bool cannotVisitBlockable = false;

  [[nodiscard]] bool canVisit(const world::Box& box) const noexcept
  {
    if(cannotVisitBlocked && box.blocked)
      return false;
    if(cannotVisitBlockable && box.blockable)
      return false;
    return true;
  }

  //! @brief Movement limits
  //! @{
  core::Length step = core::QuarterSectorSize;
  core::Length drop = -core::QuarterSectorSize;
  core::Length fly = 0_len;
  //! @}

  //! @brief The target box we need to reach
  const world::Box* target_box = nullptr;
  const world::Box* required_box = nullptr;

  core::TRVec target;

  explicit PathFinder(const world::World& world);

  void setRandomSearchTarget(const gsl::not_null<const world::Box*>& box)
  {
    required_box = box;
    const auto zSize = box->zmax - box->zmin - core::SectorSize;
    target.Z = util::rand15(zSize) + box->zmin + core::SectorSize / 2;
    const auto xSize = box->xmax - box->xmin - core::SectorSize;
    target.X = util::rand15(xSize) + box->xmin + core::SectorSize / 2;
    if(fly != 0_len)
    {
      target.Y = box->floor - 384_len;
    }
    else
    {
      target.Y = box->floor;
    }
  }

  bool calculateTarget(const world::World& world, core::TRVec& moveTarget, const objects::ObjectState& objectState);

  /**
     * @brief Incrementally calculate all paths to a specific box.
     *
     * @details
     * The algorithm performs a greedy breadth-first search, searching for all paths that lead to
     * #required_box.  While searching, @arg maxDepth limits the number of nodes expanded, so it may take multiple
     * calls to actually calculate the full path.  Until a full path is found, the nodes partially retain the old
     * paths from a previous search.
     */
  void updatePath(const world::World& world);

  void searchPath(const world::World& world);

  void serialize(const serialization::Serializer<world::World>& ser);

  void collectBoxes(const world::World& world, const gsl::not_null<const world::Box*>& box);

  // returns true if and only if the box is visited and marked unreachable
  [[nodiscard]] bool isUnreachable(const gsl::not_null<const world::Box*>& box) const
  {
    return m_visited.count(box) != 0 && !m_nodes.at(box).reachable;
  }

  [[nodiscard]] const auto& getRandomBox() const
  {
    Expects(!m_boxes.empty());
    return m_boxes[util::rand15(m_boxes.size())];
  }

  [[nodiscard]] const auto& getNextPathBox(const gsl::not_null<const world::Box*>& box) const
  {
    return m_nodes.at(box).next;
  }

private:
  std::unordered_map<const world::Box*, PathFinderNode> m_nodes;
  std::vector<gsl::not_null<const world::Box*>> m_boxes;
  std::deque<const world::Box*> m_expansions;
  //! Contains all boxes where the "traversable" state has been determined
  std::unordered_set<const world::Box*> m_visited;
};
} // namespace ai
} // namespace engine
