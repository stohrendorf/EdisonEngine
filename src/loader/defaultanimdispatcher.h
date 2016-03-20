#pragma once

#include "level.h"

namespace loader
{
class DefaultAnimDispatcher final : public irr::scene::IAnimationEndCallBack
{
private:
    const Level* const m_level;
    const AnimatedModel& m_model;
    uint16_t m_currentAnimationId;
    uint16_t m_targetState = 0;
    const std::string m_name;

public:
    DefaultAnimDispatcher(const Level* level, const AnimatedModel& model, irr::scene::IAnimatedMeshSceneNode* node, const std::string& name);

    virtual void OnAnimationEnd(irr::scene::IAnimatedMeshSceneNode* node) override;

    uint16_t getCurrentState() const;
    void setTargetState(uint16_t state) noexcept
    {
        BOOST_LOG_TRIVIAL(debug) << "Set target state=" << state << " (" << m_name << ") current=" << getCurrentState();
        m_targetState = state;
    }
    uint16_t getCurrentAnimationId() const noexcept
    {
        return m_currentAnimationId;
    }

    const std::string& getName() const
    {
        return m_name;
    }
    
private:
    void startAnimLoop(irr::scene::IAnimatedMeshSceneNode* node, irr::u32 frame);

    irr::u32 getCurrentFrame(irr::scene::IAnimatedMeshSceneNode* node) const;
};
}