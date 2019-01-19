#pragma once

#include <level/level.h>
#include "itemnode.h"

namespace engine
{
namespace items
{
class LightningBall final : public ModelItemNode
{
public:
    LightningBall(const gsl::not_null<level::Level*>& level,
                  const gsl::not_null<const loader::Room*>& room,
                  const loader::Item& item,
                  const loader::SkeletalModelType& animatedModel,
                  const gsl::not_null<std::shared_ptr<gameplay::ShaderProgram>>& boltProgram);

    void update() override;

    void collide(LaraNode& lara, CollisionInfo& info) override;

    static constexpr const size_t SegmentPoints = 16;

private:
    static constexpr const size_t ChildBolts = 5;

    void prepareRender();

    size_t m_poles = 0;
    bool m_laraHit = false;
    int m_chargeTimeout = 1;
    bool m_shooting = false;
    glm::vec3 m_mainBoltEnd;

    struct ChildBolt
    {
        size_t startIndex = 0;
        glm::vec3 end{0.0f};
        std::shared_ptr<gameplay::Mesh> mesh;
    };

    std::array<ChildBolt, ChildBolts> m_childBolts;

    std::shared_ptr<gameplay::Mesh> m_mainBoltMesh;
};
}
}