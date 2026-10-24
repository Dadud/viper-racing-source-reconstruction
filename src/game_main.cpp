#include <Windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "viper/game_assets.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using Microsoft::WRL::ComPtr;

struct Options {
    std::filesystem::path data_dir;
    bool validate_assets{};
    bool help{};
    bool software{};
    std::optional<unsigned> smoke_frames;
};

void usage(std::ostream& out)
{
    out << "Viper Racing reconstruction runtime\n"
        << "Usage: viper_game [--data-dir PATH] [--validate-assets] [--smoke-frames N] [--software] [--help]\n"
        << "Set VIPER_DATA_DIR or pass --data-dir to locate extracted game assets.\n";
}

Options parse_options(const int argc, wchar_t** argv)
{
    Options options;
    const DWORD required = GetEnvironmentVariableW(L"VIPER_DATA_DIR", nullptr, 0);
    if (required > 0) {
        std::wstring value(required, L'\0');
        const DWORD length = GetEnvironmentVariableW(L"VIPER_DATA_DIR", value.data(), required);
        if (length == 0 || length >= required)
            throw std::runtime_error("could not read VIPER_DATA_DIR");
        value.resize(length);
        options.data_dir = value;
    }
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view arg(argv[i]);
        if (arg == L"--help" || arg == L"-h") {
            options.help = true;
        } else if (arg == L"--validate-assets") {
            options.validate_assets = true;
        } else if (arg == L"--software") {
            options.software = true;
        } else if (arg == L"--smoke-frames") {
            if (++i >= argc) throw std::runtime_error("--smoke-frames requires a positive frame count");
            unsigned frames{};
            const std::wstring_view value(argv[i]);
            bool valid = !value.empty();
            for (const wchar_t ch : value) {
                if (ch < L'0' || ch > L'9' ||
                    frames > (std::numeric_limits<unsigned>::max() - static_cast<unsigned>(ch - L'0')) / 10U) {
                    valid = false;
                    break;
                }
                frames = frames * 10U + static_cast<unsigned>(ch - L'0');
            }
            if (!valid || frames == 0)
                throw std::runtime_error("--smoke-frames requires a positive integer");
            options.smoke_frames = frames;
        } else if (arg == L"--data-dir") {
            if (++i >= argc) throw std::runtime_error("--data-dir requires a path");
            options.data_dir = argv[i];
        } else {
            throw std::runtime_error("unknown argument: " + std::filesystem::path(arg).string());
        }
    }
    return options;
}

viper::GameAssets load_assets(const std::filesystem::path& root)
{
    if (root.empty()) {
        throw std::runtime_error("pass --data-dir PATH or set VIPER_DATA_DIR");
    }
    auto assets = viper::GameAssets::load(root);
    for (const auto name : {"common.res", "ui.res", "race.res", "viper.car", "bemidji.trk"})
        std::cout << "Loaded " << name << ": " << assets.bundle(name).entries().size() << " resources\n";
    std::cout << "Loaded english.lng: " << assets.language().entries.size() << " strings\n"
              << "Required assets validated in " << assets.root().string() << '\n';
    return assets;
}

class D3DWindow {
public:
    bool create(HWND hwnd, bool software)
    {
        RECT rect{};
        if (!GetClientRect(hwnd, &rect)) return false;
        width_ = static_cast<UINT>(rect.right - rect.left);
        height_ = static_cast<UINT>(rect.bottom - rect.top);
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = hwnd;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL selected{};
        const HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr,
            software ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
            nullptr, 0, levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            &desc, &swap_chain_, &device_, &selected, &context_);
        if (FAILED(hr)) return false;
        return create_target();
    }

    bool resize(UINT width, UINT height)
    {
        if (!width || !height || !swap_chain_) return true;
        if (context_) context_->OMSetRenderTargets(0, nullptr, nullptr);
        target_.Reset();
        if (FAILED(swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
        width_ = width;
        height_ = height;
        return create_target();
    }

    bool render()
    {
        if (!target_ || !context_ || !swap_chain_) return false;
        constexpr float color[]{0.035f, 0.055f, 0.075f, 1.0f};
        context_->OMSetRenderTargets(1, target_.GetAddressOf(), nullptr);
        D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width_),
                                static_cast<float>(height_), 0.0f, 1.0f};
        context_->RSSetViewports(1, &viewport);
        context_->ClearRenderTargetView(target_.Get(), color);
        return SUCCEEDED(swap_chain_->Present(1, 0));
    }

private:
    bool create_target()
    {
        ComPtr<ID3D11Texture2D> back_buffer;
        if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) return false;
        return SUCCEEDED(device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &target_));
    }

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swap_chain_;
    ComPtr<ID3D11RenderTargetView> target_;
    UINT width_{};
    UINT height_{};
};

struct WindowState { D3DWindow graphics; };

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_SIZE:
        if (state && wparam != SIZE_MINIMIZED)
            if (!state->graphics.resize(LOWORD(lparam), HIWORD(lparam))) PostQuitMessage(1);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int run_window(bool software, std::optional<unsigned> smoke_frames)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    constexpr wchar_t class_name[] = L"ViperRacingRuntimeWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = class_name;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        std::cerr << "Could not register the runtime window class.\n";
        return 1;
    }
    WindowState state;
    HWND hwnd = CreateWindowExW(0, class_name, L"Viper Racing — Runtime Foundation",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, instance, &state);
    if (!hwnd) { std::cerr << "Could not create the runtime window.\n"; return 1; }
    if (!state.graphics.create(hwnd, software)) {
        std::cerr << "Could not initialize Direct3D 11. "
                  << (software ? "WARP initialization failed.\n" : "Try --software to use WARP.\n");
        DestroyWindow(hwnd);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    MSG msg{};
    bool running = true;
    unsigned rendered_frames = 0;
    while (running) {
        if (IsIconic(hwnd)) {
            const DWORD result = MsgWaitForMultipleObjects(0, nullptr, FALSE, INFINITE, QS_ALLINPUT);
            if (result == WAIT_FAILED) return 1;
        }
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (running && !IsIconic(hwnd)) {
            if (!state.graphics.render()) {
                std::cerr << "Direct3D frame rendering or presentation failed.\n";
                DestroyWindow(hwnd);
                return 1;
            }
            if (smoke_frames && ++rendered_frames >= *smoke_frames) {
                DestroyWindow(hwnd);
                return 0;
            }
        }
    }
    return static_cast<int>(msg.wParam);
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) { usage(std::cout); return 0; }
        auto assets = load_assets(options.data_dir);
        if (options.validate_assets) return 0;
        (void)assets;
        return run_window(options.software, options.smoke_frames);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
