#include <Windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <dsound.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "viper/game_assets.hpp"
#include "viper/scene_font_8x8.hpp"
#include "viper/race_participants.hpp"
#include "viper/race_loop_callbacks.hpp"
#include "viper/race_progression.hpp"
#include "viper/sound_resource.hpp"
#include "viper/wheel_body_coupling.hpp"
#include "viper/world_track_objects.hpp"
#include "viper/career_file_store.hpp"

#include <array>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using Microsoft::WRL::ComPtr;

struct IdealLineVertex { float x; float y; };
struct SurfaceVertex { float x; float y; float r; float g; float b; };
struct CameraConstants { float center_x{}; float center_y{}; float zoom{1.0F}; float padding{}; };

std::uint32_t read_u32_le(const std::byte* bytes)
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[0])) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[2])) << 16U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[3])) << 24U);
}

std::vector<std::array<float, 2>> extract_ideal_line_points(
    const viper::IdealLineTrackData& line)
{
    std::vector<std::array<float, 2>> points;
    points.reserve(line.records.size());
    for (const auto& record : line.records) {
        const float x = std::bit_cast<float>(read_u32_le(record.data() + 0x04U));
        const float z = std::bit_cast<float>(read_u32_le(record.data() + 0x08U));
        if (!std::isfinite(x) || !std::isfinite(z))
            throw std::runtime_error("Bemidji ideal line contains a non-finite position");
        points.push_back({x, z});
    }
    if (points.size() < 2U)
        throw std::runtime_error("Bemidji ideal line needs at least two points to display");
    return points;
}

using D3DCompileFn = HRESULT (WINAPI *)(LPCVOID, SIZE_T, LPCSTR,
    const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT,
    ID3DBlob**, ID3DBlob**);

HRESULT compile_shader(const char* source, const char* entry, const char* target,
                       ComPtr<ID3DBlob>& bytecode)
{
    // D3DCompile's ID3DBlob implementation lives in this DLL. Keep the module
    // loaded for the process lifetime so returned blobs can safely Release.
    static const HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
    if (!compiler) return HRESULT_FROM_WIN32(GetLastError());
    const auto compile = reinterpret_cast<D3DCompileFn>(
        GetProcAddress(compiler, "D3DCompile"));
    if (!compile) return E_NOINTERFACE;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = compile(source, std::strlen(source), "viper_ideal_line",
        nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        &bytecode, &errors);
    if (FAILED(result) && errors)
        OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
    return result;
}

struct Options {
    std::filesystem::path data_dir;
    std::optional<std::filesystem::path> setup_config_directory;
    std::optional<std::string> track;
    std::optional<std::string> car;
    bool validate_assets{};
    bool help{};
    bool software{};
    bool grid_mode{};
    bool audio_smoke{};
    std::optional<unsigned> smoke_frames;
    std::optional<std::filesystem::path> record_trace;
    std::optional<std::filesystem::path> play_trace;
};

struct TraceInputFrame {
    float delta_seconds{};
    bool steer_left{};
    bool steer_right{};
    bool throttle{};
    bool brake{};
};

void usage(std::ostream& out)
{
    out << "Viper Racing reconstruction runtime\n"
        << "Usage: viper_game [--data-dir PATH] [--config-dir PATH] [--track NAME] [--car NAME] [-grid] [--validate-assets] [--smoke-frames N] [--smoke-audio] [--record-trace PATH] [--play-trace PATH] [--software] [--help]\n"
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
        } else if (arg.size() >= 5U &&
                   (arg.front() == L'-' || arg.front() == L'/') &&
                   arg.substr(1U, 4U) == L"grid") {
            // race.bin's command-line parser sets DAT_004dc1b0 for these
            // switches; 0x45CAE0 then forces the default track-basis pose.
            options.grid_mode = true;
        } else if (arg == L"--smoke-audio") {
            options.audio_smoke = true;
        } else if (arg == L"--record-trace") {
            if (++i >= argc) throw std::runtime_error("--record-trace requires a path");
            options.record_trace = std::filesystem::path(argv[i]);
        } else if (arg == L"--play-trace") {
            if (++i >= argc) throw std::runtime_error("--play-trace requires a path");
            options.play_trace = std::filesystem::path(argv[i]);
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
        } else if (arg == L"--config-dir") {
            if (++i >= argc) throw std::runtime_error("--config-dir requires a path");
            options.setup_config_directory = std::filesystem::path(argv[i]);
        } else if (arg == L"--track" || arg == L"--car") {
            if (++i >= argc) throw std::runtime_error(std::filesystem::path(arg).string() + " requires a name");
            const std::wstring_view value(argv[i]);
            std::string name;
            name.reserve(value.size());
            for (const wchar_t ch : value) {
                if (ch > 0x7f || ch < 0x20)
                    throw std::runtime_error("track and car names must be ASCII stems");
                name.push_back(static_cast<char>(ch));
            }
            if (arg == L"--track") options.track = std::move(name);
            else options.car = std::move(name);
        } else {
            throw std::runtime_error("unknown argument: " + std::filesystem::path(arg).string());
        }
    }
    return options;
}

std::vector<TraceInputFrame> load_input_trace(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("could not open input trace: " + path.string());
    std::string line;
    if (!std::getline(input, line) ||
        !line.starts_with("frame,elapsed_seconds,delta_seconds,steer_left,steer_right,throttle,brake,"))
        throw std::runtime_error("input trace has an unsupported header");
    std::vector<TraceInputFrame> frames;
    std::size_t line_number = 1U;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;
        std::array<std::string_view, 7> fields{};
        std::size_t begin = 0U;
        bool complete = true;
        for (std::size_t field = 0; field < fields.size(); ++field) {
            const auto end = line.find(',', begin);
            if (end == std::string::npos) {
                complete = false;
                break;
            }
            fields[field] = std::string_view(line).substr(begin, end - begin);
            begin = end + 1U;
        }
        if (!complete)
            throw std::runtime_error("input trace row has too few columns at line " +
                std::to_string(line_number));
        try {
            std::size_t consumed{};
            const float delta = std::stof(std::string(fields[2]), &consumed);
            if (consumed != fields[2].size() || !std::isfinite(delta) || delta < 0.0F)
                throw std::runtime_error("invalid delta");
            const auto read_button = [&](const std::size_t index) {
                if (fields[index] == "0") return false;
                if (fields[index] == "1") return true;
                throw std::runtime_error("invalid button");
            };
            frames.push_back({delta, read_button(3U), read_button(4U),
                read_button(5U), read_button(6U)});
        } catch (const std::exception&) {
            throw std::runtime_error("invalid input trace values at line " +
                std::to_string(line_number));
        }
    }
    if (frames.empty()) throw std::runtime_error("input trace contains no frames");
    return frames;
}

viper::GameAssets load_assets(const std::filesystem::path& root,
                              const std::string_view track,
                              const std::string_view car,
                              viper::GameOptions game_options,
                              const std::filesystem::path& setup_config_directory)
{
    if (root.empty()) {
        throw std::runtime_error("pass --data-dir PATH or set VIPER_DATA_DIR");
    }
    auto assets = viper::GameAssets::load(root, track, car,
        std::move(game_options), setup_config_directory);
    if (assets.default_tire().label.empty() ||
        !std::all_of(assets.default_tire().parameters.begin(),
            assets.default_tire().parameters.end(),
            [](const float value) { return std::isfinite(value); })) {
        throw std::runtime_error("common.res default tire has an empty label or non-finite values");
    }
    const auto car_bundle = std::string(assets.car_name()) + ".car";
    const auto track_bundle = std::string(assets.track_name()) + ".trk";
    for (const auto name : {"common.res", "ui.res", "race.res", car_bundle.c_str(), track_bundle.c_str(), "drivers.res"})
        std::cout << "Loaded " << name << ": " << assets.bundle(name).entries().size() << " resources\n";
    std::cout << "Loaded english.lng: " << assets.language().entries.size() << " strings\n"
              << "Parsed " << assets.track_name() << " terrain: " << assets.terrain().planes.size() << " planes, "
              << assets.terrain().partition_nodes.size() << " partition nodes\n"
              << "Parsed " << assets.track_name() << " ideal line: " << assets.ideal_line().records.size()
              << " records\n"
              << "Parsed " << assets.car_name() << " CARF: version " << assets.player_car().car_file->resource_version
              << ", " << assets.player_car().car_file->raw_words.size() << " configuration words\n"
              << "Loaded shared tire " << assets.default_tire().label << " from common.res\n"
              << "Loaded " << assets.car_name() << " human setup record: "
              << assets.player_car_setup().size() << " bytes ("
              << (assets.player_setup_file_loaded() ? "player CSU" : "CCS resource")
              << ")\n"
              << "Loaded " << assets.track_name() << " AI default setup record: "
              << assets.ai_vehicle_setup().size() << " bytes (CCS v2)\n"
              << "Loaded settings: " << assets.options().entries().size()
              << " sections from options.def\n"
              << "Parsed " << assets.car_name() << " level-0 FNIM: " << assets.player_model().vertices.size() << " vertices, "
              << assets.player_model().material_groups.size() << " material groups, "
              << assets.player_model().section_counts[2] << " triangles; validated "
              << assets.model_resource_count() << " FNIM resources total\n"
              << "Required assets validated in " << assets.root().string() << '\n';
    constexpr std::array<std::string_view, viper::ai_driver_group_count> ai_group_names{
        "easy", "medium", "hard", "career1", "career2", "career3", "career4", "career5"};
    std::cout << "Parsed drivers.res AIDR v2 groups:";
    for (std::size_t index = 0; index < ai_group_names.size(); ++index)
        std::cout << ' ' << ai_group_names[index] << '='
                  << assets.ai_drivers().active_counts[index];
    std::cout << '\n';
    return assets;
}

std::filesystem::path resolve_setup_config_directory(
    const std::filesystem::path& data_dir)
{
    const std::array candidates{
        data_dir / "Config",
        data_dir.parent_path() / "Config",
        data_dir.parent_path() / "GameRoot" / "Config"};
    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_directory(candidate, error) && !error)
            return candidate;
    }
    return {};
}

std::string configured_track_stem(const std::filesystem::path& data_dir,
                                  const viper::GameOptions& options)
{
    const auto common = viper::ResourceBundle::open(data_dir / "common.res");
    constexpr auto stab_type = viper::TrackObjectTextTable::bats_resource_type_code;
    const auto payload = common.find_payload("tracks.tab", stab_type);
    if (!payload) throw std::runtime_error("common.res tracks.tab is missing");
    const auto table = viper::parse_track_object_text_table(*payload);
    if (!table) throw std::runtime_error("common.res tracks.tab is invalid");

    auto index = options.integer("GLOBAL", "track_no").value_or(0);
    // The original selector resets only past-end values. -1 selects its empty
    // row, which cannot be represented as a track bundle by this runtime.
    if (index < 0)
        throw std::runtime_error("options.def track_no selects an empty track entry");
    if (static_cast<std::size_t>(index) >= table->line_count) index = 0;
    const auto stem = viper::track_object_text_line(
        *table, static_cast<std::size_t>(index), 0);
    if (!stem || stem->empty())
        throw std::runtime_error("tracks.tab selected an empty track name");
    if (*stem == "random")
        throw std::runtime_error(
            "options.def selects the random track; random track setup is not implemented");
    return std::string(*stem);
}

std::vector<std::string> selectable_track_stems(
    const std::filesystem::path& data_dir)
{
    const auto common = viper::ResourceBundle::open(data_dir / "common.res");
    const auto payload = common.find_payload("tracks.tab",
        viper::TrackObjectTextTable::bats_resource_type_code);
    if (!payload) throw std::runtime_error("common.res tracks.tab is missing");
    const auto table = viper::parse_track_object_text_table(*payload);
    if (!table) throw std::runtime_error("common.res tracks.tab is invalid");

    std::vector<std::string> tracks;
    for (std::size_t index = 0; index < table->line_count; ++index) {
        const auto name = viper::track_object_text_line(*table, index, 0U);
        if (!name || name->empty()) continue;
        std::string stem(*name);
        std::transform(stem.begin(), stem.end(), stem.begin(), [](const char value) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        });
        if (stem == "random") continue;
        std::error_code error;
        if (!std::filesystem::is_regular_file(data_dir / (stem + ".trk"), error) || error)
            continue;
        tracks.push_back(std::move(stem));
    }
    if (tracks.empty()) throw std::runtime_error(
        "tracks.tab contains no installed selectable track bundles");
    return tracks;
}

std::vector<std::string> selectable_car_stems(
    const std::filesystem::path& data_dir)
{
    const auto wildcard = (data_dir / L"*.car").wstring();
    WIN32_FIND_DATAW file{};
    const HANDLE search = FindFirstFileW(wildcard.c_str(), &file);
    if (search == INVALID_HANDLE_VALUE)
        throw std::runtime_error("no .car bundles found under " + data_dir.string());
    struct SearchHandle {
        HANDLE value;
        ~SearchHandle() { if (value != INVALID_HANDLE_VALUE) FindClose(value); }
    } close_search{search};

    std::vector<std::string> cars;
    do {
        if ((file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) continue;
        const auto stem = std::filesystem::path(file.cFileName).stem().string();
        if (!stem.empty()) cars.push_back(stem);
    } while (FindNextFileW(search, &file));
    if (cars.empty()) throw std::runtime_error("no usable .car bundles found");
    return cars;
}


std::string configured_car_stem(const std::filesystem::path& data_dir,
                                const viper::GameOptions& options)
{
    const auto stems = selectable_car_stems(data_dir);
    auto index = options.integer("GLOBAL", "car_no").value_or(0);
    // The original constructor resets a selector at or above the enumerated
    // car count to zero. The enumeration order is the native *.car order.
    if (index < 0)
        throw std::runtime_error("options.def car_no selects an empty car entry");
    if (static_cast<std::size_t>(index) >= stems.size()) index = 0;
    return stems[static_cast<std::size_t>(index)];
}

struct FrontendSelection {
    std::string track;
    std::string car;
};

struct SetupWindowState {
    std::vector<std::string> tracks;
    std::vector<std::string> cars;
    std::size_t track_index{};
    std::size_t car_index{};
    std::filesystem::path config_directory;
    std::array<std::string, 5> career_rows{};
    int career_slot{};
    bool career_screen{};
    std::string career_message;
    bool accepted{};
};

std::wstring ascii_to_wide(const std::string_view text)
{
    return std::wstring(text.begin(), text.end());
}

void draw_setup_window(HWND hwnd, const SetupWindowState& state)
{
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(hwnd, &paint);
    RECT client{};
    GetClientRect(hwnd, &client);
    const auto background = CreateSolidBrush(RGB(8, 20, 24));
    FillRect(dc, &client, background);
    DeleteObject(background);

    const auto old_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(238, 244, 240));
    const auto write = [dc](const std::wstring& text, const int x, const int y,
                            const int height, const UINT format = DT_LEFT) {
        RECT rect{x, y, 850, y + height};
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect,
            format | DT_SINGLELINE | DT_VCENTER);
    };
    if (!state.career_screen) {
        write(L"VIPER RACING", 56, 42, 54);
        SetTextColor(dc, RGB(118, 208, 155));
        write(L"RACE SETUP", 58, 110, 34);
        SetTextColor(dc, RGB(238, 244, 240));
        const auto track = state.tracks.empty() ? L"(no tracks)" :
            ascii_to_wide(state.tracks[state.track_index]);
        const auto car = state.cars.empty() ? L"(no cars)" :
            ascii_to_wide(state.cars[state.car_index]);
        write(L"TRACK", 74, 190, 30);
        write(L"<  " + track + L"  >", 74, 228, 42);
        write(L"CAR", 490, 190, 30);
        write(L"<  " + car + L"  >", 490, 228, 42);
        SetTextColor(dc, RGB(190, 205, 198));
        write(L"UP / DOWN: CHOOSE TRACK", 74, 332, 30);
        write(L"LEFT / RIGHT: CHOOSE CAR", 74, 370, 30);
        write(L"ENTER: START RACE", 74, 426, 30);
        write(L"C: CAREER SLOTS", 74, 464, 30);
        write(L"ESC: EXIT", 74, 502, 30);
    }
    if (state.career_screen) {
        SetTextColor(dc, RGB(118, 208, 155));
        write(L"CAREER SLOT VALIDATION", 74, 120, 34);
        SetTextColor(dc, RGB(238, 244, 240));
        for (std::size_t index = 0; index < state.career_rows.size(); ++index) {
            std::wstring row = index == static_cast<std::size_t>(state.career_slot)
                ? L"> " : L"  ";
            row += L"SLOT " + std::to_wstring(index + 1U) + L"  ";
            row += ascii_to_wide(state.career_rows[index]);
            write(row, 100, 190 + static_cast<int>(index) * 44, 36);
        }
        SetTextColor(dc, RGB(190, 205, 198));
        write(L"UP / DOWN: SELECT     ENTER: VALIDATE", 100, 430, 30);
        if (!state.career_message.empty())
            write(ascii_to_wide(state.career_message), 100, 462, 30);
        write(L"ESC: BACK", 100, 500, 30);
    }
    SelectObject(dc, old_font);
    EndPaint(hwnd, &paint);
}

LRESULT CALLBACK setup_window_proc(HWND hwnd, const UINT message,
                                    const WPARAM wparam, const LPARAM lparam)
{
    auto* state = reinterpret_cast<SetupWindowState*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_PAINT:
        if (state) draw_setup_window(hwnd, *state);
        else { PAINTSTRUCT paint{}; BeginPaint(hwnd, &paint); EndPaint(hwnd, &paint); }
        return 0;
    case WM_KEYDOWN:
        if (state && (lparam & (1LL << 30)) == 0) {
            if (state->career_screen) {
                if (wparam == VK_UP || wparam == VK_DOWN) {
                    state->career_slot = (state->career_slot +
                        (wparam == VK_UP ? 4 : 1)) % 5;
                    state->career_message.clear();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == VK_ESCAPE) {
                    state->career_screen = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == VK_RETURN) {
                    const auto loaded = viper::load_career_slot(
                        state->config_directory, state->career_slot);
                    if (loaded.file.returns_success) {
                        state->career_message = "VALID FILE - native career loop pending";
                    } else {
                        state->career_rows[static_cast<std::size_t>(state->career_slot)] = "EMPTY OR INVALID";
                        state->career_message = "SLOT FAILED NATIVE VALIDATION";
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                return 0;
            }
            if (wparam == VK_UP && !state->tracks.empty())
                state->track_index = (state->track_index + state->tracks.size() - 1U) % state->tracks.size();
            else if (wparam == VK_DOWN && !state->tracks.empty())
                state->track_index = (state->track_index + 1U) % state->tracks.size();
            else if (wparam == VK_LEFT && !state->cars.empty())
                state->car_index = (state->car_index + state->cars.size() - 1U) % state->cars.size();
            else if (wparam == VK_RIGHT && !state->cars.empty())
                state->car_index = (state->car_index + 1U) % state->cars.size();
            else if (wparam == VK_RETURN && !state->tracks.empty() && !state->cars.empty()) {
                state->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            } else if (wparam == 'C' &&
                       !state->config_directory.empty()) {
                state->career_screen = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            } else if (wparam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            } else {
                return 0;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

std::optional<FrontendSelection> show_setup_frontend(
    std::vector<std::string> tracks, std::vector<std::string> cars,
    const std::string_view initial_track, const std::string_view initial_car,
    const std::filesystem::path& config_directory)
{
    if (tracks.empty() || cars.empty())
        throw std::runtime_error("frontend cannot show an empty track or car inventory");
    SetupWindowState state{};
    state.tracks = std::move(tracks);
    state.cars = std::move(cars);
    state.config_directory = config_directory;
    for (std::size_t slot = 0; slot < state.career_rows.size(); ++slot) {
        const auto loaded = viper::load_career_slot(config_directory,
            static_cast<int>(slot));
        if (loaded.file.returns_success) {
            const auto& payload = loaded.file.decoded_payload;
            const auto name_end = payload.begin() + std::min<std::size_t>(
                payload.size(), 0x10U);
            const auto end = std::find(payload.begin(), name_end, 0U);
            state.career_rows[slot] = "AVAILABLE: " + std::string(
                payload.begin(), end);
        } else {
            state.career_rows[slot] = "EMPTY OR INVALID";
        }
    }
    const auto track = std::find(state.tracks.begin(), state.tracks.end(), initial_track);
    if (track != state.tracks.end())
        state.track_index = static_cast<std::size_t>(track - state.tracks.begin());
    const auto car = std::find_if(state.cars.begin(), state.cars.end(),
        [initial_car](const std::string& value) {
            if (value.size() != initial_car.size()) return false;
            return std::equal(value.begin(), value.end(), initial_car.begin(),
                [](const char left, const char right) {
                    return std::tolower(static_cast<unsigned char>(left)) ==
                        std::tolower(static_cast<unsigned char>(right));
                });
        });
    if (car != state.cars.end())
        state.car_index = static_cast<std::size_t>(car - state.cars.begin());

    constexpr wchar_t class_name[] = L"ViperRaceSetupFrontendWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = setup_window_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = class_name;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw std::runtime_error("could not register the setup frontend window");
    const HWND hwnd = CreateWindowExW(0, class_name, L"Viper Racing — Race Setup",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 920, 590,
        nullptr, nullptr, wc.hInstance, &state);
    if (!hwnd) throw std::runtime_error("could not create the setup frontend window");
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    MSG message{};
    int message_result{};
    while ((message_result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (message_result < 0)
        throw std::runtime_error("setup frontend message loop failed");
    if (!state.accepted) return std::nullopt;
    return FrontendSelection{
        state.tracks[state.track_index], state.cars[state.car_index]};
}

class D3DWindow {
public:
    void set_vsync(const bool enabled) noexcept { vsync_interval_ = enabled ? 1U : 0U; }

    bool create(HWND hwnd, bool software,
                const viper::TerrainSurfaceResourcePayload& terrain,
                const viper::IdealLineTrackData& ideal_line,
                const viper::ModelResource& player_model,
                const viper::ModelResource& ai_model,
                const std::size_t player_car_index,
                const viper::TrackObjectTextTable& track_objects,
                const std::string_view player_car_name,
                const bool force_default_placement)
    {
        try {
            ideal_line_points_ = extract_ideal_line_points(ideal_line);
            extract_terrain_points(terrain);
            player_model_ = &player_model;
            ai_model_ = &ai_model;
            player_car_index_ = player_car_index;
            const auto spawns = car_spawns(track_objects);
            if (spawns.empty()) throw std::runtime_error("track.obt has no usable obj car start position");
            const auto spline_nodes = viper::make_track_spline_index_nodes(ideal_line);
            const float placement_base_key = spline_nodes.empty() ? 0.0F :
                spline_nodes.back().segment_key + spline_nodes.back().segment_duration;
            viper::TrackBasisTransform track_basis{};
            track_basis.basis = {1.0F, 0.0F, 0.0F,
                0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
            car_poses_.reserve(spawns.size());
            std::size_t spline_placements = 0U;
            std::size_t default_placements = 0U;
            for (std::size_t index = 0; index < spawns.size(); ++index) {
                const auto& spawn = spawns[index];
                const bool is_player = index == player_car_index;
                const auto make_placement = [&](const float terrain_height) {
                    viper::CarTrackLineObservation observation{};
                    observation.configured_car_index = index;
                    observation.configured_car_count = spawns.size();
                    observation.raw_subtype = is_player
                        ? static_cast<std::uint32_t>(viper::TrackCarSubtype::pcar)
                        : static_cast<std::uint32_t>(viper::TrackCarSubtype::ncar);
                    observation.model_name = std::string(is_player
                        ? player_car_name : std::string_view("viper"));
                    observation.x = spawn[0];
                    observation.z = spawn[1];
                    observation.force_default_placement_0x4dc1b0 =
                        force_default_placement;
                    observation.track_basis = track_basis;
                    observation.terrain_height = terrain_height;
                    if (!force_default_placement && !spline_nodes.empty()) {
                        observation.spline_placement =
                            viper::CarSplineTrackPlacementObservation{
                                true, 0U, spline_nodes, placement_base_key,
                                static_cast<std::int32_t>(index), terrain_height};
                    }
                    return viper::plan_car_track_line(observation);
                };
                const auto preliminary = make_placement(0.0F);
                if (preliminary.disposition !=
                        viper::CarTrackLineDisposition::construction_ready)
                    throw std::runtime_error("track.obt car placement plan is incomplete");
                const auto& provisional_transform = preliminary.transform;
                const auto surface = viper::sample_terrain_resource(
                    terrain, {provisional_transform[9], 1000.0f,
                        provisional_transform[11]}, true);
                if (!surface.query_succeeded)
                    throw std::runtime_error("track.obt car start position has no terrain surface");
                const auto placement = make_placement(surface.sampled_position[1]);
                if (placement.disposition !=
                        viper::CarTrackLineDisposition::construction_ready)
                    throw std::runtime_error("track.obt car placement plan is incomplete");
                const auto& transform = placement.transform;
                const float yaw = std::atan2(transform[6], transform[0]);
                car_poses_.push_back({transform[9], transform[10], transform[11], yaw});
                if (placement.placement_route ==
                        viper::CarTrackPlacementRoute::precomputed_transform)
                    ++spline_placements;
                else
                    ++default_placements;
            }
            std::cout << "Initialized " << car_poses_.size()
                      << " grid poses from native placement plans: "
                      << spline_placements << " spline, " << default_placements
                      << " track-basis/default"
                      << (force_default_placement ? " (-grid forced)" : "") << ".\n";
        } catch (const std::exception& e) {
            OutputDebugStringA(e.what());
            return false;
        }
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
        return create_target() && create_surface_pipeline() && create_line_pipeline() &&
            update_surface_buffer() && update_car_buffer() && update_line_buffer();
    }

    bool resize(UINT width, UINT height)
    {
        if (!width || !height || !swap_chain_) return true;
        if (context_) context_->OMSetRenderTargets(0, nullptr, nullptr);
        target_.Reset();
        if (FAILED(swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
        width_ = width;
        height_ = height;
        return create_target() && update_surface_buffer() && update_car_buffer() && update_line_buffer();
    }

    bool render(const std::string_view hud_text,
                const float focus_x, const float focus_z)
    {
        if (!target_ || !context_ || !swap_chain_) return false;
        constexpr float color[]{0.035f, 0.055f, 0.075f, 1.0f};
        context_->OMSetRenderTargets(1, target_.GetAddressOf(), nullptr);
        D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width_),
                                static_cast<float>(height_), 0.0f, 1.0f};
        context_->RSSetViewports(1, &viewport);
        context_->ClearRenderTargetView(target_.Get(), color);
        const auto focus = map_to_clip(focus_x, focus_z);
        camera_constants_ = {focus[0], focus[1], 1.6F, 0.0F};
        context_->UpdateSubresource(camera_buffer_.Get(), 0U, nullptr,
            &camera_constants_, 0U, 0U);
        context_->VSSetConstantBuffers(0U, 1U, camera_buffer_.GetAddressOf());
        if (!surface_buffer_ || surface_vertex_count_ == 0U) return false;
        const UINT surface_stride = sizeof(SurfaceVertex);
        constexpr UINT surface_offset = 0;
        context_->RSSetState(surface_rasterizer_.Get());
        context_->IASetInputLayout(surface_input_layout_.Get());
        context_->IASetVertexBuffers(0, 1, surface_buffer_.GetAddressOf(),
            &surface_stride, &surface_offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(surface_vertex_shader_.Get(), nullptr, 0);
        context_->PSSetShader(surface_pixel_shader_.Get(), nullptr, 0);
        context_->Draw(surface_vertex_count_, 0);
        if (!line_buffer_ || ideal_line_vertex_count_ < 2U) return false;
        const UINT stride = sizeof(IdealLineVertex);
        constexpr UINT offset = 0;
        context_->IASetInputLayout(line_input_layout_.Get());
        context_->IASetVertexBuffers(0, 1, line_buffer_.GetAddressOf(), &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        context_->VSSetShader(line_vertex_shader_.Get(), nullptr, 0);
        context_->PSSetShader(line_pixel_shader_.Get(), nullptr, 0);
        context_->Draw(ideal_line_vertex_count_, 0);
        if (car_buffer_ && !car_draw_groups_.empty()) {
            const UINT car_stride = sizeof(SurfaceVertex);
            constexpr UINT car_offset = 0;
            context_->RSSetState(surface_rasterizer_.Get());
            context_->IASetInputLayout(surface_input_layout_.Get());
            context_->IASetVertexBuffers(0, 1, car_buffer_.GetAddressOf(), &car_stride, &car_offset);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(surface_vertex_shader_.Get(), nullptr, 0);
            context_->PSSetShader(surface_pixel_shader_.Get(), nullptr, 0);
            for (const auto& group : car_draw_groups_)
                if (group.vertex_count) context_->Draw(group.vertex_count, group.first_vertex);
        }
        if (!draw_hud(hud_text)) return false;
        return SUCCEEDED(swap_chain_->Present(vsync_interval_, 0));
    }

    bool set_car_pose(const std::size_t car_index, const float anchor_x, const float anchor_y,
                      const float anchor_z, const float yaw)
    {
        if (car_index >= car_poses_.size()) return false;
        car_poses_[car_index] = {anchor_x, anchor_y, anchor_z, yaw};
        return update_car_buffer();
    }

    bool set_car_poses(const std::span<const std::array<float, 4>> poses)
    {
        if (poses.size() != car_poses_.size()) return false;
        for (std::size_t index = 0; index < poses.size(); ++index) {
            const auto& pose = poses[index];
            car_poses_[index] = {pose[0], pose[1], pose[2], pose[3]};
        }
        return update_car_buffer();
    }

    std::size_t car_count() const noexcept { return car_poses_.size(); }

    std::array<float, 2> car_position(const std::size_t car_index) const
    {
        if (car_index >= car_poses_.size()) return {};
        return {car_poses_[car_index].x, car_poses_[car_index].z};
    }

    float car_yaw(const std::size_t car_index) const
    {
        return car_index < car_poses_.size() ? car_poses_[car_index].yaw : 0.0F;
    }

    float car_height(const std::size_t car_index) const
    {
        return car_index < car_poses_.size() ? car_poses_[car_index].y : 0.0F;
    }

private:
    bool draw_hud(const std::string_view text)
    {
        if (!device_ || !context_ || !hud_buffer_ || width_ == 0U || height_ == 0U)
            return false;
        std::vector<SurfaceVertex> vertices;
        vertices.reserve(12000U);
        const auto append_rectangle = [this, &vertices](const float left,
            const float top, const float right, const float bottom,
            const std::array<float, 3>& color) {
            const auto x0 = 2.0F * left / static_cast<float>(width_) - 1.0F;
            const auto x1 = 2.0F * right / static_cast<float>(width_) - 1.0F;
            const auto y0 = 1.0F - 2.0F * top / static_cast<float>(height_);
            const auto y1 = 1.0F - 2.0F * bottom / static_cast<float>(height_);
            vertices.insert(vertices.end(), {{x0, y0, color[0], color[1], color[2]},
                {x1, y0, color[0], color[1], color[2]},
                {x1, y1, color[0], color[1], color[2]},
                {x0, y0, color[0], color[1], color[2]},
                {x1, y1, color[0], color[1], color[2]},
                {x0, y1, color[0], color[1], color[2]}});
        };
        constexpr float panel_x = 18.0F;
        constexpr float panel_y = 18.0F;
        constexpr float scale = 2.0F;
        constexpr float advance = 18.0F;
        constexpr float line_height = 22.0F;
        std::size_t line_count = 1U;
        std::size_t longest_line = 0U;
        std::size_t current_line = 0U;
        for (const char character : text) {
            if (character == '\n') {
                longest_line = (std::max)(longest_line, current_line);
                current_line = 0U;
                ++line_count;
            } else {
                ++current_line;
            }
        }
        longest_line = (std::max)(longest_line, current_line);
        const float panel_width = (std::max)(290.0F,
            16.0F + static_cast<float>(longest_line) * advance);
        const float panel_height = (std::max)(108.0F,
            12.0F + static_cast<float>(line_count) * line_height);
        append_rectangle(panel_x, panel_y, panel_x + panel_width,
            panel_y + panel_height,
            {0.035F, 0.060F, 0.080F});
        float x = panel_x + 8.0F;
        float y = panel_y + 6.0F;
        for (const auto raw_character : text) {
            if (raw_character == '\n') {
                x = panel_x + 8.0F;
                y += line_height;
                continue;
            }
            const auto character = static_cast<unsigned char>(raw_character);
            if (character < 0x20U || character >= 0x80U) continue;
            const auto& glyph = viper::detail::scene_font_8x8[
                static_cast<std::size_t>(character - 0x20U)];
            for (std::size_t row = 0; row < glyph.size(); ++row) {
                const auto row_bits = glyph[row];
                for (std::size_t column = 0; column < 8U;) {
                    if ((row_bits & (std::uint8_t{1} << column)) == 0U) {
                        ++column;
                        continue;
                    }
                    const auto run_start = column;
                    while (column < 8U &&
                        (row_bits & (std::uint8_t{1} << column)) != 0U) ++column;
                    append_rectangle(x + static_cast<float>(run_start) * scale,
                        y + static_cast<float>(row) * scale,
                        x + static_cast<float>(column) * scale,
                        y + static_cast<float>(row + 1U) * scale,
                        {0.94F, 0.96F, 0.98F});
                }
            }
            x += advance;
        }
        constexpr std::size_t max_vertices = 65536U;
        if (vertices.size() > max_vertices) return false;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context_->Map(hud_buffer_.Get(), 0U,
                D3D11_MAP_WRITE_DISCARD, 0U, &mapped))) return false;
        std::memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(SurfaceVertex));
        context_->Unmap(hud_buffer_.Get(), 0U);
        const UINT stride = sizeof(SurfaceVertex);
        constexpr UINT offset = 0U;
        context_->IASetInputLayout(surface_input_layout_.Get());
        camera_constants_ = {};
        camera_constants_.zoom = 1.0F;
        context_->UpdateSubresource(camera_buffer_.Get(), 0U, nullptr,
            &camera_constants_, 0U, 0U);
        context_->VSSetConstantBuffers(0U, 1U, camera_buffer_.GetAddressOf());
        context_->IASetVertexBuffers(0U, 1U, hud_buffer_.GetAddressOf(), &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(surface_vertex_shader_.Get(), nullptr, 0U);
        context_->PSSetShader(surface_pixel_shader_.Get(), nullptr, 0U);
        context_->Draw(static_cast<UINT>(vertices.size()), 0U);
        return true;
    }

    void extract_terrain_points(const viper::TerrainSurfaceResourcePayload& terrain)
    {
        if (terrain.planes.empty())
            throw std::runtime_error("Bemidji terrain has no surface triangles");
        terrain_points_.reserve(terrain.planes.size() * 3U);
        for (const auto& plane : terrain.planes) {
            const auto& corners = plane.trailing_values;
            const auto& normal = std::array<float, 3>{
                plane.normal_x, plane.normal_y, plane.normal_z};
            for (std::size_t vertex = 0; vertex < 3U; ++vertex) {
                const float x = corners[vertex * 3U];
                const float y = corners[vertex * 3U + 1U];
                const float z = corners[vertex * 3U + 2U];
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                    throw std::runtime_error("Bemidji terrain has a non-finite triangle corner");
                terrain_points_.push_back({x, y, z});
                const float normal_length = std::sqrt(normal[0] * normal[0] +
                    normal[1] * normal[1] + normal[2] * normal[2]);
                if (!(normal_length > 1.0e-6f) || !std::isfinite(normal_length))
                    throw std::runtime_error("terrain contains a degenerate surface normal");
                terrain_normals_.push_back({normal[0] / normal_length,
                    normal[1] / normal_length, normal[2] / normal_length});
            }
        }
        map_bounds_ = {terrain_points_.front()[0], terrain_points_.front()[0],
            terrain_points_.front()[2], terrain_points_.front()[2]};
        for (const auto& point : terrain_points_) {
            map_bounds_[0] = (std::min)(map_bounds_[0], point[0]);
            map_bounds_[1] = (std::max)(map_bounds_[1], point[0]);
            map_bounds_[2] = (std::min)(map_bounds_[2], point[2]);
            map_bounds_[3] = (std::max)(map_bounds_[3], point[2]);
        }
        for (const auto& point : ideal_line_points_) {
            map_bounds_[0] = (std::min)(map_bounds_[0], point[0]);
            map_bounds_[1] = (std::max)(map_bounds_[1], point[0]);
            map_bounds_[2] = (std::min)(map_bounds_[2], point[1]);
            map_bounds_[3] = (std::max)(map_bounds_[3], point[1]);
        }
        const auto [min_y, max_y] = std::minmax_element(terrain_points_.begin(), terrain_points_.end(),
            [](const auto& left, const auto& right) { return left[1] < right[1]; });
        min_height_ = (*min_y)[1];
        max_height_ = (*max_y)[1];
        if (!(map_bounds_[1] > map_bounds_[0]) || !(map_bounds_[3] > map_bounds_[2]))
            throw std::runtime_error("Bemidji terrain has degenerate horizontal bounds");
    }

    std::array<float, 2> map_to_clip(const float x, const float z) const
    {
        const float range = (std::max)(map_bounds_[1] - map_bounds_[0],
            map_bounds_[3] - map_bounds_[2]);
        const float pixels_per_unit = 0.88f * static_cast<float>((std::min)(width_, height_)) / range;
        const float center_x = (map_bounds_[0] + map_bounds_[1]) * 0.5f;
        const float center_z = (map_bounds_[2] + map_bounds_[3]) * 0.5f;
        return {2.0f * (x - center_x) * pixels_per_unit / static_cast<float>(width_),
            -2.0f * (z - center_z) * pixels_per_unit / static_cast<float>(height_)};
    }

    bool create_surface_pipeline()
    {
        static constexpr char vertex_source[] =
            "struct V { float2 p : POSITION; float3 c : COLOR; };\n"
            "struct O { float4 p : SV_POSITION; float3 c : COLOR; };\n"
            "cbuffer Camera : register(b0) { float2 center; float zoom; float pad; };\n"
            "O VSMain(V v) { O o; o.p=float4((v.p-center)*zoom,0.5,1); o.c=v.c; return o; }\n";
        static constexpr char pixel_source[] =
            "struct O { float4 p : SV_POSITION; float3 c : COLOR; };\n"
            "float4 PSMain(O i) : SV_TARGET { return float4(i.c,1); }\n";
        ComPtr<ID3DBlob> vertex_code;
        ComPtr<ID3DBlob> pixel_code;
        if (FAILED(compile_shader(vertex_source, "VSMain", "vs_4_0", vertex_code)) ||
            FAILED(compile_shader(pixel_source, "PSMain", "ps_4_0", pixel_code))) return false;
        if (FAILED(device_->CreateVertexShader(vertex_code->GetBufferPointer(),
                vertex_code->GetBufferSize(), nullptr, &surface_vertex_shader_)) ||
            FAILED(device_->CreatePixelShader(pixel_code->GetBufferPointer(),
                pixel_code->GetBufferSize(), nullptr, &surface_pixel_shader_))) return false;
        const D3D11_INPUT_ELEMENT_DESC elements[]{
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 2U * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0}};
        if (FAILED(device_->CreateInputLayout(elements, static_cast<UINT>(std::size(elements)),
                vertex_code->GetBufferPointer(), vertex_code->GetBufferSize(), &surface_input_layout_)))
            return false;
        D3D11_RASTERIZER_DESC raster{};
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE;
        if (FAILED(device_->CreateRasterizerState(&raster, &surface_rasterizer_)))
            return false;
        D3D11_BUFFER_DESC hud_desc{};
        hud_desc.ByteWidth = 65536U * sizeof(SurfaceVertex);
        hud_desc.Usage = D3D11_USAGE_DYNAMIC;
        hud_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        hud_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&hud_desc, nullptr, &hud_buffer_))) return false;
        D3D11_BUFFER_DESC camera_desc{};
        camera_desc.ByteWidth = sizeof(CameraConstants);
        camera_desc.Usage = D3D11_USAGE_DEFAULT;
        camera_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA camera_data{};
        camera_data.pSysMem = &camera_constants_;
        return SUCCEEDED(device_->CreateBuffer(&camera_desc, &camera_data, &camera_buffer_));
    }

    bool update_surface_buffer()
    {
        if (!width_ || !height_ || terrain_points_.empty()) return true;
        std::vector<SurfaceVertex> vertices;
        vertices.reserve(terrain_points_.size());
        const float height_range = (std::max)(max_height_ - min_height_, 1.0e-5f);
        for (std::size_t index = 0; index < terrain_points_.size(); ++index) {
            const auto& point = terrain_points_[index];
            const auto pos = map_to_clip(point[0], point[2]);
            const float elevation = std::clamp((point[1] - min_height_) / height_range, 0.0f, 1.0f);
            const auto& normal = terrain_normals_[index];
            const float light = 0.35f + 0.65f * std::clamp(
                normal[0] * -0.35f + normal[1] * 0.8f + normal[2] * -0.45f,
                0.0f, 1.0f);
            vertices.push_back({pos[0], pos[1],
                (0.11f + 0.20f * elevation) * light,
                (0.22f + 0.35f * elevation) * light,
                (0.16f + 0.17f * elevation) * light});
        }
        if (vertices.size() > std::numeric_limits<UINT>::max() / sizeof(SurfaceVertex)) return false;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(SurfaceVertex));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices.data();
        ComPtr<ID3D11Buffer> buffer;
        if (FAILED(device_->CreateBuffer(&desc, &data, &buffer))) return false;
        surface_buffer_ = std::move(buffer);
        surface_vertex_count_ = static_cast<UINT>(vertices.size());
        return true;
    }

    static std::vector<std::array<float, 2>> car_spawns(
        const viper::TrackObjectTextTable& table)
    {
        std::vector<std::array<float, 2>> result;
        for (std::size_t row = 1; row < table.line_count; ++row) {
            const auto line = viper::track_object_text_line(table, row);
            if (!line) continue;
            const auto directive = viper::classify_track_directive(*line);
            if (directive.kind != viper::TrackDirectiveKind::object ||
                !directive.object_line.starts_with("obj car ")) continue;
            float x{}, z{};
            if (sscanf_s(directive.object_line.data(), "obj car %f, %f", &x, &z) == 2 &&
                std::isfinite(x) && std::isfinite(z)) result.push_back({x, z});
        }
        return result;
    }

    bool update_car_buffer()
    {
        if (!width_ || !height_ || player_model_ == nullptr || ai_model_ == nullptr ||
            player_model_->vertices.empty() || ai_model_->vertices.empty()) return true;
        if (ideal_line_points_.size() < 2U || car_poses_.empty()) return false;
        std::vector<SurfaceVertex> vertices;
        std::vector<CarDrawGroup> groups;
        for (std::size_t car_index = 0; car_index < car_poses_.size(); ++car_index) {
            const auto& pose = car_poses_[car_index];
            const auto& model = car_index == player_car_index_
                ? *player_model_ : *ai_model_;
            const float sin_yaw = std::sin(pose.yaw);
            const float cos_yaw = std::cos(pose.yaw);
            for (const auto& model_group : model.material_groups) {
                CarDrawGroup draw_group{};
                draw_group.first_vertex = static_cast<UINT>(vertices.size());
                const auto material = model_group.material_name;
                const std::array<float, 3> color = material.find("WHEEL") != std::string::npos
                    ? std::array<float, 3>{0.10f, 0.11f, 0.12f}
                    : material.find("EFFECT") != std::string::npos
                        ? std::array<float, 3>{0.95f, 0.57f, 0.12f}
                        : material.find("UCAR") != std::string::npos
                            ? std::array<float, 3>{0.18f, 0.48f, 0.82f}
                            : std::array<float, 3>{0.78f, 0.20f, 0.12f};
                for (const auto local_index : model_group.local_indices) {
                    const auto vertex_index = static_cast<std::size_t>(model_group.first_vertex) + local_index;
                    if (vertex_index >= model.vertices.size()) return false;
                    const auto& model_vertex = model.vertices[vertex_index].fields;
                    // FNIM stores vehicle-local XYZ; this overview projects local X/Z onto the map.
                    const float world_x = pose.x + model_vertex[0] * cos_yaw + model_vertex[2] * sin_yaw;
                    const float world_z = pose.z - model_vertex[0] * sin_yaw + model_vertex[2] * cos_yaw;
                    const auto pos = map_to_clip(world_x, world_z);
                    vertices.push_back({pos[0], pos[1], color[0], color[1], color[2]});
                }
                draw_group.vertex_count = static_cast<UINT>(vertices.size()) - draw_group.first_vertex;
                groups.push_back(draw_group);
            }
        }
        if (vertices.size() > std::numeric_limits<UINT>::max() / sizeof(SurfaceVertex)) return false;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(SurfaceVertex));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices.data();
        ComPtr<ID3D11Buffer> buffer;
        if (FAILED(device_->CreateBuffer(&desc, &data, &buffer))) return false;
        car_buffer_ = std::move(buffer);
        car_draw_groups_ = std::move(groups);
        return true;
    }

    struct CarDrawGroup { UINT first_vertex{}; UINT vertex_count{}; };
    struct CarPose { float x{}; float y{}; float z{}; float yaw{}; };

    bool create_line_pipeline()
    {
        static constexpr char vertex_source[] =
            "struct V { float2 p : POSITION; };\n"
            "cbuffer Camera : register(b0) { float2 center; float zoom; float pad; };\n"
            "float4 VSMain(V v) : SV_POSITION { return float4((v.p-center)*zoom, 0.0, 1.0); }\n";
        static constexpr char pixel_source[] =
            "float4 PSMain() : SV_TARGET { return float4(1.0, 0.66, 0.12, 1.0); }\n";
        ComPtr<ID3DBlob> vertex_code;
        ComPtr<ID3DBlob> pixel_code;
        if (FAILED(compile_shader(vertex_source, "VSMain", "vs_4_0", vertex_code)) ||
            FAILED(compile_shader(pixel_source, "PSMain", "ps_4_0", pixel_code)))
            return false;
        if (FAILED(device_->CreateVertexShader(vertex_code->GetBufferPointer(),
                vertex_code->GetBufferSize(), nullptr, &line_vertex_shader_)) ||
            FAILED(device_->CreatePixelShader(pixel_code->GetBufferPointer(),
                pixel_code->GetBufferSize(), nullptr, &line_pixel_shader_)))
            return false;
        const D3D11_INPUT_ELEMENT_DESC element{
            "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0};
        return SUCCEEDED(device_->CreateInputLayout(&element, 1,
            vertex_code->GetBufferPointer(), vertex_code->GetBufferSize(), &line_input_layout_));
    }

    bool update_line_buffer()
    {
        if (!width_ || !height_ || ideal_line_points_.size() < 2U) return true;
        std::vector<IdealLineVertex> vertices;
        vertices.reserve(ideal_line_points_.size() + 1U);
        for (const auto& point : ideal_line_points_) {
            const auto pos = map_to_clip(point[0], point[1]);
            vertices.push_back({pos[0], pos[1]});
        }
        // The normalized ideal line carries circular next-row links. Close the visual strip.
        vertices.push_back(vertices.front());
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(IdealLineVertex));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices.data();
        ComPtr<ID3D11Buffer> buffer;
        if (FAILED(device_->CreateBuffer(&desc, &data, &buffer))) return false;
        line_buffer_ = std::move(buffer);
        ideal_line_vertex_count_ = static_cast<UINT>(vertices.size());
        return true;
    }

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
    ComPtr<ID3D11VertexShader> surface_vertex_shader_;
    ComPtr<ID3D11PixelShader> surface_pixel_shader_;
    ComPtr<ID3D11InputLayout> surface_input_layout_;
    ComPtr<ID3D11RasterizerState> surface_rasterizer_;
    ComPtr<ID3D11Buffer> surface_buffer_;
    ComPtr<ID3D11Buffer> hud_buffer_;
    ComPtr<ID3D11Buffer> camera_buffer_;
    ComPtr<ID3D11Buffer> car_buffer_;
    ComPtr<ID3D11VertexShader> line_vertex_shader_;
    ComPtr<ID3D11PixelShader> line_pixel_shader_;
    ComPtr<ID3D11InputLayout> line_input_layout_;
    ComPtr<ID3D11Buffer> line_buffer_;
    std::vector<std::array<float, 2>> ideal_line_points_;
    std::vector<std::array<float, 3>> terrain_points_;
    std::vector<std::array<float, 3>> terrain_normals_;
    const viper::ModelResource* player_model_{};
    const viper::ModelResource* ai_model_{};
    std::size_t player_car_index_{};
    std::vector<CarPose> car_poses_;
    std::vector<CarDrawGroup> car_draw_groups_;
    std::array<float, 4> map_bounds_{};
    float min_height_{};
    float max_height_{};
    UINT ideal_line_vertex_count_{};
    UINT surface_vertex_count_{};
    UINT width_{};
    UINT height_{};
    UINT vsync_interval_{1U};
    CameraConstants camera_constants_{};
};

class DirectSoundAudio {
public:
    bool initialize(HWND window, const viper::ResourceBundle& car_bundle,
                    const std::string_view car_name)
    {
        if (FAILED(DirectSoundCreate(nullptr, &device_, nullptr)) ||
            FAILED(device_->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
            return false;
        }

        WAVEFORMATEX primary_format{};
        primary_format.wFormatTag = WAVE_FORMAT_PCM;
        primary_format.nChannels = 2;
        primary_format.nSamplesPerSec = 44100;
        primary_format.wBitsPerSample = 16;
        primary_format.nBlockAlign = 4;
        primary_format.nAvgBytesPerSec = 176400;
        DSBUFFERDESC primary_desc{};
        primary_desc.dwSize = sizeof(primary_desc);
        primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        if (FAILED(device_->CreateSoundBuffer(&primary_desc, &primary_, nullptr)) ||
            FAILED(primary_->SetFormat(&primary_format))) {
            primary_.Reset();
            return false;
        }

        const std::string stem(car_name);
        if (!load_sound(car_bundle, stem + "i.sfx", true)) return false;
        for (unsigned band = 0U; band < 3U; ++band) {
            const auto specific = stem + std::to_string(band) + ".sfx";
            if (!load_sound(car_bundle, specific, false)) {
                const auto fallback = "engine" + std::to_string(band) + ".sfx";
                static_cast<void>(load_sound(car_bundle, fallback, false));
            }
        }
        if (sounds_.size() < 2U) return false;

        for (auto& sound : sounds_) {
            if (FAILED(sound.buffer->SetVolume(DSBVOLUME_MIN)) ||
                FAILED(sound.buffer->SetCurrentPosition(0U)) ||
                FAILED(sound.buffer->Play(0U, 0U, DSBPLAY_LOOPING))) {
                return false;
            }
        }
        idle_index_ = 0U;
        static_cast<void>(sounds_[idle_index_].buffer->SetVolume(-850));
        initialized_ = true;
        return true;
    }

    void update(const float speed, const bool accelerating) noexcept
    {
        if (!initialized_ || sounds_.empty()) return;
        const auto absolute_speed = std::abs(speed);
        std::size_t desired = idle_index_;
        // Temporary player-only band selection; the native ESFX threshold
        // records are known but are not parsed by this runtime yet.
        if (accelerating || absolute_speed > 0.5F) {
            const auto band = absolute_speed < 12.0F ? 0U :
                absolute_speed < 25.0F ? 1U : 2U;
            if (const auto found = band_indices_.find(band);
                found != band_indices_.end()) {
                desired = found->second;
            } else if (!band_indices_.empty()) {
                desired = band_indices_.rbegin()->second;
            }
        }

        const float pitch = 1.0F + std::clamp(absolute_speed / 38.0F, 0.0F, 1.0F) * 0.45F;
        const auto frequency = static_cast<DWORD>(std::clamp(
            std::lround(static_cast<double>(sounds_[desired].sample_rate) * pitch),
            static_cast<long>(DSBFREQUENCY_MIN), static_cast<long>(DSBFREQUENCY_MAX)));
        for (std::size_t index = 0; index < sounds_.size(); ++index) {
            const LONG volume = index == desired ? -850 : DSBVOLUME_MIN;
            static_cast<void>(sounds_[index].buffer->SetVolume(volume));
            if (index == desired)
                static_cast<void>(sounds_[index].buffer->SetFrequency(frequency));
        }
    }

    [[nodiscard]] std::size_t sound_count() const noexcept
    {
        return sounds_.size();
    }

    ~DirectSoundAudio()
    {
        for (auto& sound : sounds_) {
            if (sound.buffer) static_cast<void>(sound.buffer->Stop());
        }
    }

private:
    struct Sound {
        ComPtr<IDirectSoundBuffer> buffer;
        DWORD sample_rate{};
    };

    bool load_sound(const viper::ResourceBundle& bundle,
                    const std::string& name, const bool idle)
    {
        constexpr auto sfx0 = viper::resource_type_code('S', 'F', 'X', '0');
        const auto* entry = bundle.find(name, sfx0);
        if (entry == nullptr) return false;
        const auto pcm = viper::parse_pcm_sfx0_resource(bundle.payload(*entry));
        if (!pcm || pcm->pcm_data.empty() ||
            pcm->pcm_data.size() > std::numeric_limits<DWORD>::max()) {
            return false;
        }

        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = pcm->channels;
        format.nSamplesPerSec = pcm->sample_rate;
        format.wBitsPerSample = pcm->bits_per_sample;
        format.nBlockAlign = pcm->block_align;
        format.nAvgBytesPerSec = pcm->average_bytes_per_second;
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY |
            DSBCAPS_GLOBALFOCUS;
        desc.dwBufferBytes = static_cast<DWORD>(pcm->pcm_data.size());
        desc.lpwfxFormat = &format;

        Sound sound{};
        if (FAILED(device_->CreateSoundBuffer(&desc, &sound.buffer, nullptr)))
            return false;
        void* first_region = nullptr;
        void* second_region = nullptr;
        DWORD first_bytes = 0U;
        DWORD second_bytes = 0U;
        if (FAILED(sound.buffer->Lock(0U, desc.dwBufferBytes, &first_region,
                &first_bytes, &second_region, &second_bytes, 0U))) {
            return false;
        }
        std::memcpy(first_region, pcm->pcm_data.data(), first_bytes);
        if (second_bytes != 0U)
            std::memcpy(second_region, pcm->pcm_data.data() + first_bytes,
                second_bytes);
        if (FAILED(sound.buffer->Unlock(first_region, first_bytes,
                second_region, second_bytes))) {
            return false;
        }
        sound.sample_rate = pcm->sample_rate;
        const auto index = sounds_.size();
        sounds_.push_back(std::move(sound));
        if (idle) idle_index_ = index;
        else band_indices_[band_from_name(name)] = index;
        return true;
    }

    static unsigned band_from_name(const std::string_view name) noexcept
    {
        for (auto it = name.rbegin(); it != name.rend(); ++it) {
            if (*it >= '0' && *it <= '2') return static_cast<unsigned>(*it - '0');
        }
        return 0U;
    }

    ComPtr<IDirectSound> device_;
    ComPtr<IDirectSoundBuffer> primary_;
    std::vector<Sound> sounds_;
    std::map<unsigned, std::size_t> band_indices_;
    std::size_t idle_index_{};
    bool initialized_{};
};

struct WindowState {
    DirectSoundAudio audio;
    D3DWindow graphics;
    const viper::TerrainSurfaceResourcePayload* terrain{};
    std::vector<std::array<float, 2>> route;
    std::size_t player_car_index{};
    viper::RouteMotionState motion{};
    std::vector<viper::RouteMotionState> opponent_motions;
    std::vector<std::array<float, 4>> pose_buffer;
    // Base setup-derived native constructor inputs retained for the session.
    // Wheel anchors currently feed live terrain/contact observations; route
    // motion remains the visible pose authority until the missing simulation
    // inputs are reconstructed.
    std::vector<std::byte> player_runtime_car_record;
    std::optional<viper::NcarBodyConstructorPlan> player_body_constructor_plan;
    std::optional<float> player_body_inverse_mass_0x200;
    std::optional<viper::NcarFourWheelCoreInitialization> player_wheel_core_initialization;
    std::optional<viper::NcarWheelVertexAnchorResult> player_wheel_anchors;
    std::array<std::optional<viper::NcarWheelTireSetup>, 4> player_wheel_tire_setups{};
    // Per-wheel observations use the current renderer's route pose and the
    // constructor's wheel-anchor points. They are exposed in race traces while
    // route motion remains the current pose authority.
    std::array<viper::WheelContactState, 4> player_wheel_contact_states{};
    std::array<viper::WheelContactObservation, 4> player_wheel_contact_observations{};
    std::array<viper::WheelContactResult, 4> player_wheel_contact_results{};
    std::array<bool, 4> player_wheel_terrain_queries_succeeded{};
    bool player_constructor_inputs_ready{};
    bool accelerate{};
    bool brake{};
    bool left{};
    bool right{};
    bool playback_active{};
    bool return_to_frontend{};
    // The native binding evaluator consumes a post-poll device snapshot. The
    // Win32 adapter records key events into its digital-button observation.
    std::array<std::uint8_t, 256> keyboard_button_states{};
    std::array<viper::RaceInputBinding, 4> control_bindings{{
        {1U, 0xcbU}, {1U, 0xcdU}, {1U, 0xc8U}, {1U, 0xd0U}}};
    HWND window{};
    std::wstring title_base;
    std::uint32_t target_laps{1U};
    ULONGLONG last_title_update{};
    ULONGLONG last_tick{};

    bool initialize_player_constructor_inputs(
        const viper::CarFileResourcePayloads& car_resources,
        const std::span<const std::byte> player_setup,
        const std::span<const viper::ModelVertex> model_vertices,
        const std::array<viper::TireDefinition, 4>& tire_definitions,
        const std::optional<std::uint32_t> serialized_stream_offset)
    {
        bool host_fdiv_compatibility = false;
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_ARM64)
#else
        // The x86 compatibility probe is not yet wired into this runtime.
        return false;
#endif
        const auto preparation =
            viper::prepare_car_file_runtime_mapping_from_setup_payload(
                car_resources, player_setup, {}, false, host_fdiv_compatibility);
        if (!preparation.input_complete || !preparation.mapping.has_value())
            return false;
        player_runtime_car_record.assign(
            viper::CarFileRuntimeMapping::runtime_record_size, std::byte{0});
        if (!viper::apply_car_file_runtime_mapping(
                player_runtime_car_record, *preparation.mapping)) {
            player_runtime_car_record.clear();
            return false;
        }
        const auto body_observation =
            viper::make_ncar_body_constructor_observation_from_runtime_record(
                player_runtime_car_record, serialized_stream_offset,
                host_fdiv_compatibility);
        if (!body_observation.has_value()) {
            player_runtime_car_record.clear();
            return false;
        }
        player_body_constructor_plan =
            viper::plan_ncar_body_constructor(*body_observation);
        player_body_inverse_mass_0x200 =
            player_body_constructor_plan->reciprocal_dimensions_0x200_to_0x20c[0];
        player_wheel_core_initialization =
            viper::initialize_ncar_four_wheel_core({
                player_runtime_car_record, model_vertices,
                player_body_constructor_plan->scaled_dimensions_0x1f0_to_0x1fc[0],
                host_fdiv_compatibility});
        if (!player_wheel_core_initialization->input_complete) {
            player_runtime_car_record.clear();
            player_body_constructor_plan.reset();
            player_wheel_core_initialization.reset();
            return false;
        }
        player_wheel_anchors = player_wheel_core_initialization->anchors;
        if (!player_wheel_anchors.has_value()) {
            player_runtime_car_record.clear();
            return false;
        }
        if (!std::all_of(player_wheel_anchors->vertex_indices.begin(),
                player_wheel_anchors->vertex_indices.end(),
                [](const auto& index) { return index.has_value(); })) {
            player_runtime_car_record.clear();
            player_wheel_anchors.reset();
            return false;
        }
        for (std::size_t wheel = 0; wheel < player_wheel_tire_setups.size(); ++wheel) {
            player_wheel_tire_setups[wheel] =
                viper::initialize_ncar_wheel_tire_setup(
                    player_runtime_car_record, wheel, tire_definitions[wheel],
                    host_fdiv_compatibility);
            if (!player_wheel_tire_setups[wheel].has_value()) {
                player_runtime_car_record.clear();
                player_wheel_anchors.reset();
                player_wheel_tire_setups = {};
                return false;
            }
        }
        player_constructor_inputs_ready = true;
        return true;
    }

    void refresh_title(const bool force = false)
    {
        if (window == nullptr) return;
        const auto now = GetTickCount64();
        if (!force && !motion.race.finished && now - last_title_update < 1000U)
            return;
        last_title_update = now;
        std::wstring title = title_base;
        if (motion.race.finished) {
            const auto minutes = static_cast<unsigned>(motion.race.elapsed_seconds / 60.0F);
            const auto seconds = static_cast<unsigned>(motion.race.elapsed_seconds) % 60U;
            title += L" — Finished " + std::to_wstring(minutes) + L":" +
                (seconds < 10U ? L"0" : L"") + std::to_wstring(seconds);
        } else {
            title += L" — Lap " +
                std::to_wstring(motion.race.completed_laps + 1U) + L"/" +
                std::to_wstring(target_laps);
        }
        SetWindowTextW(window, title.c_str());
    }

    bool place_on_route(const float x, const float z)
    {
        if (route.size() < 2U || terrain == nullptr) return false;
        float nearest = std::numeric_limits<float>::infinity();
        for (std::size_t index = 0; index < route.size(); ++index) {
            const auto& a = route[index];
            const auto& b = route[(index + 1U) % route.size()];
            const float dx = b[0] - a[0], dz = b[1] - a[1];
            const float length_sq = dx * dx + dz * dz;
            if (!(length_sq > 1.0e-8f)) continue;
            const float fraction = std::clamp(((x-a[0])*dx + (z-a[1])*dz) / length_sq, 0.0f, 1.0f);
            const float px = a[0] + dx * fraction, pz = a[1] + dz * fraction;
            const float ex = x - px, ez = z - pz;
            const float distance = ex * ex + ez * ez;
            if (distance < nearest) {
                nearest = distance;
                motion.route_segment = index;
                motion.route_fraction = fraction;
            }
        }
        if (!std::isfinite(nearest)) return false;
        const auto surface = viper::sample_terrain_resource(*terrain, {x, 1000.0f, z}, true);
        // D3DWindow has already initialized the full track.obt placement
        // transform. Route projection only seeds progression state; replacing
        // that transform here would discard the native spline heading/height.
        if (!surface.query_succeeded) return false;
        viper::initialize_race_progression(motion.race, motion.route_segment);
        refresh_title(true);
        return true;
    }

    void update_player_wheel_contacts(const float x, const float y, const float z,
                                      const float yaw)
    {
        player_wheel_terrain_queries_succeeded.fill(false);
        player_wheel_contact_observations = {};
        player_wheel_contact_results = {};
        if (terrain == nullptr || !player_constructor_inputs_ready ||
            !player_wheel_core_initialization.has_value()) {
            for (auto& state : player_wheel_contact_states)
                state.contact_active_0x168 = 0U;
            return;
        }
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        for (std::size_t wheel = 0; wheel < player_wheel_contact_states.size(); ++wheel) {
            const auto& core = player_wheel_core_initialization->wheels[wheel];
            if (!core.has_value()) {
                player_wheel_contact_states[wheel].contact_active_0x168 = 0U;
                continue;
            }

            // The selected vertex is subtracted by native 0x441C10 when it
            // stores +0x04..+0x0C, then added back by 0x437BA0. This is the
            // equivalent local point before the current renderer's yaw-only
            // pose transform.
            const float local_x = core->position_relative_to_selected_vertex[0] +
                core->selected_model_vertex[0];
            const float local_y = core->position_relative_to_selected_vertex[1] +
                core->selected_model_vertex[1];
            const float local_z = core->position_relative_to_selected_vertex[2] +
                core->selected_model_vertex[2];
            const viper::PhysicsVector3 wheel_world_point{
                x + local_x * cosine + local_z * sine,
                y + local_y,
                z - local_x * sine + local_z * cosine};

            // The live renderer has yaw but no roll/pitch, so its modeled body
            // query axis remains world-up. The native cheat reset hook is off
            // for the ordinary quick-race path.
            viper::WheelContactObservation observation{};
            observation.body_query_enabled = true;
            observation.reset_feature_hook_active = false;
            observation.contact_projection_origin = wheel_world_point;
            observation.contact_direction = {0.0F, 1.0F, 0.0F};
            observation.wheel_depth_limit_0x44 = core->value_0x44;
            const auto query = viper::sample_terrain_surface(
                viper::make_terrain_surface_query_input(
                    *terrain,
                    {wheel_world_point.x, wheel_world_point.y, wheel_world_point.z},
                    true));
            player_wheel_terrain_queries_succeeded[wheel] = query.query_succeeded;
            observation.query_succeeded = query.query_succeeded;
            if (query.query_succeeded) {
                observation.query_contact_point = {
                    query.sampled_position[0], query.sampled_position[1],
                    query.sampled_position[2]};
                observation.query_contact_normal = {
                    query.plane_normal[0], query.plane_normal[1],
                    query.plane_normal[2]};
                observation.query_feature_output_0x19c =
                    static_cast<std::int32_t>(query.surface_value_0x34);
            }
            player_wheel_contact_observations[wheel] = observation;
            player_wheel_contact_results[wheel] = viper::update_wheel_contact(
                player_wheel_contact_states[wheel], observation);
        }
    }

    bool initialize_opponents()
    {
        if (route.size() < 2U || terrain == nullptr) return false;
        opponent_motions.resize(graphics.car_count());
        pose_buffer.resize(graphics.car_count());
        for (std::size_t car = 0; car < graphics.car_count(); ++car) {
            if (car == player_car_index) continue;
            const auto spawn = graphics.car_position(car);
            auto& ai = opponent_motions[car];
            float nearest = std::numeric_limits<float>::infinity();
            for (std::size_t segment = 0; segment < route.size(); ++segment) {
                const auto& a = route[segment];
                const auto& b = route[(segment + 1U) % route.size()];
                const float dx = b[0] - a[0], dz = b[1] - a[1];
                const float length_sq = dx * dx + dz * dz;
                if (!(length_sq > 1.0e-8F)) continue;
                const float fraction = std::clamp(
                    ((spawn[0] - a[0]) * dx + (spawn[1] - a[1]) * dz) / length_sq,
                    0.0F, 1.0F);
                const float ex = spawn[0] - (a[0] + dx * fraction);
                const float ez = spawn[1] - (a[1] + dz * fraction);
                const float distance = ex * ex + ez * ez;
                if (distance < nearest) {
                    nearest = distance;
                    ai.route_segment = segment;
                    ai.route_fraction = fraction;
                }
            }
            if (!std::isfinite(nearest)) return false;
            ai.speed = 8.0F + static_cast<float>(car % 5U) * 1.5F;
            viper::initialize_race_progression(ai.race, ai.route_segment);
        }
        return true;
    }

    bool advance(float delta_seconds, const bool update_graphics = true)
    {
        if (route.size() < 2U || terrain == nullptr) return false;
        auto candidate_motion = motion;
        const auto route_result = viper::advance_route_motion(candidate_motion, route,
            left, right, accelerate, brake, delta_seconds, target_laps);
        if (!route_result.valid && !route_result.finished) return false;
        std::optional<viper::TerrainSurfaceQueryResult> surface;
        if (route_result.valid) {
            surface = viper::sample_terrain_resource(
                *terrain, {route_result.x, 1000.0f, route_result.z}, true);
            if (!surface->query_succeeded) return true;
            if (!viper::commit_route_motion_after_terrain_query(
                    motion, candidate_motion, surface->query_succeeded)) return true;
        } else if (route_result.finished) {
            motion = candidate_motion;
        }
        if (route_result.valid)
            update_player_wheel_contacts(route_result.x, surface->sampled_position[1],
                route_result.z, route_result.yaw);
        if (route_result.lap_completed) refresh_title(true);
        if (update_graphics) {
            for (std::size_t car = 0; car < graphics.car_count(); ++car) {
                const auto position = graphics.car_position(car);
                pose_buffer[car] = {position[0], graphics.car_height(car), position[1],
                    graphics.car_yaw(car)};
            }
            if (route_result.valid)
                pose_buffer[player_car_index] = {route_result.x, surface->sampled_position[1],
                    route_result.z, route_result.yaw};
        }
        for (std::size_t car = 0; car < opponent_motions.size(); ++car) {
            if (car == player_car_index) continue;
            auto& ai = opponent_motions[car];
            auto candidate_ai = ai;
            const auto ai_result = viper::advance_route_motion(candidate_ai, route, false, false,
                true, false, delta_seconds, target_laps);
            if (!ai_result.valid) continue;
            const auto ai_surface = viper::sample_terrain_resource(
                *terrain, {ai_result.x, 1000.0f, ai_result.z}, true);
            if (!ai_surface.query_succeeded ||
                !viper::commit_route_motion_after_terrain_query(
                    ai, candidate_ai, ai_surface.query_succeeded)) continue;
            if (update_graphics) {
                pose_buffer[car] = {ai_result.x, ai_surface.sampled_position[1],
                    ai_result.z, ai_result.yaw};
            }
        }
        if (update_graphics && !graphics.set_car_poses(pose_buffer)) return false;
        audio.update(motion.race.finished ? 0.0F : motion.speed,
            !motion.race.finished && accelerate);
        refresh_title();
        return true;
    }

    bool results_ready() const noexcept
    {
        bool every_entrant_finished = motion.race.finished;
        for (std::size_t index = 0; index < opponent_motions.size() &&
                every_entrant_finished; ++index) {
            if (index != player_car_index &&
                !opponent_motions[index].race.finished) {
                every_entrant_finished = false;
            }
        }
        return every_entrant_finished;
    }

    std::string hud_text() const
    {
        if (results_ready()) {
            const auto standings = viper::build_route_race_standings(
                motion, opponent_motions, player_car_index);
            std::string results = "RACE RESULTS";
            for (std::size_t rank = 0; rank < standings.size(); ++rank) {
                const auto& standing = standings[rank];
                const auto minutes = static_cast<unsigned>(
                    standing.finish_time_seconds / 60.0F);
                const auto seconds = static_cast<unsigned>(
                    standing.finish_time_seconds) % 60U;
                const auto hundredths = static_cast<unsigned>(std::fmod(
                    standing.finish_time_seconds, 1.0F) * 100.0F);
                results += "\n" + std::to_string(rank + 1U) + " ";
                results += standing.player ? "YOU " : "AI ";
                if (!standing.player) results += std::to_string(standing.participant_index + 1U) + " ";
                results += std::to_string(minutes) + ":" +
                    (seconds < 10U ? "0" : "") + std::to_string(seconds) + "." +
                    (hundredths < 10U ? "0" : "") + std::to_string(hundredths);
            }
            results += "\nENTER: SETUP / ESC: EXIT";
            return results;
        }
        const auto minutes = static_cast<unsigned>(motion.race.elapsed_seconds / 60.0F);
        const auto seconds = static_cast<unsigned>(motion.race.elapsed_seconds) % 60U;
        const auto hundredths = static_cast<unsigned>(
            std::fmod(std::max(0.0F, motion.race.elapsed_seconds), 1.0F) * 100.0F);
        std::string status = "LAP " + std::to_string(std::min(target_laps,
            motion.race.completed_laps + (motion.race.finished ? 0U : 1U))) +
            " / " + std::to_string(target_laps);
        status += "\nTIME " + std::to_string(minutes) + ":" +
            (seconds < 10U ? "0" : "") + std::to_string(seconds) + "." +
            (hundredths < 10U ? "0" : "") + std::to_string(hundredths);
        status += "\nSPEED " + std::to_string(static_cast<int>(
            std::lround(std::abs(motion.speed) * 3.6F))) + " KM/H";
        if (motion.race.finished) status += "\nRACE FINISHED";
        return status;
    }
};

std::optional<UINT> parse_keyboard_binding(const std::string_view binding)
{
    if (binding.empty()) return std::nullopt;
    std::string key(binding);
    if (key.starts_with("Key_")) key.erase(0, 4);
    for (auto& character : key) {
        character = static_cast<char>(std::toupper(
            static_cast<unsigned char>(character)));
    }
    if (key == "LEFT") return VK_LEFT;
    if (key == "RIGHT") return VK_RIGHT;
    if (key == "UP") return VK_UP;
    if (key == "DOWN") return VK_DOWN;
    if (key == "SPACE") return VK_SPACE;
    if (key == "RETURN" || key == "ENTER") return VK_RETURN;
    if (key == "TAB") return VK_TAB;
    if (key.size() == 1U &&
        ((key[0] >= 'A' && key[0] <= 'Z') ||
         (key[0] >= '0' && key[0] <= '9'))) {
        return static_cast<UINT>(key[0]);
    }
    throw std::runtime_error("unsupported keyboard binding in options.def: " +
        std::string(binding));
}

std::optional<std::uint8_t> directinput_key_selector(const UINT virtual_key)
{
    const UINT mapped = MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC_EX);
    if (mapped == 0U) return std::nullopt;
    const UINT scan_code = mapped & 0xffU;
    const UINT prefix = (mapped >> 8U) & 0xffU;
    // DirectInput's keyboard state uses DIK scan-code indexes. Extended E0
    // scan codes occupy the high half of that table (e.g. Left = 0xCB).
    return static_cast<std::uint8_t>(scan_code | (prefix == 0xe0U ? 0x80U : 0U));
}

void load_keyboard_bindings(WindowState& state,
                            const viper::GameOptions& options)
{
    constexpr std::array<std::string_view, 4> action_names{
        "steer_left", "steer_right", "throttle", "braking"};
    for (std::size_t index = 0; index < action_names.size(); ++index) {
        const auto configured = options.get("CONTROL", action_names[index]);
        if (configured) {
            const auto key = parse_keyboard_binding(*configured);
            if (!key) {
                state.control_bindings[index] = {};
                continue;
            }
            const auto selector = directinput_key_selector(*key);
            if (!selector)
                throw std::runtime_error("could not map keyboard binding to a DirectInput scan code: " +
                    std::string(*configured));
            state.control_bindings[index] = {1U, *selector};
        }
    }
}

void refresh_keyboard_controls(WindowState& state)
{
    viper::RaceInputBindingObservation observation{};
    observation.device_button_states = state.keyboard_button_states;
    state.left = viper::race_input_binding_is_active(
        state.control_bindings[0], observation);
    state.right = viper::race_input_binding_is_active(
        state.control_bindings[1], observation);
    state.accelerate = viper::race_input_binding_is_active(
        state.control_bindings[2], observation);
    state.brake = viper::race_input_binding_is_active(
        state.control_bindings[3], observation);
}

void update_control_key(WindowState& state, const UINT key, const bool pressed)
{
    const auto selector = directinput_key_selector(key);
    if (!selector) return;
    state.keyboard_button_states[*selector] = static_cast<std::uint8_t>(pressed);
    refresh_keyboard_controls(state);
}

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
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wparam == VK_RETURN && state && state->results_ready()) {
            state->return_to_frontend = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (state && !state->playback_active && (lparam & (1LL << 30)) == 0) {
            update_control_key(*state, static_cast<UINT>(wparam), true);
        }
        return 0;
    case WM_KEYUP:
        if (state && !state->playback_active) {
            update_control_key(*state, static_cast<UINT>(wparam), false);
        }
        break;
    case WM_KILLFOCUS:
    case WM_CANCELMODE:
        if (state && !state->playback_active) {
            state->keyboard_button_states.fill(0U);
            refresh_keyboard_controls(*state);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int run_window(bool software, std::optional<unsigned> smoke_frames,
               const bool smoke_audio,
               const std::optional<std::filesystem::path>& record_trace,
               const std::vector<TraceInputFrame>* playback_trace,
               const bool hold_results_for_frontend,
               const std::size_t player_car_index,
               const std::uint32_t target_laps,
               const std::string_view track_name,
               const std::string_view car_name,
               const viper::TerrainSurfaceResourcePayload& terrain,
               const viper::IdealLineTrackData& ideal_line,
               const viper::ModelResource& player_model,
               const viper::ModelResource& ai_model,
               const viper::TrackObjectTextTable& track_objects,
               const viper::ResourceBundle& car_bundle,
               const viper::CarFileResourcePayloads& player_car_resources,
               const std::array<std::byte, 0x8c>& player_car_setup,
               const std::array<viper::TireDefinition, 4>& wheel_tires,
               const bool force_default_placement,
               const viper::GameOptions& options)
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
    state.terrain = &terrain;
    const auto cursor_plan =
        viper::plan_track_player_physics_cursor(track_objects, player_car_index);
    const auto serialized_stream_offset = cursor_plan.serialized_object_stream_offset;
    if (!state.initialize_player_constructor_inputs(player_car_resources,
            player_car_setup, player_model.vertices, wheel_tires,
            serialized_stream_offset)) {
        std::cerr << "Could not prepare base CARF wheel constructor inputs from the active setup.\n";
        return 1;
    }
    std::cout << "Prepared session-owned base CARF constructor inputs: mapped record, "
              << "body dimension scale "
              << state.player_body_constructor_plan->scaled_dimensions_0x1f0_to_0x1fc[0]
              << " at +0x1F0, "
              << "inverse mass " << *state.player_body_inverse_mass_0x200
              << " at +0x200, "
              << state.player_wheel_anchors->vertex_indices.size()
              << " FNIM anchors and four wheel core setups, plus four axle-selected "
              << "tire response objects. ";
    if (serialized_stream_offset.has_value()) {
        std::cout << "Inherited constructor argument 2 uses track-derived physics "
                  << "stream offset 0x" << std::hex << *serialized_stream_offset
                  << std::dec << ". ";
    } else if (cursor_plan.unsupported_object_row.has_value()) {
        const auto row = *cursor_plan.unsupported_object_row;
        const auto line = viper::track_object_text_line(track_objects, row);
        std::cout << "Inherited constructor argument 2 is unresolved at track.obt row "
                  << row << " (" << (line ? *line : std::string_view{})
                  << "); its serialized class advance is not yet mapped. ";
    } else {
        std::cout << "Inherited constructor argument 2 is unresolved because an "
                  << "earlier non-car physics object is not yet mapped. ";
    }
    std::cout
              << "Upgrade overlays are not requested in this race path.\n";
    state.playback_active = playback_trace != nullptr;
    state.route = extract_ideal_line_points(ideal_line);
    state.player_car_index = player_car_index;
    state.target_laps = std::max(1U, target_laps);
    state.title_base = std::wstring(L"Viper Racing — ") +
        std::wstring(track_name.begin(), track_name.end()) + L" & " +
        std::wstring(car_name.begin(), car_name.end());
    load_keyboard_bindings(state, options);
    const auto title_utf8 = std::string("Viper Racing — ") + std::string(track_name) +
        " & " + std::string(car_name) + " Race (WASD / Arrows)";
    const int title_length = MultiByteToWideChar(CP_UTF8, 0, title_utf8.c_str(), -1, nullptr, 0);
    if (title_length <= 0) return 1;
    std::wstring title(static_cast<std::size_t>(title_length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, title_utf8.c_str(), -1,
            title.data(), title_length) <= 0) return 1;
    title.pop_back();
    HWND hwnd = CreateWindowExW(0, class_name, title.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, instance, &state);
    if (!hwnd) { std::cerr << "Could not create the runtime window.\n"; return 1; }
    state.window = hwnd;
    if (!state.graphics.create(hwnd, software, terrain, ideal_line, player_model,
            ai_model, player_car_index, track_objects, car_name,
            force_default_placement)) {
        std::cerr << "Could not initialize Direct3D 11. "
                  << (software ? "WARP initialization failed.\n" : "Try --software to use WARP.\n");
        DestroyWindow(hwnd);
        return 1;
    }
    if (playback_trace != nullptr) state.graphics.set_vsync(false);
    const auto spawn = state.graphics.car_position(player_car_index);
    if (!state.place_on_route(spawn[0], spawn[1])) {
        std::cerr << "Could not align the grid start with the ideal line and terrain.\n";
        DestroyWindow(hwnd);
        return 1;
    }
    if (!state.initialize_opponents()) {
        std::cerr << "Could not initialize provisional opponent route states.\n";
        DestroyWindow(hwnd);
        return 1;
    }
    std::ofstream trace;
    if (record_trace) {
        trace.open(*record_trace, std::ios::out | std::ios::trunc);
        if (!trace) {
            std::cerr << "Could not create race trace: " << record_trace->string() << '\n';
            DestroyWindow(hwnd);
            return 1;
        }
        trace << "frame,elapsed_seconds,delta_seconds,steer_left,steer_right,throttle,brake,player_segment,player_fraction,player_speed,player_steering,completed_laps,finished";
        for (std::size_t car = 0; car < state.opponent_motions.size(); ++car) {
            if (car == player_car_index) continue;
            trace << ",car" << car << "_segment,car" << car
                  << "_fraction,car" << car << "_speed,car" << car
                  << "_completed_laps,car" << car << "_finished";
        }
        for (std::size_t wheel = 0; wheel < state.player_wheel_contact_states.size(); ++wheel) {
            trace << ",wheel" << wheel << "_query_ok,wheel" << wheel
                  << "_contact_gate,wheel" << wheel << "_active,wheel" << wheel
                  << "_feature,wheel" << wheel << "_depth";
        }
        for (std::size_t wheel = 0; wheel < state.player_wheel_contact_states.size(); ++wheel) {
            trace << ",wheel" << wheel << "_surface_x,wheel" << wheel
                  << "_surface_y,wheel" << wheel << "_surface_z,wheel" << wheel
                  << "_normal_x,wheel" << wheel << "_normal_y,wheel" << wheel
                  << "_normal_z";
        }
        trace << '\n' << std::setprecision(9);
        if (!trace) {
            std::cerr << "Could not write race trace header.\n";
            DestroyWindow(hwnd);
            return 1;
        }
    }
    if ((!smoke_frames || smoke_audio) &&
        !state.audio.initialize(hwnd, car_bundle, car_name)) {
        std::cerr << "DirectSound initialization or engine sample loading failed; "
                     "running without audio.\n";
    } else if (!smoke_frames || smoke_audio) {
        std::cout << "DirectSound initialized with " << state.audio.sound_count()
                  << " parsed SFX0 PCM engine resources.\n";
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    MSG msg{};
    bool running = true;
    unsigned rendered_frames = 0;
    std::uint64_t trace_frame = 0;
    std::size_t playback_frame = 0U;
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
            if (playback_trace != nullptr && playback_frame >= playback_trace->size()) {
                if (hold_results_for_frontend && state.results_ready()) {
                    playback_trace = nullptr;
                    state.playback_active = false;
                    state.left = state.right = state.accelerate = state.brake = false;
                    state.last_tick = GetTickCount64();
                    continue;
                }
                DestroyWindow(hwnd);
                continue;
            }
            const ULONGLONG now = GetTickCount64();
            const TraceInputFrame* replay = playback_trace == nullptr
                ? nullptr : &(*playback_trace)[playback_frame];
            if (replay != nullptr) {
                state.left = replay->steer_left;
                state.right = replay->steer_right;
                state.accelerate = replay->throttle;
                state.brake = replay->brake;
            }
            const float delta_seconds = replay != nullptr ? replay->delta_seconds :
                state.last_tick == 0U ? 0.0F :
                    static_cast<float>(now - state.last_tick) / 1000.0F;
            const bool render_frame = replay == nullptr ||
                (playback_frame % 8U) == 0U || playback_frame + 1U == playback_trace->size();
            if ((replay != nullptr || state.last_tick != 0U) &&
                !state.advance(delta_seconds, render_frame)) {
                std::cerr << "Vehicle pose update failed.\n";
                DestroyWindow(hwnd);
                return 1;
            }
            state.last_tick = now;
            if (record_trace) {
                trace << trace_frame++ << ',' << state.motion.race.elapsed_seconds << ','
                      << delta_seconds << ',' << state.left << ',' << state.right << ','
                      << state.accelerate << ',' << state.brake << ','
                      << state.motion.route_segment << ',' << state.motion.route_fraction << ','
                      << state.motion.speed << ',' << state.motion.steering << ','
                      << state.motion.race.completed_laps << ','
                      << state.motion.race.finished;
                for (std::size_t car = 0; car < state.opponent_motions.size(); ++car) {
                    if (car == player_car_index) continue;
                    const auto& ai = state.opponent_motions[car];
                    trace << ',' << ai.route_segment << ',' << ai.route_fraction << ',' << ai.speed
                          << ',' << ai.race.completed_laps << ',' << ai.race.finished;
                }
                for (std::size_t wheel = 0; wheel < state.player_wheel_contact_states.size(); ++wheel) {
                    trace << ',' << state.player_wheel_terrain_queries_succeeded[wheel]
                          << ',' << state.player_wheel_contact_results[wheel].contact_gate_passed
                          << ',' << static_cast<unsigned>(
                                state.player_wheel_contact_states[wheel].contact_active_0x168)
                          << ',' << state.player_wheel_contact_states[wheel].contact_feature_0x19c
                          << ',' << state.player_wheel_contact_results[wheel].projected_depth;
                }
                for (std::size_t wheel = 0; wheel < state.player_wheel_contact_states.size(); ++wheel) {
                    const auto& observation = state.player_wheel_contact_observations[wheel];
                    trace << ',' << observation.query_contact_point.x
                          << ',' << observation.query_contact_point.y
                          << ',' << observation.query_contact_point.z
                          << ',' << observation.query_contact_normal.x
                          << ',' << observation.query_contact_normal.y
                          << ',' << observation.query_contact_normal.z;
                }
                trace << '\n';
                if (!trace) {
                    std::cerr << "Could not write race trace.\n";
                    DestroyWindow(hwnd);
                    return 1;
                }
                if ((trace_frame % 120U) == 0U) trace.flush();
            }
            const auto camera_focus =
                state.graphics.car_position(state.player_car_index);
            if (render_frame && !state.graphics.render(state.hud_text(),
                    camera_focus[0], camera_focus[1])) {
                std::cerr << "Direct3D frame rendering or presentation failed.\n";
                DestroyWindow(hwnd);
                return 1;
            }
            if (replay != nullptr) ++playback_frame;
            if (render_frame && smoke_frames && ++rendered_frames >= *smoke_frames) {
                DestroyWindow(hwnd);
                return 0;
            }
        }
    }
    return state.return_to_frontend ? 10 : static_cast<int>(msg.wParam);
}

void report_start_grid(const viper::GameAssets& assets)
{
    const auto& table = assets.track_objects();
    std::size_t car_directives = 0;
    std::cout << "Recovered " << assets.track_name()
              << " track.obt: " << table.line_count
              << " rows, stride " << table.line_stride << ".\n";
    for (std::size_t row = 1; row < table.line_count; ++row) {
        const auto line = viper::track_object_text_line(table, row);
        if (!line) continue;
        const auto directive = viper::classify_track_directive(*line);
        if (directive.kind != viper::TrackDirectiveKind::object ||
            !directive.object_line.starts_with("obj car ")) continue;
        ++car_directives;
        std::cout << "  grid row " << row << ": " << directive.object_line << '\n';
    }
    std::cout << "  observed car placement directives: " << car_directives << '\n';
}

int run_selected_race(const Options& options, const std::string_view track,
                     const std::string_view car,
                     const std::vector<TraceInputFrame>* playback_trace,
                     const bool hold_results_for_frontend)
{
    const auto config_path = options.data_dir / "options.def";
    viper::GameOptions configured_options;
    std::error_code config_error;
    if (std::filesystem::is_regular_file(config_path, config_error) && !config_error)
        configured_options = viper::GameOptions::load(config_path);
    const auto setup_config_directory = options.setup_config_directory
        ? *options.setup_config_directory
        : resolve_setup_config_directory(options.data_dir);
    auto assets = load_assets(options.data_dir, track, car,
        std::move(configured_options), setup_config_directory);
    if (assets.player_setup_version_warning())
        std::cerr << "Warning: player CSU version is not 1; native loader continues with its first 0x8c bytes\n";
    report_start_grid(assets);
    if (options.validate_assets) return 0;
    const auto driver_order = viper::initialize_single_race_driver_order();
    constexpr std::array<std::string_view, 7> ai_names{
        "AI slot 0", "AI slot 1", "AI slot 2", "AI slot 3",
        "AI slot 4", "AI slot 5", "AI slot 6"};
    viper::SingleRaceEntrantObservation entrant_observation{};
    entrant_observation.configured_car_count = assets.options()
        .integer("GAME", "car_count").value_or(8);
    entrant_observation.driver_order_values = driver_order;
    entrant_observation.ai_names_by_slot = ai_names;
    entrant_observation.player_car_resource_name = assets.car_name();
    entrant_observation.resolved_player_name =
        assets.options().get("GLOBAL", "player_name");
    entrant_observation.player_paintjob_value =
        assets.options().integer("GLOBAL", "player_paintjob");
    const auto entrants = viper::build_single_race_entrants(entrant_observation);
    if (!entrants.player_record_index)
        throw std::runtime_error("single-race setup could not resolve the human participant row");
    std::cout << "Selected human grid slot: "
              << (*entrants.player_record_index + 1U) << " of "
              << entrants.participant_count << " (fresh single-race driver order)\n";
    std::vector<viper::SingleRaceAISetupPlan> ai_setup_variants;
    if (entrants.participant_count > 1) {
        if (!assets.track_index())
            throw std::runtime_error("selected track has no matching tracks.tab row for AI setup");
        ai_setup_variants.reserve(viper::ai_driver_group_count * 2U);
        std::size_t complete_setup_records = 0;
        for (std::size_t group = 0; group < viper::ai_driver_group_count; ++group) {
            for (std::uint8_t mode = 0; mode <= 1U; ++mode) {
                viper::SingleRaceAISetupObservation ai_observation{};
                ai_observation.entrants = &entrants;
                ai_observation.drivers = &assets.ai_drivers();
                ai_observation.driver_group =
                    static_cast<viper::AIDriverGroup>(group);
                ai_observation.track_index = *assets.track_index();
                ai_observation.tuning_mode = mode;
                ai_observation.base_setup_payload = assets.ai_vehicle_setup();
                ai_observation.configuration_data_available = true;
                ai_observation.loaded_car_file = assets.ai_car().car_file;
                ai_observation.compatibility_division_helper_enabled = false;
                auto plan = viper::prepare_single_race_ai_setups(ai_observation);
                complete_setup_records += static_cast<std::size_t>(std::count_if(
                    plan.ai_setups.begin(), plan.ai_setups.end(),
                    [](const auto& record) { return record.input_complete; }));
                ai_setup_variants.push_back(std::move(plan));
            }
        }
        std::size_t prepared_records = 0;
        for (const auto& variant : ai_setup_variants)
            prepared_records += variant.ai_setups.size();
        std::cout << "Prepared " << prepared_records
                  << " AI setup variants across all AIDR profiles and tuning modes ("
                  << complete_setup_records
                  << " numerically complete with the current FDIV model)\n";
    }
    return run_window(options.software, options.smoke_frames, options.audio_smoke,
        options.record_trace, playback_trace, hold_results_for_frontend,
        *entrants.player_record_index,
        static_cast<std::uint32_t>(std::clamp(
            assets.options().integer("GAME", "laps").value_or(1), 1, 99)),
        assets.track_name(), assets.car_name(), assets.terrain(),
        assets.ideal_line(), assets.player_model(), assets.ai_model(),
        assets.track_objects(),
        assets.bundle(std::string(assets.car_name()) + ".car"),
        assets.player_car(), assets.player_car_setup(), assets.wheel_tires(),
        options.grid_mode, assets.options());
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) { usage(std::cout); return 0; }
        if (options.data_dir.empty())
            throw std::runtime_error("pass --data-dir PATH or set VIPER_DATA_DIR");
        std::optional<std::vector<TraceInputFrame>> playback_trace;
        if (options.play_trace) playback_trace = load_input_trace(*options.play_trace);
        const auto config_path = options.data_dir / "options.def";
        viper::GameOptions configured_options;
        std::error_code config_error;
        if (std::filesystem::is_regular_file(config_path, config_error) && !config_error)
            configured_options = viper::GameOptions::load(config_path);
        const auto track = options.track
            ? *options.track
            : configured_track_stem(options.data_dir, configured_options);
        const auto car = options.car
            ? *options.car
            : configured_car_stem(options.data_dir, configured_options);
        const bool interactive_frontend = !options.validate_assets &&
            !options.track && !options.car && !options.smoke_frames &&
            !options.audio_smoke && !options.record_trace;
        auto selected_track = track;
        auto selected_car = car;
        while (true) {
            if (interactive_frontend) {
                const auto selection = show_setup_frontend(
                    selectable_track_stems(options.data_dir),
                    selectable_car_stems(options.data_dir),
                    selected_track, selected_car,
                    options.setup_config_directory
                        ? *options.setup_config_directory
                        : resolve_setup_config_directory(options.data_dir));
                if (!selection) return 0;
                selected_track = selection->track;
                selected_car = selection->car;
            }
            const auto result = run_selected_race(options, selected_track,
                selected_car, playback_trace ? &*playback_trace : nullptr,
                interactive_frontend);
            if (result != 10) return result;
            if (!interactive_frontend) return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
