#pragma once

#include "abstractstatehandler.h"
#include "engine/collisioninfo.h"
#include "engine/inputstate.h"
#include "level/level.h"

#include "engine/laranode.h"

namespace engine
{
    namespace lara
    {
        class StateHandler_OnWater : public AbstractStateHandler
        {
        public:
            explicit StateHandler_OnWater(LaraNode& lara)
                    : AbstractStateHandler(lara)
            {
            }

        protected:
            std::unique_ptr<AbstractStateHandler> commonOnWaterHandling(CollisionInfo& collisionInfo)
            {
                collisionInfo.yAngle = getMovementAngle();
                collisionInfo.initHeightInfo(getPosition() + core::ExactTRCoordinates(0, 700, 0), getLevel(), 700);
                applyCollisionFeedback(collisionInfo);
                if( collisionInfo.current.floor.distance < 0
                    || collisionInfo.axisCollisions == CollisionInfo::AxisColl_InvalidPosition
                    || collisionInfo.axisCollisions == CollisionInfo::AxisColl_InsufficientFrontCeilingSpace
                    || collisionInfo.axisCollisions == CollisionInfo::AxisColl_ScalpCollision
                    || collisionInfo.axisCollisions == CollisionInfo::AxisColl_FrontForwardBlocked
                        )
                {
                    setFallSpeed(core::makeInterpolatedValue(0.0f));
                    setPosition(collisionInfo.position);
                }
                else
                {
                    if( collisionInfo.axisCollisions == CollisionInfo::AxisColl_FrontLeftBlocked )
                        m_yRotationSpeed = 5_deg;
                    else if( collisionInfo.axisCollisions == CollisionInfo::AxisColl_FrontRightBlocked )
                        m_yRotationSpeed = -5_deg;
                    else
                        m_yRotationSpeed = 0_deg;
                }

                auto wsh = getController().getWaterSurfaceHeight();
                if( wsh && *wsh > getPosition().Y - 100 )
                {
                    return tryClimbOutOfWater(collisionInfo);
                }

                setTargetState(LaraStateId::UnderwaterForward);
                playAnimation(loader::AnimationId::FREE_FALL_TO_UNDERWATER_ALTERNATE, 2041);
                setXRotation(-45_deg);
                setFallSpeed(core::makeInterpolatedValue(80.0f));
                setUnderwaterState(UnderwaterState::Diving);
                return createWithRetainedAnimation(LaraStateId::UnderwaterDiving);
            }

        private:
            std::unique_ptr<AbstractStateHandler> tryClimbOutOfWater(CollisionInfo& collisionInfo)
            {
                if( getMovementAngle() != getRotation().Y )
                    return nullptr;

                if( collisionInfo.axisCollisions != CollisionInfo::AxisColl_FrontForwardBlocked )
                    return nullptr;

                if( !getLevel().m_inputHandler->getInputState().action )
                    return nullptr;

                const auto gradient = std::abs(collisionInfo.frontLeft.floor.distance - collisionInfo.frontRight.floor.distance);
                if( gradient >= core::MaxGrabbableGradient )
                    return nullptr;

                if( collisionInfo.front.ceiling.distance > 0 )
                    return nullptr;

                if( collisionInfo.current.ceiling.distance > -core::ClimbLimit2ClickMin )
                    return nullptr;

                if( collisionInfo.front.floor.distance + 700 <= -2 * loader::QuarterSectorSize )
                    return nullptr;

                if( collisionInfo.front.floor.distance + 700 > 100 )
                    return nullptr;

                const auto yRot = core::alignRotation(getRotation().Y, 35_deg);
                if( !yRot )
                    return nullptr;

                setPosition(getPosition() + core::ExactTRCoordinates(0, 695 + gsl::narrow_cast<float>(collisionInfo.front.floor.distance), 0));
                getController().updateFloorHeight(-381);
                core::ExactTRCoordinates d = getPosition();
                if( *yRot == 0_deg )
                    d.Z = (std::floor(getPosition().Z / loader::SectorSize) + 1) * loader::SectorSize + 100;
                else if( *yRot == 180_deg )
                    d.Z = (std::floor(getPosition().Z / loader::SectorSize) + 0) * loader::SectorSize - 100;
                else if( *yRot == -90_deg )
                    d.X = (std::floor(getPosition().X / loader::SectorSize) + 0) * loader::SectorSize - 100;
                else if( *yRot == 90_deg )
                    d.X = (std::floor(getPosition().X / loader::SectorSize) + 1) * loader::SectorSize + 100;
                else
                    throw std::runtime_error("Unexpected angle value");

                setPosition(d);

                setTargetState(LaraStateId::Stop);
                playAnimation(loader::AnimationId::CLIMB_OUT_OF_WATER, 1849);
                setHorizontalSpeed(core::makeInterpolatedValue(0.0f));
                setFallSpeed(core::makeInterpolatedValue(0.0f));
                setFalling(false);
                setXRotation(0_deg);
                setYRotation(*yRot);
                setZRotation(0_deg);
                setHandStatus(1);
                setUnderwaterState(UnderwaterState::OnLand);
                return createWithRetainedAnimation(LaraStateId::OnWaterExit);
            }
        };
    }
}