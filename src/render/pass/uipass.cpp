#include "uipass.h"

#include "config.h"
#include "render/scene/materialmanager.h"
#include "render/scene/mesh.h"
#include "render/scene/rendercontext.h"
#include "render/scene/rendermode.h"

#include <algorithm>
#include <gl/debuggroup.h>
#include <gl/framebuffer.h>
#include <gl/glassert.h>
#include <gl/pixel.h>
#include <gl/program.h>
#include <gl/renderstate.h>
#include <gl/sampler.h>
#include <gl/texture2d.h>
#include <gl/texturehandle.h>
#include <glm/mat4x4.hpp>
#include <gsl/gsl-lite.hpp>
#include <gslu.h>
#include <optional>
#include <utility>

namespace render::scene
{
class Node;
}

namespace render::pass
{
// NOLINTNEXTLINE(readability-make-member-function-const)
void UIPass::bind()
{
  gl::Framebuffer::unbindAll();
  m_colorBuffer->clear({0, 0, 0, 0});
  m_fb->bindWithAttachments();
}

UIPass::UIPass(scene::MaterialManager& materialManager, const glm::ivec2& viewport)
    : m_material{materialManager.getFlat(true, false)}
    , m_mesh{scene::createScreenQuad(m_material, "ui")}
    , m_colorBuffer{std::make_shared<gl::Texture2D<gl::SRGBA8>>(viewport, "ui-color")}
    , m_colorBufferHandle{std::make_shared<gl::TextureHandle<gl::Texture2D<gl::SRGBA8>>>(
        m_colorBuffer,
        gslu::make_nn_unique<gl::Sampler>("ui-color")
          | set(gl::api::SamplerParameterI::TextureWrapS, gl::api::TextureWrapMode::ClampToEdge)
          | set(gl::api::SamplerParameterI::TextureWrapT, gl::api::TextureWrapMode::ClampToEdge)
          | set(gl::api::TextureMinFilter::Nearest) | set(gl::api::TextureMagFilter::Nearest))}
    , m_fb{gl::FrameBufferBuilder()
             .texture(gl::api::FramebufferAttachment::ColorAttachment0, m_colorBuffer)
             .build("ui-fb")}
{
  m_mesh->bind("u_input",
               [this](const render::scene::Node& /*node*/, const render::scene::Mesh& /*mesh*/, gl::Uniform& uniform)
               { uniform.set(gsl::not_null{m_colorBufferHandle}); });
  m_mesh->getRenderState().setBlend(true);
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void UIPass::render(float alpha)
{
  SOGLB_DEBUGGROUP("ui-pass");
  gl::Framebuffer::unbindAll();

  scene::RenderContext context{scene::RenderMode::Full, std::nullopt};
  m_mesh->bind("u_alphaMultiplier",
               [alpha](const render::scene::Node& /*node*/, const render::scene::Mesh& /*mesh*/, gl::Uniform& uniform)
               { uniform.set(alpha); });
  m_mesh->render(context);

  if constexpr(FlushPasses)
    GL_ASSERT(gl::api::finish());
}
} // namespace render::pass
