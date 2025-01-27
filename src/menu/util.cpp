#include "util.h"

#include "engine/presenter.h"
#include "engine/world/world.h"
#include "hid/inputhandler.h"
#include "hid/inputstate.h"
#include "menuobject.h"

#include <boost/assert.hpp>

namespace menu
{
void rotateForSelection(MenuObject& object)
{
  if(object.rotationY == object.selectedRotationY)
    return;

  if(const auto dy = object.selectedRotationY - object.rotationY; dy > 0_deg && dy < 180_deg)
  {
    object.rotationY += 1024_au * 2;
  }
  else
  {
    object.rotationY -= 1024_au * 2;
  }
  object.rotationY -= object.rotationY % (1024_au * 2);
}

void idleRotation(engine::world::World& world, MenuObject& object, bool force)
{
  if(force || world.getPresenter().getInputHandler().getInputState().xMovement == hid::AxisMovement::Null)
  {
    object.rotationY += 256_au * 2;
  }
}

void zeroRotation(MenuObject& object, const core::Angle& speed)
{
  BOOST_ASSERT(speed >= 0_deg);

  if(object.rotationY < 0_deg)
  {
    object.rotationY += speed * 2;
    if(object.rotationY > 0_deg)
      object.rotationY = 0_deg;
  }
  else if(object.rotationY > 0_deg)
  {
    object.rotationY -= speed * 2;
    if(object.rotationY < 0_deg)
      object.rotationY = 0_deg;
  }
}
} // namespace menu
