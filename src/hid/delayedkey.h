#pragma once

#include "inputhandler.h"

#include <chrono>
#include <optional>

namespace hid
{
enum class GlfwKey;

class DelayedKey
{
public:
  explicit DelayedKey(GlfwKey key, int seconds)
      : m_key{key}
      , m_delay{seconds}
  {
  }

  [[nodiscard]] auto getKey() const
  {
    return m_key;
  }

  [[nodiscard]] const auto& getDelay() const
  {
    return m_delay;
  }

  void update(const InputHandler& handler)
  {
    if(!m_pressedSince.has_value())
    {
      if(handler.hasKey(m_key))
        m_pressedSince = std::chrono::system_clock::now();
    }
    else
    {
      if(!handler.hasKey(m_key))
        m_pressedSince.reset();
    }
  }

  [[nodiscard]] bool isActive() const
  {
    return m_pressedSince.has_value() && std::chrono::system_clock::now() - m_pressedSince.value() >= m_delay;
  }

private:
  const GlfwKey m_key;
  const std::chrono::seconds m_delay;
  std::optional<std::chrono::system_clock::time_point> m_pressedSince{};
};
} // namespace hid
