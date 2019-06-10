#pragma once

#include "bindableresource.h"

namespace render
{
namespace gl
{
class VertexBuffer : public BindableResource
{
public:
    explicit VertexBuffer(const std::string& label = {})
            : BindableResource{glGenBuffers,
                               [](const GLuint handle) { glBindBuffer( GL_ARRAY_BUFFER, handle ); },
                               glDeleteBuffers,
                               GL_BUFFER,
                               label}
    {
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    const void* map()
    {
        bind();
        const auto data = GL_ASSERT_FN( glMapBuffer( GL_ARRAY_BUFFER, GL_READ_ONLY ) );
        return data;
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    void* mapRw()
    {
        bind();
        const auto data = GL_ASSERT_FN( glMapBuffer( GL_ARRAY_BUFFER, GL_READ_WRITE ) );
        return data;
    }

    static void unmap()
    {
        GL_ASSERT( glUnmapBuffer( GL_ARRAY_BUFFER ) );
    }
};
}
}
