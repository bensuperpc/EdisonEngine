#include "block.h"

#include "engine/laranode.h"
#include "level/level.h"
#include "core/boundingbox.h"

namespace engine
{
namespace items
{
void Block::collide(LaraNode& lara, CollisionInfo& collisionInfo)
{
    if( !getLevel().m_inputHandler->getInputState().action
        || m_state.triggerState == TriggerState::Active
        || lara.m_state.falling
        || lara.m_state.position.position.Y != m_state.position.position.Y )
    {
        return;
    }

    static const InteractionLimits limits{
            core::BoundingBox{{-300, 0, -692},
                              {200,  0, -512}},
            {-10_deg, -30_deg, -10_deg},
            {+10_deg, +30_deg, +10_deg}
    };

    auto axis = core::axisFromAngle( lara.m_state.rotation.Y, 45_deg );
    Expects( axis.is_initialized() );

    if( lara.getCurrentAnimState() == loader::LaraStateId::Stop )
    {
        if( getLevel().m_inputHandler->getInputState().zMovement != AxisMovement::Null
            || lara.getHandStatus() != HandStatus::None )
        {
            return;
        }

        const core::Angle y = core::alignRotation( *axis );
        m_state.rotation.Y = y;

        if( !limits.canInteract( m_state, lara.m_state ) )
        {
            return;
        }

        lara.m_state.rotation.Y = y;

        int32_t core::TRVec::*vp;
        int d = 0;
        switch( *axis )
        {
            case core::Axis::PosZ:
                d = loader::SectorSize - 100;
                vp = &core::TRVec::Z;
                break;
            case core::Axis::PosX:
                d = loader::SectorSize - 100;
                vp = &core::TRVec::X;
                break;
            case core::Axis::NegZ:
                d = 100;
                vp = &core::TRVec::Z;
                break;
            case core::Axis::NegX:
                d = 100;
                vp = &core::TRVec::X;
                break;
            default:
                Expects( false );
        }

        lara.m_state.position.position.*vp =
                (lara.m_state.position.position.*vp / loader::SectorSize) * loader::SectorSize + d;

        lara.setGoalAnimState( loader::LaraStateId::PushableGrab );
        lara.updateImpl();
        if( lara.getCurrentAnimState() == loader::LaraStateId::PushableGrab )
        {
            lara.setHandStatus( HandStatus::Grabbing );
        }
        return;
    }

    if( lara.getCurrentAnimState() != loader::LaraStateId::PushableGrab
        || lara.m_state.frame_number != 2091
        || !limits.canInteract( m_state, lara.m_state ) )
    {
        return;
    }

    if( getLevel().m_inputHandler->getInputState().zMovement == AxisMovement::Forward )
    {
        if( !canPushBlock( loader::SectorSize, *axis ) )
        {
            return;
        }

        m_state.goal_anim_state = 2;
        lara.setGoalAnimState( loader::LaraStateId::PushablePush );
    }
    else if( getLevel().m_inputHandler->getInputState().zMovement == AxisMovement::Backward )
    {
        if( !canPullBlock( loader::SectorSize, *axis ) )
        {
            return;
        }

        m_state.goal_anim_state = 3;
        lara.setGoalAnimState( loader::LaraStateId::PushablePull );
    }
    else
    {
        return;
    }

    activate();
    loader::Room::patchHeightsForBlock( *this, loader::SectorSize );
    m_state.triggerState = TriggerState::Active;

    ModelItemNode::update();
    getLevel().m_lara->updateImpl();
}

void Block::update()
{
    if( m_state.activationState.isOneshot() )
    {
        loader::Room::patchHeightsForBlock( *this, loader::SectorSize );
        kill();
        return;
    }

    ModelItemNode::update();

    auto pos = m_state.position;
    auto sector = to_not_null( getLevel().findRealFloorSector( pos ) );
    auto height = HeightInfo::fromFloor( sector, pos.position, getLevel().m_itemNodes ).y;
    if( height > pos.position.Y )
    {
        m_state.falling = true;
    }
    else if( m_state.falling )
    {
        pos.position.Y = height;
        m_state.position.position = pos.position;
        m_state.falling = false;
        m_state.triggerState = TriggerState::Deactivated;
        getLevel().dinoStompEffect( *this );
        playSoundEffect( 70 );
    }

    setCurrentRoom( pos.room );

    if( m_state.triggerState != TriggerState::Deactivated )
    {
        return;
    }

    m_state.triggerState = TriggerState::Inactive;
    deactivate();
    loader::Room::patchHeightsForBlock( *this, -loader::SectorSize );
    pos = m_state.position;
    sector = to_not_null( getLevel().findRealFloorSector( pos ) );
    getLevel().m_lara->handleCommandSequence(
            HeightInfo::fromFloor( sector, pos.position, getLevel().m_itemNodes ).lastCommandSequenceOrDeath, true );
}

bool Block::isOnFloor(int height) const
{
    auto sector = getLevel().findRealFloorSector( m_state.position.position, m_state.position.room );
    return sector->floorHeight == -127
           || sector->floorHeight * loader::QuarterSectorSize == m_state.position.position.Y - height;
}

bool Block::canPushBlock(int height, core::Axis axis) const
{
    if( !isOnFloor( height ) )
    {
        return false;
    }

    auto pos = m_state.position.position;
    switch( axis )
    {
        case core::Axis::PosZ:
            pos.Z += loader::SectorSize;
            break;
        case core::Axis::PosX:
            pos.X += loader::SectorSize;
            break;
        case core::Axis::NegZ:
            pos.Z -= loader::SectorSize;
            break;
        case core::Axis::NegX:
            pos.X -= loader::SectorSize;
            break;
        default:
            break;
    }

    CollisionInfo tmp;
    tmp.facingAxis = axis;
    tmp.collisionRadius = 500;
    if( tmp.checkStaticMeshCollisions( pos, 1000, getLevel() ) )
    {
        return false;
    }

    auto targetSector = getLevel().findRealFloorSector( pos, m_state.position.room );
    if( targetSector->floorHeight * loader::QuarterSectorSize != pos.Y )
    {
        return false;
    }

    pos.Y -= height;
    return pos.Y
           >= getLevel().findRealFloorSector( pos, m_state.position.room )->ceilingHeight * loader::QuarterSectorSize;
}

bool Block::canPullBlock(int height, core::Axis axis) const
{
    if( !isOnFloor( height ) )
    {
        return false;
    }

    auto pos = m_state.position.position;
    switch( axis )
    {
        case core::Axis::PosZ:
            pos.Z -= loader::SectorSize;
            break;
        case core::Axis::PosX:
            pos.X -= loader::SectorSize;
            break;
        case core::Axis::NegZ:
            pos.Z += loader::SectorSize;
            break;
        case core::Axis::NegX:
            pos.X += loader::SectorSize;
            break;
        default:
            break;
    }

    auto room = m_state.position.room;
    auto sector = getLevel().findRealFloorSector( pos, to_not_null( &room ) );

    CollisionInfo tmp;
    tmp.facingAxis = axis;
    tmp.collisionRadius = 500;
    if( tmp.checkStaticMeshCollisions( pos, 1000, getLevel() ) )
    {
        return false;
    }

    if( sector->floorHeight * loader::QuarterSectorSize != pos.Y )
    {
        return false;
    }

    auto topPos = pos;
    topPos.Y -= height;
    auto topSector = getLevel().findRealFloorSector( topPos, m_state.position.room );
    if( topPos.Y < topSector->ceilingHeight * loader::QuarterSectorSize )
    {
        return false;
    }

    auto laraPos = pos;
    switch( axis )
    {
        case core::Axis::PosZ:
            laraPos.Z -= loader::SectorSize;
            break;
        case core::Axis::PosX:
            laraPos.X -= loader::SectorSize;
            break;
        case core::Axis::NegZ:
            laraPos.Z += loader::SectorSize;
            break;
        case core::Axis::NegX:
            laraPos.X += loader::SectorSize;
            break;
        default:
            break;
    }

    sector = getLevel().findRealFloorSector( laraPos, to_not_null( &room ) );
    if( sector->floorHeight * loader::QuarterSectorSize != pos.Y )
    {
        return false;
    }

    laraPos.Y -= core::ScalpHeight;
    sector = getLevel().findRealFloorSector( laraPos, to_not_null( &room ) );
    if( laraPos.Y < sector->ceilingHeight * loader::QuarterSectorSize )
    {
        return false;
    }

    laraPos = getLevel().m_lara->m_state.position.position;
    switch( axis )
    {
        case core::Axis::PosZ:
            laraPos.Z -= loader::SectorSize;
            tmp.facingAxis = core::Axis::NegZ;
            break;
        case core::Axis::PosX:
            laraPos.X -= loader::SectorSize;
            tmp.facingAxis = core::Axis::NegX;
            break;
        case core::Axis::NegZ:
            laraPos.Z += loader::SectorSize;
            tmp.facingAxis = core::Axis::PosZ;
            break;
        case core::Axis::NegX:
            laraPos.X += loader::SectorSize;
            tmp.facingAxis = core::Axis::PosX;
            break;
        default:
            break;
    }
    tmp.collisionRadius = core::DefaultCollisionRadius;

    return !tmp.checkStaticMeshCollisions( laraPos, core::ScalpHeight, getLevel() );
}
}
}
