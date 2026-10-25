#ifndef VIPER_DIRECTINPUT_KEYBOARD_SERVICE_H
#define VIPER_DIRECTINPUT_KEYBOARD_SERVICE_H

#include <Windows.h>

#include <memory>

#include "viper/directinput_keyboard_state.hpp"

namespace viper {

// Owns a DirectInput8 system keyboard device. The returned state is the
// unmodified 256-byte DirectInput keyboard state; callers decide how to map it.
class DirectInputKeyboardService {
public:
    using KeyboardState = DirectInputKeyboardState;

    DirectInputKeyboardService();
    ~DirectInputKeyboardService();

    DirectInputKeyboardService(const DirectInputKeyboardService&) = delete;
    DirectInputKeyboardService& operator=(const DirectInputKeyboardService&) = delete;
    DirectInputKeyboardService(DirectInputKeyboardService&&) noexcept;
    DirectInputKeyboardService& operator=(DirectInputKeyboardService&&) noexcept;

    // Initializes DirectInput and acquires the system keyboard for hwnd.
    // Returns false if DirectInput/device setup fails.
    bool initialize(HWND hwnd);

    // Polls the device and reacquires after input loss/not-acquired errors.
    // Returns false if no valid state could be obtained this call.
    bool poll(KeyboardState& state);

    // Releases the device and DirectInput interface. Safe to call repeatedly.
    void shutdown() noexcept;

    bool is_initialized() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace viper

#endif // VIPER_DIRECTINPUT_KEYBOARD_SERVICE_H
