#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"

namespace engine::lara
{
class StateHandler_39 final : public AbstractStateHandler
{
public:
  explicit StateHandler_39(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::PickUp}
  {
  }

  void handleInput(CollisionInfo& collisionInfo) override
  {
    collisionInfo.policies &= ~CollisionInfo::SpazPushPolicy;
    setCameraRotationAroundLara(-15_deg, -130_deg);
    setCameraDistance(1024_len);
  }

  void postprocessFrame(CollisionInfo& collisionInfo) override
  {
    collisionInfo.facingAngle = getLara().m_state.rotation.Y;
    setMovementAngle(collisionInfo.facingAngle);
    collisionInfo.floorCollisionRangeMin = core::ClimbLimit2ClickMin;
    collisionInfo.floorCollisionRangeMax = -core::ClimbLimit2ClickMin;
    collisionInfo.ceilingCollisionRangeMin = 0_len;
    collisionInfo.policies |= CollisionInfo::SlopeBlockingPolicy;
    collisionInfo.initHeightInfo(getLara().m_state.location.position, getWorld(), core::LaraWalkHeight);
  }
};
} // namespace engine::lara
