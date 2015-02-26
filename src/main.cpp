#include "util.hpp"
#include "os.hpp"
#include "genesis_editor.hpp"
#include "genesis.h"

#include <SDL2/SDL.h>

static int print_usage(char *arg0) {
    fprintf(stderr, "%s [filename]\n", arg0);
    return -1;
}

int main(int argc, char *argv[]) {
    genesis_init();
    os_init();

    const char *input_filename = NULL;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            return print_usage(argv[0]);
        } else if (!input_filename) {
            input_filename = arg;
        } else {
            return print_usage(argv[0]);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        panic("SDL initialize");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

    GenesisEditor genesis_editor;
    if (input_filename)
        genesis_editor.edit_file(input_filename);
    genesis_editor.exec();

    SDL_Quit();
    return 0;
}
