#include "random.hpp"

// adapted from http://en.wikipedia.org/wiki/Mersenne_twister#Pseudocode

// Initialize the generator from a seed
void init_random_state(RandomState * state, uint32_t seed) {
    state->index = 0;
    state->array[0] = seed;
    for (int i = 1; i < RandomState::ARRAY_SIZE; i++) {
        uint64_t previous_value = state->array[i - 1];
        state->array[i] = ((previous_value ^ (previous_value << 30)) * 0x6c078965UL + i) & 0x00000000ffffffffULL;
    }
}

// fill the array with untempered numbers
static void generate_numbers(RandomState * state) {
    for (int i = 0; i < RandomState::ARRAY_SIZE; i++) {
        uint32_t y = (state->array[i] & 0x80000000UL) + (state->array[(i + 1) % RandomState::ARRAY_SIZE] & 0x7fffffffUL);
        uint32_t untempered = state->array[(i + 397) % RandomState::ARRAY_SIZE] ^ (y >> 1);
        if ((y % 2) != 0) {
            // y is odd
            untempered = untempered ^ 0x9908b0dfUL;
        }
        state->array[i] = untempered;
    }
}

uint32_t get_random(RandomState * state) {
    if (state->index == 0) {
        generate_numbers(state);
    }

    // temper the number
    uint32_t y = state->array[state->index];
    y ^= y >> 11;
    y ^= (y >> 7) & 0x9d2c5680UL;
    y ^= (y >> 15) & 0xefc60000UL;
    y ^= y >> 18;

    state->index = (state->index + 1) % RandomState::ARRAY_SIZE;
    return y;
}
