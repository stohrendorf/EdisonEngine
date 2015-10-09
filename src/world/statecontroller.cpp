#include "statecontroller.h"

#include "animation/animids.h"
#include "audio/audio.h"
#include "audio/engine.h"
#include "character.h"
#include "character_controller.h"
#include "engine/engine.h"
#include "engine/system.h"
#include "resource.h"
#include "room.h"

#include <cassert>

namespace world
{

namespace
{

constexpr glm::float_t PenetrationTestOffset = 48.0f;        ///@TODO: tune it!
constexpr glm::float_t WalkForwardOffset = 96.0f;        ///@FIXME: find real offset
constexpr glm::float_t WalkBackOffset = 16.0f;
constexpr glm::float_t RunForwardOffset = 128.0f;       ///@FIXME: find real offset
constexpr glm::float_t CrawlForwardOffset = 256.0f;
constexpr glm::float_t LaraHangWallDistance = 128.0f - 24.0f;
constexpr glm::float_t LaraHangVerticalOffset = 12.0f;        // in original is 0, in real life hands are little more higher than edge
constexpr glm::float_t LaraTryHangWallOffset = 72.0f;        // It works more stable than 32 or 128
constexpr glm::float_t LaraHangSensorZ = 800.0f;       // It works more stable than 1024 (after collision critical fix, of course)

using animation::SSAnimation;

void onFrameStopTraverse(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        glm::vec4& v = ent->m_traversedObject->m_transform[3];
        int i = static_cast<int>(v[0] / MeteringSectorSize);
        v[0] = i * MeteringSectorSize + 512.0f;
        i = static_cast<int>(v[1] / MeteringSectorSize);
        v[1] = i * MeteringSectorSize + 512.0f;
        ent->m_traversedObject->updateRigidBody(true);
        ent->m_traversedObject = nullptr;
        ss_anim->onFrame = nullptr;
    }
}

void onFrameSetOnFloor(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_moveType = MoveType::OnFloor;
        ent->m_transform[3][2] = ent->m_heightInfo.floor_point[2];
        ent->ghostUpdate();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameSetOnFloorAfterClimb(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate /*state*/)
{
    // FIXME: this is more like an end-of-anim operation
    if(ss_anim->current_animation != ss_anim->lerp_last_animation)
    {
        ent->m_transform[3] = glm::vec4(ent->m_climb.point,1);

        // FIXME: position adjust after climb
        glm::vec3 climbfix(0, ent->m_climbR, 0);
        ent->m_transform[3] = glm::vec4(ent->m_climb.point + glm::mat3(ent->m_transform) * climbfix, 1.0f);

        ent->m_moveType = MoveType::OnFloor;
        ss_anim->onFrame = nullptr;
    }
}

void onFrameSetUnderwater(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_moveType = MoveType::Underwater;
        ss_anim->onFrame = nullptr;
    }
}

void onFrameSetFreeFalling(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_moveType = MoveType::FreeFalling;
        ss_anim->onFrame = nullptr;
    }
}

void onFrameSetCmdSlide(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_response.slide = SlideType::Back;
        ss_anim->onFrame = nullptr;
    }
}

void onFrameCorrectDivingAngle(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_angles[1] = -45.0;
        ent->updateTransform();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameToOnWater(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_transform[3][2] = ent->m_heightInfo.transition_level;
        ent->ghostUpdate();
        ent->m_moveType = MoveType::OnWater;
        ss_anim->onFrame = nullptr;
    }
}

void onFrameClimbOutOfWater(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_transform[3] = glm::vec4(ent->m_climb.point,0) + ent->m_transform[1] * 48.0f;             // temporary stick
        ent->m_transform[3][2] = ent->m_climb.point[2];
        ent->ghostUpdate();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameToEdgeClimb(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_transform[3][0] = ent->m_climb.point[0] - ent->m_transform[1][0] * ent->m_bf.boundingBox.max[1];
        ent->m_transform[3][1] = ent->m_climb.point[1] - ent->m_transform[1][1] * ent->m_bf.boundingBox.max[1];
        ent->m_transform[3][2] = ent->m_climb.point[2] - ent->m_bf.boundingBox.max[2];
        ent->ghostUpdate();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameToMonkeyswing(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_moveType = MoveType::Monkeyswing;
        ent->m_transform[3][2] = ent->m_heightInfo.ceiling_point[2] - ent->m_bf.boundingBox.max[2];
        ent->ghostUpdate();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameToTightrope(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_moveType = MoveType::Climbing;
        ent->ghostUpdate();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameFromTightrope(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        ent->m_moveType = MoveType::OnFloor;
        ent->ghostUpdate();
        ss_anim->onFrame = nullptr;
    }
}

void onFrameCrawlToClimb(Character* ent, SSAnimation* ss_anim, animation::AnimUpdate state)
{
    if(state == animation::AnimUpdate::NewAnim)
    {
        if(!ent->m_command.action)
        {
            ent->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
            ent->m_moveType = MoveType::FreeFalling;
            ent->m_moveDir = MoveDirection::Backward;
        }
        else
        {
            ent->setAnimation(TR_ANIMATION_LARA_HANG_IDLE, -1);
        }

        ent->m_bt.no_fix_all = false;
        onFrameToEdgeClimb(ent, ss_anim, state);
        ss_anim->onFrame = nullptr;
    }
}
} // anonymous namespace

StateController::StateController(Character *c)
    : m_character(c)
{
    assert( c != nullptr );

    on(LaraState::STOP, &StateController::stop);
    on(LaraState::JUMP_PREPARE, &StateController::jumpPrepare);
    on(LaraState::JUMP_BACK, &StateController::jumpBack);
    on(LaraState::JUMP_LEFT, &StateController::jumpLeft);
    on(LaraState::JUMP_RIGHT, &StateController::jumpRight);
    on(LaraState::RUN_BACK, &StateController::runBack);
    on(LaraState::TURN_LEFT_SLOW, &StateController::turnSlow);
    on(LaraState::TURN_RIGHT_SLOW, &StateController::turnSlow);
    on(LaraState::TURN_FAST, &StateController::turnFast);
    on(LaraState::RUN_FORWARD, &StateController::runForward);
    on(LaraState::SPRINT, &StateController::sprint);
    on(LaraState::WALK_FORWARD, &StateController::walkForward);
    on(LaraState::WADE_FORWARD, &StateController::wadeForward);
    on(LaraState::WALK_BACK, &StateController::walkBack);
    on(LaraState::WALK_LEFT, &StateController::walkLeft);
    on(LaraState::WALK_RIGHT, &StateController::walkRight);
    on(LaraState::SLIDE_BACK, &StateController::slideBack);
    on(LaraState::SLIDE_FORWARD, &StateController::slideForward);
    on(LaraState::PUSHABLE_GRAB, &StateController::pushableGrab);
    on(LaraState::PUSHABLE_PUSH, &StateController::pushablePush);
    on(LaraState::PUSHABLE_PULL, &StateController::pushablePull);
    on(LaraState::ROLL_FORWARD, &StateController::rollForward);
    on(LaraState::ROLL_BACKWARD, &StateController::rollBackward);
    on(LaraState::JUMP_UP, &StateController::jumpUp);
    on(LaraState::REACH, &StateController::reach);
    on(LaraState::HANG, &StateController::hang);
    on(LaraState::LADDER_IDLE, &StateController::ladderIdle);
    on(LaraState::LADDER_LEFT, &StateController::ladderLeft);
    on(LaraState::LADDER_RIGHT, &StateController::ladderRight);
    on(LaraState::LADDER_UP, &StateController::ladderUp);
    on(LaraState::LADDER_DOWN, &StateController::ladderDown);
    on(LaraState::SHIMMY_LEFT, &StateController::shimmyLeft);
    on(LaraState::SHIMMY_RIGHT, &StateController::shimmyRight);
    on(LaraState::ONWATER_EXIT, &StateController::onwaterExit);
    on(LaraState::JUMP_FORWARD, &StateController::jumpForwardFallBackward);
    on(LaraState::FALL_BACKWARD, &StateController::jumpForwardFallBackward);
    on(LaraState::UNDERWATER_DIVING, &StateController::underwaterDiving);
    on(LaraState::FREEFALL, &StateController::freefall);
    on(LaraState::SWANDIVE_BEGIN, &StateController::swandiveBegin);
    on(LaraState::SWANDIVE_END, &StateController::swandiveEnd);
    on(LaraState::UNDERWATER_STOP, &StateController::underwaterStop);
    on(LaraState::WATER_DEATH, &StateController::waterDeath);
    on(LaraState::UNDERWATER_FORWARD, &StateController::underwaterForward);
    on(LaraState::UNDERWATER_INERTIA, &StateController::underwaterInertia);
    on(LaraState::ONWATER_STOP, &StateController::onwaterStop);
    on(LaraState::ONWATER_FORWARD, &StateController::onwaterForward);
    on(LaraState::ONWATER_BACK, &StateController::onwaterBack);
    on(LaraState::ONWATER_LEFT, &StateController::onwaterLeft);
    on(LaraState::ONWATER_RIGHT, &StateController::onwaterRight);
    on(LaraState::CROUCH_IDLE, &StateController::crouchIdle);
    on(LaraState::CROUCH_ROLL, &StateController::roll);
    on(LaraState::SPRINT_ROLL, &StateController::roll);
    on(LaraState::CRAWL_IDLE, &StateController::crawlIdle);
    on(LaraState::CRAWL_TO_CLIMB, &StateController::crawlToClimb);
    on(LaraState::CRAWL_FORWARD, &StateController::crawlForward);
    on(LaraState::CRAWL_BACK, &StateController::crawlBack);
    on(LaraState::CRAWL_TURN_LEFT, &StateController::crawlTurnLeft);
    on(LaraState::CRAWL_TURN_RIGHT, &StateController::crawlTurnRight);
    on(LaraState::CROUCH_TURN_LEFT, &StateController::crouchTurnLeftRight);
    on(LaraState::CROUCH_TURN_RIGHT, &StateController::crouchTurnLeftRight);
    on(LaraState::MONKEYSWING_IDLE, &StateController::monkeyswingIdle);
    on(LaraState::MONKEYSWING_TURN_LEFT, &StateController::monkeyswingTurnLeft);
    on(LaraState::MONKEYSWING_TURN_RIGHT, &StateController::monkeyswingTurnRight);
    on(LaraState::MONKEYSWING_FORWARD, &StateController::monkeyswingForward);
    on(LaraState::MONKEYSWING_LEFT, &StateController::monkeyswingLeft);
    on(LaraState::MONKEYSWING_RIGHT, &StateController::monkeyswingRight);
    on(LaraState::TIGHTROPE_ENTER, &StateController::tightropeEnter);
    on(LaraState::TIGHTROPE_EXIT, &StateController::tightropeExit);
    on(LaraState::TIGHTROPE_IDLE, &StateController::tightropeIdle);
    on(LaraState::TIGHTROPE_FORWARD, &StateController::tightropeForward);
    on(LaraState::TIGHTROPE_BALANCING_LEFT, &StateController::tightropeBalancingLeft);
    on(LaraState::TIGHTROPE_BALANCING_RIGHT, &StateController::tightropeBalancingRight);
    on(LaraState::HANDSTAND, &StateController::fixEndOfClimbOn);
    on(LaraState::CLIMBING, &StateController::fixEndOfClimbOn);
    on(LaraState::CLIMB_TO_CRAWL, &StateController::fixEndOfClimbOn);
}

void StateController::handle(LaraState state)
{
    m_character->m_bf.animations.mode = animation::SSAnimationMode::NormalControl;
    m_character->updateCurrentHeight();

    if(m_character->m_response.killed)  // Stop any music, if Lara is dead.
    {
        engine::engine_world.audioEngine.endStreams(audio::StreamType::Oneshot);
        engine::engine_world.audioEngine.endStreams(audio::StreamType::Chat);
    }

    auto it = m_handlers.find(state);
    if(it != m_handlers.end())
    {
        (this->*it->second)();
    }
    else
    {
        m_character->m_command.rot[0] = 0.0;
        if((m_character->m_moveType == MoveType::Monkeyswing) || (m_character->m_moveType == MoveType::WallsClimb))
        {
            if(!m_character->m_command.action)
            {
                m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
                m_character->m_moveDir = MoveDirection::Stay;
                m_character->m_moveType = MoveType::FreeFalling;
            }
        }
    }

    // Extra animation control.

    switch(m_character->m_bf.animations.current_animation)
    {
    case TR_ANIMATION_LARA_STAY_JUMP_SIDES:
        m_character->m_bt.no_fix_body_parts |= BODY_PART_HEAD;
        break;

    case TR_ANIMATION_LARA_TRY_HANG_SOLID:
    case TR_ANIMATION_LARA_FLY_FORWARD_TRY_HANG:
        if((m_character->m_moveType == MoveType::FreeFalling) && m_character->m_command.action &&
                (m_character->m_speed[0] * m_character->m_transform[1][0] + m_character->m_speed[1] * m_character->m_transform[1][1] < 0.0))
        {
            m_character->m_speed[0] = -m_character->m_speed[0];
            m_character->m_speed[1] = -m_character->m_speed[1];
        }
        break;

    case TR_ANIMATION_LARA_AH_BACKWARD:
    case TR_ANIMATION_LARA_AH_FORWARD:
    case TR_ANIMATION_LARA_AH_LEFT:
    case TR_ANIMATION_LARA_AH_RIGHT:
        if(m_character->m_bf.animations.current_frame > 12)
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_SOLID, 0);
        break;
    };
}

void StateController::on(LaraState state, StateController::Handler handler)
{
    auto it = m_handlers.find(state);
    if(it != m_handlers.end())
        throw std::runtime_error("State handler already assigned");

    m_handlers[state] = handler;
}

void StateController::stop()
{
    // Reset directional flag only on intermediate animation!

    if(m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_STAY_SOLID)
    {
        m_character->m_moveDir = MoveDirection::Stay;
    }

    if(m_character->m_moveType == MoveType::OnFloor)
        m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS | BODY_PART_LEGS;

    m_character->m_command.rot[0] = 0;
    m_character->m_command.crouch |= isLowVerticalSpace();
    m_character->lean(0.0);

    if((m_character->m_climb.can_hang &&
        (m_character->m_climb.next_z_space >= m_character->m_height - LaraHangVerticalEpsilon) &&
        (m_character->m_moveType == MoveType::Climbing)) ||
            (m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_STAY_SOLID))
    {
        m_character->m_moveType = MoveType::OnFloor;
    }

    if(m_character->m_moveType == MoveType::OnFloor)
    {
        m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS_2 | BODY_PART_LEGS_3;
    }

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
        m_character->m_moveDir = MoveDirection::Stay;
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::DEATH);
    }
    else if(m_character->m_response.slide == SlideType::Front)
    {
        engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_LANDING, audio::EmitterType::Entity, m_character->getId());

        if(m_character->m_command.jump)
        {
            m_character->m_moveDir = MoveDirection::Forward;
            m_character->setAnimation(TR_ANIMATION_LARA_JUMP_FORWARD_BEGIN, 0);
        }
        else
        {
            m_character->setAnimation(TR_ANIMATION_LARA_SLIDE_FORWARD, 0);
        }
    }
    else if(m_character->m_response.slide == SlideType::Back)
    {
        if(m_character->m_command.jump)
        {
            m_character->m_moveDir = MoveDirection::Backward;
            m_character->setAnimation(TR_ANIMATION_LARA_JUMP_BACK_BEGIN, 0);
            engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_LANDING, audio::EmitterType::Entity, m_character->getId());
        }
        else
        {
            m_character->setAnimation(TR_ANIMATION_LARA_START_SLIDE_BACKWARD, 0);
        }
    }
    else if(m_character->m_command.jump)
    {
        if(m_character->m_heightInfo.quicksand == QuicksandPosition::None)
            setNextState(LaraState::JUMP_PREPARE);  // Jump sideways
    }
    else if(m_character->m_command.roll)
    {
        if(m_character->m_heightInfo.quicksand == QuicksandPosition::None && m_character->m_bf.animations.current_animation != TR_ANIMATION_LARA_CLIMB_2CLICK)
        {
            m_character->m_moveDir = MoveDirection::Forward;
            m_character->setAnimation(TR_ANIMATION_LARA_ROLL_BEGIN, 0);
        }
    }
    else if(m_character->m_command.crouch)
    {
        if(m_character->m_heightInfo.quicksand == QuicksandPosition::None)
            setNextState(LaraState::CROUCH_IDLE);
    }
    else if(m_character->m_command.action && m_character->findTraverse())
    {
        setNextState(LaraState::PUSHABLE_GRAB);
        glm::float_t t;
        if(m_character->m_transform[1].x > 0.9)
        {
            t = -m_character->m_traversedObject->m_bf.boundingBox.min[0] + 72.0f;
        }
        else if(m_character->m_transform[1].x < -0.9)
        {
            t = m_character->m_traversedObject->m_bf.boundingBox.max[0] + 72.0f;
        }
        else if(m_character->m_transform[1].y > 0.9)
        {
            t = -m_character->m_traversedObject->m_bf.boundingBox.min[1] + 72.0f;
        }
        else if(m_character->m_transform[1].y < -0.9)
        {
            t = m_character->m_traversedObject->m_bf.boundingBox.max[1] + 72.0f;
        }
        else
        {
            t = 512.0 + 72.0;  ///@PARANOID
        }
        const glm::vec4& v = m_character->m_traversedObject->m_transform[3];
        m_character->m_transform[3][0] = v[0] - m_character->m_transform[1].x * t;
        m_character->m_transform[3][1] = v[1] - m_character->m_transform[1].y * t;
    }
    else if(m_character->m_command.move[0] == 1)
    {
        if(m_character->m_command.shift)
        {
            glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
            glm::vec3 global_offset( m_character->m_transform[1] * WalkForwardOffset );
            global_offset[2] += m_character->m_bf.boundingBox.max[2];
            global_offset += glm::vec3(m_character->m_transform[3]);
            HeightInfo next_fc = initHeightInfo();
            Character::getHeightInfo(global_offset, &next_fc);
            if(((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00)) &&
                    (next_fc.floor_hit && (next_fc.floor_point[2] > m_character->m_transform[3][2] - m_character->m_maxStepUpHeight) && (next_fc.floor_point[2] <= m_character->m_transform[3][2] + m_character->m_maxStepUpHeight)))
            {
                m_character->m_moveType = MoveType::OnFloor;
                m_character->m_moveDir = MoveDirection::Forward;
                if((m_character->m_heightInfo.water || m_character->m_heightInfo.quicksand != QuicksandPosition::None) && m_character->m_heightInfo.floor_hit && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth))
                {
                    setNextState(LaraState::WADE_FORWARD);
                }
                else
                {
                    setNextState(LaraState::WALK_FORWARD);
                }
            }
        }       // end IF CMD->SHIFT
        else
        {
            glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
            glm::vec3 global_offset( m_character->m_transform[1] * RunForwardOffset );
            global_offset[2] += m_character->m_bf.boundingBox.max[2];
            HeightInfo next_fc = initHeightInfo();
            m_character->checkNextStep(global_offset, &next_fc);
            if(((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00)) && !m_character->hasStopSlant(next_fc))
            {
                m_character->m_moveType = MoveType::OnFloor;
                m_character->m_moveDir = MoveDirection::Forward;
                if((m_character->m_heightInfo.water || m_character->m_heightInfo.quicksand != QuicksandPosition::None) && m_character->m_heightInfo.floor_hit && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth))
                {
                    setNextState(LaraState::WADE_FORWARD);
                }
                else
                {
                    setNextState(LaraState::RUN_FORWARD);
                }
            }
        }

        if(m_character->m_command.action &&
                ((m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_STAY_IDLE) ||
                 (m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_STAY_SOLID) ||
                 (m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_WALL_SMASH_LEFT) ||
                 (m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_WALL_SMASH_RIGHT)))
        {
            glm::float_t t = m_character->m_forwardSize + LaraTryHangWallOffset;
            glm::vec3 global_offset( m_character->m_transform[1] * t );

            global_offset[2] += 0.5 * DEFAULT_CLIMB_UP_HEIGHT;
            HeightInfo next_fc = initHeightInfo();
            m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.5 * DEFAULT_CLIMB_UP_HEIGHT);
            if(m_character->m_climb.edge_hit &&
                    (m_character->m_climb.next_z_space >= m_character->m_height - LaraHangVerticalEpsilon) &&
                    (m_character->m_transform[3][2] + m_character->m_maxStepUpHeight < next_fc.floor_point[2]) &&
                    (m_character->m_transform[3][2] + 2944.0 >= next_fc.floor_point[2]) &&
                    (next_fc.floor_normale[2] >= m_character->m_criticalSlantZComponent)) // trying to climb on
            {
                if(m_character->m_transform[3][2] + 640.0 >= next_fc.floor_point[2])
                {
                    m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
                    m_character->m_transform[3][2] = next_fc.floor_point[2] - 512.0f;
                    m_character->m_climb.point = next_fc.floor_point;
                    m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_2CLICK, 0);
                    m_character->m_bt.no_fix_all = true;
                    m_character->m_bf.animations.onFrame = onFrameSetOnFloorAfterClimb;
                    return;
                }
                else if(m_character->m_transform[3][2] + 896.0 >= next_fc.floor_point[2])
                {
                    m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
                    m_character->m_transform[3][2] = next_fc.floor_point[2] - 768.0f;
                    m_character->m_climb.point = next_fc.floor_point;
                    m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_3CLICK, 0);
                    m_character->m_bt.no_fix_all = true;
                    m_character->m_bf.animations.onFrame = onFrameSetOnFloorAfterClimb;
                    return;
                }
            }   // end IF MOVE_LITTLE_CLIMBING

            global_offset[2] += 0.5 * DEFAULT_CLIMB_UP_HEIGHT;
            m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, DEFAULT_CLIMB_UP_HEIGHT);
            if(m_character->m_climb.edge_hit &&
                    (m_character->m_climb.next_z_space >= m_character->m_height - LaraHangVerticalEpsilon) &&
                    (m_character->m_transform[3][2] + m_character->m_maxStepUpHeight < next_fc.floor_point[2]) &&
                    (m_character->m_transform[3][2] + 2944.0 >= next_fc.floor_point[2]))  // Trying to climb on
            {
                if(m_character->m_transform[3][2] + 1920.0 >= next_fc.floor_point[2])
                {
                    // Fixme: grabheight/gravity values
                    const glm::float_t grabheight = 800.0f;  // Lara arms-up...estimated
                    const glm::float_t distance = next_fc.floor_point[2] - m_character->m_transform[3][2] - grabheight;
                    const glm::float_t gravity = 6;          // based on tr gravity accel (6 units / tick^2)
                    m_character->m_vspeed_override = 3.0f + sqrt(gravity * 2.0f * distance);
                    setNextState(LaraState::JUMP_UP);
                    return;
                }
            }   // end IF MOVE_BIG_CLIMBING

            m_character->m_climb = m_character->checkWallsClimbability();
            if(m_character->m_climb.wall_hit != ClimbType::None)
            {
                setNextState(LaraState::JUMP_UP);
                return;
            }
        }
    }       // end CMD->MOVE FORWARD
    else if(m_character->m_command.move[0] == -1)
    {
        if(m_character->m_command.shift)
        {
            glm::vec3 move( m_character->m_transform[1] * -PenetrationTestOffset );
            if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00))
            {
                glm::vec3 global_offset( m_character->m_transform[1] * -WalkBackOffset );
                global_offset[2] += m_character->m_bf.boundingBox.max[2];
                global_offset += glm::vec3(m_character->m_transform[3]);
                HeightInfo next_fc = initHeightInfo();
                Character::getHeightInfo(global_offset, &next_fc);
                if((next_fc.floor_hit && (next_fc.floor_point[2] > m_character->m_transform[3][2] - m_character->m_maxStepUpHeight) && (next_fc.floor_point[2] <= m_character->m_transform[3][2] + m_character->m_maxStepUpHeight)))
                {
                    m_character->m_moveDir = MoveDirection::Backward;
                    setNextState(LaraState::WALK_BACK);
                }
            }
        }
        else    // RUN BACK
        {
            glm::vec3 move( m_character->m_transform[1] * -PenetrationTestOffset );
            if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00))
            {
                m_character->m_moveDir = MoveDirection::Backward;
                if((m_character->m_heightInfo.water || m_character->m_heightInfo.quicksand != QuicksandPosition::None) && m_character->m_heightInfo.floor_hit && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth))
                {
                    setNextState(LaraState::WALK_BACK);
                }
                else
                {
                    setNextState(LaraState::RUN_BACK);
                }
            }
        }
    }       // end CMD->MOVE BACK
    else if(m_character->m_command.move[1] == 1)
    {
        if(m_character->m_command.shift)
        {
            glm::vec3 move( m_character->m_transform[0] * PenetrationTestOffset );
            if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00))
            {
                glm::vec3 global_offset( m_character->m_transform[0] * RunForwardOffset );
                global_offset[2] += m_character->m_bf.boundingBox.max[2];
                HeightInfo next_fc = initHeightInfo();
                if((m_character->m_response.horizontal_collide == 0) && isLittleStep(m_character->checkNextStep(global_offset, &next_fc)))
                {
                    m_character->m_command.rot[0] = 0.0;
                    m_character->m_moveDir = MoveDirection::Right;
                    setNextState(LaraState::WALK_RIGHT);
                }
            }
        }       //end IF CMD->SHIFT
        else
        {
            setNextState(LaraState::TURN_RIGHT_SLOW);
        }
    }       // end MOVE RIGHT
    else if(m_character->m_command.move[1] == -1)
    {
        if(m_character->m_command.shift)
        {
            glm::vec3 move( m_character->m_transform[0] * -PenetrationTestOffset );
            if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00))
            {
                glm::vec3 global_offset( m_character->m_transform[0] * -RunForwardOffset );
                global_offset[2] += m_character->m_bf.boundingBox.max[2];
                HeightInfo next_fc = initHeightInfo();
                if((m_character->m_response.horizontal_collide == 0) && isLittleStep(m_character->checkNextStep(global_offset, &next_fc)))
                {
                    m_character->m_command.rot[0] = 0.0;
                    m_character->m_moveDir = MoveDirection::Left;
                    setNextState(LaraState::WALK_LEFT);
                }
            }
        }       //end IF CMD->SHIFT
        else
        {
            setNextState(LaraState::TURN_LEFT_SLOW);
        }
    }       // end MOVE LEFT
}

void StateController::jumpPrepare()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS | BODY_PART_HANDS | BODY_PART_HEAD;
    m_character->m_command.rot[0] = 0;
    m_character->lean(0.0);

    if(m_character->m_response.slide == SlideType::Back)      // Slide checking is only for jumps direction correction!
    {
        m_character->setAnimation(TR_ANIMATION_LARA_JUMP_BACK_BEGIN, 0);
        m_character->m_command.move[0] = -1;
    }
    else if(m_character->m_response.slide == SlideType::Front)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_JUMP_FORWARD_BEGIN, 0);
        m_character->m_command.move[0] = 1;
    }
    if((m_character->m_heightInfo.water || m_character->m_heightInfo.quicksand != QuicksandPosition::None) && m_character->m_heightInfo.floor_hit && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth))
    {
        //Stay, directional jumps are not allowed whilst in wade depth
    }
    else if(m_character->m_command.move[0] == 1)
    {
        m_character->m_moveDir = MoveDirection::Forward;
        glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
        if(m_character->checkNextPenetration(move) == 0)
        {
            setNextState(LaraState::JUMP_FORWARD);  // Jump forward
        }
    }
    else if(m_character->m_command.move[0] == -1)
    {
        m_character->m_moveDir = MoveDirection::Backward;
        glm::vec3 move( m_character->m_transform[1] * -PenetrationTestOffset );
        if(m_character->checkNextPenetration(move) == 0)
        {
            setNextState(LaraState::JUMP_BACK);  // Jump backward
        }
    }
    else if(m_character->m_command.move[1] == 1)
    {
        m_character->m_moveDir = MoveDirection::Right;
        glm::vec3 move( m_character->m_transform[0] * PenetrationTestOffset );
        if(m_character->checkNextPenetration(move) == 0)
        {
            setNextState(LaraState::JUMP_LEFT);  // Jump right
        }
    }
    else if(m_character->m_command.move[1] == -1)
    {
        m_character->m_moveDir = MoveDirection::Left;
        glm::vec3 move( m_character->m_transform[0] * -PenetrationTestOffset );
        if(m_character->checkNextPenetration(move) == 0)
        {
            setNextState(LaraState::JUMP_RIGHT);  // Jump left
        }
    }
}

void StateController::jumpBack()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS | BODY_PART_HANDS | BODY_PART_HEAD;
    m_character->m_command.rot[0] = 0.0;

    if(m_character->m_response.vertical_collide & 0x01 || m_character->m_moveType == MoveType::OnFloor)
    {
        if(m_character->m_heightInfo.quicksand != QuicksandPosition::None)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
        }
        else
        {
            setNextState(LaraState::STOP);  // Landing
        }
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        engine::Controls_JoyRumble(200.0, 200);
        m_character->setAnimation(TR_ANIMATION_LARA_SMASH_JUMP, 0);
        m_character->m_moveDir = MoveDirection::Forward;
        m_character->updateCurrentSpeed(true);
    }
    else if((m_character->m_moveType == MoveType::Underwater) || (m_character->m_speed[2] <= -FREE_FALL_SPEED_2))
    {
        setNextState(LaraState::FREEFALL);  // Free falling
    }
    else if(m_character->m_command.roll)
    {
        setNextState(LaraState::JUMP_ROLL);
    }
}

void StateController::jumpLeft()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS | BODY_PART_HANDS | BODY_PART_HEAD;
    m_character->m_command.rot[0] = 0.0;

    if(m_character->m_response.vertical_collide & 0x01 || m_character->m_moveType == MoveType::OnFloor)
    {
        if(m_character->m_heightInfo.quicksand != QuicksandPosition::None)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
        }
        else
        {
            setNextState(LaraState::STOP);  // Landing
        }
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        engine::Controls_JoyRumble(200.0, 200);
        m_character->setAnimation(TR_ANIMATION_LARA_SMASH_JUMP, 0);
        m_character->m_moveDir = MoveDirection::Right;
        m_character->updateCurrentSpeed(true);
    }
    else
    {
        setNextState(LaraState::FREEFALL);
    }
}

void StateController::jumpRight()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS | BODY_PART_HANDS | BODY_PART_HEAD;
    m_character->m_command.rot[0] = 0.0;

    if(m_character->m_response.vertical_collide & 0x01 || m_character->m_moveType == MoveType::OnFloor)
    {
        if(m_character->m_heightInfo.quicksand != QuicksandPosition::None)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
        }
        else
        {
            setNextState(LaraState::STOP);  // Landing
        }
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        engine::Controls_JoyRumble(200.0, 200);
        m_character->setAnimation(TR_ANIMATION_LARA_SMASH_JUMP, 0);
        m_character->m_moveDir = MoveDirection::Left;
        m_character->updateCurrentSpeed(true);
    }
    else
    {
        setNextState(LaraState::FREEFALL);
    }
}

void StateController::runBack()
{
    m_character->m_moveDir = MoveDirection::Backward;

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->m_moveDir = MoveDirection::Forward;
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_BACK, 0);
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_2CLICK_END, 0);
    }
}

void StateController::turnSlow()
{
    m_character->m_command.rot[0] *= 0.7f;
    m_character->m_moveDir = MoveDirection::Stay;
    m_character->lean(0.0);
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS_2 | BODY_PART_LEGS_3;

    if(m_character->m_command.move[0] == 1)
    {
        Substance substance_state = m_character->getSubstanceState();
        if((substance_state == Substance::None) ||
                (substance_state == Substance::WaterShallow))
        {
            if(m_character->m_command.shift)
            {
                setNextState(LaraState::WALK_FORWARD);
                m_character->m_moveDir = MoveDirection::Forward;
            }
            else
            {
                setNextState(LaraState::RUN_FORWARD);
                m_character->m_moveDir = MoveDirection::Forward;
            }
        }
        else
        {
            setNextState(LaraState::WADE_FORWARD);
            m_character->m_moveDir = MoveDirection::Forward;
        }
    }
    else if(((m_character->m_bf.animations.last_state == LaraState::TURN_LEFT_SLOW) && (m_character->m_command.move[1] == -1)) ||
            ((m_character->m_bf.animations.last_state == LaraState::TURN_RIGHT_SLOW) && (m_character->m_command.move[1] == 1)))
    {
        Substance substance_state = m_character->getSubstanceState();
        if(isLastFrame() &&
                (substance_state != Substance::WaterWade) &&
                (substance_state != Substance::QuicksandConsumed) &&
                (substance_state != Substance::QuicksandShallow))
        {
            setNextState(LaraState::TURN_FAST);
        }
    }
    else
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::turnFast()
{
    // 65 - wade
    m_character->m_moveDir = MoveDirection::Stay;
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS_2 | BODY_PART_LEGS_3;
    m_character->lean(0.0);

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
    }
    else if(m_character->m_command.move[0] == 1 && !m_character->m_command.jump && !m_character->m_command.crouch && m_character->m_command.shift)
    {
        setNextState(LaraState::WALK_FORWARD);
        m_character->m_moveDir = MoveDirection::Forward;
    }
    else if(m_character->m_command.move[0] == 1 && !m_character->m_command.jump && !m_character->m_command.crouch && !m_character->m_command.shift)
    {
        setNextState(LaraState::RUN_FORWARD);
        m_character->m_moveDir = MoveDirection::Forward;
    }
    else if(m_character->m_command.move[1] == 0)
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::runForward()
{
    glm::vec3 global_offset( m_character->m_transform[1] * RunForwardOffset );
    global_offset[2] += m_character->m_bf.boundingBox.max[2];
    HeightInfo next_fc = initHeightInfo();
    StepType nextStep = m_character->checkNextStep(global_offset, &next_fc);
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_command.crouch |= isLowVerticalSpace();

    if(m_character->m_moveType == MoveType::OnFloor)
        m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS | BODY_PART_LEGS;

    m_character->lean(6.0);

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_FORWARD, 0);
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::DEATH);
    }
    else if(m_character->m_response.slide == SlideType::Front)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_SLIDE_FORWARD, 0);
    }
    else if(m_character->m_response.slide == SlideType::Back)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_SLIDE_BACKWARD, 0);
        m_character->m_moveDir = MoveDirection::Backward;
    }
    else if(m_character->hasStopSlant(next_fc))
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
    }
    else if(m_character->m_command.crouch)
    {
        setNextState(LaraState::CROUCH_IDLE);
    }
    else if((m_character->m_command.move[0] == 1) && !m_character->m_command.crouch && (next_fc.floor_normale[2] >= m_character->m_criticalSlantZComponent) && nextStep == StepType::UpBig)
    {
        m_character->m_moveDir = MoveDirection::Stay;
        int i = m_character->getAnimDispatchCase(LaraState::STOP);  // Select correct anim dispatch.
        if(i == 0)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_RUN_UP_STEP_RIGHT, 0);
            m_character->m_transform[3][2] = next_fc.floor_point[2];
            m_character->m_moveDir = MoveDirection::Forward;
        }
        else //if(i == 1)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_RUN_UP_STEP_LEFT, 0);
            m_character->m_transform[3][2] = next_fc.floor_point[2];
            m_character->m_moveDir = MoveDirection::Forward;
        }
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        global_offset = glm::vec3(m_character->m_transform[1] * RunForwardOffset);
        global_offset[2] += 1024.0;
        if(m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_STAY_TO_RUN)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
        }
        else
        {
            engine::Controls_JoyRumble(200.0, 200);

            if(m_character->m_command.move[0] == 1)
            {
                int i = m_character->getAnimDispatchCase(LaraState::STOP);
                if(i == 1)
                {
                    m_character->setAnimation(TR_ANIMATION_LARA_WALL_SMASH_LEFT, 0);
                }
                else
                {
                    m_character->setAnimation(TR_ANIMATION_LARA_WALL_SMASH_RIGHT, 0);
                }
            }
            else
            {
                m_character->setAnimation(TR_ANIMATION_LARA_STAY_SOLID, 0);
            }
        }
        m_character->updateCurrentSpeed(false);
    }
    else if(m_character->m_command.move[0] == 1)  // If we continue running...
    {
        if((m_character->m_heightInfo.water || m_character->m_heightInfo.quicksand != QuicksandPosition::None) && m_character->m_heightInfo.floor_hit && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth))
        {
            setNextState(LaraState::WADE_FORWARD);
        }
        else if(m_character->m_command.shift)
        {
            setNextState(LaraState::WALK_FORWARD);
        }
        else if(m_character->m_command.jump)
        {
            setNextState(LaraState::JUMP_FORWARD);
        }
        else if(m_character->m_command.roll)
        {
            m_character->m_moveDir = MoveDirection::Forward;
            m_character->setAnimation(TR_ANIMATION_LARA_ROLL_BEGIN, 0);
        }
        else if(m_character->m_command.sprint)
        {
            setNextState(LaraState::SPRINT);
        }
    }
    else
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::sprint()
{
    glm::vec3 global_offset( m_character->m_transform[1] * RunForwardOffset );
    m_character->lean(12.0);
    global_offset[2] += m_character->m_bf.boundingBox.max[2];
    HeightInfo next_fc = initHeightInfo();
    StepType nextStep = m_character->checkNextStep(global_offset, &next_fc);
    m_character->m_command.crouch |= isLowVerticalSpace();

    if(m_character->m_moveType == MoveType::OnFloor)
    {
        m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS;
    }

    if(!m_character->getParam(PARAM_STAMINA))
    {
        setNextState(LaraState::RUN_FORWARD);
    }
    else if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_FORWARD, 0);
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::RUN_FORWARD);  // Normal run then die
    }
    else if(m_character->m_response.slide == SlideType::Front)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_SLIDE_FORWARD, 0);
    }
    else if(m_character->m_response.slide == SlideType::Back)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_SLIDE_BACKWARD, 0);
    }
    else if((next_fc.floor_normale[2] < m_character->m_criticalSlantZComponent) && nextStep > StepType::Horizontal)
    {
        m_character->m_currentSpeed = 0.0;
        m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
    }
    else if((next_fc.floor_normale[2] >= m_character->m_criticalSlantZComponent) && nextStep == StepType::UpBig)
    {
        setNextState(LaraState::RUN_FORWARD);  // Interrupt sprint
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        engine::Controls_JoyRumble(200.0, 200);

        int i = m_character->getAnimDispatchCase(LaraState::STOP);
        if(i == 1)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_WALL_SMASH_LEFT, 0);
        }
        else if(i == 0)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_WALL_SMASH_RIGHT, 0);
        }
        m_character->updateCurrentSpeed(false);
    }
    else if(!m_character->m_command.sprint)
    {
        if(m_character->m_command.move[0] == 1)
        {
            setNextState(LaraState::RUN_FORWARD);
        }
        else
        {
            setNextState(LaraState::STOP);
        }
    }
    else
    {
        if(m_character->m_command.jump == 1)
        {
            setNextState(LaraState::SPRINT_ROLL);
        }
        else if(m_character->m_command.roll == 1)
        {
            m_character->m_moveDir = MoveDirection::Forward;
            m_character->setAnimation(TR_ANIMATION_LARA_ROLL_BEGIN, 0);
        }
        else if(m_character->m_command.crouch)
        {
            setNextState(LaraState::CROUCH_IDLE);
        }
        else if(m_character->m_command.move[0] == 0)
        {
            setNextState(LaraState::STOP);
        }
    }
}

void StateController::walkForward()
{
    m_character->m_command.rot[0] *= 0.4f;
    m_character->lean(0.0);

    glm::vec3 global_offset( m_character->m_transform[1] * WalkForwardOffset );
    global_offset[2] += m_character->m_bf.boundingBox.max[2];
    HeightInfo next_fc = initHeightInfo();
    StepType nextStep = m_character->checkNextStep(global_offset, &next_fc);
    m_character->m_moveDir = MoveDirection::Forward;

    if(m_character->m_moveType == MoveType::OnFloor)
    {
        m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS;
    }

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::STOP);
    }
    else if((next_fc.floor_normale[2] >= m_character->m_criticalSlantZComponent) && nextStep == StepType::UpBig)
    {
        // Climb up

        m_character->m_moveDir = MoveDirection::Stay;
        int i = m_character->getAnimDispatchCase(LaraState::STOP);
        if(i == 1)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_WALK_UP_STEP_RIGHT, 0);
            m_character->m_transform[3] = glm::vec4(next_fc.floor_point, 1.0f);
            m_character->m_moveType = MoveType::OnFloor;
            m_character->m_moveDir = MoveDirection::Forward;
        }
        else
        {
            m_character->setAnimation(TR_ANIMATION_LARA_WALK_UP_STEP_LEFT, 0);
            m_character->m_transform[3] = glm::vec4(next_fc.floor_point, 1.0f);
            m_character->m_moveType = MoveType::OnFloor;
            m_character->m_moveDir = MoveDirection::Forward;
        }
    }
    else if((next_fc.floor_normale[2] >= m_character->m_criticalSlantZComponent) && nextStep == StepType::DownBig)
    {
        // Climb down

        m_character->m_moveDir = MoveDirection::Stay;
        int i = m_character->getAnimDispatchCase(LaraState::STOP);
        if(i == 1)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_WALK_DOWN_RIGHT, 0);
            m_character->m_climb.point = next_fc.floor_point;
            m_character->m_transform[3] = glm::vec4(next_fc.floor_point, 1.0f);
            m_character->m_moveType = MoveType::OnFloor;
            m_character->m_moveDir = MoveDirection::Forward;
        }
        else //if(i == 0)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_WALK_DOWN_LEFT, 0);
            m_character->m_climb.point = next_fc.floor_point;
            m_character->m_transform[3] = glm::vec4(next_fc.floor_point, 1.0f);
            m_character->m_moveType = MoveType::OnFloor;
            m_character->m_moveDir = MoveDirection::Forward;
        }
    }
    else if((m_character->m_response.horizontal_collide & 0x01) || !isWakableStep(nextStep) || (isLowVerticalSpace()))
    {
        // Too high!

        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
    }
    else if(m_character->m_command.move[0] != 1)
    {
        setNextState(LaraState::STOP);
    }
    else if((m_character->m_heightInfo.water || m_character->m_heightInfo.quicksand != QuicksandPosition::None) && m_character->m_heightInfo.floor_hit && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth))
    {
        setNextState(LaraState::WADE_FORWARD);
    }
    else if(m_character->m_command.move[0] == 1 && !m_character->m_command.crouch && !m_character->m_command.shift)
    {
        setNextState(LaraState::RUN_FORWARD);
    }
}

void StateController::wadeForward()
{
    m_character->m_command.rot[0] *= 0.4f;
    m_character->m_moveDir = MoveDirection::Forward;

    if(m_character->m_heightInfo.quicksand != QuicksandPosition::None)
    {
        m_character->m_currentSpeed = std::min(m_character->m_currentSpeed, MAX_SPEED_QUICKSAND);
    }

    if(m_character->m_command.move[0] == 1)
    {
        glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
        m_character->checkNextPenetration(move);
    }

    if(m_character->m_response.killed)
    {
        setNextState(LaraState::STOP);
    }

    if(!m_character->m_heightInfo.floor_hit || m_character->m_moveType == MoveType::FreeFalling)  // Free fall, then swim
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
    }
    else if(m_character->m_heightInfo.water)
    {
        if((m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] <= m_character->m_wadeDepth))
        {
            // run / walk case
            if((m_character->m_command.move[0] == 1) && (m_character->m_response.horizontal_collide == 0))
            {
                setNextState(LaraState::RUN_FORWARD);
            }
            else
            {
                setNextState(LaraState::STOP);
            }
        }
        else if(m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > (m_character->m_height - m_character->m_swimDepth))
        {
            // Swim case
            if(m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_height + m_character->m_maxStepUpHeight)
            {
                m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);                                    // swim underwater
            }
            else
            {
                m_character->setAnimation(TR_ANIMATION_LARA_ONWATER_IDLE, 0);                                       // swim onwater
                m_character->m_moveType = MoveType::OnWater;
                m_character->m_transform[3][2] = m_character->m_heightInfo.transition_level;
            }
        }
        else if(m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] > m_character->m_wadeDepth)              // wade case
        {
            if((m_character->m_command.move[0] != 1) || (m_character->m_response.horizontal_collide != 0))
            {
                setNextState(LaraState::STOP);
            }
        }
    }
    else                                                                // no water, stay or run / walk
    {
        if((m_character->m_command.move[0] == 1) && (m_character->m_response.horizontal_collide == 0))
        {
            if(m_character->m_heightInfo.quicksand == QuicksandPosition::None)
            {
                setNextState(LaraState::RUN_FORWARD);
            }
        }
        else
        {
            setNextState(LaraState::STOP);
        }
    }
}

void StateController::walkBack()
{
    m_character->m_command.rot[0] *= 0.4f;
    m_character->m_moveDir = MoveDirection::Backward;

    if(m_character->m_heightInfo.quicksand != QuicksandPosition::None)
    {
        m_character->m_currentSpeed = std::min(m_character->m_currentSpeed, MAX_SPEED_QUICKSAND);
    }

    glm::vec3 global_offset( m_character->m_transform[1] * -WalkBackOffset );
    global_offset[2] += m_character->m_bf.boundingBox.max[2];
    HeightInfo next_fc = initHeightInfo();
    StepType nextStep = m_character->checkNextStep(global_offset, &next_fc);
    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
    }
    else if(m_character->m_heightInfo.water && (m_character->m_heightInfo.floor_point[2] + m_character->m_height < m_character->m_heightInfo.transition_level))
    {
        m_character->setAnimation(TR_ANIMATION_LARA_ONWATER_SWIM_BACK, 0);
        setNextState(LaraState::ONWATER_BACK);
        m_character->m_moveType = MoveType::OnWater;
    }
    else if(!isWakableStep(nextStep))
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_2CLICK_END, 0);
    }
    else if((next_fc.floor_normale[2] >= m_character->m_criticalSlantZComponent) && nextStep == StepType::DownBig)
    {
        if(!m_character->m_bt.no_fix_all)
        {
            int frames_count = static_cast<int>(m_character->m_bf.animations.model->animations[TR_ANIMATION_LARA_WALK_DOWN_BACK_LEFT].frames.size());
            int frames_count2 = (frames_count + 1) / 2;
            if((m_character->m_bf.animations.current_frame >= 0) && (m_character->m_bf.animations.current_frame <= frames_count2))
            {
                m_character->setAnimation(TR_ANIMATION_LARA_WALK_DOWN_BACK_LEFT, m_character->m_bf.animations.current_frame);
                m_character->m_moveDir = MoveDirection::Backward;
                m_character->m_transform[3][2] -= (m_character->m_heightInfo.floor_point[2] - next_fc.floor_point[2]);
                m_character->m_bt.no_fix_all = true;
            }
            else if((m_character->m_bf.animations.current_frame >= frames_count) && (m_character->m_bf.animations.current_frame <= frames_count + frames_count2))
            {
                m_character->setAnimation(TR_ANIMATION_LARA_WALK_DOWN_BACK_RIGHT, m_character->m_bf.animations.current_frame - frames_count);
                m_character->m_moveDir = MoveDirection::Backward;
                m_character->m_transform[3][2] -= (m_character->m_heightInfo.floor_point[2] - next_fc.floor_point[2]);
                m_character->m_bt.no_fix_all = true;
            }
            else
            {
                m_character->m_moveDir = MoveDirection::Stay;                               // waiting for correct frame
            }
        }
    }
    else if((m_character->m_command.move[0] == -1) && (m_character->m_command.shift || m_character->m_heightInfo.quicksand != QuicksandPosition::None))
    {
        m_character->m_moveDir = MoveDirection::Backward;
        setNextState(LaraState::WALK_BACK);
    }
    else
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::walkLeft()
{
    m_character->m_command.rot[0] = 0;
    m_character->m_moveDir = MoveDirection::Left;
    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
    }
    else if(m_character->m_command.move[1] == -1 && m_character->m_command.shift)
    {
        glm::vec3 global_offset( m_character->m_transform[0] * -RunForwardOffset );  // not an error - RUN_... more correct here
        global_offset[2] += m_character->m_bf.boundingBox.max[2];
        global_offset += glm::vec3(m_character->m_transform[3]);
        HeightInfo next_fc = initHeightInfo();
        Character::getHeightInfo(global_offset, &next_fc);
        if(next_fc.floor_hit && (next_fc.floor_point[2] > m_character->m_transform[3][2] - m_character->m_maxStepUpHeight) && (next_fc.floor_point[2] <= m_character->m_transform[3][2] + m_character->m_maxStepUpHeight))
        {
            if(!m_character->m_heightInfo.water || (m_character->m_heightInfo.floor_point[2] + m_character->m_height > m_character->m_heightInfo.transition_level)) // if (floor_hit == 0) then we went to MoveType::FreeFalling.
            {
                // continue walking
            }
            else
            {
                setNextState(LaraState::ONWATER_LEFT);
                m_character->m_bf.animations.onFrame = onFrameToOnWater;
            }
        }
        else
        {
            m_character->m_moveDir = MoveDirection::Stay;
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_SOLID, 0);
        }
    }
    else
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::walkRight()
{
    m_character->m_command.rot[0] = 0;
    m_character->m_moveDir = MoveDirection::Right;
    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_FREE_FALL, 0);
    }
    else if(m_character->m_command.move[1] == 1 && m_character->m_command.shift)
    {
        // Not a error - RUN_... constant is more correct here
        glm::vec3 global_offset( m_character->m_transform[0] * RunForwardOffset );
        global_offset[2] += m_character->m_bf.boundingBox.max[2];
        global_offset += glm::vec3(m_character->m_transform[3]);
        HeightInfo next_fc = initHeightInfo();
        Character::getHeightInfo(global_offset, &next_fc);
        if(next_fc.floor_hit && (next_fc.floor_point[2] > m_character->m_transform[3][2] - m_character->m_maxStepUpHeight) && (next_fc.floor_point[2] <= m_character->m_transform[3][2] + m_character->m_maxStepUpHeight))
        {
            if(!m_character->m_heightInfo.water || (m_character->m_heightInfo.floor_point[2] + m_character->m_height > m_character->m_heightInfo.transition_level)) // if (floor_hit == 0) then we went to MoveType::FreeFalling.
            {
                // continue walking
            }
            else
            {
                setNextState(LaraState::ONWATER_RIGHT);
                m_character->m_bf.animations.onFrame = onFrameToOnWater;
            }
        }
        else
        {
            m_character->m_moveDir = MoveDirection::Stay;
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_SOLID, 0);
        }
    }
    else
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::slideBack()
{
    m_character->m_command.rot[0] = 0;
    m_character->lean(0.0);
    m_character->m_moveDir = MoveDirection::Backward;

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        if(m_character->m_command.action)
        {
            m_character->m_speed[0] = -m_character->m_transform[1][0] * 128.0f;
            m_character->m_speed[1] = -m_character->m_transform[1][1] * 128.0f;
        }

        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_BACK, 0);
    }
    else if(m_character->m_response.slide == SlideType::None)
    {
        setNextState(LaraState::STOP);
    }
    else if(m_character->m_response.slide != SlideType::None && m_character->m_command.jump)
    {
        setNextState(LaraState::JUMP_BACK);
    }
    else
    {
        return;
    }

    engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_SLIDING, audio::EmitterType::Entity, m_character->getId());
}

void StateController::slideForward()
{
    m_character->m_command.rot[0] = 0;
    m_character->lean(0.0);
    m_character->m_moveDir = MoveDirection::Forward;

    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_FORWARD, 0);
    }
    else if(m_character->m_response.slide == SlideType::None)
    {
        if((m_character->m_command.move[0] == 1) && (engine::engine_world.engineVersion >= loader::Engine::TR3))
        {
            setNextState(LaraState::RUN_FORWARD);
        }
        else
        {
            setNextState(LaraState::STOP);                  // stop
        }
    }
    else if(m_character->m_response.slide != SlideType::None && m_character->m_command.jump)
    {
        setNextState(LaraState::JUMP_FORWARD);               // jump
    }
    else
    {
        return;
    }

    engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_SLIDING, audio::EmitterType::Entity, m_character->getId());
}

void StateController::pushableGrab()
{
    m_character->m_moveType = MoveType::OnFloor;
    m_character->m_bt.no_fix_all = true;
    m_character->m_command.rot[0] = 0.0;

    if(m_character->m_command.action)  //If Lara is grabbing the block
    {
        int tf = m_character->checkTraverse(*m_character->m_traversedObject);
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  //We hold it (loop last frame)

        if((m_character->m_command.move[0] == 1) && (tf & Character::TraverseForward))  // If player presses up, then push
        {
            m_character->m_moveDir = MoveDirection::Forward;
            m_character->m_bf.animations.mode = animation::SSAnimationMode::NormalControl;
            setNextState(LaraState::PUSHABLE_PUSH);
        }
        else if((m_character->m_command.move[0] == -1) && (tf & Character::TraverseBackward))  //If player presses down, then pull
        {
            m_character->m_moveDir = MoveDirection::Backward;
            m_character->m_bf.animations.mode = animation::SSAnimationMode::NormalControl;
            setNextState(LaraState::PUSHABLE_PULL);
        }
    }
    else  //Lara has let go of the block
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->m_bf.animations.mode = animation::SSAnimationMode::NormalControl;  // We're no longer looping last frame
        setNextState(LaraState::STOP);   // Switch to next Lara state
    }
}

void StateController::pushablePush()
{
    m_character->m_bt.no_fix_all = true;
    m_character->m_bf.animations.onFrame = onFrameStopTraverse;
    m_character->m_command.rot[0] = 0.0;
    m_character->m_camFollowCenter = 64;
    int i = static_cast<int>(m_character->m_bf.animations.getCurrentAnimationFrame().frames.size());

    if(!m_character->m_command.action || !(Character::TraverseForward & m_character->checkTraverse(*m_character->m_traversedObject)))   //For TOMB4/5 If Lara is pushing and action let go, don't push
    {
        setNextState(LaraState::STOP);
    }

    if((m_character->m_traversedObject != nullptr) && (m_character->m_bf.animations.current_frame > 16) && (m_character->m_bf.animations.current_frame < i - 16)) ///@FIXME: magick 16
    {
        bool was_traversed = false;

        if(m_character->m_transform[1][0] > 0.9)
        {
            glm::float_t t = m_character->m_transform[3][0] + (m_character->m_bf.boundingBox.max[1] - m_character->m_traversedObject->m_bf.boundingBox.min[0] - 32.0f);
            if(t > m_character->m_traversedObject->m_transform[3][0])
            {
                m_character->m_traversedObject->m_transform[3][0] = t;
                was_traversed = true;
            }
        }
        else if(m_character->m_transform[1][0] < -0.9)
        {
            glm::float_t t = m_character->m_transform[3][0] - (m_character->m_bf.boundingBox.max[1] + m_character->m_traversedObject->m_bf.boundingBox.max[0] - 32.0f);
            if(t < m_character->m_traversedObject->m_transform[3][0])
            {
                m_character->m_traversedObject->m_transform[3][0] = t;
                was_traversed = true;
            }
        }
        else if(m_character->m_transform[1][1] > 0.9)
        {
            glm::float_t t = m_character->m_transform[3][1] + (m_character->m_bf.boundingBox.max[1] - m_character->m_traversedObject->m_bf.boundingBox.min[1] - 32.0f);
            if(t > m_character->m_traversedObject->m_transform[3][1])
            {
                m_character->m_traversedObject->m_transform[3][1] = t;
                was_traversed = true;
            }
        }
        else if(m_character->m_transform[1][1] < -0.9)
        {
            glm::float_t t = m_character->m_transform[3][1] - (m_character->m_bf.boundingBox.max[1] + m_character->m_traversedObject->m_bf.boundingBox.max[1] - 32.0f);
            if(t < m_character->m_traversedObject->m_transform[3][1])
            {
                m_character->m_traversedObject->m_transform[3][1] = t;
                was_traversed = true;
            }
        }

        if(engine::engine_world.engineVersion > loader::Engine::TR3)
        {
            if(was_traversed)
            {
                if(engine::engine_world.audioEngine.findSource(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId()) == -1)
                    engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
            }
            else
            {
                engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
            }
        }
        else
        {
            if((m_character->m_bf.animations.current_frame == 49) ||
                    (m_character->m_bf.animations.current_frame == 110) ||
                    (m_character->m_bf.animations.current_frame == 142))
            {
                if(engine::engine_world.audioEngine.findSource(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId()) == -1)
                    engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
            }
        }

        m_character->m_traversedObject->updateRigidBody(true);
    }
    else
    {
        if(engine::engine_world.engineVersion > loader::Engine::TR3)
        {
            engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
        }
    }
}

void StateController::pushablePull()
{
    m_character->m_bt.no_fix_all = true;
    m_character->m_bf.animations.onFrame = onFrameStopTraverse;
    m_character->m_command.rot[0] = 0.0;
    m_character->m_camFollowCenter = 64;
    int i = static_cast<int>(m_character->m_bf.animations.getCurrentAnimationFrame().frames.size());

    if(!m_character->m_command.action || !(Character::TraverseBackward & m_character->checkTraverse(*m_character->m_traversedObject)))   //For TOMB4/5 If Lara is pulling and action let go, don't pull
    {
        setNextState(LaraState::STOP);
    }

    if((m_character->m_traversedObject != nullptr) && (m_character->m_bf.animations.current_frame > 20) && (m_character->m_bf.animations.current_frame < i - 16)) ///@FIXME: magick 20
    {
        bool was_traversed = false;

        if(m_character->m_transform[1][0] > 0.9)
        {
            glm::float_t t = m_character->m_transform[3][0] + (m_character->m_bf.boundingBox.max[1] - m_character->m_traversedObject->m_bf.boundingBox.min[0] - 32.0f);
            if(t < m_character->m_traversedObject->m_transform[3][0])
            {
                m_character->m_traversedObject->m_transform[3][0] = t;
                was_traversed = true;
            }
        }
        else if(m_character->m_transform[1][0] < -0.9)
        {
            glm::float_t t = m_character->m_transform[3][0] - (m_character->m_bf.boundingBox.max[1] + m_character->m_traversedObject->m_bf.boundingBox.max[0] - 32.0f);
            if(t > m_character->m_traversedObject->m_transform[3][0])
            {
                m_character->m_traversedObject->m_transform[3][0] = t;
                was_traversed = true;
            }
        }
        else if(m_character->m_transform[1][1] > 0.9)
        {
            glm::float_t t = m_character->m_transform[3][1] + (m_character->m_bf.boundingBox.max[1] - m_character->m_traversedObject->m_bf.boundingBox.min[1] - 32.0f);
            if(t < m_character->m_traversedObject->m_transform[3][1])
            {
                m_character->m_traversedObject->m_transform[3][1] = t;
                was_traversed = true;
            }
        }
        else if(m_character->m_transform[1][1] < -0.9)
        {
            glm::float_t t = m_character->m_transform[3][1] - (m_character->m_bf.boundingBox.max[1] + m_character->m_traversedObject->m_bf.boundingBox.max[1] - 32.0f);
            if(t > m_character->m_traversedObject->m_transform[3][1])
            {
                m_character->m_traversedObject->m_transform[3][1] = t;
                was_traversed = true;
            }
        }

        if(engine::engine_world.engineVersion > loader::Engine::TR3)
        {
            if(was_traversed)
            {
                if(engine::engine_world.audioEngine.findSource(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId()) == -1)
                    engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
            }
            else
            {
                engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
            }
        }
        else
        {
            if((m_character->m_bf.animations.current_frame == 40) ||
                    (m_character->m_bf.animations.current_frame == 92) ||
                    (m_character->m_bf.animations.current_frame == 124) ||
                    (m_character->m_bf.animations.current_frame == 156))
            {
                if(engine::engine_world.audioEngine.findSource(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId()) == -1)
                    engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
            }
        }

        m_character->m_traversedObject->updateRigidBody(true);
    }
    else
    {
        if(engine::engine_world.engineVersion > loader::Engine::TR3)
        {
            engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_PUSHABLE, audio::EmitterType::Entity, m_character->getId());
        }
    }
}

void StateController::rollForward()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS;
}

void StateController::rollBackward()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS;
    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_FORWARD, 0);
    }
    else if(isLowVerticalSpace())
    {
        m_character->m_moveDir = MoveDirection::Stay;
    }
    else if(m_character->m_response.slide == SlideType::Front)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_SLIDE_FORWARD, 0);
    }
    else if(m_character->m_response.slide == SlideType::Back)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_START_SLIDE_BACKWARD, 0);
    }
}

void StateController::jumpUp()
{
    m_character->m_command.rot[0] = 0.0;
    if(m_character->m_command.action && (m_character->m_moveType != MoveType::WallsClimb) && (m_character->m_moveType != MoveType::Climbing))
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += m_character->m_bf.boundingBox.max[2] + LaraHangVerticalEpsilon + engine::engine_frame_time * m_character->m_speed[2];
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.edge_hit)
        {
            m_character->m_climb.point = m_character->m_climb.edge_point;
            m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
            m_character->updateTransform();
            m_character->m_moveType = MoveType::Climbing;                             // hang on
            m_character->m_speed = {0,0,0};

            m_character->m_transform[3][0] = m_character->m_climb.point[0] - LaraHangWallDistance * m_character->m_transform[1][0];
            m_character->m_transform[3][1] = m_character->m_climb.point[1] - LaraHangWallDistance * m_character->m_transform[1][1];
            m_character->m_transform[3][2] = m_character->m_climb.point[2] - m_character->m_bf.boundingBox.max[2] + LaraHangVerticalOffset;
        }
        else
        {
            m_character->m_climb = m_character->checkWallsClimbability();
            if(m_character->m_climb.wall_hit != ClimbType::None &&
                    (m_character->m_speed[2] < 0.0)) // Only hang if speed is lower than zero.
            {
                // Fix the position to the TR metering step.
                m_character->m_transform[3][2] = std::floor(m_character->m_transform[3][2] / MeteringStep) * MeteringStep;
                m_character->m_moveType = MoveType::WallsClimb;
                m_character->setAnimation(TR_ANIMATION_LARA_HANG_IDLE, -1);
                return;
            }
        }
    }

    if(m_character->m_command.move[0] == 1)
    {
        m_character->m_moveDir = MoveDirection::Forward;
    }
    else if(m_character->m_command.move[0] == -1)
    {
        m_character->m_moveDir = MoveDirection::Backward;
    }
    else if(m_character->m_command.move[1] == 1)
    {
        m_character->m_moveDir = MoveDirection::Right;
    }
    else if(m_character->m_command.move[1] == -1)
    {
        m_character->m_moveDir = MoveDirection::Left;
    }
    else
    {
        m_character->m_moveDir = MoveDirection::Forward;///Lara can move forward towards walls in this state
    }

    if(m_character->m_moveType == MoveType::Underwater)
    {
        m_character->m_angles[1] = -45.0;
        m_character->m_command.rot[1] = 0.0;
        m_character->updateTransform();
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_TO_UNDERWATER, 0);
    }
    else if(m_character->m_command.action && m_character->m_heightInfo.ceiling_climb && (m_character->m_heightInfo.ceiling_hit) && (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] > m_character->m_heightInfo.ceiling_point[2] - 64.0))
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
        m_character->m_bf.animations.onFrame = onFrameToMonkeyswing;
    }
    else if(m_character->m_command.action && (m_character->m_moveType == MoveType::Climbing))
    {
        setNextState(LaraState::HANG);
        m_character->setAnimation(TR_ANIMATION_LARA_HANG_IDLE, -1);
    }
    else if((m_character->m_response.vertical_collide & 0x01) || (m_character->m_moveType == MoveType::OnFloor))
    {
        setNextState(LaraState::STOP);  // Landing immediately
    }
    else
    {
        if(m_character->m_speed[2] < -FREE_FALL_SPEED_2)  // Next free fall stage
        {
            m_character->m_moveType = MoveType::FreeFalling;
            setNextState(LaraState::FREEFALL);
        }
    }
}

void StateController::reach()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS | BODY_PART_HANDS_1 | BODY_PART_HANDS_2;
    m_character->m_command.rot[0] = 0.0;
    if(m_character->m_moveType == MoveType::Underwater)
    {
        m_character->m_angles[1] = -45.0;
        m_character->m_command.rot[1] = 0.0;
        m_character->updateTransform();
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_TO_UNDERWATER, 0);
        return;
    }

    if(m_character->m_command.action && (m_character->m_moveType == MoveType::FreeFalling))
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += m_character->m_bf.boundingBox.max[2] + LaraHangVerticalEpsilon + engine::engine_frame_time * m_character->m_speed[2];
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.edge_hit && m_character->m_climb.can_hang)
        {
            m_character->m_climb.point = m_character->m_climb.edge_point;
            m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
            m_character->updateTransform();
            m_character->m_moveType = MoveType::Climbing;  // Hang on
            m_character->m_speed = {0,0,0};
        }

        // If Lara is moving backwards off the ledge we want to move Lara slightly forwards
        // depending on the current angle.
        if((m_character->m_moveDir == MoveDirection::Backward) && (m_character->m_moveType == MoveType::Climbing))
        {
            m_character->m_transform[3][0] = m_character->m_climb.point[0] - m_character->m_transform[1][0] * (m_character->m_forwardSize + 16.0f);
            m_character->m_transform[3][1] = m_character->m_climb.point[1] - m_character->m_transform[1][1] * (m_character->m_forwardSize + 16.0f);
        }
    }

    if(((m_character->m_moveType != MoveType::OnFloor)) && m_character->m_command.action && m_character->m_heightInfo.ceiling_climb && (m_character->m_heightInfo.ceiling_hit) && (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] > m_character->m_heightInfo.ceiling_point[2] - 64.0))
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
        m_character->m_bf.animations.onFrame = onFrameToMonkeyswing;
        return;
    }
    if(((m_character->m_response.vertical_collide & 0x01) || (m_character->m_moveType == MoveType::OnFloor)) && (!m_character->m_command.action || !m_character->m_climb.can_hang))
    {
        setNextState(LaraState::STOP);  // Middle landing
        return;
    }

    if((m_character->m_speed[2] < -FREE_FALL_SPEED_2))
    {
        m_character->m_moveType = MoveType::FreeFalling;
        setNextState(LaraState::FREEFALL);
        return;
    }

    if(m_character->m_moveType == MoveType::Climbing)
    {
        m_character->m_speed = {0,0,0};
        setNextState(LaraState::HANG);
        m_character->m_bf.animations.onFrame = onFrameToEdgeClimb;
    }
}

void StateController::hang()
{
    m_character->m_command.rot[0] = 0.0;

    if(m_character->m_moveType == MoveType::WallsClimb)
    {
        if(m_character->m_command.action)
        {
            if(m_character->m_climb.wall_hit == ClimbType::FullBody && (m_character->m_command.move[0] == 0) && (m_character->m_command.move[1] == 0))
            {
                setNextState(LaraState::LADDER_IDLE);
            }
            else if(m_character->m_command.move[0] == 1)  // UP
            {
                m_character->setAnimation(TR_ANIMATION_LARA_LADDER_UP_HANDS, 0);
            }
            else if(m_character->m_command.move[0] == -1)  // DOWN
            {
                m_character->setAnimation(TR_ANIMATION_LARA_LADDER_DOWN_HANDS, 0);
            }
            else if(m_character->m_command.move[1] == 1)
            {
                m_character->m_moveDir = MoveDirection::Right;
                m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_RIGHT, 0);  // Edge climb right
            }
            else if(m_character->m_command.move[1] == -1)
            {
                m_character->m_moveDir = MoveDirection::Left;
                m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_LEFT, 0);  // Edge climb left
            }
            else if(m_character->m_climb.wall_hit == ClimbType::None)
            {
                m_character->m_moveType = MoveType::FreeFalling;
                m_character->setAnimation(TR_ANIMATION_LARA_STOP_HANG_VERTICAL, 0);  // Fall down
            }
            else
            {
                m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  // Disable shake
            }
        }
        else
        {
            m_character->m_moveType = MoveType::FreeFalling;
            m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);  // Fall down
        }
        return;
    }

    if(!m_character->m_response.killed && m_character->m_command.action)  // We have to update climb point every time so entity can move
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += m_character->m_bf.boundingBox.max[2] + LaraHangVerticalEpsilon;
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.can_hang)
        {
            m_character->m_climb.point = m_character->m_climb.edge_point;
            m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
            m_character->updateTransform();
            m_character->m_moveType = MoveType::Climbing;  // Hang on
        }
    }
    else
    {
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);  // Fall down
        return;
    }

    if(m_character->m_moveType == MoveType::Climbing)
    {
        if(m_character->m_command.move[0] == 1)
        {
            if(m_character->m_climb.edge_hit && (m_character->m_climb.next_z_space >= 512.0) && ((m_character->m_climb.next_z_space < m_character->m_height - LaraHangVerticalEpsilon) || m_character->m_command.crouch))
            {
                m_character->m_climb.point = m_character->m_climb.edge_point;
                setNextState(LaraState::CLIMB_TO_CRAWL);  // Crawlspace climb
            }
            else if(m_character->m_climb.edge_hit && (m_character->m_climb.next_z_space >= m_character->m_height - LaraHangVerticalEpsilon))
            {
                engine::Sys_DebugLog(LOG_FILENAME, "Zspace = %f", m_character->m_climb.next_z_space);
                m_character->m_climb.point = m_character->m_climb.edge_point;
                setNextState((m_character->m_command.shift) ? (LaraState::HANDSTAND) : (LaraState::CLIMBING));               // climb up
            }
            else
            {
                m_character->m_transform[3][0] = m_character->m_climb.point[0] - LaraHangWallDistance * m_character->m_transform[1][0];
                m_character->m_transform[3][1] = m_character->m_climb.point[1] - LaraHangWallDistance * m_character->m_transform[1][1];
                m_character->m_transform[3][2] = m_character->m_climb.point[2] - m_character->m_bf.boundingBox.max[2] + LaraHangVerticalOffset;
                m_character->m_speed = {0,0,0};
                m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  // Disable shake
            }
        }
        else if(m_character->m_command.move[0] == -1)  // Check walls climbing
        {
            m_character->m_climb = m_character->checkWallsClimbability();
            if(m_character->m_climb.wall_hit != ClimbType::None)
            {
                m_character->m_moveType = MoveType::WallsClimb;
            }
            m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  // Disable shake
        }
        else if(m_character->m_command.move[1] == -1)
        {
            glm::vec3 move( m_character->m_transform[0] * -PenetrationTestOffset );
            if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00)) //we only want lara to shimmy when last frame is reached!
            {
                m_character->m_moveDir = MoveDirection::Left;
                m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_LEFT, 0);
            }
            else
            {
                m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  // Disable shake
            }
        }
        else if(m_character->m_command.move[1] == 1)
        {
            glm::vec3 move( m_character->m_transform[0] * PenetrationTestOffset );
            if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00)) //we only want lara to shimmy when last frame is reached!
            {
                m_character->m_moveDir = MoveDirection::Right;
                m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_RIGHT, 0);
            }
            else
            {
                m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  // Disable shake
            }
        }
        else
        {
            m_character->m_bf.animations.mode = animation::SSAnimationMode::LoopLastFrame;  // Disable shake
            m_character->m_transform[3][0] = m_character->m_climb.point[0] - LaraHangWallDistance * m_character->m_transform[1][0];
            m_character->m_transform[3][1] = m_character->m_climb.point[1] - LaraHangWallDistance * m_character->m_transform[1][1];
            m_character->m_transform[3][2] = m_character->m_climb.point[2] - m_character->m_bf.boundingBox.max[2] + LaraHangVerticalOffset;
            m_character->m_speed = {0,0,0};
        }
    }
    else if(m_character->m_command.action && m_character->m_heightInfo.ceiling_climb && (m_character->m_heightInfo.ceiling_hit) && (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] > m_character->m_heightInfo.ceiling_point[2] - 64.0))
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
        m_character->m_bf.animations.onFrame = onFrameToMonkeyswing;
    }
    else
    {
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);  // Fall down
    }
}

void StateController::ladderIdle()
{
    m_character->m_command.rot[0] = 0;
    m_character->m_moveType = MoveType::WallsClimb;
    m_character->m_moveDir = MoveDirection::Stay;
    m_character->m_camFollowCenter = 64;
    if(m_character->m_moveType == MoveType::Climbing)
    {
        setNextState(LaraState::CLIMBING);
        return;
    }

    if(!m_character->m_command.action)
    {
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->setAnimation(TR_ANIMATION_LARA_STOP_HANG_VERTICAL, 0);  // Fall down
    }
    else if(m_character->m_command.jump)
    {
        setNextState(LaraState::JUMP_BACK);
        m_character->m_moveDir = MoveDirection::Backward;
    }
    else if(m_character->m_command.move[0] == 1)
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += m_character->m_bf.boundingBox.max[2] + LaraHangVerticalEpsilon;
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.edge_hit && (m_character->m_climb.next_z_space >= 512.0))
        {
            m_character->m_moveType = MoveType::Climbing;
            setNextState(LaraState::CLIMBING);
        }
        else if((!m_character->m_heightInfo.ceiling_hit) || (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] < m_character->m_heightInfo.ceiling_point[2]))
        {
            setNextState(LaraState::LADDER_UP);
        }
    }
    else if(m_character->m_command.move[0] == -1)
    {
        setNextState(LaraState::LADDER_DOWN);
    }
    else if(m_character->m_command.move[1] == 1)
    {
        setNextState(LaraState::LADDER_RIGHT);
    }
    else if(m_character->m_command.move[1] == -1)
    {
        setNextState(LaraState::LADDER_LEFT);
    }
}

void StateController::ladderLeft()
{
    m_character->m_moveDir = MoveDirection::Left;
    if(!m_character->m_command.action || m_character->m_climb.wall_hit == ClimbType::None)
    {
        setNextState(LaraState::HANG);
    }
    else
    {
        setNextState(LaraState::LADDER_IDLE);
    }
}

void StateController::ladderRight()
{
    m_character->m_moveDir = MoveDirection::Right;
    if(!m_character->m_command.action || m_character->m_climb.wall_hit == ClimbType::None)
    {
        setNextState(LaraState::HANG);
    }
    else
    {
        setNextState(LaraState::LADDER_IDLE);
    }
}

void StateController::ladderUp()
{
    m_character->m_camFollowCenter = 64;
    if(m_character->m_moveType == MoveType::Climbing)
    {
        setNextState(LaraState::LADDER_IDLE);
        return;
    }

    if(m_character->m_command.action && m_character->m_climb.wall_hit != ClimbType::None)
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += m_character->m_bf.boundingBox.max[2] + LaraHangVerticalEpsilon;
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.edge_hit && (m_character->m_climb.next_z_space >= 512.0))
        {
            m_character->m_moveType = MoveType::Climbing;
            setNextState(LaraState::LADDER_IDLE);
        }
        else if((m_character->m_command.move[0] <= 0) && (m_character->m_heightInfo.ceiling_hit || (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] >= m_character->m_heightInfo.ceiling_point[2])))
        {
            setNextState(LaraState::LADDER_IDLE);
        }

        if(m_character->m_heightInfo.ceiling_hit && (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] > m_character->m_heightInfo.ceiling_point[2]))
        {
            m_character->m_transform[3][2] = m_character->m_heightInfo.ceiling_point[2] - m_character->m_bf.boundingBox.max[2];
        }
    }
    else
    {
        // Free fall after stop
        setNextState(LaraState::LADDER_IDLE);
    }
}

void StateController::ladderDown()
{
    m_character->m_camFollowCenter = 64;
    if(m_character->m_command.action && m_character->m_climb.wall_hit != ClimbType::None && (m_character->m_command.move[1] < 0))
    {
        if(m_character->m_climb.wall_hit != ClimbType::FullBody)
        {
            setNextState(LaraState::LADDER_IDLE);
        }
    }
    else
    {
        setNextState(LaraState::LADDER_IDLE);
    }
}

void StateController::shimmyLeft()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS;

    m_character->m_command.rot[0] = 0.0;
    m_character->m_moveDir = MoveDirection::Left;
    if(!m_character->m_command.action)
    {
        m_character->m_speed = {0,0,0};
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0); // fall down
        return;
    }

    if(m_character->m_moveType == MoveType::WallsClimb)
    {
        if(m_character->m_climb.wall_hit == ClimbType::None)
        {
            m_character->m_moveType = MoveType::FreeFalling;
            m_character->setAnimation(TR_ANIMATION_LARA_STOP_HANG_VERTICAL, 0); // fall down
        }
    }
    else
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += LaraHangSensorZ + LaraHangVerticalEpsilon;
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.edge_hit)
        {
            m_character->m_climb.point = m_character->m_climb.edge_point;
            m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
            m_character->updateTransform();
            m_character->m_moveType = MoveType::Climbing;                             // hang on
            m_character->m_transform[3][0] = m_character->m_climb.point[0] - LaraHangWallDistance * m_character->m_transform[1][0];
            m_character->m_transform[3][1] = m_character->m_climb.point[1] - LaraHangWallDistance * m_character->m_transform[1][1];
            m_character->m_transform[3][2] = m_character->m_climb.point[2] - m_character->m_bf.boundingBox.max[2] + LaraHangVerticalOffset;
            m_character->m_speed = {0,0,0};
        }
        else
        {
            m_character->m_moveType = MoveType::FreeFalling;
            m_character->setAnimation(TR_ANIMATION_LARA_STOP_HANG_VERTICAL, 0); // fall down
            return;
        }
    }

    if(m_character->m_command.move[1] == -1)
    {
        glm::vec3 move( m_character->m_transform[0] * -PenetrationTestOffset );
        if((m_character->checkNextPenetration(move) > 0) && (m_character->m_response.horizontal_collide != 0x00))
        {
            setNextState(LaraState::HANG);
        }
    }
    else
    {
        setNextState(LaraState::HANG);
    }
}

void StateController::shimmyRight()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_LEGS;

    m_character->m_command.rot[0] = 0.0;
    m_character->m_moveDir = MoveDirection::Right;
    if(!m_character->m_command.action)
    {
        m_character->m_speed = {0,0,0};
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0); // fall down
        return;
    }

    if(m_character->m_moveType == MoveType::WallsClimb)
    {
        if(m_character->m_climb.wall_hit == ClimbType::None)
        {
            m_character->m_moveType = MoveType::FreeFalling;
            m_character->setAnimation(TR_ANIMATION_LARA_STOP_HANG_VERTICAL, 0); // fall down
        }
    }
    else
    {
        glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
        glm::vec3 global_offset( m_character->m_transform[1] * t );
        global_offset[2] += LaraHangSensorZ + LaraHangVerticalEpsilon;
        HeightInfo next_fc = initHeightInfo();
        m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
        if(m_character->m_climb.edge_hit)
        {
            m_character->m_climb.point = m_character->m_climb.edge_point;
            m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
            m_character->updateTransform();
            m_character->m_moveType = MoveType::Climbing;                             // hang on
            m_character->m_transform[3][0] = m_character->m_climb.point[0] - LaraHangWallDistance * m_character->m_transform[1][0];
            m_character->m_transform[3][1] = m_character->m_climb.point[1] - LaraHangWallDistance * m_character->m_transform[1][1];
            m_character->m_transform[3][2] = m_character->m_climb.point[2] - m_character->m_bf.boundingBox.max[2] + LaraHangVerticalOffset;
            m_character->m_speed = {0,0,0};
        }
        else
        {
            m_character->m_moveType = MoveType::FreeFalling;
            m_character->setAnimation(TR_ANIMATION_LARA_STOP_HANG_VERTICAL, 0); // fall down
            return;
        }
    }

    if(m_character->m_command.move[1] == 1)
    {
        glm::vec3 move( m_character->m_transform[0] * PenetrationTestOffset );
        if((m_character->checkNextPenetration(move) > 0) && (m_character->m_response.horizontal_collide != 0x00))
        {
            setNextState(LaraState::HANG);
        }
    }
    else
    {
        setNextState(LaraState::HANG);
    }
}

void StateController::onwaterExit()
{
    m_character->m_command.rot[0] *= 0.0;
    m_character->m_bt.no_fix_all = true;
    m_character->m_bf.animations.onFrame = onFrameSetOnFloorAfterClimb;
}

void StateController::jumpForwardFallBackward()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS | BODY_PART_LEGS | BODY_PART_HEAD;
    m_character->lean(4.0);

    if((m_character->m_response.vertical_collide & 0x01) || (m_character->m_moveType == MoveType::OnFloor))
    {
        if(m_character->getRoom()->flags & TR_ROOM_FLAG_QUICKSAND)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
        }
        else if(!m_character->m_command.action && (m_character->m_command.move[0] == 1) && !m_character->m_command.crouch)
        {
            m_character->m_moveType = MoveType::OnFloor;
            setNextState(LaraState::RUN_FORWARD);
        }
        else
        {
            setNextState(LaraState::STOP);
        }
    }
    else if(m_character->m_moveType == MoveType::Underwater)
    {
        m_character->m_angles[1] = -45.0;
        m_character->m_command.rot[1] = 0.0;
        m_character->updateTransform();
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_TO_UNDERWATER, 0);
    }
    else if(m_character->m_response.horizontal_collide & 0x01)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_SMASH_JUMP, 0);
        m_character->m_moveDir = MoveDirection::Backward;
        m_character->updateCurrentSpeed(true);
    }
    else if(m_character->m_speed[2] <= -FREE_FALL_SPEED_2)
    {
        setNextState(LaraState::FREEFALL);                    // free falling
    }
    else if(m_character->m_command.action)
    {
        setNextState(LaraState::REACH);
    }
    else if(m_character->m_command.shift)
    {
        setNextState(LaraState::SWANDIVE_BEGIN);              // fly like fish
    }
    else if(m_character->m_speed[2] <= -FREE_FALL_SPEED_2)
    {
        setNextState(LaraState::FREEFALL);                    // free falling
    }
    else if(m_character->m_command.roll)
    {
        setNextState(LaraState::JUMP_ROLL);
    }
}

void StateController::underwaterDiving()
{
    m_character->m_angles[1] = -45.0;
    m_character->m_command.rot[1] = 0.0;
    m_character->updateTransform();
    m_character->m_bf.animations.onFrame = onFrameCorrectDivingAngle;
}

void StateController::freefall()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS | BODY_PART_LEGS;
    m_character->lean(1.0);

    if((int(m_character->m_speed[2]) <= -FREE_FALL_SPEED_CRITICAL) &&
            (int(m_character->m_speed[2]) >= (-FREE_FALL_SPEED_CRITICAL - 100)))
    {
        m_character->m_speed[2] = -FREE_FALL_SPEED_CRITICAL - 101;
        engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_LARASCREAM, audio::EmitterType::Entity, m_character->getId());       // Scream
    }
    else if(m_character->m_speed[2] <= -FREE_FALL_SPEED_MAXSAFE)
    {
        //Reset these to zero so Lara is only falling downwards
        m_character->m_speed[0] = 0.0;
        m_character->m_speed[1] = 0.0;
    }

    if(m_character->m_moveType == MoveType::Underwater)
    {
        m_character->m_angles[1] = -45.0;
        m_character->m_command.rot[1] = 0.0;
        m_character->updateTransform();                                     // needed here to fix underwater in wall collision bug
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_TO_UNDERWATER, 0);
        engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_LARASCREAM, audio::EmitterType::Entity, m_character->getId());       // Stop scream

        // Splash sound is hardcoded, beginning with TR3.
        if(engine::engine_world.engineVersion > loader::Engine::TR2)
        {
            engine::engine_world.audioEngine.send(TR_AUDIO_SOUND_SPLASH, audio::EmitterType::Entity, m_character->getId());
        }
    }
    else if((m_character->m_response.vertical_collide & 0x01) || (m_character->m_moveType == MoveType::OnFloor))
    {
        if(m_character->getRoom()->flags & TR_ROOM_FLAG_QUICKSAND)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_STAY_IDLE, 0);
            engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_LARASCREAM, audio::EmitterType::Entity, m_character->getId());
        }
        else if(m_character->m_speed[2] <= -FREE_FALL_SPEED_MAXSAFE)
        {
            if(!m_character->changeParam(PARAM_HEALTH, (m_character->m_speed[2] + FREE_FALL_SPEED_MAXSAFE) / 2))
            {
                m_character->m_response.killed = true;
                m_character->setAnimation(TR_ANIMATION_LARA_LANDING_DEATH, 0);
                engine::Controls_JoyRumble(200.0, 500);
            }
            else
            {
                m_character->setAnimation(TR_ANIMATION_LARA_LANDING_HARD, 0);
            }
        }
        else if(m_character->m_speed[2] <= -FREE_FALL_SPEED_2)
        {
            m_character->setAnimation(TR_ANIMATION_LARA_LANDING_HARD, 0);
        }
        else
        {
            m_character->setAnimation(TR_ANIMATION_LARA_LANDING_MIDDLE, 0);
        }

        if(m_character->m_response.killed)
        {
            setNextState(LaraState::DEATH);
            engine::engine_world.audioEngine.kill(TR_AUDIO_SOUND_LARASCREAM, audio::EmitterType::Entity, m_character->getId());
        }
    }
    else if(m_character->m_command.action)
    {
        m_character->m_moveDir = MoveDirection::Forward;
        setNextState(LaraState::REACH);
    }
}

void StateController::swandiveBegin()
{
    m_character->m_command.rot[0] *= 0.4f;
    if(m_character->m_response.vertical_collide & 0x01 || m_character->m_moveType == MoveType::OnFloor)
    {
        setNextState(LaraState::STOP);                        // landing - roll
    }
    else if(m_character->m_moveType == MoveType::Underwater)
    {
        setNextState(LaraState::UNDERWATER_DIVING);
    }
    else
    {
        setNextState(LaraState::SWANDIVE_END);                // next stage
    }
}

void StateController::swandiveEnd()
{
    m_character->m_command.rot[0] = 0.0;

    //Reset these to zero so Lara is only falling downwards
    m_character->m_speed[0] = 0.0;
    m_character->m_speed[1] = 0.0;

    if((m_character->m_response.vertical_collide & 0x01) || (m_character->m_moveType == MoveType::OnFloor))
    {
        if(m_character->m_heightInfo.quicksand != QuicksandPosition::None)
        {
            m_character->m_response.killed = true;
            m_character->setParam(PARAM_HEALTH, 0.0);
            m_character->setParam(PARAM_AIR, 0.0);
            m_character->setAnimation(TR_ANIMATION_LARA_LANDING_DEATH, -1);
        }
        else
        {
            m_character->setParam(PARAM_HEALTH, 0.0);
            setNextState(LaraState::DEATH);
        }
    }
    else if(m_character->m_moveType == MoveType::Underwater)
    {
        setNextState(LaraState::UNDERWATER_DIVING);
    }
    else if(m_character->m_command.jump)
    {
        setNextState(LaraState::JUMP_ROLL);
    }
}

void StateController::underwaterStop()
{
    if(m_character->m_moveType != MoveType::Underwater && m_character->m_moveType != MoveType::OnWater)
    {
        m_character->setAnimation(0, 0);
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::WATER_DEATH);
    }
    else if(m_character->m_command.roll)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_ROLL_BEGIN, 0);
    }
    else if(m_character->m_command.jump)
    {
        setNextState(LaraState::UNDERWATER_FORWARD);
    }
    else if(m_character->m_moveType == MoveType::OnWater)
    {
        m_character->m_inertiaLinear = 0.0;
        setNextState(LaraState::ONWATER_STOP);
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_TO_ONWATER, 0); // go to the air
    }
}

void StateController::waterDeath()
{
    if(m_character->m_moveType != MoveType::OnWater)
    {
        m_character->m_transform[3][2] += (MeteringSectorSize / 4) * engine::engine_frame_time;     // go to the air
    }
}

void StateController::underwaterForward()
{
    if(m_character->m_moveType != MoveType::Underwater && m_character->m_moveType != MoveType::OnWater)
    {
        m_character->setAnimation(0, 0);
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::WATER_DEATH);
    }
    else if(m_character->m_heightInfo.floor_hit && m_character->m_heightInfo.water && (m_character->m_heightInfo.transition_level - m_character->m_heightInfo.floor_point[2] <= m_character->m_maxStepUpHeight))
    {
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_TO_WADE, 0); // go to the air
        setNextState(LaraState::STOP);
        m_character->m_climb.point = m_character->m_heightInfo.floor_point;  ///@FIXME: without it Lara are pulled high up, but this string was not been here.
        m_character->m_moveType = MoveType::OnFloor;
    }
    else if(m_character->m_command.roll)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_ROLL_BEGIN, 0);
    }
    else if(m_character->m_moveType == MoveType::OnWater)
    {
        m_character->m_inertiaLinear = 0.0;
        setNextState(LaraState::ONWATER_STOP);
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_TO_ONWATER, 0); // go to the air
    }
    else if(!m_character->m_command.jump)
    {
        setNextState(LaraState::UNDERWATER_INERTIA);
    }
}

void StateController::underwaterInertia()
{
    if(m_character->m_moveType == MoveType::OnWater)
    {
        m_character->m_inertiaLinear = 0.0;
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_TO_ONWATER, 0); // go to the air
    }
    else if(m_character->m_response.killed)
    {
        setNextState(LaraState::WATER_DEATH);
    }
    else if(m_character->m_command.roll)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_UNDERWATER_ROLL_BEGIN, 0);
    }
    else if(m_character->m_command.jump)
    {
        setNextState(LaraState::UNDERWATER_FORWARD);
    }
    else
    {
        setNextState(LaraState::UNDERWATER_STOP);
    }
}

void StateController::onwaterStop()
{
    if(m_character->m_response.killed)
    {
        setNextState(LaraState::WATER_DEATH);
    }
    else if((m_character->m_command.move[0] == 1) || m_character->m_command.jump)                    // dive works correct only after LaraState::ONWATER_FORWARD
    {
        m_character->m_moveDir = MoveDirection::Forward;
        setNextState(LaraState::ONWATER_FORWARD);
    }
    else if(m_character->m_command.move[0] == -1)
    {
        m_character->m_moveDir = MoveDirection::Backward;
        setNextState(LaraState::ONWATER_BACK);
    }
    else if(m_character->m_command.move[1] == -1)
    {
        if(m_character->m_command.shift)
        {
            m_character->m_moveDir = MoveDirection::Left;
            m_character->m_command.rot[0] = 0.0;
            setNextState(LaraState::ONWATER_LEFT);
        }
        else
        {
            // rotate on water
        }
    }
    else if(m_character->m_command.move[1] == 1)
    {
        if(m_character->m_command.shift)
        {
            m_character->m_moveDir = MoveDirection::Right;
            m_character->m_command.rot[0] = 0.0;
            setNextState(LaraState::ONWATER_RIGHT);
        }
        else
        {
            // rotate on water
        }
    }
    else if(m_character->m_moveType == MoveType::Underwater)
    {
        m_character->m_moveType = MoveType::OnWater;
    }
}

void StateController::onwaterForward()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS;
    m_character->m_moveType = MoveType::OnWater;

    if(m_character->m_response.killed)
    {
        setNextState(LaraState::WATER_DEATH);
    }
    else if(m_character->m_command.jump)
    {
        glm::float_t t = m_character->m_transform[3][2];
        HeightInfo next_fc = initHeightInfo();
        Character::getHeightInfo(glm::vec3(m_character->m_transform[3]), &next_fc);
        m_character->m_transform[3][2] = t;
        setNextState(LaraState::UNDERWATER_FORWARD);
        m_character->m_bf.animations.onFrame = onFrameSetUnderwater;                          // dive
    }
    else if(m_character->m_command.move[0] == 1)
    {
        if(m_character->m_command.action)
        {
            bool low_vertical_space = isLowVerticalSpace();

            if(m_character->m_moveType != MoveType::Climbing)
            {
                glm::float_t t = LaraTryHangWallOffset + LaraHangWallDistance;
                glm::vec3 global_offset( m_character->m_transform[1] * t );
                global_offset[2] += LaraHangVerticalEpsilon;                        // inc for water_surf.z
                HeightInfo next_fc = initHeightInfo();
                m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
                if(m_character->m_climb.edge_hit)
                {
                    low_vertical_space = true;
                }
                else
                {
                    low_vertical_space = false;
                    global_offset[2] += m_character->m_maxStepUpHeight + LaraHangVerticalEpsilon;
                    m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 0.0);
                }

                if(m_character->m_climb.edge_hit && (m_character->m_climb.next_z_space >= m_character->m_height - LaraHangVerticalEpsilon))// && (climb->edge_point[2] - pos[2] < ent->character->max_step_up_height))   // max_step_up_height is not correct value here
                {
                    m_character->m_moveDir = MoveDirection::Stay;
                    m_character->m_moveType = MoveType::Climbing;
                    m_character->m_bt.no_fix_all = true;
                    m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
                    m_character->updateTransform();
                    m_character->m_climb.point = m_character->m_climb.edge_point;
                }
            }

            if(m_character->m_moveType == MoveType::Climbing)
            {
                m_character->m_speed = {0,0,0};
                m_character->m_command.rot[0] = 0.0;
                m_character->m_bt.no_fix_all = true;
                if(low_vertical_space)
                {
                    m_character->setAnimation(TR_ANIMATION_LARA_ONWATER_TO_LAND_LOW, 0);
                }
                else
                {
                    m_character->setAnimation(TR_ANIMATION_LARA_CLIMB_OUT_OF_WATER, 0);
                }
                onFrameClimbOutOfWater(m_character, &m_character->m_bf.animations, world::animation::AnimUpdate::NewAnim);
            }
        }
        else if(!m_character->m_heightInfo.floor_hit || (m_character->m_transform[3][2] - m_character->m_height > m_character->m_heightInfo.floor_point[2] - m_character->m_swimDepth))
        {
            //ent->last_state = ent->last_state;                          // swim forward
        }
        else
        {
            setNextState(LaraState::WADE_FORWARD);
            m_character->m_bf.animations.onFrame = onFrameSetOnFloor;                        // to wade
        }
    }
    else
    {
        setNextState(LaraState::ONWATER_STOP);
    }
}

void StateController::onwaterBack()
{
    if(m_character->m_command.move[0] == -1 && !m_character->m_command.jump)
    {
        if(!m_character->m_heightInfo.floor_hit || (m_character->m_heightInfo.floor_point[2] + m_character->m_height < m_character->m_heightInfo.transition_level))
        {
            //ent->current_state = TR_STATE_CURRENT;                      // continue swimming
        }
        else
        {
            setNextState(LaraState::ONWATER_STOP);
        }
    }
    else
    {
        setNextState(LaraState::ONWATER_STOP);
    }
}

void StateController::onwaterLeft()
{
    m_character->m_command.rot[0] = 0.0;
    if(!m_character->m_command.jump)
    {
        if(m_character->m_command.move[1] == -1 && m_character->m_command.shift)
        {
            if(!m_character->m_heightInfo.floor_hit || (m_character->m_transform[3][2] - m_character->m_height > m_character->m_heightInfo.floor_point[2]))
            {
                // walk left
                setNextState(LaraState::ONWATER_LEFT);
            }
            else
            {
                // walk left
                setNextState(LaraState::WALK_LEFT);
                m_character->m_bf.animations.onFrame = onFrameSetOnFloor;
            }
        }
        else
        {
            setNextState(LaraState::ONWATER_STOP);
        }
    }
    else
    {
        setNextState(LaraState::UNDERWATER_DIVING);
    }
}

void StateController::onwaterRight()
{
    m_character->m_command.rot[0] = 0.0;
    if(!m_character->m_command.jump)
    {
        if(m_character->m_command.move[1] == 1 && m_character->m_command.shift)
        {
            if(!m_character->m_heightInfo.floor_hit || (m_character->m_transform[3][2] - m_character->m_height > m_character->m_heightInfo.floor_point[2]))
            {
                // swim RIGHT
                setNextState(LaraState::ONWATER_RIGHT);
            }
            else
            {
                // walk left
                setNextState(LaraState::WALK_RIGHT);
                m_character->m_bf.animations.onFrame = onFrameSetOnFloor;
            }
        }
        else
        {
            setNextState(LaraState::ONWATER_STOP);
        }
    }
    else
    {
        setNextState(LaraState::UNDERWATER_DIVING);
    }
}

void StateController::crouchIdle()
{
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    glm::vec3 move( m_character->m_transform[3] );
    move[2] += 0.5f * (m_character->m_bf.boundingBox.max[2] - m_character->m_bf.boundingBox.min[2]);
    HeightInfo next_fc = initHeightInfo();
    Character::getHeightInfo(move, &next_fc);

    m_character->lean(0.0);

    if(!m_character->m_command.crouch && !isLowVerticalSpace())
    {
        setNextState(LaraState::STOP);                        // Back to stand
    }
    else if((m_character->m_command.move[0] != 0) || m_character->m_response.killed)
    {
        setNextState(LaraState::CRAWL_IDLE);                  // Both forward & back provoke crawl stage
    }
    else if(m_character->m_command.jump)
    {
        setNextState(LaraState::CROUCH_ROLL);                 // Crouch roll
    }
    else
    {
        if(engine::engine_world.engineVersion > loader::Engine::TR3)
        {
            if(m_character->m_command.move[1] == 1)
            {
                m_character->m_moveDir = MoveDirection::Forward;
                setNextState(LaraState::CROUCH_TURN_RIGHT);
            }
            else if(m_character->m_command.move[1] == -1)
            {
                m_character->m_moveDir = MoveDirection::Forward;
                setNextState(LaraState::CROUCH_TURN_LEFT);
            }
        }
        else
        {
            m_character->m_command.rot[0] = 0.0;
        }
    }
}

void StateController::roll()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->lean(0.0);
    if(m_character->m_moveType == MoveType::FreeFalling)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_FORWARD, 0);
    }

    glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
    if((m_character->checkNextPenetration(move) > 0) && (m_character->m_response.horizontal_collide != 0x00))  // Smash into wall
    {
        setNextState(LaraState::STOP);
    }
}

void StateController::crawlIdle()
{
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    if(m_character->m_response.killed)
    {
        m_character->m_moveDir = MoveDirection::Stay;
        setNextState(LaraState::DEATH);
    }
    else if(m_character->m_command.move[1] == -1)
    {
        m_character->m_moveDir = MoveDirection::Forward;
        m_character->setAnimation(TR_ANIMATION_LARA_CRAWL_TURN_LEFT, 0);
    }
    else if(m_character->m_command.move[1] == 1)
    {
        m_character->m_moveDir = MoveDirection::Forward;
        m_character->setAnimation(TR_ANIMATION_LARA_CRAWL_TURN_RIGHT, 0);
    }
    else if(m_character->m_command.move[0] == 1)
    {
        glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
        if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00))
        {
            glm::vec3 global_offset( m_character->m_transform[1] * CrawlForwardOffset );
            global_offset[2] += 0.5f * (m_character->m_bf.boundingBox.max[2] + m_character->m_bf.boundingBox.min[2]);
            global_offset += glm::vec3(m_character->m_transform[3]);
            HeightInfo next_fc = initHeightInfo();
            Character::getHeightInfo(global_offset, &next_fc);
            if((next_fc.floor_point[2] < m_character->m_transform[3][2] + m_character->m_minStepUpHeight) &&
                    (next_fc.floor_point[2] > m_character->m_transform[3][2] - m_character->m_minStepUpHeight))
            {
                setNextState(LaraState::CRAWL_FORWARD);           // In TR4+, first state is crawlspace jump
            }
        }
    }
    else if(m_character->m_command.move[0] == -1)
    {
        glm::vec3 move( m_character->m_transform[1] * -PenetrationTestOffset );
        if((m_character->checkNextPenetration(move) == 0) || (m_character->m_response.horizontal_collide == 0x00))
        {
            glm::vec3 global_offset( m_character->m_transform[1] * -CrawlForwardOffset );
            global_offset[2] += 0.5f * (m_character->m_bf.boundingBox.max[2] + m_character->m_bf.boundingBox.min[2]);
            global_offset += glm::vec3(m_character->m_transform[3]);
            HeightInfo next_fc = initHeightInfo();
            Character::getHeightInfo(global_offset, &next_fc);
            if((next_fc.floor_point[2] < m_character->m_transform[3][2] + m_character->m_minStepUpHeight) &&
                    (next_fc.floor_point[2] > m_character->m_transform[3][2] - m_character->m_minStepUpHeight))
            {
                m_character->m_moveDir = MoveDirection::Backward;
                setNextState(LaraState::CRAWL_BACK);
            }
            else if(m_character->m_command.action && (m_character->m_response.horizontal_collide == 0) &&
                    (next_fc.floor_point[2] < m_character->m_transform[3][2] - m_character->m_height))
            {
                auto temp = m_character->m_transform[3];                                       // save entity position
                m_character->m_transform[3][0] = next_fc.floor_point[0];
                m_character->m_transform[3][1] = next_fc.floor_point[1];
                global_offset = glm::vec3(m_character->m_transform[1] * 0.5f * CrawlForwardOffset);
                global_offset[2] += 128.0;
                m_character->m_heightInfo.floor_hit = next_fc.floor_hit;
                m_character->m_heightInfo.floor_point = next_fc.floor_point;
                m_character->m_heightInfo.floor_normale = next_fc.floor_normale;
                m_character->m_heightInfo.floor_obj = next_fc.floor_obj;
                m_character->m_heightInfo.ceiling_hit = next_fc.ceiling_hit;
                m_character->m_heightInfo.ceiling_point = next_fc.ceiling_point;
                m_character->m_heightInfo.ceiling_normale = next_fc.ceiling_normale;
                m_character->m_heightInfo.ceiling_obj = next_fc.ceiling_obj;

                m_character->m_climb = m_character->checkClimbability(global_offset, &next_fc, 1.5f * m_character->m_bf.boundingBox.max[2]);
                m_character->m_transform[3] = temp;                                       // restore entity position
                if(m_character->m_climb.can_hang)
                {
                    m_character->m_angles[0] = m_character->m_climb.edge_z_ang;
                    m_character->m_moveDir = MoveDirection::Backward;
                    m_character->m_moveType = MoveType::Climbing;
                    m_character->m_climb.point = m_character->m_climb.edge_point;
                    setNextState(LaraState::CRAWL_TO_CLIMB);
                }
            }
        }
    }
    else if(!m_character->m_command.crouch)
    {
        setNextState(LaraState::CROUCH_IDLE);                // Back to crouch.
    }
}

void StateController::crawlToClimb()
{
    m_character->m_bt.no_fix_all = true;
    m_character->m_bf.animations.onFrame = onFrameCrawlToClimb;
}

void StateController::crawlForward()
{
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    m_character->m_command.rot[0] = m_character->m_command.rot[0] * 0.5f;
    glm::vec3 move( m_character->m_transform[1] * PenetrationTestOffset );
    if((m_character->checkNextPenetration(move) > 0) && (m_character->m_response.horizontal_collide != 0x00))
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_CRAWL_IDLE, 0);
        return;
    }
    glm::vec3 global_offset( m_character->m_transform[1] * CrawlForwardOffset );
    global_offset[2] += 0.5f * (m_character->m_bf.boundingBox.max[2] + m_character->m_bf.boundingBox.min[2]);
    global_offset += glm::vec3(m_character->m_transform[3]);
    HeightInfo next_fc = initHeightInfo();
    Character::getHeightInfo(global_offset, &next_fc);

    if((m_character->m_command.move[0] != 1) || m_character->m_response.killed)
    {
        setNextState(LaraState::CRAWL_IDLE); // Stop
    }
    else if((next_fc.floor_point[2] >= m_character->m_transform[3][2] + m_character->m_minStepUpHeight) ||
            (next_fc.floor_point[2] <= m_character->m_transform[3][2] - m_character->m_minStepUpHeight))
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_CRAWL_IDLE, 0);
    }
}

void StateController::crawlBack()
{
    m_character->m_moveDir = MoveDirection::Forward;   // Absurd? No, Core Design.
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    m_character->m_command.rot[0] = m_character->m_command.rot[0] * 0.5f;
    glm::vec3 move( m_character->m_transform[1] * -PenetrationTestOffset );
    if((m_character->checkNextPenetration(move) > 0) && (m_character->m_response.horizontal_collide != 0x00))
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_CRAWL_IDLE, 0);
        return;
    }
    glm::vec3 global_offset( m_character->m_transform[1] * -CrawlForwardOffset );
    global_offset[2] += 0.5f * (m_character->m_bf.boundingBox.max[2] + m_character->m_bf.boundingBox.min[2]);
    global_offset += glm::vec3(m_character->m_transform[3]);
    HeightInfo next_fc = initHeightInfo();
    Character::getHeightInfo(global_offset, &next_fc);
    if((m_character->m_command.move[0] != -1) || m_character->m_response.killed)
    {
        setNextState(LaraState::CRAWL_IDLE); // Stop
    }
    else if((next_fc.floor_point[2] >= m_character->m_transform[3][2] + m_character->m_minStepUpHeight) ||
            (next_fc.floor_point[2] <= m_character->m_transform[3][2] - m_character->m_minStepUpHeight))
    {
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->setAnimation(TR_ANIMATION_LARA_CRAWL_IDLE, 0);
    }
}

void StateController::crawlTurnLeft()
{
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    m_character->m_command.rot[0] *= ((m_character->m_bf.animations.current_frame > 3) && (m_character->m_bf.animations.current_frame < 14)) ? (1.0f) : (0.0f);

    if((m_character->m_command.move[1] != -1) || m_character->m_response.killed)
    {
        setNextState(LaraState::CRAWL_IDLE); // stop
    }
}

void StateController::crawlTurnRight()
{
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    m_character->m_command.rot[0] *= ((m_character->m_bf.animations.current_frame > 3) && (m_character->m_bf.animations.current_frame < 14)) ? (1.0f) : (0.0f);

    if((m_character->m_command.move[1] != 1) || m_character->m_response.killed)
    {
        setNextState(LaraState::CRAWL_IDLE); // stop
    }
}

void StateController::crouchTurnLeftRight()
{
    m_character->m_bt.no_fix_body_parts = BODY_PART_HANDS_2 | BODY_PART_HANDS_3 | BODY_PART_LEGS_3;
    m_character->m_command.rot[0] *= ((m_character->m_bf.animations.current_frame > 3) && (m_character->m_bf.animations.current_frame < 23)) ? (0.6f) : (0.0f);

    if((m_character->m_command.move[1] == 0) || m_character->m_response.killed)
    {
        setNextState(LaraState::CROUCH_IDLE);
    }
}

void StateController::monkeyswingIdle()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->m_moveDir = MoveDirection::Stay;
    ///@FIXME: stick for TR3+ monkey swing fix... something wrong with anim 150
    if(m_character->m_command.action && (m_character->m_moveType != MoveType::Monkeyswing) && m_character->m_heightInfo.ceiling_climb && (m_character->m_heightInfo.ceiling_hit) && (m_character->m_transform[3][2] + m_character->m_bf.boundingBox.max[2] > m_character->m_heightInfo.ceiling_point[2] - 96.0))
    {
        m_character->m_moveType = MoveType::Monkeyswing;
        m_character->setAnimation(TR_ANIMATION_LARA_MONKEY_IDLE, 0);
        setNextState(LaraState::MONKEYSWING_IDLE);
        m_character->m_transform[3][2] = m_character->m_heightInfo.ceiling_point[2] - m_character->m_bf.boundingBox.max[2];
    }

    if((m_character->m_moveType != MoveType::Monkeyswing) || !m_character->m_command.action)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->m_moveType = MoveType::FreeFalling;
    }
    else if(m_character->m_command.shift && (m_character->m_command.move[1] == -1))
    {
        setNextState(LaraState::MONKEYSWING_LEFT);
    }
    else if(m_character->m_command.shift && (m_character->m_command.move[1] == 1))
    {
        setNextState(LaraState::MONKEYSWING_RIGHT);
    }
    else if(m_character->m_command.move[0] == 1)
    {
        m_character->m_moveDir = MoveDirection::Forward;
        setNextState(LaraState::MONKEYSWING_FORWARD);
    }
    else if(m_character->m_command.move[1] == -1)
    {
        setNextState(LaraState::MONKEYSWING_TURN_LEFT);
    }
    else if(m_character->m_command.move[1] == 1)
    {
        setNextState(LaraState::MONKEYSWING_TURN_RIGHT);
    }
}

void StateController::monkeyswingTurnLeft()
{
    m_character->m_command.rot[0] *= 0.5;
    if((m_character->m_moveType != MoveType::Monkeyswing) || !m_character->m_command.action)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->m_moveType = MoveType::FreeFalling;
    }
    else if(m_character->m_command.move[1] != -1)
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
    }
}

void StateController::monkeyswingTurnRight()
{
    m_character->m_command.rot[0] *= 0.5;
    if((m_character->m_moveType != MoveType::Monkeyswing) || !m_character->m_command.action)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);
        m_character->m_moveDir = MoveDirection::Stay;
        m_character->m_moveType = MoveType::FreeFalling;
    }
    else if(m_character->m_command.move[1] != 1)
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
    }
}

void StateController::monkeyswingForward()
{
    m_character->m_command.rot[0] *= 0.45f;
    m_character->m_moveDir = MoveDirection::Forward;

    if((m_character->m_moveType != MoveType::Monkeyswing) || !m_character->m_command.action)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);
        m_character->m_moveType = MoveType::FreeFalling;
    }
    else if(m_character->m_command.move[0] != 1)
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
    }
}

void StateController::monkeyswingLeft()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->m_moveDir = MoveDirection::Left;

    if((m_character->m_moveType != MoveType::Monkeyswing) || !m_character->m_command.action)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);
        m_character->m_moveType = MoveType::FreeFalling;
    }
    else if(m_character->m_command.move[0] != 1)
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
    }
}

void StateController::monkeyswingRight()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->m_moveDir = MoveDirection::Right;

    if((m_character->m_moveType != MoveType::Monkeyswing) || !m_character->m_command.action)
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TRY_HANG_VERTICAL, 0);
        m_character->m_moveType = MoveType::FreeFalling;
    }
    else if(m_character->m_command.move[0] != 1)
    {
        setNextState(LaraState::MONKEYSWING_IDLE);
    }
}

void StateController::tightropeEnter()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->m_bt.no_fix_all = true;
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bf.animations.onFrame = onFrameToTightrope;
    setNextState(LaraState::TIGHTROPE_IDLE);
}

void StateController::tightropeExit()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->m_bt.no_fix_all = true;
    m_character->m_moveDir = MoveDirection::Forward;
    m_character->m_bf.animations.onFrame = onFrameFromTightrope;
    setNextState(LaraState::STOP);
}

void StateController::tightropeIdle()
{
    m_character->m_command.rot[0] = 0.0;

    if(m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_TIGHTROPE_STAND)
    {
        if(m_character->m_response.lean == LeanType::Left)
        {
            setNextState(LaraState::TIGHTROPE_BALANCING_LEFT);
            m_character->m_response.lean = LeanType::None;
            return;
        }
        else if(m_character->m_response.lean == LeanType::Right)
        {
            setNextState(LaraState::TIGHTROPE_BALANCING_RIGHT);
            m_character->m_response.lean = LeanType::None;
            return;
        }
        else if(isLastFrame())
        {
            uint16_t chance_to_fall = rand() % 0x7FFF;

            if(chance_to_fall > 0x5FFF)
                setNextState(LaraState::TIGHTROPE_BALANCING_LEFT);
            else if(chance_to_fall < 0x2000)
                setNextState(LaraState::TIGHTROPE_BALANCING_RIGHT);
        }
    }

    if((m_character->m_command.roll) || (m_character->m_command.move[0] == -1))
    {
        m_character->setAnimation(TR_ANIMATION_LARA_TIGHTROPE_TURN, 0);
        m_character->m_moveDir = MoveDirection::Forward;
    }
    else if(m_character->m_command.move[0] == 1)
    {
        setNextState(LaraState::TIGHTROPE_FORWARD);
    }
}

void StateController::tightropeForward()
{
    m_character->m_command.rot[0] = 0.0;
    m_character->m_moveDir = MoveDirection::Forward;

    if(m_character->m_command.move[0] != 1)
    {
        setNextState(LaraState::TIGHTROPE_IDLE);
    }
    else
    {
        uint16_t chance_to_unbal = rand() % 0x7FFF;
        if(chance_to_unbal < 0x00FF)
        {
            setNextState(LaraState::TIGHTROPE_IDLE);

            if(chance_to_unbal > 0x007F)
                m_character->m_response.lean = LeanType::Left;
            else
                m_character->m_response.lean = LeanType::Right;
        }
    }
}

void StateController::tightropeBalancingLeft()
{
    m_character->m_command.rot[0] = 0.0;

    if((m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_TIGHTROPE_FALL_LEFT) && isLastFrame())
    {
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_LONG, 0);
        m_character->m_transform[3] += m_character->m_transform * glm::vec4(-256.0, 192.0, -640.0, 0);
    }
    else if((m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_TIGHTROPE_LOOSE_LEFT) && (m_character->m_bf.animations.current_frame >= m_character->m_bf.animations.getCurrentAnimationFrame().frames.size() / 2) && (m_character->m_command.move[1] == 1))
    {
        // MAGIC: mirroring animation offset.
        m_character->setAnimation(TR_ANIMATION_LARA_TIGHTROPE_RECOVER_LEFT, m_character->m_bf.animations.getCurrentAnimationFrame().frames.size()-m_character->m_bf.animations.current_frame);
    }
}

void StateController::tightropeBalancingRight()
{
    m_character->m_command.rot[0] = 0.0;

    if((m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_TIGHTROPE_FALL_RIGHT) && isLastFrame())
    {
        m_character->m_moveType = MoveType::FreeFalling;
        m_character->m_transform[3] += m_character->m_transform * glm::vec4(256.0, 192.0, -640.0, 0);
        m_character->setAnimation(TR_ANIMATION_LARA_FREE_FALL_LONG, 0);
    }
    else if((m_character->m_bf.animations.current_animation == TR_ANIMATION_LARA_TIGHTROPE_LOOSE_RIGHT) && (m_character->m_bf.animations.current_frame >= m_character->m_bf.animations.getCurrentAnimationFrame().frames.size() / 2) && (m_character->m_command.move[1] == -1))
    {
        // MAGIC: mirroring animation offset.
        m_character->setAnimation(TR_ANIMATION_LARA_TIGHTROPE_RECOVER_RIGHT, m_character->m_bf.animations.getCurrentAnimationFrame().frames.size()-m_character->m_bf.animations.current_frame);
    }
}

void StateController::fixEndOfClimbOn()
{
    // The code here prevents Lara's UGLY move in end of "climb on" states.
    m_character->m_command.rot[0] = 0;
    m_character->m_bt.no_fix_all = true;

}

HeightInfo StateController::initHeightInfo() const
{
    HeightInfo next_fc;
    next_fc.sp = m_character->m_heightInfo.sp;
    next_fc.cb = m_character->m_rayCb;
    next_fc.cb->m_closestHitFraction = 1.0;
    next_fc.cb->m_collisionObject = nullptr;
    next_fc.ccb = m_character->m_convexCb;
    next_fc.ccb->m_closestHitFraction = 1.0;
    next_fc.ccb->m_hitCollisionObject = nullptr;

    return next_fc;
}

bool StateController::isLowVerticalSpace() const
{
    return m_character->m_heightInfo.floor_hit
            && m_character->m_heightInfo.ceiling_hit
            && (m_character->m_heightInfo.ceiling_point[2] - m_character->m_heightInfo.floor_point[2] < m_character->m_height - LaraHangVerticalEpsilon);
}

bool StateController::isLastFrame() const
{
    return static_cast<int>(m_character->m_bf.animations.getCurrentAnimationFrame().frames.size()) <= m_character->m_bf.animations.current_frame + 1;
}

void StateController::setNextState(LaraState state)
{
    if(m_handlers.find(state) == m_handlers.end())
        throw std::runtime_error("Invalid state");
    m_character->m_bf.animations.next_state = state;
}

} // namespace world
