#include "gui.hpp"

Gui::Gui() :
    _text_shader_program(R"VERTEX(

#version 150 core

in vec3 VertexPosition;
in vec2 TexCoord;

out vec2 FragTexCoord;

uniform mat4 MVP;

void main(void)
{
    FragTexCoord = TexCoord;
    gl_Position = MVP * vec4(VertexPosition, 1.0);
}

)VERTEX", R"FRAGMENT(

#version 150 core

in vec2 FragTexCoord;
out vec4 FragColor;

uniform sampler2D Tex;
uniform vec4 Color;


void main(void)
{
    FragColor = vec4(1, 1, 1, texture(Tex, FragTexCoord).a) * Color;
}

)FRAGMENT", NULL)
{
}

Gui::~Gui() {

}
