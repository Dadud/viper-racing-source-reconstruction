#define DIRECTINPUT_VERSION 0x0800

#include "directinput_keyboard_service.h"

#include <windows.h>
#include <dinput.h>

#include <utility>

namespace viper {

struct DirectInputKeyboardService::Impl {
    IDirectInput8W* direct_input = nullptr;
    IDirectInputDevice8W* keyboard = nullptr;
    HWND window = nullptr;

    void shutdown() noexcept {
        if (keyboard != nullptr) {
            keyboard->Unacquire();
            keyboard->Release();
            keyboard = nullptr;
        }
        if (direct_input != nullptr) {
            direct_input->Release();
            direct_input = nullptr;
        }
        window = nullptr;
    }

    ~Impl() { shutdown(); }

    bool acquire() noexcept {
        if (keyboard == nullptr) {
            return false;
        }
        const HRESULT result = keyboard->Acquire();
        return SUCCEEDED(result) || result == S_FALSE;
    }
};

DirectInputKeyboardService::DirectInputKeyboardService()
    : impl_(std::make_unique<Impl>()) {}

DirectInputKeyboardService::~DirectInputKeyboardService() = default;

DirectInputKeyboardService::DirectInputKeyboardService(
    DirectInputKeyboardService&&) noexcept = default;

DirectInputKeyboardService& DirectInputKeyboardService::operator=(
    DirectInputKeyboardService&&) noexcept = default;

bool DirectInputKeyboardService::initialize(HWND hwnd) {
    shutdown();
    if (hwnd == nullptr || !impl_) {
        return false;
    }

    impl_->window = hwnd;
    HRESULT result = DirectInput8Create(
        GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W,
        reinterpret_cast<void**>(&impl_->direct_input), nullptr);
    if (FAILED(result)) {
        shutdown();
        return false;
    }

    result = impl_->direct_input->CreateDevice(GUID_SysKeyboard,
                                                &impl_->keyboard, nullptr);
    if (FAILED(result)) {
        shutdown();
        return false;
    }

    result = impl_->keyboard->SetDataFormat(&c_dfDIKeyboard);
    if (FAILED(result)) {
        shutdown();
        return false;
    }

    result = impl_->keyboard->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(result)) {
        shutdown();
        return false;
    }
    // The runtime window may not yet own foreground focus. Keep the device
    // configured and let poll() acquire it once focus is available.
    (void)impl_->acquire();
    return true;
}

bool DirectInputKeyboardService::poll(KeyboardState& state) {
    if (!impl_ || impl_->keyboard == nullptr) {
        return false;
    }

    HRESULT result = impl_->keyboard->GetDeviceState(
        static_cast<DWORD>(state.size()), state.data());
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        // The original routine reacquires and returns immediately. A new
        // state sample is taken on the next race update, not in this one.
        (void)impl_->acquire();
        return false;
    }
    if (FAILED(result)) {
        return false;
    }

    // The original poller exposes three combined-key selectors in the last
    // bytes of its 256-byte state table.
    refresh_original_keyboard_aliases(state);
    return true;
}

void DirectInputKeyboardService::shutdown() noexcept {
    if (impl_) {
        impl_->shutdown();
    }
}

bool DirectInputKeyboardService::is_initialized() const noexcept {
    return impl_ != nullptr && impl_->keyboard != nullptr;
}

} // namespace viper
