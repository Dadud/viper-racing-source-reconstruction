#ifndef VIPER_DIRECTINPUT_KEYBOARD_STATE_HPP
#define VIPER_DIRECTINPUT_KEYBOARD_STATE_HPP

#include <array>
#include <cstdint>

namespace viper {

using DirectInputKeyboardState = std::array<std::uint8_t, 256>;

// Mirrors the three synthetic selector bytes refreshed by original poller
// 0x4130D0 after a successful DirectInput keyboard read.
inline void refresh_original_keyboard_aliases(
    DirectInputKeyboardState& state) noexcept
{
    state[0xfdU] = static_cast<std::uint8_t>(state[0x36U] | state[0x2aU]);
    state[0xfeU] = static_cast<std::uint8_t>(state[0x9dU] | state[0x1dU]);
    state[0xffU] = static_cast<std::uint8_t>(state[0x38U] | state[0xb8U]);
}

} // namespace viper

#endif // VIPER_DIRECTINPUT_KEYBOARD_STATE_HPP
