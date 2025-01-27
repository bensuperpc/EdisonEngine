#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "engine/objects/laraobject.h"
#include "hid/inputstate.h"

namespace engine::lara
{
class StateHandler_OnWater : public AbstractStateHandler
{
public:
  explicit StateHandler_OnWater(objects::LaraObject& lara, const LaraStateId id)
      : AbstractStateHandler{lara, id}
  {
  }

protected:
  void commonOnWaterHandling(CollisionInfo& collisionInfo)
  {
    collisionInfo.facingAngle = getMovementAngle();
    collisionInfo.initHeightInfo(getLara().m_state.location.position + core::TRVec(0_len, core::LaraSwimHeight, 0_len),
                                 getWorld(),
                                 core::LaraSwimHeight);
    applyShift(collisionInfo);
    if(collisionInfo.mid.floor.y < 0_len || collisionInfo.collisionType == CollisionInfo::AxisColl::TopFront
       || collisionInfo.collisionType == CollisionInfo::AxisColl::TopBottom
       || collisionInfo.collisionType == CollisionInfo::AxisColl::Top
       || collisionInfo.collisionType == CollisionInfo::AxisColl::Front)
    {
      getLara().m_state.fallspeed = 0_spd;
      getLara().m_state.location.position = collisionInfo.initialPosition;
    }
    else if(collisionInfo.collisionType == CollisionInfo::AxisColl::Left)
    {
      getLara().m_state.rotation.Y += 5_deg;
    }
    else if(collisionInfo.collisionType == CollisionInfo::AxisColl::Right)
    {
      getLara().m_state.rotation.Y -= 5_deg;
    }

    auto wsh = getLara().getWaterSurfaceHeight();
    if(wsh.has_value() && *wsh > getLara().m_state.location.position.Y - core::DefaultCollisionRadius)
    {
      tryClimbOutOfWater(collisionInfo);
      return;
    }

    setAnimation(AnimationId::FREE_FALL_TO_UNDERWATER_ALTERNATE);
    setGoalAnimState(LaraStateId::UnderwaterForward);
    setCurrentAnimState(LaraStateId::UnderwaterDiving);
    getLara().m_state.rotation.X = -45_deg;
    getLara().m_state.fallspeed = 80_spd;
    setUnderwaterState(objects::UnderwaterState::Diving);
  }

private:
  void tryClimbOutOfWater(const CollisionInfo& collisionInfo)
  {
    if(getMovementAngle() != getLara().m_state.rotation.Y)
    {
      return;
    }

    if(collisionInfo.collisionType != CollisionInfo::AxisColl::Front)
    {
      return;
    }

    if(!getWorld().getPresenter().getInputHandler().hasAction(hid::Action::Action))
    {
      return;
    }

    const auto gradient = abs(collisionInfo.frontLeft.floor.y - collisionInfo.frontRight.floor.y);
    if(gradient >= core::MaxGrabbableGradient)
    {
      return;
    }

    if(collisionInfo.front.ceiling.y > 0_len)
    {
      return;
    }

    if(collisionInfo.mid.ceiling.y > -core::ClimbLimit2ClickMin)
    {
      return;
    }

    if(collisionInfo.front.floor.y + core::LaraSwimHeight <= 2 * -core::QuarterSectorSize)
    {
      return;
    }

    if(collisionInfo.front.floor.y + core::LaraSwimHeight > core::DefaultCollisionRadius)
    {
      return;
    }

    const auto yRot = snapRotation(getLara().m_state.rotation.Y, 35_deg);
    if(!yRot.has_value())
    {
      return;
    }

    getLara().m_state.location.move(core::TRVec(0_len, 695_len + collisionInfo.front.floor.y, 0_len));
    getLara().updateFloorHeight(-381_len);
    core::TRVec d = getLara().m_state.location.position;
    if(*yRot == 0_deg)
    {
      d.Z = (getLara().m_state.location.position.Z / core::SectorSize + 1) * core::SectorSize
            + core::DefaultCollisionRadius;
    }
    else if(*yRot == 180_deg)
    {
      d.Z = (getLara().m_state.location.position.Z / core::SectorSize + 0) * core::SectorSize
            - core::DefaultCollisionRadius;
    }
    else if(*yRot == -90_deg)
    {
      d.X = (getLara().m_state.location.position.X / core::SectorSize + 0) * core::SectorSize
            - core::DefaultCollisionRadius;
    }
    else if(*yRot == 90_deg)
    {
      d.X = (getLara().m_state.location.position.X / core::SectorSize + 1) * core::SectorSize
            + core::DefaultCollisionRadius;
    }
    else
    {
      throw std::runtime_error("Unexpected angle value");
    }

    getLara().m_state.location.position = d;

    setAnimation(AnimationId::CLIMB_OUT_OF_WATER);
    setGoalAnimState(LaraStateId::Stop);
    setCurrentAnimState(LaraStateId::OnWaterExit);
    getLara().m_state.speed = 0_spd;
    getLara().m_state.fallspeed = 0_spd;
    getLara().m_state.falling = false;
    getLara().m_state.rotation.X = 0_deg;
    getLara().m_state.rotation.Y = *yRot;
    getLara().m_state.rotation.Z = 0_deg;
    setHandStatus(objects::HandStatus::Grabbing);
    setUnderwaterState(objects::UnderwaterState::OnLand);
  }
};
} // namespace engine::lara
