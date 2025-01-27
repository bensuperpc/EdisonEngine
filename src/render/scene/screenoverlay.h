#pragma once

#include "renderable.h"

#include <algorithm>
#include <gl/image.h>
#include <gl/pixel.h>
#include <gl/soglb_fwd.h>
#include <glm/fwd.hpp>
#include <gsl/gsl-lite.hpp>
#include <memory>

// IWYU pragma: no_forward_declare gl::Texture2D
// IWYU pragma: no_forward_declare gl::TextureHandle

namespace render::scene
{
class Mesh;
class MaterialManager;
class RenderContext;

class ScreenOverlay : public Renderable
{
public:
  ScreenOverlay(const ScreenOverlay&) = delete;
  ScreenOverlay(ScreenOverlay&&) = delete;
  ScreenOverlay& operator=(ScreenOverlay&&) = delete;
  ScreenOverlay& operator=(const ScreenOverlay&) = delete;

  explicit ScreenOverlay(MaterialManager& materialManager, const glm::ivec2& viewport);

  void init(MaterialManager& materialManager, const glm::ivec2& viewport);

  ~ScreenOverlay() override;

  bool render(RenderContext& context) override;

  [[nodiscard]] const auto& getImage() const
  {
    return m_image;
  }

  [[nodiscard]] const auto& getTexture() const
  {
    return m_texture;
  }

  void setAlphaMultiplier(float value)
  {
    m_alphaMultiplier = value;
  }

private:
  const gsl::not_null<std::shared_ptr<gl::Image<gl::SRGBA8>>> m_image{std::make_shared<gl::Image<gl::SRGBA8>>()};
  std::shared_ptr<gl::TextureHandle<gl::Texture2D<gl::SRGBA8>>> m_texture;
  std::shared_ptr<Mesh> m_mesh{nullptr};
  float m_alphaMultiplier{1.0f};
};
} // namespace render::scene
