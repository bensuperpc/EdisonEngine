#include "objectmanager.h"

#include "core/id.h"
#include "core/magic.h"
#include "core/units.h"
#include "items_tr1.h"
#include "loader/file/item.h"
#include "location.h"
#include "objects/laraobject.h"
#include "objects/object.h"
#include "objects/objectfactory.h"
#include "objects/objectstate.h"
#include "particle.h"
#include "render/scene/node.h"
#include "serialization/map.h"
#include "serialization/not_null.h"
#include "serialization/objectreference.h" // IWYU pragma: keep
#include "serialization/serialization.h"
#include "world/room.h"
#include "world/world.h"

#include <algorithm>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/throw_exception.hpp>
#include <exception>
#include <limits>
#include <stdexcept>

namespace engine
{
void ObjectManager::createObjects(world::World& world, std::vector<loader::file::Item>& items)
{
  Expects(m_objectCounter == 0);
  m_objectCounter = gsl::narrow<ObjectId>(items.size());

  m_lara = nullptr;
  for(const auto& idItem : items | boost::adaptors::indexed())
  {
    auto object = objects::createObject(world, idItem.value(), idItem.index());
    if(idItem.value().type == TR1ItemId::Lara)
    {
      m_lara = std::dynamic_pointer_cast<objects::LaraObject>(object);
      Expects(m_lara != nullptr);
    }

    if(object != nullptr)
    {
      m_objects.emplace(gsl::narrow<ObjectId>(idItem.index()), object);
    }
  }
}

void ObjectManager::applyScheduledDeletions()
{
  if(m_scheduledDeletions.empty())
    return;

  for(const auto& del : m_scheduledDeletions)
  {
    auto it = std::find_if(m_dynamicObjects.begin(),
                           m_dynamicObjects.end(),
                           [del](const std::shared_ptr<objects::Object>& i) { return i.get() == del; });
    if(it != m_dynamicObjects.end())
    {
      m_dynamicObjects.erase(it);
      continue;
    }

    auto it2 = std::find_if(m_objects.begin(),
                            m_objects.end(),
                            [del](const std::pair<uint16_t, gsl::not_null<std::shared_ptr<objects::Object>>>& i)
                            { return i.second.get().get() == del; });
    if(it2 != m_objects.end())
    {
      m_objects.erase(it2);
      continue;
    }
  }

  m_scheduledDeletions.clear();
}

void ObjectManager::registerObject(const gsl::not_null<std::shared_ptr<objects::Object>>& object)
{
  if(m_objectCounter == std::numeric_limits<ObjectId>::max())
    BOOST_THROW_EXCEPTION(std::runtime_error("Artificial object counter exceeded"));

  m_objects.emplace(m_objectCounter++, object);
}

std::shared_ptr<objects::Object> ObjectManager::find(const objects::Object* object) const
{
  if(object == nullptr)
    return nullptr;

  auto it = std::find_if(m_objects.begin(),
                         m_objects.end(),
                         [object](const std::pair<uint16_t, gsl::not_null<std::shared_ptr<objects::Object>>>& x)
                         { return x.second.get().get() == object; });

  if(it == m_objects.end())
    return nullptr;

  return it->second;
}

std::shared_ptr<objects::Object> ObjectManager::getObject(ObjectId id) const
{
  const auto it = m_objects.find(id);
  if(it == m_objects.end())
    return nullptr;

  return it->second.get();
}

void ObjectManager::update(world::World& world, bool godMode)
{
  for(const auto& object : m_objects | boost::adaptors::map_values)
  {
    if(object.get() == m_lara) // Lara is special and needs to be updated last
      continue;

    if(object->m_isActive)
      object->update();

    object->updateLighting();
    object->getNode()->setVisible(object->m_state.triggerState != objects::TriggerState::Invisible);
  }

  for(const auto& object : m_dynamicObjects)
  {
    if(object->m_isActive)
      object->update();

    object->updateLighting();
    object->getNode()->setVisible(object->m_state.triggerState != objects::TriggerState::Invisible);
  }

  auto currentParticles = std::move(m_particles);
  for(const auto& particle : currentParticles)
  {
    if(particle->update(world))
    {
      setParent(particle, particle->location.room->node);
      m_particles.emplace_back(particle);
    }
    else
    {
      setParent(particle, nullptr);
    }
  }

  if(m_lara != nullptr)
  {
    if(godMode && !m_lara->isDead())
      m_lara->m_state.health = core::LaraHealth;
    m_lara->update();
    m_lara->updateLighting();
  }

  applyScheduledDeletions();
}

void ObjectManager::serialize(const serialization::Serializer<world::World>& ser)
{
  ser(S_NV("objectCounter", m_objectCounter),
      S_NV("objects", m_objects),
      S_NV("lara", serialization::ObjectReference{m_lara}));
}

void ObjectManager::eraseParticle(const std::shared_ptr<Particle>& particle)
{
  if(particle == nullptr)
    return;

  const auto it
    = std::find_if(m_particles.begin(), m_particles.end(), [particle](const auto& p) { return particle == p.get(); });
  if(it != m_particles.end())
    m_particles.erase(it);

  setParent(gsl::not_null{particle}, nullptr);
}
} // namespace engine
