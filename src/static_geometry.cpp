#include "static_geometry.hpp"

StaticGeometry::StaticGeometry() {
    GLfloat rect_2d_vertexes[4][3] = {
        {0,    0,    0},
        {0,    1.0f, 0},
        {1.0f, 0,    0},
        {1.0f, 1.0f, 0},
    };
    glGenBuffers(1, &_rect_2d_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, _rect_2d_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(GLfloat), rect_2d_vertexes, GL_STATIC_DRAW);


    GLfloat rect_2d_tex_coords[4][2] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {1, 1},
    };
    glGenBuffers(1, &_rect_2d_tex_coord_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, _rect_2d_tex_coord_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), rect_2d_tex_coords, GL_STATIC_DRAW);

}

StaticGeometry::~StaticGeometry() {
    glDeleteBuffers(1, &_rect_2d_tex_coord_buffer);
    glDeleteBuffers(1, &_rect_2d_vertex_buffer);
}
