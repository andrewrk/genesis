#include "shader_program_manager.hpp"
#include "debug.hpp"

ShaderProgramManager::ShaderProgramManager() :
    _texture_shader_program(R"VERTEX(

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

void main(void)
{
    FragColor = texture(Tex, FragTexCoord);
}

)FRAGMENT", NULL),
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
    FragColor = vec4(1, 1, 1, texture(Tex, FragTexCoord).r) * Color;
}

)FRAGMENT", NULL),
    _primitive_shader_program(R"VERTEX(

#version 150 core

in vec3 VertexPosition;

uniform mat4 MVP;

void main(void) {
    gl_Position = MVP * vec4(VertexPosition, 1.0);
}

)VERTEX", R"FRAGMENT(

#version 150 core

out vec4 FragColor;

uniform vec4 Color;

void main(void) {
    FragColor = Color;
}

)FRAGMENT", NULL),
    texture_color_program(R"VERTEX(

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
    _texture_attrib_tex_coord = _texture_shader_program.attrib_location("TexCoord");
    _texture_attrib_position = _texture_shader_program.attrib_location("VertexPosition");
    _texture_uniform_mvp = _texture_shader_program.uniform_location("MVP");
    _texture_uniform_tex = _texture_shader_program.uniform_location("Tex");

    _text_attrib_tex_coord = _text_shader_program.attrib_location("TexCoord");
    _text_attrib_position = _text_shader_program.attrib_location("VertexPosition");
    _text_uniform_mvp = _text_shader_program.uniform_location("MVP");
    _text_uniform_tex = _text_shader_program.uniform_location("Tex");
    _text_uniform_color = _text_shader_program.uniform_location("Color");

    _primitive_attrib_position = _primitive_shader_program.attrib_location("VertexPosition");
    _primitive_uniform_mvp = _primitive_shader_program.uniform_location("MVP");
    _primitive_uniform_color = _primitive_shader_program.uniform_location("Color");

    texture_color_attrib_tex_coord = texture_color_program.attrib_location("TexCoord");
    texture_color_attrib_position = texture_color_program.attrib_location("VertexPosition");
    texture_color_uniform_mvp = texture_color_program.uniform_location("MVP");
    texture_color_uniform_tex = texture_color_program.uniform_location("Tex");
    texture_color_uniform_color = texture_color_program.uniform_location("Color");

    assert_no_gl_error();
}

ShaderProgramManager::~ShaderProgramManager() {

}
