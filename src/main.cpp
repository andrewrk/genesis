#include "util.hpp"
#include "genesis.hpp"
#include "gui.hpp"

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    genesis_init();

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("SDL initialize");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("genesis",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        1366, 768, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if (!window)
        panic("unable to create SDL window");

    SDL_GLContext context = SDL_GL_CreateContext(window);

    GLenum status = glewInit();
    if (status != GLEW_OK)
        panic("glew init error: %s", glewGetErrorString(status));
    // glewInit sometimes returns invalid enum
    GLenum err = glGetError();
    if (err != GL_NO_ERROR && err != GL_INVALID_ENUM) {
        char *string = (char *)gluErrorString(err);
        panic("gl glew init error: %s", string);
    }

    Gui gui(window);

    LabelWidget *label_widget = gui.create_label_widget();
    label_widget->set_text(String("abcdefghijklmnop andy"));
    label_widget->set_pos(100, 100);

    gui.exec();

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
