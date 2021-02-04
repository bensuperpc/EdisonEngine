#pragma once

#include "material.h"
#include "mesh.h"
#include "shadermanager.h"
#include "shaderprogram.h"
#include "uniformparameter.h"

#include <gl/debuggroup.h>
#include <gl/framebuffer.h>
#include <gl/texture2d.h>

namespace render::scene
{
template<typename PixelT>
class SingleBlur
{
public:
  using Texture = gl::Texture2D<PixelT>;

  explicit SingleBlur(
    std::string name, ShaderManager& shaderManager, uint8_t dir, uint8_t extent, bool gauss, bool fillGaps)
      : m_name{std::move(name)}
      , m_shader{shaderManager.getBlur(extent, dir, PixelT::Channels, gauss, fillGaps)}
      , m_material{std::make_shared<Material>(m_shader)}
  {
    Expects(dir == 1 || dir == 2);
    Expects(extent > 0);
  }

  void setInput(const std::shared_ptr<Texture>& src)
  {
    m_blurredTexture = std::make_shared<Texture>(src->size(), m_name + "/blurred");
    m_blurredTexture->set(gl::api::TextureParameterName::TextureWrapS, gl::api::TextureWrapMode::ClampToEdge)
      .set(gl::api::TextureParameterName::TextureWrapT, gl::api::TextureWrapMode::ClampToEdge)
      .set(gl::api::TextureMinFilter::Linear)
      .set(gl::api::TextureMagFilter::Linear);

    m_material->getUniform("u_input")->set(src);

    m_mesh = createQuadFullscreen(
      gsl::narrow<float>(src->size().x), gsl::narrow<float>(src->size().y), m_shader->getHandle());
    m_mesh->getRenderState().setCullFace(false);
    m_mesh->getRenderState().setDepthTest(false);
    m_mesh->getRenderState().setDepthWrite(false);
    m_mesh->getMaterial().set(RenderMode::Full, m_material);

    m_framebuffer = gl::FrameBufferBuilder()
                      .textureNoBlend(gl::api::FramebufferAttachment::ColorAttachment0, m_blurredTexture)
                      .build(m_name + "/framebuffer");
  }

  void render() const
  {
    gl::DebugGroup dbg{m_name + "/blur-pass"};
    GL_ASSERT(gl::api::viewport(0, 0, m_blurredTexture->size().x, m_blurredTexture->size().y));

    gl::RenderState state;
    state.setBlend(false);
    state.apply(true);
    RenderContext context{RenderMode::Full, std::nullopt};

    m_framebuffer->bindWithAttachments();
    m_mesh->render(context);
  }

  [[nodiscard]] const std::shared_ptr<Texture>& getBlurredTexture() const
  {
    return m_blurredTexture;
  }

private:
  const std::string m_name;
  std::shared_ptr<Texture> m_blurredTexture;
  std::shared_ptr<Mesh> m_mesh;
  const std::shared_ptr<ShaderProgram> m_shader;
  const std::shared_ptr<Material> m_material;
  std::shared_ptr<gl::Framebuffer> m_framebuffer;
};

template<typename PixelT>
class SeparableBlur
{
public:
  using Texture = gl::Texture2D<PixelT>;

  explicit SeparableBlur(
    const std::string& name, ShaderManager& shaderManager, uint8_t extent, bool gauss, bool fillGaps)
      : m_blur1{name + "/blur-1", shaderManager, 1, extent, gauss, fillGaps}
      , m_blur2{name + "/blur-2", shaderManager, 2, extent, gauss, fillGaps}
  {
  }

  void setInput(const std::shared_ptr<Texture>& src)
  {
    m_blur1.setInput(src);
    m_blur2.setInput(m_blur1.getBlurredTexture());
  }

  void render()
  {
    m_blur1.render();
    m_blur2.render();
  }

  [[nodiscard]] const std::shared_ptr<Texture>& getBlurredTexture() const
  {
    return m_blur2.getBlurredTexture();
  }

private:
  SingleBlur<PixelT> m_blur1;
  SingleBlur<PixelT> m_blur2;
};
} // namespace render::scene
