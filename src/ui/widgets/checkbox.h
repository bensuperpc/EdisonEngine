#pragma once

#include "widget.h"

#include <glm/vec2.hpp>
#include <gsl/gsl-lite.hpp>
#include <memory>
#include <string>

namespace ui
{
class Ui;
} // namespace ui

namespace engine
{
class Presenter;
}

namespace ui::widgets
{
class Label;

class Checkbox : public Widget
{
public:
  explicit Checkbox(const std::string& label);
  ~Checkbox() override;
  void update(bool hasFocus) override;
  void draw(ui::Ui& ui, const engine::Presenter& presenter) const override;

  void setChecked(bool checked)
  {
    m_checked = checked;
  }

  void setPosition(const glm::ivec2& position) override;

  [[nodiscard]] glm::ivec2 getPosition() const override;
  [[nodiscard]] glm::ivec2 getSize() const override;
  void setSize(const glm::ivec2& size) override;
  void fitToContent() override;

  void setLabel(const std::string& label);

private:
  glm::ivec2 m_size{0, 0};
  gsl::not_null<std::unique_ptr<Label>> m_label;
  bool m_checked = false;
};
} // namespace ui::widgets
