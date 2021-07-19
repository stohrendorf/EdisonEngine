#pragma once

#include "modelobject.h"

namespace engine::objects
{
class SwingingBlade final : public ModelObject
{
public:
  SwingingBlade(const gsl::not_null<world::World*>& world, const RoomBoundPosition& location)
      : ModelObject{world, location}
  {
  }

  SwingingBlade(const gsl::not_null<world::World*>& world,
                const gsl::not_null<const world::Room*>& room,
                const loader::file::Item& item,
                const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
      : ModelObject{world, room, item, true, animatedModel}
  {
  }

  void update() override;

  void collide(CollisionInfo& collisionInfo) override;
};
} // namespace engine::objects
