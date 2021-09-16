#include "trapdoorup.h"

#include "modelobject.h"
#include "objectstate.h"

namespace engine::objects
{
void TrapDoorUp::update()
{
  ModelObject::update();
  m_state.location.updateRoom();
  setCurrentRoom(m_state.location.room);
}
} // namespace engine::objects
