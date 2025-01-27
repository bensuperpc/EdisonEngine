#include "door.h"

#include "core/angle.h"
#include "core/id.h"
#include "core/magic.h"
#include "core/units.h"
#include "engine/collisioninfo.h"
#include "engine/location.h"
#include "engine/objectmanager.h"
#include "engine/skeletalmodelnode.h"
#include "engine/world/box.h"
#include "engine/world/room.h"
#include "engine/world/world.h"
#include "laraobject.h"
#include "modelobject.h"
#include "objectstate.h"
#include "qs/quantity.h"
#include "serialization/serialization.h"
#include "serialization/vector_element.h"

#include <exception>
#include <gl/renderstate.h>
#include <memory>

namespace engine::objects
{
// #define NO_DOOR_BLOCK

Door::Door(const std::string& name,
           const gsl::not_null<world::World*>& world,
           const gsl::not_null<const world::Room*>& room,
           const loader::file::Item& item,
           const gsl::not_null<const world::SkeletalModelType*>& animatedModel)
    : ModelObject{name, world, room, item, true, animatedModel}
{
#ifndef NO_DOOR_BLOCK
  core::Length dx = 0_len, dz = 0_len;
  const auto axis = axisFromAngle(m_state.rotation.Y);
  switch(axis)
  {
  case core::Axis::PosZ: dz = -core::SectorSize; break;
  case core::Axis::PosX: dx = -core::SectorSize; break;
  case core::Axis::NegZ: dz = core::SectorSize; break;
  case core::Axis::NegX: dx = core::SectorSize; break;
  }

  m_wingsPosition = m_state.location.position + core::TRVec{dx, 0_len, dz};

  m_info.init(*m_state.location.room, m_wingsPosition);
  if(m_state.location.room->alternateRoom != nullptr)
  {
    m_alternateInfo.init(*m_state.location.room->alternateRoom, m_wingsPosition);
  }

  m_info.close();
  m_alternateInfo.close();

  if(m_info.originalSector.boundaryRoom != nullptr)
  {
    m_target.init(*m_info.originalSector.boundaryRoom, m_state.location.position);
    if(m_state.location.room->alternateRoom != nullptr)
    {
      Expects(m_alternateInfo.originalSector.boundaryRoom != nullptr);
      m_alternateTarget.init(*m_alternateInfo.originalSector.boundaryRoom, m_state.location.position);
    }

    m_target.close();
    m_alternateTarget.close();
  }
#endif

  getSkeleton()->getRenderState().setScissorTest(false);
  getSkeleton()->getRenderState().setPolygonOffsetFill(true);
  getSkeleton()->getRenderState().setPolygonOffset(-1, -1);
}

void Door::update()
{
  if(m_state.updateActivationTimeout())
  {
    if(m_state.current_anim_state == 0_as)
    {
      m_state.goal_anim_state = 1_as;
    }
    else
    {
#ifndef NO_DOOR_BLOCK
      m_info.open();
      m_target.open();
      m_alternateInfo.open();
      m_alternateTarget.open();
#endif
    }
  }
  else
  {
    if(m_state.current_anim_state == 1_as)
    {
      m_state.goal_anim_state = 0_as;
    }
    else
    {
#ifndef NO_DOOR_BLOCK
      m_info.close();
      m_target.close();
      m_alternateInfo.close();
      m_alternateTarget.close();
#endif
    }
  }

  ModelObject::update();
}

void Door::collide(CollisionInfo& collisionInfo)
{
#ifndef NO_DOOR_BLOCK
  if(!isNear(getWorld().getObjectManager().getLara(), collisionInfo.collisionRadius))
    return;

  if(!testBoneCollision(getWorld().getObjectManager().getLara()))
    return;

  if(!collisionInfo.policies.is_set(CollisionInfo::PolicyFlags::EnableBaddiePush))
    return;

  if(m_state.current_anim_state == m_state.goal_anim_state)
  {
    enemyPush(collisionInfo, false, true);
  }
  else
  {
    const auto enableSpaz = collisionInfo.policies.is_set(CollisionInfo::PolicyFlags::EnableSpaz);
    enemyPush(collisionInfo, enableSpaz, true);
  }
#endif
}

void Door::serialize(const serialization::Serializer<world::World>& ser)
{
  ModelObject::serialize(ser);
  ser(S_NV("info", m_info),
      S_NV("alternateInfo", m_alternateInfo),
      S_NV("target", m_target),
      S_NV("alternateTarget", m_alternateTarget),
      S_NV("wingsPosition", m_wingsPosition));

  if(ser.loading)
  {
    getSkeleton()->getRenderState().setScissorTest(false);
    getSkeleton()->getRenderState().setPolygonOffsetFill(true);
    getSkeleton()->getRenderState().setPolygonOffset(-1, -1);

    ser.lazy(
      [this](const serialization::Serializer<world::World>& /*ser*/)
      {
        m_info.wingsSector
          = const_cast<world::Sector*>(m_state.location.room->getSectorByAbsolutePosition(m_wingsPosition));
        if(m_info.originalSector.boundaryRoom != nullptr)
        {
          m_target.wingsSector = const_cast<world::Sector*>(
            m_info.originalSector.boundaryRoom->getSectorByAbsolutePosition(m_state.location.position));
        }

        if(m_state.location.room->alternateRoom != nullptr)
        {
          m_alternateInfo.wingsSector = const_cast<world::Sector*>(
            m_state.location.room->alternateRoom->getSectorByAbsolutePosition(m_wingsPosition));
          if(m_alternateInfo.originalSector.boundaryRoom != nullptr)
          {
            m_alternateTarget.wingsSector = const_cast<world::Sector*>(
              m_alternateInfo.originalSector.boundaryRoom->getSectorByAbsolutePosition(m_state.location.position));
          }
        }
      });
  }
}

void Door::Info::open() // NOLINT(readability-make-member-function-const)
{
  if(wingsSector == nullptr)
    return;

  *wingsSector = originalSector;
  if(wingsBox != nullptr)
    wingsBox->blocked = false;
}

void Door::Info::close() // NOLINT(readability-make-member-function-const)
{
  if(wingsSector == nullptr)
    return;

  *wingsSector = world::Sector{};
  if(wingsBox != nullptr)
    wingsBox->blocked = true;
}

void Door::Info::init(const world::Room& room, const core::TRVec& wingsPosition)
{
  wingsSector = const_cast<world::Sector*>(room.getSectorByAbsolutePosition(wingsPosition));
  Expects(wingsSector != nullptr);
  originalSector = *wingsSector;

  if(wingsSector->boundaryRoom == nullptr)
  {
    wingsBox = const_cast<world::Box*>(wingsSector->box);
  }
  else
  {
    wingsBox = const_cast<world::Box*>(wingsSector->boundaryRoom->getSectorByAbsolutePosition(wingsPosition)->box);
  }
  if(wingsBox != nullptr && !wingsBox->blockable)
  {
    wingsBox = nullptr;
  }
}

void Door::Info::serialize(const serialization::Serializer<world::World>& ser)
{
  ser(S_NV("originalSector", originalSector), S_NV_VECTOR_ELEMENT("box", ser.context.getBoxes(), wingsBox));
  if(ser.loading)
  {
    wingsSector = nullptr;
  }
}
} // namespace engine::objects
