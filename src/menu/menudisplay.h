#pragma once

#include "engine/items_tr1.h"
#include "menuringtransform.h"
#include "ui/text.h"

#include <cstddef>
#include <gsl/gsl-lite.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace engine::world
{
class World;
}

namespace ui
{
class Ui;
}

namespace menu
{
enum class InventoryMode
{
  GameMode,
  TitleMode,
  SaveMode,
  LoadMode,
  DeathMode
};

struct MenuObject;
struct MenuRing;
class MenuState;

enum class MenuResult
{
  None,
  Closed,
  ExitToTitle,
  ExitGame,
  NewGame,
  LaraHome,
  RequestLoad
};

struct MenuDisplay
{
  explicit MenuDisplay(InventoryMode mode, engine::world::World& world);
  ~MenuDisplay();

  const InventoryMode mode;
  std::optional<engine::TR1ItemId> inventoryChosen{};
  bool allowMenuClose = true;
  bool allowSave = true;

  std::shared_ptr<MenuRingTransform> ringTransform = std::make_shared<MenuRingTransform>();
  std::unique_ptr<MenuState> m_currentState;

  void display(ui::Ui& ui, engine::world::World& world);
  MenuResult result = MenuResult::None;
  std::optional<size_t> requestLoad;

  std::vector<gsl::not_null<std::unique_ptr<MenuRing>>> rings;
  size_t currentRingIndex = 0;
  bool passOpen = false;
  static bool doOptions(engine::world::World& world, MenuObject& object);
  static void drawMenuObjectDescription(ui::Ui& ui, engine::world::World& world, const MenuObject& object);

  [[nodiscard]] MenuRing& getCurrentRing()
  {
    return *rings.at(currentRingIndex);
  }

  [[nodiscard]] const MenuRing& getCurrentRing() const
  {
    return *rings.at(currentRingIndex);
  }

private:
  [[nodiscard]] static std::vector<MenuObject> getOptionRingObjects(const engine::world::World& world,
                                                                    bool withHomePolaroid);
  [[nodiscard]] static std::vector<MenuObject> getMainRingObjects(const engine::world::World& world);
  [[nodiscard]] static std::vector<MenuObject> getKeysRingObjects(const engine::world::World& world);

  ui::Text m_upArrow;
  ui::Text m_downArrow;
};
} // namespace menu
