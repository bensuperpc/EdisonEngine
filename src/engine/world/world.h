#pragma once

#include "animation.h"
#include "atlastile.h"
#include "audio/soundengine.h"
#include "box.h"
#include "engine/floordata/floordata.h"
#include "engine/objectmanager.h"
#include "loader/file/datatypes.h"
#include "loader/file/item.h"
#include "mesh.h"
#include "room.h"
#include "skeletalmodeltype.h"
#include "sprite.h"
#include "staticmesh.h"
#include "transition.h"
#include "ui/pickupwidget.h"

#include <pybind11/pytypes.h>

namespace gl
{
class CImgWrapper;
}

namespace loader::file
{
enum class AnimationId : uint16_t;
struct AnimFrame;
struct Mesh;
struct SkeletalModelType;
} // namespace loader::file

namespace loader::file::level
{
class Level;
}

namespace render
{
class TextureAnimator;
}

namespace engine::objects
{
class ModelObject;
class PickupObject;
} // namespace engine::objects

namespace engine
{
class Presenter;
class Engine;
class AudioEngine;
struct SavegameMeta;
class CameraController;
class Player;
enum class TR1TrackId : int32_t;
} // namespace engine

namespace engine::world
{
struct Animation;
class RenderMeshData;

extern std::tuple<int8_t, int8_t> getFloorSlantInfo(gsl::not_null<const Sector*> sector, const core::TRVec& position);

class World final
{
public:
  explicit World(Engine& engine,
                 std::unique_ptr<loader::file::level::Level>&& level,
                 std::string title,
                 size_t totalSecrets,
                 const std::optional<TR1TrackId>& track,
                 bool useAlternativeLara,
                 std::unordered_map<std::string, std::unordered_map<TR1ItemId, std::string>> itemTitles,
                 std::shared_ptr<Player> player);

  ~World();

  [[nodiscard]] const auto& getLevel() const
  {
    BOOST_ASSERT(m_level != nullptr);
    return *m_level;
  }

  [[nodiscard]] bool roomsAreSwapped() const
  {
    return m_roomsAreSwapped;
  }

  CameraController& getCameraController()
  {
    return *m_cameraController;
  }

  [[nodiscard]] const CameraController& getCameraController() const
  {
    return *m_cameraController;
  }

  ObjectManager& getObjectManager()
  {
    return m_objectManager;
  }

  [[nodiscard]] const ObjectManager& getObjectManager() const
  {
    return m_objectManager;
  }

  void finishLevel()
  {
    m_levelFinished = true;
  }

  bool levelFinished() const
  {
    return m_levelFinished;
  }

  void setGlobalEffect(size_t fx)
  {
    m_activeEffect = fx;
    m_effectTimer = 0_frame;
  }

  template<typename T>
  std::shared_ptr<T> createObject(const core::TypeId type,
                                  const gsl::not_null<const Room*>& room,
                                  const core::Angle& angle,
                                  const core::TRVec& position,
                                  const uint16_t activationState)
  {
    const auto& model = findAnimatedModelForType(type);
    if(model == nullptr)
      return nullptr;

    loader::file::Item item;
    item.type = type;
    item.room = uint16_t(-1);
    item.position = position;
    item.rotation = angle;
    item.shade = core::Shade{core::Shade::type{0}};
    item.activationState = activationState;

    auto object = std::make_shared<T>(this, room, item, model.get());

    m_objectManager.registerDynamicObject(object);
    addChild(room->node, object->getNode());

    return object;
  }

  template<typename T>
  std::shared_ptr<T> createObject(const core::RoomBoundPosition& position)
  {
    auto object = std::make_shared<T>(this, position);

    m_objectManager.registerDynamicObject(object);

    return object;
  }

  void swapAllRooms();
  bool isValid(const loader::file::AnimFrame* frame) const;
  void swapWithAlternate(Room& orig, Room& alternate);
  [[nodiscard]] const std::vector<Box>& getBoxes() const;
  [[nodiscard]] const std::vector<Room>& getRooms() const;
  std::vector<Room>& getRooms();
  [[nodiscard]] const StaticMesh* findStaticMeshById(core::StaticMeshId meshId) const;
  [[nodiscard]] std::shared_ptr<render::scene::Mesh> findStaticRenderMeshById(core::StaticMeshId meshId) const;
  [[nodiscard]] const std::unique_ptr<SpriteSequence>& findSpriteSequenceForType(core::TypeId type) const;
  [[nodiscard]] const Animation& getAnimation(loader::file::AnimationId id) const;
  [[nodiscard]] const std::vector<loader::file::CinematicFrame>& getCinematicFrames() const;
  [[nodiscard]] const std::vector<loader::file::Camera>& getCameras() const;
  [[nodiscard]] const std::vector<int16_t>& getAnimCommands() const;
  void update(bool godMode);
  void dinoStompEffect(objects::Object& object);
  void laraNormalEffect();
  void laraBubblesEffect(objects::Object& object);
  void finishLevelEffect();
  void earthquakeEffect();
  void floodEffect();
  void chandelierEffect();
  void raisingBlockEffect();
  void stairsToSlopeEffect();
  void sandEffect();
  void explosionEffect();
  void laraHandsFreeEffect();
  void flipMapEffect();
  void chainBlockEffect();
  void flickerEffect();
  void doGlobalEffect();
  void loadSceneData(const std::vector<gsl::not_null<const Mesh*>>& meshesDirect);
  std::shared_ptr<objects::PickupObject>
    createPickup(core::TypeId type, const gsl::not_null<const Room*>& room, const core::TRVec& position);
  void useAlternativeLaraAppearance(bool withHead = false);
  void runEffect(size_t id, objects::Object* object);
  [[nodiscard]] const std::unique_ptr<SkeletalModelType>& findAnimatedModelForType(core::TypeId type) const;
  [[nodiscard]] const std::vector<Animation>& getAnimations() const;
  [[nodiscard]] const std::vector<int16_t>& getPoseFrames() const;
  [[nodiscard]] gsl::not_null<std::shared_ptr<RenderMeshData>> getRenderMesh(size_t idx) const;
  [[nodiscard]] const std::vector<Mesh>& getMeshes() const;
  void turn180Effect(objects::Object& object);
  void unholsterRightGunEffect(const objects::ModelObject& object);
  [[nodiscard]] std::array<gl::SRGBA8, 256> getPalette() const;
  [[nodiscard]] const loader::file::Palette& getRawPalette() const;
  void handleCommandSequence(const floordata::FloorDataValue* floorData, bool fromHeavy);
  core::TypeId find(const SkeletalModelType* model) const;
  core::TypeId find(const Sprite* sprite) const;
  void serialize(const serialization::Serializer<World>& ser);
  void gameLoop(bool godMode);
  bool cinematicLoop();
  void load(const std::filesystem::path& filename);
  void load(size_t slot);
  void save(const std::filesystem::path& filename);
  void save(size_t slot);
  [[nodiscard]] std::map<size_t, SavegameMeta> getSavedGames() const;
  [[nodiscard]] bool hasSavedGames() const;

  [[nodiscard]] const Presenter& getPresenter() const;
  [[nodiscard]] Presenter& getPresenter();

  auto getPierre() const
  {
    return m_pierre;
  }

  void setPierre(objects::Object* pierre)
  {
    m_pierre = pierre;
  }

  const Engine& getEngine() const
  {
    return m_engine;
  }

  Engine& getEngine()
  {
    return m_engine;
  }

  const AudioEngine& getAudioEngine() const
  {
    return *m_audioEngine;
  }

  AudioEngine& getAudioEngine()
  {
    return *m_audioEngine;
  }

  const std::string& getTitle() const
  {
    return m_title;
  }

  auto getTotalSecrets() const
  {
    return m_totalSecrets;
  }

  [[nodiscard]] const auto& getTextureAnimator() const
  {
    BOOST_ASSERT(m_textureAnimator != nullptr);
    return *m_textureAnimator;
  }

  [[nodiscard]] auto& getTextureAnimator()
  {
    BOOST_ASSERT(m_textureAnimator != nullptr);
    return *m_textureAnimator;
  }

  void addPickupWidget(Sprite sprite)
  {
    m_pickupWidgets.emplace_back(75_frame, std::move(sprite));
  }

  std::optional<std::string> getItemTitle(TR1ItemId id) const;

  auto& getPlayer()
  {
    Expects(m_player != nullptr);
    return *m_player;
  }

  const auto& getPlayer() const
  {
    Expects(m_player != nullptr);
    return *m_player;
  }

  const auto& getPlayerPtr() const
  {
    return m_player;
  }

  const auto& getAtlasTiles() const
  {
    return m_atlasTiles;
  }

  const auto& getSprites() const
  {
    return m_sprites;
  }

private:
  void createMipmaps(const std::vector<std::shared_ptr<gl::CImgWrapper>>& images, size_t nMips);

  void drawPickupWidgets(ui::Ui& ui);

  Engine& m_engine;

  std::unique_ptr<AudioEngine> m_audioEngine;

  gsl::not_null<std::unique_ptr<loader::file::level::Level>> m_level;
  std::unique_ptr<CameraController> m_cameraController;

  core::Frame m_effectTimer = 0_frame;
  std::optional<size_t> m_activeEffect{};
  std::shared_ptr<audio::Voice> m_globalSoundEffect{};

  bool m_roomsAreSwapped = false;
  std::vector<size_t> m_roomOrder;

  ObjectManager m_objectManager;

  bool m_levelFinished = false;

  struct PositionalEmitter final : public audio::Emitter
  {
    glm::vec3 position;

    PositionalEmitter(const glm::vec3& position, const gsl::not_null<audio::SoundEngine*>& engine)
        : Emitter{engine}
        , position{position}
    {
    }

    glm::vec3 getPosition() const override
    {
      return position;
    }
  };

  std::vector<PositionalEmitter> m_positionalEmitters;

  std::bitset<16> m_secretsFoundBitmask = 0;

  std::array<floordata::ActivationState, 10> m_mapFlipActivationStates;
  objects::Object* m_pierre = nullptr;
  std::string m_title{};
  size_t m_totalSecrets = 0;
  std::unordered_map<std::string, std::unordered_map<TR1ItemId, std::string>> m_itemTitles{};
  std::shared_ptr<gl::Texture2DArray<gl::SRGBA8>> m_allTextures;
  core::Frame m_uvAnimTime = 0_frame;
  std::unique_ptr<render::TextureAnimator> m_textureAnimator;

  std::vector<ui::PickupWidget> m_pickupWidgets{};
  const std::shared_ptr<Player> m_player;

  std::vector<Animation> m_animations;
  std::vector<Transitions> m_transitions;
  std::vector<TransitionCase> m_transitionCases;
  std::vector<Box> m_boxes;
  std::unordered_map<core::StaticMeshId, StaticMesh> m_staticMeshes;
  std::vector<Mesh> m_meshes;
  std::map<core::TypeId, std::unique_ptr<SkeletalModelType>> m_animatedModels;
  std::vector<Sprite> m_sprites;
  std::map<core::TypeId, std::unique_ptr<SpriteSequence>> m_spriteSequences;
  std::vector<AtlasTile> m_atlasTiles;
  std::vector<Room> m_rooms;

  std::vector<gsl::not_null<const Mesh*>> initFromLevel();
  void connectSectors();
};
} // namespace engine::world