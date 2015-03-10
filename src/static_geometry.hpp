#ifndef STATIC_GEOMETRY_HPP
#define STATIC_GEOMETRY_HPP

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class StaticGeometry {
public:
    StaticGeometry();
    ~StaticGeometry();

    GLuint _rect_2d_vertex_buffer;
    GLuint _rect_2d_tex_coord_buffer;


private:

    StaticGeometry(const StaticGeometry &copy) = delete;
    StaticGeometry &operator=(const StaticGeometry &copy) = delete;
};

#endif
