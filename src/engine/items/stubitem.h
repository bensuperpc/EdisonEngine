#pragma once

#include "itemnode.h"

namespace engine
{
namespace items
{
class StubItem final : public ModelItemNode
{
public:
    StubItem(const gsl::not_null<level::Level*>& level,
             const std::string& name,
             const gsl::not_null<const loader::Room*>& room,
             const core::Angle& angle,
             const core::TRCoordinates& position,
             uint16_t activationState,
             int16_t darkness,
             const loader::SkeletalModelType& animatedModel)
            : ModelItemNode( level, name, room, angle, position, activationState, false, 0, darkness, animatedModel )
    {
    }
};


class ScriptedItem final : public ModelItemNode
{
public:
    ScriptedItem(const gsl::not_null<level::Level*>& level,
                 const std::string& name,
                 const gsl::not_null<const loader::Room*>& room,
                 const core::Angle& angle,
                 const core::TRCoordinates& position,
                 uint16_t activationState,
                 int16_t darkness,
                 const loader::SkeletalModelType& animatedModel,
                 const sol::table& objectInfo)
            : ModelItemNode( level, name, room, angle, position, activationState, false, objectInfo["flags"], darkness,
                             animatedModel )
            , m_objectInfo( objectInfo )
    {
        auto initialise = objectInfo["initialise"];
        if( initialise )
            initialise.call( m_state );
    }

private:
    sol::table m_objectInfo;
};
}
}
