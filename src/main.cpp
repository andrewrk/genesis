#include "util.hpp"
#include "os.hpp"
#include "genesis_editor.hpp"

#include <SDL2/SDL.h>
#include <groove/groove.h>


int main(int argc, char *argv[]) {
    os_init();
    groove_init();
    groove_set_logging(GROOVE_LOG_INFO);

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("SDL initialize");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

    GenesisEditor genesis_editor;
    genesis_editor.exec();

    SDL_Quit();
    groove_finish();
    return 0;
}
