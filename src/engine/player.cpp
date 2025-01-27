#include "player.h"

#include "engine/objectmanager.h"
#include "objects/laraobject.h"
#include "serialization/chrono.h"
#include "serialization/quantity.h"
#include "serialization/serialization.h"
#include "world/world.h"

#include <exception>
#include <gsl/gsl-lite.hpp>
#include <memory>

namespace engine
{
void Player::serialize(const serialization::Serializer<world::World>& ser)
{
  ser(S_NV("inventory", m_inventory),
      S_NV("laraHealth", laraHealth),
      S_NV("selectedWeaponType", selectedWeaponType),
      S_NV("requestedWeaponType", requestedWeaponType),
      S_NV("pickups", pickups),
      S_NV("kills", kills),
      S_NV("secrets", secrets),
      S_NV("timeSpent", timeSpent));

  if(ser.loading && ser.context.getObjectManager().getLaraPtr() != nullptr)
    ser.lazy([](const serialization::Serializer<world::World>& ser)
             { ser.context.getObjectManager().getLara().initWeaponAnimData(); });
}
} // namespace engine
