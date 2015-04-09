#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <stdint.h>

struct RandomState {
    static const int ARRAY_SIZE = 624;
    uint32_t array[ARRAY_SIZE];
    int index;
};

void init_random_state(RandomState * state, uint32_t seed);
uint32_t get_random(RandomState * state);

#endif

