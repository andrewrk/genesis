#ifndef COLOR_HPP
#define COLOR_HPP

#include "glm.hpp"

#include <stdio.h>

static glm::vec4 parse_color(const char *color) {
    if (color[0] == '#')
        color += 1;
    size_t len = strlen(color);
    glm::vec4 out;
    unsigned r, g, b, a;
    if (len == 6) {
        sscanf(color, "%2x%2x%2x", &r, &g, &b);
        out[0] = (r / 255.0f);
        out[1] = (g / 255.0f);
        out[2] = (b / 255.0f);
        out[3] = 1.0f;
    } else if (len == 8) {
        sscanf(color, "%2x%2x%2x%2x", &r, &g, &b, &a);
        out[0] = (r / 255.0f);
        out[1] = (g / 255.0f);
        out[2] = (b / 255.0f);
        out[3] = (a / 255.0f);
    } else {
        panic("invalid color length");
    }
    return out;
}

#endif
