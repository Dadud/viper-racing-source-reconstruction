#include "viper/directinput_keyboard_state.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    viper::DirectInputKeyboardState state{};
    state[0x36U] = 0x01U;
    state[0x2aU] = 0x80U;
    state[0x9dU] = 0x02U;
    state[0x1dU] = 0x40U;
    state[0x38U] = 0x04U;
    state[0xb8U] = 0x20U;

    viper::refresh_original_keyboard_aliases(state);
    if (state[0xfdU] != 0x81U || state[0xfeU] != 0x42U ||
        state[0xffU] != 0x24U) {
        std::cerr << "combined keyboard selectors did not OR both key bytes\n";
        return EXIT_FAILURE;
    }

    state[0x36U] = 0U;
    state[0x2aU] = 0U;
    state[0x9dU] = 0U;
    state[0x1dU] = 0U;
    state[0x38U] = 0U;
    state[0xb8U] = 0U;
    viper::refresh_original_keyboard_aliases(state);
    if (state[0xfdU] != 0U || state[0xfeU] != 0U || state[0xffU] != 0U) {
        std::cerr << "combined keyboard selectors did not clear on release\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
