#include "laranode.h"

#include "cameracontroller.h"
#include "level/level.h"
#include "render/textureanimator.h"

#include <boost/range/adaptors.hpp>
#include <chrono>


namespace engine
{
    void LaraNode::setTargetState(LaraStateId st)
    {
        ItemNode::setTargetState( static_cast<uint16_t>(st) );
    }


    loader::LaraStateId LaraNode::getTargetState() const
    {
        return static_cast<LaraStateId>(ItemNode::getTargetState());
    }


    void LaraNode::setAnimIdGlobal(loader::AnimationId anim, const boost::optional<uint16_t>& firstFrame)
    {
        ItemNode::setAnimIdGlobal( static_cast<uint16_t>(anim), firstFrame.get_value_or( 0 ) );
    }


    void LaraNode::handleLaraStateOnLand(const std::chrono::microseconds& deltaTime)
    {
        CollisionInfo collisionInfo;
        collisionInfo.position = getPosition();
        collisionInfo.collisionRadius = 100; //!< @todo MAGICK 100
        collisionInfo.frobbelFlags = CollisionInfo::FrobbelFlag10 | CollisionInfo::FrobbelFlag08;

        BOOST_ASSERT( m_currentStateHandler != nullptr );

        auto nextHandler = m_currentStateHandler->handleInput( collisionInfo );

        m_currentStateHandler->animate( collisionInfo, deltaTime );

        if( nextHandler.is_initialized() && nextHandler != m_currentStateHandler->getId() )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create( *nextHandler, *this );
            BOOST_LOG_TRIVIAL( debug ) << "New input state override: "
                                       << loader::toString( m_currentStateHandler->getId() );
        }

        // "slowly" revert rotations to zero
        if( getRotation().Z < 0_deg )
        {
            addZRotation( core::makeInterpolatedValue( +1_deg ).getScaled( deltaTime ) );
            if( getRotation().Z >= 0_deg )
                setZRotation( 0_deg );
        }
        else if( getRotation().Z > 0_deg )
        {
            addZRotation( -core::makeInterpolatedValue( +1_deg ).getScaled( deltaTime ) );
            if( getRotation().Z <= 0_deg )
                setZRotation( 0_deg );
        }

        if( getYRotationSpeed() < 0_deg )
        {
            m_yRotationSpeed.add( 2_deg, deltaTime ).limitMax( 0_deg );
        }
        else if( getYRotationSpeed() > 0_deg )
        {
            m_yRotationSpeed.sub( 2_deg, deltaTime ).limitMin( 0_deg );
        }
        else
        {
            setYRotationSpeed( 0_deg );
        }

        addYRotation( m_yRotationSpeed.getScaled( deltaTime ) );

        applyTransform();

        if( getLevel().m_cameraController->getCamOverrideType() != 2 )
        {
            auto x = makeInterpolatedValue( getLevel().m_cameraController->getHeadRotation().X * 0.125f )
                    .getScaled( deltaTime );
            auto y = makeInterpolatedValue( getLevel().m_cameraController->getHeadRotation().Y * 0.125f )
                    .getScaled( deltaTime );
            getLevel().m_cameraController->addHeadRotationXY( -x, -y );
            getLevel().m_cameraController->setTorsoRotation( getLevel().m_cameraController->getHeadRotation() );
        }

        //BOOST_LOG_TRIVIAL(debug) << "Post-processing state: " << loader::toString(m_currentStateHandler->getId());

/*
        if( getCurrentFrameChangeType() == FrameChangeType::SameFrame )
        {
            return;
        }
*/

        testInteractions();

        nextHandler = m_currentStateHandler->postprocessFrame( collisionInfo );
        if( nextHandler.is_initialized() && *nextHandler != m_currentStateHandler->getId() )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create( *nextHandler, *this );
            BOOST_LOG_TRIVIAL( debug ) << "New post-processing state override: "
                                       << loader::toString( m_currentStateHandler->getId() );
        }

        updateFloorHeight( -381 );
        handleTriggers( collisionInfo.current.floor.lastTriggerOrKill, false );

#ifndef NDEBUG
        lastUsedCollisionInfo = collisionInfo;
#endif
    }


    void LaraNode::handleLaraStateDiving(const std::chrono::microseconds& deltaTime)
    {
        CollisionInfo collisionInfo;
        collisionInfo.position = getPosition();
        collisionInfo.collisionRadius = 300; //!< @todo MAGICK 300
        collisionInfo.frobbelFlags &= ~(CollisionInfo::FrobbelFlag10 | CollisionInfo::FrobbelFlag08
                                        | CollisionInfo::FrobbelFlag_UnwalkableDeadlyFloor
                                        | CollisionInfo::FrobbelFlag_UnwalkableSteepFloor
                                        | CollisionInfo::FrobbelFlag_UnpassableSteepUpslant);
        collisionInfo.neededCeilingDistance = 400;
        collisionInfo.passableFloorDistanceBottom = loader::HeightLimit;
        collisionInfo.passableFloorDistanceTop = -400;

        BOOST_ASSERT( m_currentStateHandler != nullptr );

        auto nextHandler = m_currentStateHandler->handleInput( collisionInfo );

        m_currentStateHandler->animate( collisionInfo, deltaTime );

        if( nextHandler.is_initialized() && *nextHandler != m_currentStateHandler->getId() )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create( *nextHandler, *this );
            BOOST_LOG_TRIVIAL( debug ) << "New input state override: "
                                       << loader::toString( m_currentStateHandler->getId() );
        }

        // "slowly" revert rotations to zero
        if( getRotation().Z < 0_deg )
        {
            addZRotation( core::makeInterpolatedValue( +2_deg ).getScaled( deltaTime ) );
            if( getRotation().Z >= 0_deg )
                setZRotation( 0_deg );
        }
        else if( getRotation().Z > 0_deg )
        {
            addZRotation( -core::makeInterpolatedValue( +2_deg ).getScaled( deltaTime ) );
            if( getRotation().Z <= 0_deg )
                setZRotation( 0_deg );
        }
        setXRotation( util::clamp( getRotation().X, -100_deg, +100_deg ) );
        setZRotation( util::clamp( getRotation().Z, -22_deg, +22_deg ) );
        {
            auto pos = getPosition();
            pos.X += getRotation().Y.sin() * getRotation().X.cos() * getFallSpeed().getScaled( deltaTime ) / 4;
            pos.Y -= getRotation().X.sin() * getFallSpeed().getScaled( deltaTime ) / 4;
            pos.Z += getRotation().Y.cos() * getRotation().X.cos() * getFallSpeed().getScaled( deltaTime ) / 4;
            setPosition( pos );
        }

/*
        if( getCurrentFrameChangeType() == FrameChangeType::SameFrame )
        {
#ifndef NDEBUG
            lastUsedCollisionInfo = collisionInfo;
#endif
            return;
        }
        */

        testInteractions();

        nextHandler = m_currentStateHandler->postprocessFrame( collisionInfo );
        if( nextHandler.is_initialized() && *nextHandler != m_currentStateHandler->getId() )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create( *nextHandler, *this );
            BOOST_LOG_TRIVIAL( debug ) << "New post-processing state override: "
                                       << loader::toString( m_currentStateHandler->getId() );
        }

        updateFloorHeight( 0 );
        handleTriggers( collisionInfo.current.floor.lastTriggerOrKill, false );
#ifndef NDEBUG
        lastUsedCollisionInfo = collisionInfo;
#endif
    }


    void LaraNode::handleLaraStateSwimming(const std::chrono::microseconds& deltaTime)
    {
        CollisionInfo collisionInfo;
        collisionInfo.position = getPosition();
        collisionInfo.collisionRadius = 100; //!< @todo MAGICK 100
        collisionInfo.frobbelFlags &= ~(CollisionInfo::FrobbelFlag10 | CollisionInfo::FrobbelFlag08
                                        | CollisionInfo::FrobbelFlag_UnwalkableDeadlyFloor
                                        | CollisionInfo::FrobbelFlag_UnwalkableSteepFloor
                                        | CollisionInfo::FrobbelFlag_UnpassableSteepUpslant);
        collisionInfo.neededCeilingDistance = 100;
        collisionInfo.passableFloorDistanceBottom = loader::HeightLimit;
        collisionInfo.passableFloorDistanceTop = -100;

        setCameraRotationX( -22_deg );

        BOOST_ASSERT( m_currentStateHandler != nullptr );

        auto nextHandler = m_currentStateHandler->handleInput( collisionInfo );

        m_currentStateHandler->animate( collisionInfo, deltaTime );

        if( nextHandler.is_initialized() && *nextHandler != m_currentStateHandler->getId() )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create( *nextHandler, *this );
            BOOST_LOG_TRIVIAL( debug ) << "New input state override: "
                                       << loader::toString( m_currentStateHandler->getId() );
        }

        // "slowly" revert rotations to zero
        if( getRotation().Z < 0_deg )
        {
            addZRotation( core::makeInterpolatedValue( +2_deg ).getScaled( deltaTime ) );
            if( getRotation().Z >= 0_deg )
                setZRotation( 0_deg );
        }
        else if( getRotation().Z > 0_deg )
        {
            addZRotation( -core::makeInterpolatedValue( +2_deg ).getScaled( deltaTime ) );
            if( getRotation().Z <= 0_deg )
                setZRotation( 0_deg );
        }

        setPosition( getPosition() + core::ExactTRCoordinates(
                getMovementAngle().sin() * getFallSpeed().getScaled( deltaTime ) / 4,
                0,
                getMovementAngle().cos() * getFallSpeed().getScaled( deltaTime ) / 4
        ) );

        if( getLevel().m_cameraController->getCamOverrideType() != 2 )
        {
            auto x = makeInterpolatedValue( getLevel().m_cameraController->getHeadRotation().X * 0.125f )
                    .getScaled( deltaTime );
            auto y = makeInterpolatedValue( getLevel().m_cameraController->getHeadRotation().Y * 0.125f )
                    .getScaled( deltaTime );
            getLevel().m_cameraController->addHeadRotationXY( -x, -y );
            auto r = getLevel().m_cameraController->getHeadRotation();
            r.X = 0_deg;
            r.Y *= 0.5f;
            getLevel().m_cameraController->setTorsoRotation( r );
        }

/*
        if( getCurrentFrameChangeType() == FrameChangeType::SameFrame )
        {
            updateFloorHeight(100);
#ifndef NDEBUG
            lastUsedCollisionInfo = collisionInfo;
#endif
            return;
        }
        */

        testInteractions();

        nextHandler = m_currentStateHandler->postprocessFrame( collisionInfo );
        if( nextHandler.is_initialized() && *nextHandler != m_currentStateHandler->getId() )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create(*nextHandler, *this);
            BOOST_LOG_TRIVIAL( debug ) << "New post-processing state override: "
                                       << loader::toString( m_currentStateHandler->getId() );
        }

        updateFloorHeight( 100 );
        handleTriggers( collisionInfo.current.floor.lastTriggerOrKill, false );
#ifndef NDEBUG
        lastUsedCollisionInfo = collisionInfo;
#endif
    }


    void LaraNode::placeOnFloor(const CollisionInfo& collisionInfo)
    {
        auto pos = getPosition();
        pos.Y += collisionInfo.current.floor.distance;
        setPosition( pos );
    }


    loader::LaraStateId LaraNode::getCurrentState() const
    {
        return m_currentStateHandler->getId();
    }


    loader::LaraStateId LaraNode::getCurrentAnimState() const
    {
        return static_cast<loader::LaraStateId>(ItemNode::getCurrentState());
    }


    LaraNode::~LaraNode() = default;


    void LaraNode::updateImpl(const std::chrono::microseconds& deltaTime)
    {
        static constexpr auto UVAnimTime = 3_frame;

        m_uvAnimTime += deltaTime;
        if( m_uvAnimTime >= UVAnimTime )
        {
            getLevel().m_textureAnimator->updateCoordinates( getLevel().m_textureProxies );
            m_uvAnimTime -= UVAnimTime;
        }

        if( m_currentStateHandler == nullptr )
        {
            m_currentStateHandler = lara::AbstractStateHandler::create( getCurrentAnimState(), *this );
        }

        if( m_underwaterState == UnderwaterState::OnLand && getCurrentRoom()->isWaterRoom() )
        {
            m_air = 1800;
            m_underwaterState = UnderwaterState::Diving;
            setFalling( false );
            setPosition( getPosition() + core::ExactTRCoordinates( 0, 100, 0 ) );
            updateFloorHeight( 0 );
            getLevel().stopSoundEffect( 30 );
            if( getCurrentAnimState() == LaraStateId::SwandiveBegin )
            {
                setXRotation( -45_deg );
                setTargetState( LaraStateId::UnderwaterDiving );
                setFallSpeed( getFallSpeed() * 2 );
            }
            else if( getCurrentAnimState() == LaraStateId::SwandiveEnd )
            {
                setXRotation( -85_deg );
                setTargetState( LaraStateId::UnderwaterDiving );
                setFallSpeed( getFallSpeed() * 2 );
            }
            else
            {
                setXRotation( -45_deg );
                setAnimIdGlobal( loader::AnimationId::FREE_FALL_TO_UNDERWATER, 1895 );
                setTargetState( LaraStateId::UnderwaterForward );
                m_currentStateHandler = lara::AbstractStateHandler::create( LaraStateId::UnderwaterDiving, *this );
                setFallSpeed( getFallSpeed() * 1.5f );
            }

            getLevel().m_cameraController->resetHeadTorsoRotation();

            //! @todo Show water splash effect
        }
        else if( m_underwaterState == UnderwaterState::Diving && !getCurrentRoom()->isWaterRoom() )
        {
            auto waterSurfaceHeight = getWaterSurfaceHeight();
            setFallSpeed( core::makeInterpolatedValue( 0.0f ) );
            setXRotation( 0_deg );
            setZRotation( 0_deg );
            getLevel().m_cameraController->resetHeadTorsoRotation();
            m_handStatus = 0;

            if( !waterSurfaceHeight || std::abs( *waterSurfaceHeight - getPosition().Y ) >= loader::QuarterSectorSize )
            {
                m_underwaterState = UnderwaterState::OnLand;
                setAnimIdGlobal( loader::AnimationId::FREE_FALL_FORWARD, 492 );
                setTargetState( LaraStateId::JumpForward );
                m_currentStateHandler = lara::AbstractStateHandler::create( LaraStateId::JumpForward, *this );
                //! @todo Check formula
                setHorizontalSpeed( getHorizontalSpeed() / 4 );
                setFalling( true );
            }
            else
            {
                m_underwaterState = UnderwaterState::Swimming;
                setAnimIdGlobal( loader::AnimationId::UNDERWATER_TO_ONWATER, 1937 );
                setTargetState( LaraStateId::OnWaterStop );
                m_currentStateHandler = lara::AbstractStateHandler::create( LaraStateId::OnWaterStop, *this );
                {
                    auto pos = getPosition();
                    pos.Y = *waterSurfaceHeight + 1;
                    setPosition( pos );
                }
                m_swimToDiveKeypressDuration = boost::none;
                updateFloorHeight( -381 );
                playSoundEffect( 36 );
            }
        }
        else if( m_underwaterState == UnderwaterState::Swimming && !getCurrentRoom()->isWaterRoom() )
        {
            m_underwaterState = UnderwaterState::OnLand;
            setAnimIdGlobal( loader::AnimationId::FREE_FALL_FORWARD, 492 );
            setTargetState( LaraStateId::JumpForward );
            m_currentStateHandler = lara::AbstractStateHandler::create( LaraStateId::JumpForward, *this );
            setFallSpeed( core::makeInterpolatedValue( 0.0f ) );
            //! @todo Check formula
            setHorizontalSpeed( getHorizontalSpeed() * 0.2f );
            setFalling( true );
            m_handStatus = 0;
            setXRotation( 0_deg );
            setZRotation( 0_deg );
            getLevel().m_cameraController->resetHeadTorsoRotation();
        }

        m_handlingFrame = true;

        if( m_underwaterState == UnderwaterState::OnLand )
        {
            m_air = 1800;
            handleLaraStateOnLand( deltaTime );
        }
        else if( m_underwaterState == UnderwaterState::Diving )
        {
            if( m_health >= 0 )
            {
                m_air.sub( 1, deltaTime );
                if( m_air < 0 )
                {
                    m_air = -1;
                    m_health.sub( 5, deltaTime );
                }
            }
            handleLaraStateDiving( deltaTime );
        }
        else if( m_underwaterState == UnderwaterState::Swimming )
        {
            if( m_health >= 0 )
            {
                m_air.add( 10, deltaTime ).limitMax( 1800 );
            }
            handleLaraStateSwimming( deltaTime );
        }

        m_handlingFrame = false;

        resetPose();
        patchBone( 7, getLevel().m_cameraController->getTorsoRotation().toMatrix() );
        patchBone( 14, getLevel().m_cameraController->getHeadRotation().toMatrix() );
    }


    void LaraNode::onFrameChanged(FrameChangeType frameChangeType)
    {
        const loader::Animation& animation = getLevel().m_animations[getAnimId()];
        if( animation.animCommandCount > 0 )
        {
            BOOST_ASSERT( animation.animCommandIndex < getLevel().m_animCommands.size() );
            const auto* cmd = &getLevel().m_animCommands[animation.animCommandIndex];
            for( uint16_t i = 0; i < animation.animCommandCount; ++i )
            {
                BOOST_ASSERT( cmd < &getLevel().m_animCommands.back() );
                const auto opcode = static_cast<AnimCommandOpcode>(*cmd);
                ++cmd;
                switch( opcode )
                {
                    case AnimCommandOpcode::SetPosition:
                        if( frameChangeType == FrameChangeType::EndOfAnim )
                        {
                            moveLocal(
                                    cmd[0],
                                    cmd[1],
                                    cmd[2]
                            );
                        }
                        cmd += 3;
                        break;
                    case AnimCommandOpcode::SetVelocity:
                        if( frameChangeType == FrameChangeType::EndOfAnim )
                        {
                            BOOST_LOG_TRIVIAL( debug ) << "End of animation velocity: override " << m_fallSpeedOverride
                                                       << ", cmd " << cmd[0];
                            setFallSpeed( core::makeInterpolatedValue<float>(
                                    m_fallSpeedOverride == 0 ? cmd[0] : m_fallSpeedOverride ) );
                            m_fallSpeedOverride = 0;
                            setHorizontalSpeed( core::makeInterpolatedValue<float>( cmd[1] ) );
                            setFalling( true );
                        }
                        cmd += 2;
                        break;
                    case AnimCommandOpcode::EmptyHands:
                        if( frameChangeType == FrameChangeType::EndOfAnim )
                        {
                            setHandStatus( 0 );
                        }
                        break;
                    case AnimCommandOpcode::PlaySound:
                        if( core::toFrame( getCurrentTime() ) == cmd[0] )
                        {
                            playSoundEffect( cmd[1] );
                        }
                        cmd += 2;
                        break;
                    case AnimCommandOpcode::PlayEffect:
                        if( core::toFrame( getCurrentTime() ) == cmd[0] )
                        {
                            BOOST_LOG_TRIVIAL( debug ) << "Anim effect: " << int( cmd[1] );
                            if( cmd[1] == 0 )
                                addYRotation( 180_deg );
                            else if( cmd[1] == 12 )
                                setHandStatus( 0 );
                            //! @todo Execute anim effect cmd[1]
                        }
                        cmd += 2;
                        break;
                    default:
                        break;
                }
            }
        }

        if(!m_handlingFrame)
            m_currentStateHandler = lara::AbstractStateHandler::create(getCurrentAnimState(), *this);
    }


    void LaraNode::updateFloorHeight(int dy)
    {
        auto pos = getPosition();
        pos.Y += dy;
        gsl::not_null<const loader::Room*> room = getCurrentRoom();
        auto sector = getLevel().findFloorSectorWithClampedPosition( pos.toInexact(), &room );
        setCurrentRoom( room );
        HeightInfo hi = HeightInfo::fromFloor( sector, pos.toInexact(), getLevel().m_cameraController );
        setFloorHeight( hi.distance );
    }


    void LaraNode::handleTriggers(const uint16_t* floorData, bool isDoppelganger)
    {
        if( floorData == nullptr )
            return;

        if( loader::extractFDFunction( *floorData ) == loader::FDFunction::Death )
        {
            if( !isDoppelganger )
            {
                if( util::fuzzyEqual( std::lround( getPosition().Y ), getFloorHeight(), 1L ) )
                {
                    //! @todo kill Lara
                }
            }

            if( loader::isLastFloordataEntry( *floorData ) )
                return;

            ++floorData;
        }

        const auto srcTriggerType = loader::extractTriggerType( *floorData );
        const auto srcTriggerArg = floorData[1];
        auto actionFloorData = floorData + 2;

        getLevel().m_cameraController->findCameraTarget( actionFloorData );

        bool runActions = false, switchIsOn = false;
        if( !isDoppelganger )
        {
            switch( srcTriggerType )
            {
                case loader::TriggerType::Trigger:runActions = true;
                    break;
                case loader::TriggerType::Pad:
                case loader::TriggerType::AntiPad:
                    runActions = util::fuzzyEqual( std::lround( getPosition().Y ), getFloorHeight(), 1L );
                    break;
                case loader::TriggerType::Switch:
                {
                    Expects(
                            getLevel().m_itemControllers.find( loader::extractTriggerFunctionParam( *actionFloorData ) )
                            != getLevel().m_itemControllers.end() );
                    ItemNode& swtch = *getLevel().m_itemControllers[loader::extractTriggerFunctionParam(
                            *actionFloorData )];
                    if( !swtch.triggerSwitch( srcTriggerArg ) )
                        return;

                    switchIsOn = (swtch.getCurrentState() == 1);
                }
                    ++actionFloorData;
                    runActions = true;
                    break;
                case loader::TriggerType::Key:
                {
                    Expects(
                            getLevel().m_itemControllers.find( loader::extractTriggerFunctionParam( *actionFloorData ) )
                            != getLevel().m_itemControllers.end() );
                    ItemNode& key = *getLevel().m_itemControllers[loader::extractTriggerFunctionParam(
                            *actionFloorData )];
                    if( key.triggerKey() )
                        runActions = true;
                }
                    ++actionFloorData;
                    return;
                case loader::TriggerType::Pickup:
                {
                    Expects(
                            getLevel().m_itemControllers.find( loader::extractTriggerFunctionParam( *actionFloorData ) )
                            != getLevel().m_itemControllers.end() );
                    ItemNode& pickup = *getLevel().m_itemControllers[loader::extractTriggerFunctionParam(
                            *actionFloorData )];
                    if( pickup.triggerPickUp() )
                        runActions = true;
                }
                    ++actionFloorData;
                    return;
                case loader::TriggerType::Combat:runActions = getHandStatus() == 4;
                    break;
                case loader::TriggerType::Heavy:
                case loader::TriggerType::Dummy:return;
                default:runActions = true;
                    break;
            }
        }
        else
        {
            runActions = srcTriggerType == loader::TriggerType::Heavy;
        }

        if( !runActions )
            return;

        ItemNode* lookAtItem = nullptr;

        while( true )
        {
            bool isLastAction = loader::isLastFloordataEntry( *actionFloorData );
            const auto actionParam = loader::extractTriggerFunctionParam( *actionFloorData );
            switch( loader::extractTriggerFunction( *actionFloorData++ ) )
            {
                case loader::TriggerFunction::Object:
                {
                    Expects( getLevel().m_itemControllers.find( actionParam ) != getLevel().m_itemControllers.end() );
                    ItemNode& item = *getLevel().m_itemControllers[actionParam];
                    if( (item.m_itemFlags & Oneshot) != 0 )
                        break;

                    item.m_triggerTimeout = std::chrono::microseconds( gsl::narrow_cast<uint8_t>( srcTriggerArg ) );
                    if( item.m_triggerTimeout.count() != 1 )
                        item.m_triggerTimeout = std::chrono::seconds(item.m_triggerTimeout.count());

                    //BOOST_LOG_TRIVIAL(trace) << "Setting trigger timeout of " << item.getName() << " to " << item.m_triggerTimeout << "ms";

                    if( srcTriggerType == loader::TriggerType::Switch )
                        item.m_itemFlags ^= srcTriggerArg & ActivationMask;
                    else if( srcTriggerType == loader::TriggerType::AntiPad )
                        item.m_itemFlags &= ~(srcTriggerArg & ActivationMask);
                    else
                        item.m_itemFlags |= srcTriggerArg & ActivationMask;

                    if( (item.m_itemFlags & ActivationMask) != ActivationMask )
                        break;

                    if( (srcTriggerArg & Oneshot) != 0 )
                        item.m_itemFlags |= Oneshot;

                    if( item.m_isActive )
                        break;

                    if( (item.m_characteristics & 0x02) == 0 )
                    {
                        item.m_flags2_02_toggledOn = true;
                        item.m_flags2_04_ready = false;
                        item.activate();
                        break;
                    }

                    if( !item.m_flags2_02_toggledOn && !item.m_flags2_04_ready )
                    {
                        //! @todo Implement baddie
                        item.m_flags2_02_toggledOn = true;
                        item.m_flags2_04_ready = false;
                        item.activate();
                        break;
                    }

                    if( !item.m_flags2_02_toggledOn || !item.m_flags2_04_ready )
                        break;

                    //! @todo Implement baddie
                    if( false ) //!< @todo unpauseBaddie
                    {
                        item.m_flags2_02_toggledOn = true;
                        item.m_flags2_04_ready = false;
                    }
                    else
                    {
                        item.m_flags2_02_toggledOn = true;
                        item.m_flags2_04_ready = true;
                    }
                    item.activate();
                }
                    break;
                case loader::TriggerFunction::CameraTarget:
                    getLevel().m_cameraController->setCamOverride( actionFloorData[0], actionParam, srcTriggerType,
                                                                   isDoppelganger, srcTriggerArg, switchIsOn );
                    isLastAction = loader::isLastFloordataEntry( *actionFloorData );
                    ++actionFloorData;
                    break;
                case loader::TriggerFunction::LookAt:lookAtItem = getLevel().getItemController( actionParam );
                    break;
                case loader::TriggerFunction::UnderwaterCurrent:
                    //! @todo handle underwater current
                    break;
                case loader::TriggerFunction::FlipMap:
                    //! @todo handle flip map
                    break;
                case loader::TriggerFunction::FlipOn:
                    //! @todo handle flip on
                    break;
                case loader::TriggerFunction::FlipOff:
                    //! @todo handle flip off
                    break;
                case loader::TriggerFunction::FlipEffect:
                    //! @todo handle flip effect
                    break;
                case loader::TriggerFunction::EndLevel:
                    //! @todo handle level end
                    break;
                case loader::TriggerFunction::PlayTrack:
                    getLevel().triggerCdTrack( actionParam, srcTriggerArg, srcTriggerType );
                    break;
                case loader::TriggerFunction::Secret:
                {
                    BOOST_ASSERT( actionParam < 16 );
                    const uint16_t mask = 1u << actionParam;
                    if( (m_secretsFoundBitmask & mask) == 0 )
                    {
                        m_secretsFoundBitmask |= mask;
                        getLevel().playCdTrack( 13 );
                    }
                }
                    break;
                default:break;
            }

            if( isLastAction )
                break;
        }

        getLevel().m_cameraController->setLookAtItem( lookAtItem );

        //! @todo Implement room swapping
    }


    boost::optional<int> LaraNode::getWaterSurfaceHeight() const
    {
        gsl::not_null<const loader::Sector*> sector = getCurrentRoom()
                ->getSectorByAbsolutePosition( getPosition().toInexact() );

        if( getCurrentRoom()->isWaterRoom() )
        {
            while( true )
            {
                if( sector->roomAbove == 0xff )
                    break;

                BOOST_ASSERT( sector->roomAbove < getLevel().m_rooms.size() );
                const auto& room = getLevel().m_rooms[sector->roomAbove];
                if( !room.isWaterRoom() )
                    break;

                sector = room.getSectorByAbsolutePosition( getPosition().toInexact() );
            }

            return sector->ceilingHeight * loader::QuarterSectorSize;
        }

        while( true )
        {
            if( sector->roomBelow == 0xff )
                break;

            BOOST_ASSERT( sector->roomBelow < getLevel().m_rooms.size() );
            const auto& room = getLevel().m_rooms[sector->roomBelow];
            if( room.isWaterRoom() )
            {
                return sector->floorHeight * loader::QuarterSectorSize;
            }

            sector = room.getSectorByAbsolutePosition( getPosition().toInexact() );
        }

        return sector->ceilingHeight * loader::QuarterSectorSize;
    }


    void LaraNode::setCameraRotation(core::Angle x, core::Angle y)
    {
        getLevel().m_cameraController->setLocalRotation( x, y );
    }


    void LaraNode::setCameraRotationY(core::Angle y)
    {
        getLevel().m_cameraController->setLocalRotationY( y );
    }


    void LaraNode::setCameraRotationX(core::Angle x)
    {
        getLevel().m_cameraController->setLocalRotationX( x );
    }


    void LaraNode::setCameraDistance(int d)
    {
        getLevel().m_cameraController->setLocalDistance( d );
    }


    void LaraNode::setCameraUnknown1(int k)
    {
        getLevel().m_cameraController->setUnknown1( k );
    }


    void LaraNode::testInteractions()
    {
        m_flags2_10 = false;

        if( m_health < 0 )
            return;

        std::set<const loader::Room*> rooms;
        rooms.insert( getCurrentRoom() );
        for( const loader::Portal& p : getCurrentRoom()->portals )
            rooms.insert( &getLevel().m_rooms[p.adjoining_room] );

        for( const std::shared_ptr<ItemNode>& item : getLevel().m_itemControllers | boost::adaptors::map_values )
        {
            if( rooms.find( item->getCurrentRoom() ) == rooms.end() )
                continue;

            if( !item->m_flags2_20 )
                continue;

            if( item->m_flags2_04_ready && item->m_flags2_02_toggledOn )
                continue;

            const auto d = getPosition() - item->getPosition();
            if( std::abs( d.X ) >= 4096 || std::abs( d.Y ) >= 4096 || std::abs( d.Z ) >= 4096 )
                continue;

            item->onInteract( *this );
        }
    }
}
