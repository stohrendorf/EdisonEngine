#pragma once

#include "api/gl.hpp"
#include "glassert.h"

#include <gsl/gsl-lite.hpp>

#ifndef SOGLB_NO_DEBUG_GROUPS
#  define SOGLB_NO_DEBUG_GROUPS NDEBUG
#endif

namespace gl
{
#if SOGLB_NO_DEBUG_GROUPS
#  define SOGLB_DEBUGGROUP(name)
#else
// https://stackoverflow.com/a/35087985

// NOLINTNEXTLINE(bugprone-reserved-identifier)
#  define _SOGLB_PASTE(x, y) x##y
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#  define _SOGLB_CAT(x, y) _SOGLB_PASTE(x, y)

#  define SOGLB_DEBUGGROUP(name)                                                \
    [[maybe_unused]] ::gl::DebugGroup _SOGLB_CAT(_soglb_debug_group_, __LINE__) \
    {                                                                           \
      name                                                                      \
    }

class DebugGroup final
{
public:
  explicit DebugGroup(const std::string& message, const uint32_t id = 0)
  {
    GL_ASSERT(api::pushDebugGroup(api::DebugSource::DebugSourceApplication,
                                  id,
                                  gsl::narrow<api::core::SizeType>(message.length()),
                                  message.c_str()));
  }

  DebugGroup(const DebugGroup&) = delete;
  DebugGroup(DebugGroup&&) noexcept = delete;
  DebugGroup& operator=(const DebugGroup&) = delete;
  DebugGroup& operator=(DebugGroup&&) = delete;

  ~DebugGroup()
  {
    GL_ASSERT(api::popDebugGroup());
  }
};
#endif
} // namespace gl
