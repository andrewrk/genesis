#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program.hpp"

class Gui {
public:
    Gui();
    ~Gui();
private:

    ShaderProgram _text_shader_program;
};

#endif
