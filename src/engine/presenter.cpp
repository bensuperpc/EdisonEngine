#include "presenter.h"

#include "audioengine.h"
#include "engine.h"
#include "engine/objects/laraobject.h"
#include "loader/file/level/level.h"
#include "objectmanager.h"
#include "render/renderpipeline.h"
#include "render/scene/camera.h"
#include "render/scene/csm.h"
#include "render/scene/materialmanager.h"
#include "render/scene/node.h"
#include "render/scene/rendercontext.h"
#include "render/scene/renderer.h"
#include "render/scene/rendervisitor.h"
#include "render/scene/screenoverlay.h"
#include "render/textureanimator.h"
#include "ui/label.h"
#include "ui/ui.h"
#include "video/player.h"

#include <boost/range/adaptors.hpp>
#include <gl/debuggroup.h>
#include <gl/font.h>

namespace
{
constexpr int StatusLineFontSize = 40;
constexpr int DebugTextFontSize = 12;
} // namespace

namespace engine
{
void Presenter::playVideo(const std::filesystem::path& path)
{
  render::scene::RenderContext context{render::scene::RenderMode::Full, std::nullopt};
  video::play(path, m_soundEngine->getSoLoud(), m_screenOverlay->getImage(), [&]() {
    glfwPollEvents();
    m_window->updateWindowSize();
    if(m_window->isMinimized())
      return true;

    m_renderer->getCamera()->setAspectRatio(m_window->getAspectRatio());
    if(m_screenOverlay->getImage()->getSize() != m_window->getViewport())
    {
      m_screenOverlay->init(*m_shaderManager, m_window->getViewport());
    }

    if(m_window->isMinimized())
      return true;

    m_screenOverlay->render(context);
    swapBuffers();
    m_inputHandler->update();
    return !m_window->windowShouldClose() && !m_inputHandler->hasDebouncedAction(hid::Action::Menu);
  });
}

void Presenter::renderWorld(ui::Ui& ui,
                            const ObjectManager& objectManager,
                            const std::vector<loader::file::Room>& rooms,
                            const CameraController& cameraController,
                            const std::unordered_set<const loader::file::Portal*>& waterEntryPortals)
{
  m_renderPipeline->updateCamera(m_renderer->getCamera());

  {
    SOGLB_DEBUGGROUP("csm-pass");
    m_renderer->resetRenderState();
    m_csm->updateCamera(*m_renderer->getCamera());
    m_csm->applyViewport();

    for(size_t i = 0; i < render::scene::CSMBuffer::NSplits; ++i)
    {
      SOGLB_DEBUGGROUP("csm-pass/" + std::to_string(i));
      m_renderer->resetRenderState();
      gl::RenderState renderState;
      renderState.setDepthClamp(true);
      renderState.apply();

      m_csm->setActiveSplit(i);
      m_csm->getActiveFramebuffer()->bind();
      m_renderer->clear(gl::api::ClearBufferMask::DepthBufferBit, {0, 0, 0, 0}, 1);

      render::scene::RenderContext context{render::scene::RenderMode::CSMDepthOnly,
                                           m_csm->getActiveMatrix(glm::mat4{1.0f})};
      render::scene::RenderVisitor visitor{context};

      for(const auto& room : rooms)
      {
        if(!room.node->isVisible())
          continue;

        for(const auto& child : room.node->getChildren())
        {
          visitor.visit(*child);
        }
      }

      m_csm->finishSplitRender();
    }
  }

  {
    SOGLB_DEBUGGROUP("geometry-pass");
    m_renderPipeline->bindGeometryFrameBuffer(m_window->getViewport());
    m_renderer->clear(
      gl::api::ClearBufferMask::ColorBufferBit | gl::api::ClearBufferMask::DepthBufferBit, {0, 0, 0, 0}, 1);

    {
      SOGLB_DEBUGGROUP("depth-prefill-pass");
      m_renderer->resetRenderState();
      render::scene::RenderContext context{render::scene::RenderMode::DepthOnly,
                                           cameraController.getCamera()->getViewProjectionMatrix()};
      for(const auto& room : rooms)
      {
        if(!room.node->isVisible())
          continue;

        SOGLB_DEBUGGROUP(room.node->getName());
        context.setCurrentNode(room.node.get());
        room.node->getRenderable()->render(context);
      }
      if constexpr(render::RenderPipeline::FlushStages)
        GL_ASSERT(gl::api::finish());
    }

    m_renderer->resetRenderState();
    m_renderer->render();

    if constexpr(render::RenderPipeline::FlushStages)
      GL_ASSERT(gl::api::finish());
  }

  {
    SOGLB_DEBUGGROUP("portal-depth-pass");
    m_renderer->resetRenderState();

    render::scene::RenderContext context{render::scene::RenderMode::DepthOnly,
                                         cameraController.getCamera()->getViewProjectionMatrix()};

    m_renderPipeline->bindPortalFrameBuffer();
    for(const auto& portal : waterEntryPortals)
    {
      portal->mesh->render(context);
    }
    if constexpr(render::RenderPipeline::FlushStages)
      GL_ASSERT(gl::api::finish());
  }

  render::scene::RenderContext context{render::scene::RenderMode::Full,
                                       cameraController.getCamera()->getViewProjectionMatrix()};

  m_renderPipeline->compositionPass(cameraController.getCurrentRoom()->isWaterRoom());

  if(m_showDebugInfo)
  {
    m_debugFont->drawText(
      *m_screenOverlay->getImage(),
      std::to_string(m_renderer->getFrameRate()).c_str(),
      glm::ivec2{m_screenOverlay->getImage()->getSize().x - 40, m_screenOverlay->getImage()->getSize().y - 20},
      gl::SRGBA8{255},
      DebugTextFontSize);

    const auto drawObjectName = [this](const std::shared_ptr<objects::Object>& object, const gl::SRGBA8& color) {
      const auto vertex
        = glm::vec3{m_renderer->getCamera()->getViewMatrix() * glm::vec4(object->getNode()->getTranslationWorld(), 1)};

      if(vertex.z > -m_renderer->getCamera()->getNearPlane() || vertex.z < -m_renderer->getCamera()->getFarPlane())
      {
        return;
      }

      glm::vec4 projVertex{vertex, 1};
      projVertex = m_renderer->getCamera()->getProjectionMatrix() * projVertex;
      projVertex /= projVertex.w;

      if(std::abs(projVertex.x) > 1 || std::abs(projVertex.y) > 1)
        return;

      projVertex.x = (projVertex.x / 2 + 0.5f) * m_window->getViewport().x;
      projVertex.y = (1 - (projVertex.y / 2 + 0.5f)) * m_window->getViewport().y;

      m_debugFont->drawText(*m_screenOverlay->getImage(),
                            object->getNode()->getName().c_str(),
                            glm::ivec2{static_cast<int>(projVertex.x), static_cast<int>(projVertex.y)},
                            color,
                            DebugTextFontSize);
    };

    for(const auto& object : objectManager.getObjects() | boost::adaptors::map_values)
    {
      drawObjectName(object, gl::SRGBA8{255});
    }
    for(const auto& object : objectManager.getDynamicObjects())
    {
      drawObjectName(object, gl::SRGBA8{0, 255, 0, 255});
    }
  }

  {
    SOGLB_DEBUGGROUP("screen-overlay-pass");
    m_renderer->resetRenderState();
    m_screenOverlay->render(context);
  }

  ui.render(m_window->getViewport());

  swapBuffers();
}

void Presenter::drawLevelName(ui::Ui& ui, const std::string& levelName)
{
  ui::Label tmp{{0, -50}, levelName};
  tmp.alignX = ui::Label::Alignment::Center;
  tmp.alignY = ui::Label::Alignment::Bottom;
  tmp.outline = true;
  tmp.addBackground({0, 0}, {0, 0});
  tmp.draw(ui, *m_trFont, getViewport());
}

void Presenter::drawBars(ui::Ui& ui, const loader::file::Palette& palette, const ObjectManager& objectManager)
{
  if(objectManager.getLara().isInWater())
  {
    const auto x0 = m_window->getViewport().x - 110;

    for(int i = 7; i <= 13; ++i)
      ui.drawHLine({x0 - 1, i}, 102, palette.colors[0].toTextureColor());
    ui.drawHLine({x0 - 2, 14}, 104, palette.colors[17].toTextureColor());
    ui.drawVLine({x0 + 102, 6}, 8, palette.colors[17].toTextureColor());
    ui.drawHLine({x0 - 2, 6}, 104, palette.colors[19].toTextureColor());
    ui.drawVLine({x0 - 2, 6}, 8, palette.colors[19].toTextureColor());

    if(const int p = std::clamp(objectManager.getLara().getAir() * 100 / core::LaraAir, 0, 100); p > 0)
    {
      ui.drawHLine({x0, 8}, p, palette.colors[32].toTextureColor());
      ui.drawHLine({x0, 9}, p, palette.colors[41].toTextureColor());
      ui.drawHLine({x0, 10}, p, palette.colors[32].toTextureColor());
      ui.drawHLine({x0, 11}, p, palette.colors[19].toTextureColor());
      ui.drawHLine({x0, 12}, p, palette.colors[21].toTextureColor());
    }
  }

  if(objectManager.getLara().getHandStatus() == objects::HandStatus::Combat || objectManager.getLara().isDead())
    m_healthBarTimeout = 40_frame;

  if(std::exchange(m_drawnHealth, objectManager.getLara().m_state.health) != objectManager.getLara().m_state.health)
    m_healthBarTimeout = 40_frame;

  m_healthBarTimeout -= 1_frame;
  if(m_healthBarTimeout <= -40_frame)
    return;

  uint8_t alpha = 255;
  if(m_healthBarTimeout < 0_frame)
  {
    alpha = gsl::narrow_cast<uint8_t>(std::clamp(255 - std::abs(255 * m_healthBarTimeout / 40_frame), 0, 255));
  }

  const int x0 = 8;
  for(int i = 7; i <= 13; ++i)
    ui.drawHLine({x0 - 1, i}, 102, palette.colors[0].toTextureColor(alpha));
  ui.drawHLine({x0 - 2, 14}, 104, palette.colors[17].toTextureColor(alpha));
  ui.drawVLine({x0 + 102, 6}, 8, palette.colors[17].toTextureColor(alpha));
  ui.drawHLine({x0 - 2, 6}, 104, palette.colors[19].toTextureColor(alpha));
  ui.drawHLine({x0 - 2, 6}, 8, palette.colors[19].toTextureColor(alpha));

  if(const int p = std::clamp(objectManager.getLara().m_state.health * 100 / core::LaraHealth, 0, 100); p > 0)
  {
    ui.drawHLine({x0, 8}, p, palette.colors[8].toTextureColor(alpha));
    ui.drawHLine({x0, 9}, p, palette.colors[11].toTextureColor(alpha));
    ui.drawHLine({x0, 10}, p, palette.colors[8].toTextureColor(alpha));
    ui.drawHLine({x0, 11}, p, palette.colors[6].toTextureColor(alpha));
    ui.drawHLine({x0, 12}, p, palette.colors[24].toTextureColor(alpha));
  }
}

Presenter::Presenter(const std::filesystem::path& rootPath, bool fullscreen, const glm::ivec2& resolution)
    : m_window{std::make_unique<gl::Window>(fullscreen, resolution)}
    , m_soundEngine{std::make_shared<audio::SoundEngine>()}
    , m_renderer{std::make_shared<render::scene::Renderer>(std::make_shared<render::scene::Camera>(
        DefaultFov, m_window->getAspectRatio(), DefaultNearPlane, DefaultFarPlane))}
    , m_splashImage{rootPath / "splash.png"}
    , m_abibasFont{std::make_unique<gl::Font>(rootPath / "abibas.ttf")}
    , m_debugFont{std::make_unique<gl::Font>(rootPath / "DroidSansMono.ttf")}
    , m_inputHandler{std::make_unique<hid::InputHandler>(m_window->getWindow())}
    , m_shaderManager{std::make_shared<render::scene::ShaderManager>(rootPath / "shaders")}
    , m_csm{std::make_shared<render::scene::CSM>(CSMResolution, *m_shaderManager)}
    , m_materialManager{std::make_unique<render::scene::MaterialManager>(m_shaderManager, m_csm, m_renderer)}
    , m_renderPipeline{std::make_unique<render::RenderPipeline>(*m_materialManager, m_window->getViewport())}
    , m_screenOverlay{std::make_unique<render::scene::ScreenOverlay>(*m_shaderManager, m_window->getViewport())}
{
  scaleSplashImage();

  m_screenOverlay->init(*m_shaderManager, m_window->getViewport());
  drawLoadingScreen("Booting");
}

Presenter::~Presenter() = default;

void Presenter::scaleSplashImage()
{
  // scale splash image so that its aspect ratio is preserved, but the boundaries match
  const float splashScale
    = std::max(gsl::narrow<float>(m_window->getViewport().x) / gsl::narrow<float>(m_splashImage.width()),
               gsl::narrow<float>(m_window->getViewport().y) / gsl::narrow<float>(m_splashImage.height()));

  m_splashImageScaled = m_splashImage;
  m_splashImageScaled.resize(static_cast<int>(gsl::narrow<float>(m_splashImageScaled.width()) * splashScale),
                             static_cast<int>(gsl::narrow<float>(m_splashImageScaled.height()) * splashScale));

  // crop to boundaries
  const auto centerX = m_splashImageScaled.width() / 2;
  const auto centerY = m_splashImageScaled.height() / 2;
  m_splashImageScaled.crop(gsl::narrow<int>(centerX - m_window->getViewport().x / 2),
                           gsl::narrow<int>(centerY - m_window->getViewport().y / 2),
                           gsl::narrow<int>(centerX - m_window->getViewport().x / 2 + m_window->getViewport().x - 1),
                           gsl::narrow<int>(centerY - m_window->getViewport().y / 2 + m_window->getViewport().y - 1));

  Expects(m_splashImageScaled.width() == m_window->getViewport().x);
  Expects(m_splashImageScaled.height() == m_window->getViewport().y);

  m_splashImageScaled.interleave();
}

void Presenter::drawLoadingScreen(const std::string& state)
{
  glfwPollEvents();
  m_window->updateWindowSize();
  if(m_window->isMinimized())
    return;
  if(m_splashImageScaled.width() != m_window->getViewport().x
     || m_splashImageScaled.height() != m_window->getViewport().y)
  {
    m_renderer->getCamera()->setAspectRatio(m_window->getAspectRatio());
    m_screenOverlay->init(*m_shaderManager, m_window->getViewport());
    scaleSplashImage();
  }

  if(m_window->isMinimized())
    return;

  m_screenOverlay->getImage()->assign(m_splashImageScaled.pixels<gl::SRGBA8>().data(),
                                      m_window->getViewport().x * m_window->getViewport().y);
  m_abibasFont->drawText(*m_screenOverlay->getImage(),
                         state.c_str(),
                         glm::ivec2{40, m_screenOverlay->getImage()->getSize().y - 100},
                         gl::SRGBA8{255, 255, 255, 192},
                         StatusLineFontSize);

  gl::Framebuffer::unbindAll();

  m_renderer->clear(
    gl::api::ClearBufferMask::ColorBufferBit | gl::api::ClearBufferMask::DepthBufferBit, {0, 0, 0, 0}, 1);
  render::scene::RenderContext context{render::scene::RenderMode::Full, std::nullopt};
  m_screenOverlay->render(context);
  swapBuffers();
}

bool Presenter::preFrame()
{
  m_window->updateWindowSize();
  if(m_window->isMinimized())
    return false;

  m_renderer->getCamera()->setAspectRatio(m_window->getAspectRatio());
  m_renderPipeline->resize(m_window->getViewport());
  if(m_screenOverlay->getImage()->getSize() != m_window->getViewport())
  {
    m_screenOverlay->init(*m_shaderManager, m_window->getViewport());
  }

  m_screenOverlay->getImage()->fill({0, 0, 0, 0});

  m_inputHandler->update();

  if(m_inputHandler->hasDebouncedAction(hid::Action::Debug))
  {
    m_showDebugInfo = !m_showDebugInfo;
  }

  m_renderer->clear(
    gl::api::ClearBufferMask::ColorBufferBit | gl::api::ClearBufferMask::DepthBufferBit, {0, 0, 0, 0}, 1);

  return true;
}

bool Presenter::shouldClose() const
{
  return m_window->windowShouldClose();
}

void Presenter::setTrFont(std::unique_ptr<ui::TRFont>&& font)
{
  m_trFont = std::move(font);
}

void Presenter::swapBuffers()
{
  m_window->swapBuffers();
  m_soundEngine->update();
}

void Presenter::clear()
{
  m_renderer->resetScene();
  m_renderer->getCamera()->setFieldOfView(DefaultFov);
}

void Presenter::debounceInput()
{
  m_inputHandler->update();
  m_inputHandler->update();
}

void Presenter::apply(const render::RenderSettings& renderSettings)
{
  setFullscreen(renderSettings.fullscreen);
  m_renderPipeline->apply(renderSettings, *m_materialManager);
}
} // namespace engine
