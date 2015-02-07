#include "util.hpp"
#include "genesis.hpp"
#include "gui.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

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

    Gui gui(window);

    LabelWidget *label_widget = gui.create_label_widget();
    label_widget->set_text("label with background");
    label_widget->set_pos(100, 100);

    gui.exec();

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    SDL_Quit();
    return 0;
}
