#pragma once

#include "menu/menustate.h"
#include "selectedmenustate.h"

#include <gsl/gsl-lite.hpp>
#include <memory>

namespace engine::world
{
class World;
}

namespace ui
{
class Ui;
}

namespace ui::widgets
{
class GridBox;
class GroupBox;
class ProgressBar;
} // namespace ui::widgets

namespace menu
{
struct MenuDisplay;
struct MenuRingTransform;

class AudioSettingsMenuState : public SelectedMenuState
{
private:
  std::unique_ptr<MenuState> m_previous;
  gsl::not_null<std::shared_ptr<ui::widgets::GridBox>> m_grid;
  gsl::not_null<std::shared_ptr<ui::widgets::GroupBox>> m_container;
  gsl::not_null<std::shared_ptr<ui::widgets::ProgressBar>> m_globalVolume;
  gsl::not_null<std::shared_ptr<ui::widgets::ProgressBar>> m_musicVolume;
  gsl::not_null<std::shared_ptr<ui::widgets::ProgressBar>> m_sfxVolume;

public:
  explicit AudioSettingsMenuState(const std::shared_ptr<MenuRingTransform>& ringTransform,
                                  std::unique_ptr<MenuState> previous,
                                  const MenuDisplay& display);
  std::unique_ptr<MenuState> onFrame(ui::Ui& ui, engine::world::World& world, MenuDisplay& display) override;
};
} // namespace menu
