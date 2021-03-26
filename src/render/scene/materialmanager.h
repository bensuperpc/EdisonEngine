#pragma once

#include <gl/pixel.h>
#include <gl/soglb_fwd.h>
#include <map>
#include <utility>

namespace render::scene
{
class CSM;
class Camera;
class Material;
class Renderer;
class ShaderManager;

class MaterialManager final
{
public:
  explicit MaterialManager(gsl::not_null<std::shared_ptr<ShaderManager>> shaderManager,
                           gsl::not_null<std::shared_ptr<CSM>> csm,
                           gsl::not_null<std::shared_ptr<Renderer>> renderer);

  [[nodiscard]] const auto& getShaderManager() const
  {
    return m_shaderManager;
  }

  [[nodiscard]] const std::shared_ptr<Material>& getSprite();

  [[nodiscard]] const std::shared_ptr<Material>& getCSMDepthOnly(bool skeletal);
  [[nodiscard]] const std::shared_ptr<Material>& getDepthOnly(bool skeletal);

  [[nodiscard]] std::shared_ptr<Material> getGeometry(bool water, bool skeletal, bool roomShadowing);

  [[nodiscard]] const std::shared_ptr<Material>& getPortal();

  [[nodiscard]] const std::shared_ptr<Material>& getLightning();

  [[nodiscard]] std::shared_ptr<Material> getComposition(bool water, bool lensDistortion, bool dof, bool filmGrain);

  [[nodiscard]] const std::shared_ptr<Material>& getCrt();

  [[nodiscard]] const std::shared_ptr<Material>& getScreenSpriteTextured();
  [[nodiscard]] const std::shared_ptr<Material>& getScreenSpriteColorRect();

  [[nodiscard]] std::shared_ptr<Material> getFlat(bool withAlpha, bool invertY = false);
  [[nodiscard]] const std::shared_ptr<Material>& getBackdrop();

  void setGeometryTextures(std::shared_ptr<gl::Texture2DArray<gl::SRGBA8>> geometryTextures);

  void setBilinearFiltering(bool enabled);

private:
  const gsl::not_null<std::shared_ptr<ShaderManager>> m_shaderManager;

  std::shared_ptr<Material> m_sprite{nullptr};
  std::map<bool, std::shared_ptr<Material>> m_csmDepthOnly{};
  std::map<bool, std::shared_ptr<Material>> m_depthOnly{};
  std::map<std::tuple<bool, bool, bool>, std::shared_ptr<Material>> m_geometry{};
  std::shared_ptr<Material> m_portal{nullptr};
  std::shared_ptr<Material> m_lightning{nullptr};
  std::map<std::tuple<bool, bool, bool, bool>, std::shared_ptr<Material>> m_composition{};
  std::shared_ptr<Material> m_crt{nullptr};
  std::shared_ptr<Material> m_screenSpriteTextured{nullptr};
  std::shared_ptr<Material> m_screenSpriteColorRect{nullptr};
  std::map<std::tuple<bool, bool>, std::shared_ptr<Material>> m_flat{};
  std::shared_ptr<Material> m_backdrop{nullptr};

  const gsl::not_null<std::shared_ptr<CSM>> m_csm;
  const gsl::not_null<std::shared_ptr<Renderer>> m_renderer;
  std::shared_ptr<gl::Texture2DArray<gl::SRGBA8>> m_geometryTextures;
};
} // namespace render::scene
