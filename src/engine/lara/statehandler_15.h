#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "hid/inputstate.h"

namespace engine::lara
{
class StateHandler_15 final : public AbstractStateHandler
{
public:
  explicit StateHandler_15(objects::LaraObject& lara)
      : AbstractStateHandler{lara, LaraStateId::JumpPrepare}
  {
  }

  void handleInput(CollisionInfo& /*collisionInfo*/) override
  {
    if(getWorld().getPresenter().getInputHandler().getInputState().zMovement == hid::AxisMovement::Forward
       && getRelativeHeightAtDirection(getLara().m_state.rotation.Y, 256_len) >= -core::ClimbLimit2ClickMin)
    {
      setMovementAngle(getLara().m_state.rotation.Y);
      setGoalAnimState(LaraStateId::JumpForward);
    }
    else
    {
      if(getWorld().getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Left
         && getRelativeHeightAtDirection(getLara().m_state.rotation.Y - 90_deg, 256_len) >= -core::ClimbLimit2ClickMin)
      {
        setMovementAngle(getLara().m_state.rotation.Y - 90_deg);
        setGoalAnimState(LaraStateId::JumpRight);
      }
      else if(getWorld().getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Right
              && getRelativeHeightAtDirection(getLara().m_state.rotation.Y + 90_deg, 256_len)
                   >= -core::ClimbLimit2ClickMin)
      {
        setMovementAngle(getLara().m_state.rotation.Y + 90_deg);
        setGoalAnimState(LaraStateId::JumpLeft);
      }
      else if(getWorld().getPresenter().getInputHandler().getInputState().zMovement == hid::AxisMovement::Backward
              && getRelativeHeightAtDirection(getLara().m_state.rotation.Y + 180_deg, 256_len)
                   >= -core::ClimbLimit2ClickMin)
      {
        setMovementAngle(getLara().m_state.rotation.Y + 180_deg);
        setGoalAnimState(LaraStateId::JumpBack);
      }
    }

    if(getLara().m_state.fallspeed > core::FreeFallSpeedThreshold)
    {
      setGoalAnimState(LaraStateId::FreeFall);
    }
  }

  void postprocessFrame(CollisionInfo& collisionInfo) override
  {
    getLara().m_state.fallspeed = 0_spd;
    getLara().m_state.falling = false;
    collisionInfo.floorCollisionRangeMin = core::HeightLimit;
    collisionInfo.floorCollisionRangeMax = -core::HeightLimit;
    collisionInfo.ceilingCollisionRangeMin = 0_len;
    collisionInfo.facingAngle = getMovementAngle();
    collisionInfo.initHeightInfo(getLara().m_state.location.position, getWorld(), core::LaraWalkHeight);

    if(collisionInfo.mid.ceiling.y <= -core::DefaultCollisionRadius)
    {
      return;
    }

    setAnimation(AnimationId::STAY_SOLID);
    setGoalAnimState(LaraStateId::Stop);
    setCurrentAnimState(LaraStateId::Stop);
    getLara().m_state.speed = 0_spd;
    getLara().m_state.location.position = collisionInfo.initialPosition;
  }
};
} // namespace engine::lara
