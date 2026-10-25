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
#include "viper/route_opponent_ai.hpp"
#include "viper/sound_resource.hpp"
#include "viper/engine_sound_setup.hpp"
#include "viper/wheel_body_coupling.hpp"
#include "viper/physics_task_stream.hpp"
#include "viper/physics_task_factory.hpp"
#include "viper/physics_scheduler.hpp"
#include "viper/track_object_task_records.hpp"
#include "viper/world_track_objects.hpp"
#include "viper/career_file_store.hpp"
#include "viper/pcar_control_update.hpp"
#include "viper/setup_preferences.hpp"
#include "viper/track_selection.hpp"
#include "viper/replay_file.hpp"
#include "directinput_keyboard_service.h"

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
#include <random>
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
    bool data_dir_explicit{};
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
    std::optional<std::filesystem::path> record_physics_tick_trace;
    std::optional<std::filesystem::path> play_trace;
    std::optional<std::filesystem::path> load_replay;
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
        << "Usage: viper_game [--data-dir PATH] [--config-dir PATH] [--track NAME] [--car NAME] [-grid] [--validate-assets] [--smoke-frames N] [--smoke-audio] [--record-trace PATH] [--play-trace PATH] [--load-replay PATH] [--software] [--help]\n"
        << "  --load-replay PATH validates and retains a native replay container for the race session; frame payload remains opaque.\n"
        << "  --record-tick-trace PATH writes one diagnostic row per scheduled player physics tick.\n"
        << "Set VIPER_DATA_DIR or pass --data-dir to locate extracted game assets.\n"
        << "Without either, searches the current and executable directories for common.res, Data/common.res, or GameData/common.res.\n";
}

std::filesystem::path executable_directory()
{
    std::vector<wchar_t> buffer(512U);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0U)
            throw std::runtime_error("could not determine the executable directory");
        if (length < buffer.size())
            return std::filesystem::path(std::wstring_view(buffer.data(), length))
                .parent_path();
        if (buffer.size() >= 32768U)
            throw std::runtime_error("executable path exceeds the Windows path limit");
        buffer.resize(std::min<std::size_t>(buffer.size() * 2U, 32768U));
    }
}

std::filesystem::path resolve_data_directory(const Options& options)
{
    if (options.data_dir_explicit || !options.data_dir.empty())
        return options.data_dir;

    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (error)
        throw std::runtime_error("could not determine the current directory: " +
            error.message());
    const auto executable = executable_directory();
    const std::array<std::filesystem::path, 13> candidates{{
        current,
        current / "Data",
        current / "data",
        current / "GameData",
        current / "data" / "GameData",
        executable,
        executable / "Data",
        executable / "GameData",
        executable.parent_path() / "Data",
        executable.parent_path() / "GameData",
        executable.parent_path().parent_path() / "Data",
        executable.parent_path().parent_path() / "GameData",
        executable / "data" / "GameData"}};
    std::vector<std::filesystem::path> attempted;
    for (const auto& candidate : candidates) {
        const auto normalized = candidate.lexically_normal();
        if (std::find(attempted.begin(), attempted.end(), normalized) != attempted.end())
            continue;
        attempted.push_back(normalized);
        error.clear();
        if (std::filesystem::is_regular_file(normalized / "common.res", error) &&
            !error)
            return normalized;
    }

    std::string message = "could not locate extracted game assets (common.res). Searched:";
    for (const auto& path : attempted)
        message += "\n  " + path.string() + "\\common.res";
    message += "\nPass --data-dir PATH or set VIPER_DATA_DIR to the extracted Data/GameData directory.";
    throw std::runtime_error(std::move(message));
}

Options parse_options(const int argc, wchar_t** argv)
{
    Options options;
    const DWORD required = GetEnvironmentVariableW(L"VIPER_DATA_DIR", nullptr, 0);
    // An empty environment variable is equivalent to leaving this optional
    // override unset. Windows reports its required size as one for the NUL.
    if (required > 1U) {
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
        } else if (arg == L"--record-tick-trace") {
            if (++i >= argc) throw std::runtime_error("--record-tick-trace requires a path");
            options.record_physics_tick_trace = std::filesystem::path(argv[i]);
        } else if (arg == L"--play-trace") {
            if (++i >= argc) throw std::runtime_error("--play-trace requires a path");
            options.play_trace = std::filesystem::path(argv[i]);
        } else if (arg == L"--load-replay") {
            if (++i >= argc) throw std::runtime_error("--load-replay requires a path");
            options.load_replay = std::filesystem::path(argv[i]);
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
            options.data_dir_explicit = true;
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
        if (stem == "random") {
            if (std::find(tracks.begin(), tracks.end(), stem) == tracks.end())
                tracks.push_back(std::move(stem));
            continue;
        }
        std::error_code error;
        if (!std::filesystem::is_regular_file(data_dir / (stem + ".trk"), error) || error)
            continue;
        tracks.push_back(std::move(stem));
    }
    if (tracks.empty()) throw std::runtime_error(
        "tracks.tab contains no installed selectable track bundles");
    return tracks;
}

std::string resolve_random_track_choice(const std::vector<std::string>& tracks)
{
    const auto installed_count = static_cast<std::size_t>(std::count_if(
        tracks.begin(), tracks.end(), [](const std::string& track) {
            return track != "random";
        }));
    if (installed_count == 0U)
        throw std::runtime_error("random track selection has no installed tracks");
    std::random_device entropy;
    std::mt19937 generator(entropy());
    std::uniform_int_distribution<std::size_t> choose(0U, installed_count - 1U);
    const auto selected = viper::resolve_track_choice_index(
        tracks, "random", choose(generator));
    if (!selected)
        throw std::runtime_error("random track choice could not be resolved");
    return tracks[*selected];
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
    std::uint32_t laps{1U};
    std::uint32_t opponent_strength{};
    std::array<std::string, 13> control_bindings{};
};

constexpr std::array<std::string_view, 13> supported_control_names{
    "steer_left", "steer_right", "throttle", "braking", "e_brake",
    "reverse", "clutch", "airlift", "look_left", "look_right",
    "look_back", "downshift", "upshift"};

std::optional<std::string> keyboard_binding_name(const UINT key)
{
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return "Key_" + std::string(1, static_cast<char>(key));
    switch (key) {
    case VK_LEFT: return "Key_Left";
    case VK_RIGHT: return "Key_Right";
    case VK_UP: return "Key_Up";
    case VK_DOWN: return "Key_Down";
    case VK_SPACE: return "Key_Space";
    case VK_RETURN: return "Key_Return";
    case VK_TAB: return "Key_Tab";
    default: return std::nullopt;
    }
}

constexpr std::array<std::string_view, viper::ai_driver_group_count>
    opponent_strength_names{"Easy", "Intermediate", "Hard", "Career 1",
        "Career 2", "Career 3", "Career 4", "Career 5"};

struct SetupWindowState {
    std::vector<std::string> tracks;
    std::vector<std::string> cars;
    std::size_t track_index{};
    std::size_t car_index{};
    std::uint32_t laps{1U};
    std::uint32_t opponent_strength{};
    std::filesystem::path config_directory;
    std::array<std::string, 5> career_rows{};
    int career_slot{};
    bool career_screen{};
    bool controls_screen{};
    std::array<std::string, 13> control_bindings{};
    std::array<std::string, 13> default_control_bindings{};
    std::size_t selected_control{};
    std::optional<std::size_t> capture_control;
    std::string settings_message;
    bool career_delete_confirmation{};
    std::string career_message;
    bool accepted{};
};

bool persist_setup_preferences(SetupWindowState& state)
{
    std::array<std::optional<std::string>, 13> bindings{};
    for (std::size_t index = 0; index < bindings.size(); ++index)
        bindings[index] = state.control_bindings[index];
    const auto saved = viper::save_setup_selection_preference(
        state.config_directory,
        {state.tracks[state.track_index], state.cars[state.car_index],
            state.laps, state.opponent_strength, bindings});
    state.settings_message = saved.saved ? "SETTINGS SAVED"
        : "SAVE FAILED: " + saved.error;
    return saved.saved;
}

std::wstring ascii_to_wide(const std::string_view text)
{
    return std::wstring(text.begin(), text.end());
}

std::wstring binding_label(const std::string_view binding)
{
    if (binding.empty()) return L"UNBOUND";
    if (binding.starts_with("Key_")) return ascii_to_wide(binding.substr(4U));
    return ascii_to_wide(binding);
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
    if (!state.career_screen && !state.controls_screen) {
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
        SetTextColor(dc, RGB(118, 208, 155));
        write(L"RACE LAPS: " + std::to_wstring(state.laps), 74, 284, 34);
        write(L"OPPONENT STRENGTH: " + ascii_to_wide(
            opponent_strength_names[state.opponent_strength]), 74, 320, 34);
        SetTextColor(dc, RGB(190, 205, 198));
        write(L"UP / DOWN: CHOOSE TRACK", 74, 362, 26);
        write(L"LEFT / RIGHT: CHOOSE CAR", 74, 390, 26);
        write(L"PAGE UP / DOWN: CHANGE LAPS", 74, 418, 26);
        write(L"[ / ]: CHANGE OPPONENT STRENGTH", 74, 446, 26);
        write(L"ENTER: START RACE    K: CONTROLS    C: CAREER    ESC: EXIT", 74, 486, 30);
    }
    if (state.controls_screen) {
        SetTextColor(dc, RGB(118, 208, 155));
        write(L"KEYBOARD CONTROLS", 74, 120, 34);
        SetTextColor(dc, RGB(238, 244, 240));
        for (std::size_t index = 0; index < supported_control_names.size(); ++index) {
            std::wstring row = index == state.selected_control ? L"> " : L"  ";
            row += ascii_to_wide(supported_control_names[index]);
            row += L"   [ " + binding_label(state.control_bindings[index]) + L" ]";
            if (state.capture_control == index) row += L"   PRESS A KEY";
            write(row, 100, 158 + static_cast<int>(index) * 25, 24);
        }
        SetTextColor(dc, RGB(190, 205, 198));
        write(L"UP / DOWN: SELECT   ENTER: REMAP   BACKSPACE: UNBIND   R: RESET", 100, 492, 24);
        write(L"ESC: BACK TO RACE SETUP", 100, 518, 24);
        if (!state.settings_message.empty())
            write(ascii_to_wide(state.settings_message), 520, 518, 24);
    }
    if (state.career_screen) {
        SetTextColor(dc, RGB(118, 208, 155));
        write(L"CAREER SLOTS", 74, 120, 34);
        SetTextColor(dc, RGB(238, 244, 240));
        for (std::size_t index = 0; index < state.career_rows.size(); ++index) {
            std::wstring row = index == static_cast<std::size_t>(state.career_slot)
                ? L"> " : L"  ";
            row += L"SLOT " + std::to_wstring(index + 1U) + L"  ";
            row += ascii_to_wide(state.career_rows[index]);
            write(row, 100, 190 + static_cast<int>(index) * 44, 36);
        }
        SetTextColor(dc, RGB(190, 205, 198));
        if (state.career_delete_confirmation) {
            SetTextColor(dc, RGB(255, 194, 112));
            write(L"DELETE THIS SLOT?  Y: DELETE   N / ESC: CANCEL", 100, 430, 30);
        } else {
            write(L"UP / DOWN: SELECT   ENTER: VALIDATE   D: DELETE", 100, 430, 30);
            write(L"ESC: BACK", 100, 500, 30);
        }
        if (!state.career_message.empty())
            write(ascii_to_wide(state.career_message), 100, 462, 30);
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
            if (state->controls_screen) {
                if (state->capture_control) {
                    if (wparam == VK_ESCAPE) {
                        state->capture_control.reset();
                    } else if (wparam == VK_BACK) {
                        state->control_bindings[*state->capture_control].clear();
                        state->capture_control.reset();
                    } else if (const auto name = keyboard_binding_name(
                                   static_cast<UINT>(wparam)); name) {
                        state->control_bindings[*state->capture_control] = *name;
                        state->capture_control.reset();
                    }
                    if (!state->capture_control)
                        (void)persist_setup_preferences(*state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == VK_ESCAPE) {
                    if (persist_setup_preferences(*state))
                        state->controls_screen = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == 'R') {
                    state->control_bindings = state->default_control_bindings;
                    (void)persist_setup_preferences(*state);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == VK_UP) {
                    state->selected_control = state->selected_control == 0U
                        ? supported_control_names.size() - 1U
                        : state->selected_control - 1U;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == VK_DOWN) {
                    state->selected_control = (state->selected_control + 1U) %
                        supported_control_names.size();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wparam == VK_RETURN) {
                    state->capture_control = state->selected_control;
                    state->settings_message.clear();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                return 0;
            }
            if (state->career_screen) {
                if (state->career_delete_confirmation) {
                    if (wparam == 'Y') {
                        const auto deleted = viper::delete_career_slot(
                            state->config_directory, state->career_slot);
                        if (deleted.error.empty()) {
                            state->career_rows[static_cast<std::size_t>(state->career_slot)] =
                                "EMPTY OR INVALID";
                            state->career_message = deleted.deleted
                                ? "CAREER SLOT DELETED"
                                : "SLOT WAS ALREADY EMPTY";
                        } else {
                            state->career_message = "DELETE FAILED: " + deleted.error;
                        }
                        state->career_delete_confirmation = false;
                    } else if (wparam == 'N' || wparam == VK_ESCAPE) {
                        state->career_delete_confirmation = false;
                        state->career_message = "DELETE CANCELLED";
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
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
                if (wparam == 'D') {
                    state->career_delete_confirmation = true;
                    state->career_message = "CONFIRM DELETION OF SELECTED SLOT";
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                return 0;
            }
            if (wparam == 'K') {
                state->controls_screen = true;
                state->capture_control.reset();
                InvalidateRect(hwnd, nullptr, FALSE);
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
            else if (wparam == VK_PRIOR)
                state->laps = std::min(99U, state->laps + 1U);
            else if (wparam == VK_NEXT)
                state->laps = std::max(1U, state->laps - 1U);
            else if (wparam == VK_OEM_4)
                state->opponent_strength = (state->opponent_strength +
                    static_cast<std::uint32_t>(opponent_strength_names.size()) - 1U) %
                    static_cast<std::uint32_t>(opponent_strength_names.size());
            else if (wparam == VK_OEM_6)
                state->opponent_strength = (state->opponent_strength + 1U) %
                    static_cast<std::uint32_t>(opponent_strength_names.size());
            else if (wparam == VK_RETURN && !state->tracks.empty() && !state->cars.empty()) {
                if (state->tracks[state->track_index] == "random") {
                    state->tracks[state->track_index] =
                        resolve_random_track_choice(state->tracks);
                }
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
    const std::uint32_t initial_laps,
    const std::uint32_t initial_opponent_strength,
    const std::array<std::string, 13>& initial_control_bindings,
    const std::array<std::string, 13>& default_control_bindings,
    const std::filesystem::path& config_directory)
{
    if (tracks.empty() || cars.empty())
        throw std::runtime_error("frontend cannot show an empty track or car inventory");
    SetupWindowState state{};
    state.tracks = std::move(tracks);
    state.cars = std::move(cars);
    state.laps = std::clamp(initial_laps, 1U, 99U);
    state.opponent_strength = initial_opponent_strength < opponent_strength_names.size()
        ? initial_opponent_strength : 0U;
    state.config_directory = config_directory;
    state.control_bindings = initial_control_bindings;
    state.default_control_bindings = default_control_bindings;
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
        state.tracks[state.track_index], state.cars[state.car_index], state.laps,
        state.opponent_strength, state.control_bindings};
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
            car_placement_plans_.reserve(spawns.size());
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
                        : static_cast<std::uint32_t>(viper::TrackCarSubtype::acar);
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
                car_placement_plans_.push_back(placement);
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

    const viper::CarTrackConstructionPlan* car_placement_plan(
        const std::size_t car_index) const noexcept
    {
        return car_index < car_placement_plans_.size()
            ? &car_placement_plans_[car_index] : nullptr;
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
    std::vector<viper::CarTrackConstructionPlan> car_placement_plans_;
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
        constexpr auto esfx = viper::resource_type_code('E', 'S', 'F', 'X');
        const auto* setup_entry = car_bundle.find(stem + "e.ens", esfx);
        if (setup_entry == nullptr) return false;
        const auto setup = viper::parse_engine_sound_setup(
            car_bundle.payload(*setup_entry));
        if (!setup || setup->band_count == 0U) return false;
        declared_band_count_ = setup->band_count;
        if (!load_sound(car_bundle, stem + "i.sfx", std::nullopt)) return false;
        for (std::uint32_t band = 0U; band < setup->band_count; ++band) {
            const auto specific = stem + std::to_string(band) + ".sfx";
            if (!load_sound(car_bundle, specific, band, &setup->bands[band])) {
                const auto fallback = "engine" + std::to_string(band) + ".sfx";
                static_cast<void>(load_sound(car_bundle, fallback, band,
                    &setup->bands[band]));
            }
        }
        if (band_indices_.size() != declared_band_count_) return false;

        for (auto& sound : sounds_) {
            if (FAILED(sound.buffer->SetVolume(DSBVOLUME_MIN)) ||
                FAILED(sound.buffer->SetCurrentPosition(0U)) ||
                FAILED(sound.buffer->Play(0U, 0U, DSBPLAY_LOOPING))) {
                return false;
            }
        }
        static_cast<void>(sounds_[idle_index_].buffer->SetVolume(-850));
        initialized_ = true;
        return true;
    }

    void update(const float speed, const bool accelerating) noexcept
    {
        if (!initialized_ || sounds_.empty()) return;
        const auto absolute_speed = std::abs(speed);
        std::size_t desired = idle_index_;
        // Temporary player-only speed band selection. The ESFX record is
        // retained with each buffer, but its vehicle inputs remain unmapped.
        if (accelerating || absolute_speed > 0.5F) {
            const auto provisional_band = absolute_speed < 12.0F ? 0U :
                absolute_speed < 25.0F ? 1U : 2U;
            const auto band = std::min(provisional_band,
                declared_band_count_ == 0U ? 0U : declared_band_count_ - 1U);
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

    void set_paused(const bool paused) noexcept
    {
        if (!initialized_ || paused_ == paused) return;
        paused_ = paused;
        for (auto& sound : sounds_) {
            if (!sound.buffer) continue;
            if (paused_) {
                static_cast<void>(sound.buffer->Stop());
            } else {
                static_cast<void>(sound.buffer->Play(0U, 0U, DSBPLAY_LOOPING));
            }
        }
    }

    [[nodiscard]] std::size_t sound_count() const noexcept
    {
        return sounds_.size();
    }

    [[nodiscard]] std::size_t band_count() const noexcept
    {
        return band_indices_.size();
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
        std::optional<std::array<float, 7>> engine_parameters;
    };

    bool load_sound(const viper::ResourceBundle& bundle,
                    const std::string& name,
                    const std::optional<std::uint32_t> band,
                    const std::array<float, 7>* engine_parameters = nullptr)
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
        if (engine_parameters != nullptr) sound.engine_parameters = *engine_parameters;
        const auto index = sounds_.size();
        sounds_.push_back(std::move(sound));
        if (band) band_indices_[*band] = index;
        else idle_index_ = index;
        return true;
    }

    ComPtr<IDirectSound> device_;
    ComPtr<IDirectSoundBuffer> primary_;
    std::vector<Sound> sounds_;
    std::map<unsigned, std::size_t> band_indices_;
    std::size_t idle_index_{};
    std::uint32_t declared_band_count_{};
    bool initialized_{};
    bool paused_{};
};

// Persistent subset of the native car object that can be populated without
// inventing the unobserved integrator, force, collision, and tire fields.
// The source placement is row-major 3x3 plus translation at [9..11].
struct MaterializedNcarRuntimeState {
    std::array<float, 12> placement_transform{};
    // Shared 0x43E490 constructor bytes are persistent session-owned state.
    // The byte mask distinguishes native constructor writes from untouched
    // fields; it is not yet advanced by the fixed-step physics callback.
    viper::NcarBodyRuntimeObjectState body_object{};
    std::array<std::array<float, 3>, 4> initial_wheel_world_positions{};
    // The derived 0x42FF90 constructor writes 1.0 here after the base body
    // constructor. Keep it as constructor state; the later callback/ring that
    // can change this value is not yet connected to the session.
    std::optional<float> constructor_field_0x50c;
    // Shared initializer 0x433AA0 writes -100.0f to +0xE44. The timer's later
    // ordinal-record producer is still unavailable, so retain only this known
    // constructor value in the byte-backed object.
    std::optional<float> constructor_field_0xe44;
    std::array<std::optional<viper::NcarWheelConstructorPlan>, 4>
        wheel_constructor_plans{};
    std::array<viper::NcarWheelRuntimeObjectState, 4>
        wheel_objects{};
    bool constructor_outputs_materialized{};
};

struct AcarCurveSourceRuntimeState {
    viper::ConfiguredCarTaskStreamBinding stream_binding{};
    viper::AcarCurveSourceRecord source_record{};
};

struct WindowState;
void refresh_keyboard_controls(WindowState& state);

enum class PhysicsTickInputProvenance : std::uint8_t {
    merged_live_keyboard = 0U,
    logical_trace_actions = 1U,
};

// Immutable per-fixed-step capture of the values sampled at the outer update
// boundary. This records provenance for later physics integration without
// treating the current route prototype or terrain projections as body state.
struct PlayerPhysicsTickContext {
    PhysicsTickInputProvenance input_provenance{
        PhysicsTickInputProvenance::merged_live_keyboard};
    std::array<std::uint8_t, 256> keyboard_snapshot{};
    viper::RaceLoop43B290State controls{};
    viper::PcarCallbackGlobals pcar_callback_globals{};
    std::int32_t scheduler_clock_milliseconds{};
    std::uint32_t tick_index{};
    std::uint32_t scheduled_step_index{};
    std::uint32_t scheduled_step_count{};
    std::uint64_t outer_update_index{};
    float simulation_time_seconds{};
    bool body_kinematics_input_ready{};
    bool body_kinematics_non_finite{};
    bool body_kinematics_reader_inventory_mismatch{};
    std::size_t body_kinematics_missing_float_count{};
    std::array<std::uint32_t, 44> body_kinematics_missing_float_offsets{};
    bool body_integrator_timer_ramp_source_ready{};
    bool body_integrator_timer_known_fields_finite{};
    std::size_t body_integrator_timer_missing_count{};
    std::array<std::uint32_t, 3> body_integrator_timer_missing_offsets{};
    // Post-callback byte-backed body fields in this order: orientation basis,
    // position, linear/angular velocity, accumulated force/moment, then the
    // constructor timer sentinel. Unknown native bytes remain empty.
    std::array<std::optional<float>, 25> body_state_after_tick{};
    std::array<std::optional<std::uint8_t>, 4> wheel_contact_active_after_tick{};
    std::array<std::optional<float>, 4> wheel_contact_depth_after_tick{};
};

struct WindowState {
    DirectSoundAudio audio;
    D3DWindow graphics;
    const viper::TerrainSurfaceResourcePayload* terrain{};
    std::vector<std::array<float, 2>> route;
    std::size_t player_car_index{};
    std::vector<std::string> participant_names;
    viper::RouteMotionState motion{};
    std::vector<viper::RouteMotionState> opponent_motions;
    std::vector<viper::RouteOpponentAiState> opponent_ai_states;
    std::vector<viper::RouteOpponentAiDecision> last_opponent_ai_decisions;
    std::vector<std::array<float, 4>> pose_buffer;
    // Setup- and track-placement-derived PCAR record inputs retained for the
    // session. The complete ordered stream is validated and the recovered
    // 16 ms scheduler drives the route prototype; native task dispatch, the
    // interpolation ring, and body integration remain disconnected.
    std::vector<std::byte> player_runtime_car_record;
    std::vector<viper::ConfiguredCarTaskRecordInput> configured_car_task_records;
    std::optional<viper::PhysicsTaskRecordStreamSession> player_task_record_stream;
    std::optional<viper::PhysicsTaskRecordStreamSession> full_task_record_stream;
    std::optional<viper::PhysicsTaskRecordStreamRead> full_task_record_readback;
    std::vector<viper::PhysicsTaskObjectInventoryEntry> physics_task_object_inventory;
    std::optional<viper::PhysicsTaskRuntimeCollection> physics_task_runtime_collection;
    // Decoded replay data is retained for the lifetime of the race session.
    // Its packet payload is intentionally opaque until native field semantics
    // are recovered; this does not claim to drive playback.
    std::optional<viper::ReplayFile> loaded_replay;
    std::vector<viper::ConfiguredCarTaskStreamBinding> configured_car_stream_bindings;
    std::vector<AcarCurveSourceRuntimeState> ai_acar_curve_sources;
    std::optional<viper::PlayerPcarTaskBinding> player_pcar_task_binding;
    std::vector<viper::PhysicsStatTaskRuntimeState> inactive_stat_task_states;
    std::optional<viper::NcarBodyConstructorPlan> player_body_constructor_plan;
    std::optional<float> player_body_inverse_mass_0x200;
    std::optional<viper::NcarFourWheelCoreInitialization> player_wheel_core_initialization;
    MaterializedNcarRuntimeState player_runtime_state{};
    std::optional<viper::NcarWheelVertexAnchorResult> player_wheel_anchors;
    std::array<std::optional<viper::NcarWheelTireSetup>, 4> player_wheel_tire_setups{};
    // Per-wheel terrain observations transform constructor wheel anchors by
    // the initialized PCAR body pose. The body remains static in this runtime;
    // route motion still supplies the rendered pose.
    std::array<viper::WheelContactState, 4> player_wheel_contact_states{};
    std::array<viper::WheelContactObservation, 4> player_wheel_contact_observations{};
    std::array<viper::WheelContactResult, 4> player_wheel_contact_results{};
    std::array<bool, 4> player_wheel_terrain_queries_succeeded{};
    std::optional<std::array<float, 4>> player_simulated_pose;
    std::vector<std::optional<std::array<float, 4>>> opponent_simulated_poses;
    std::optional<viper::TrackPhysicsTaskManifest> physics_task_manifest;
    bool player_constructor_inputs_ready{};
    bool accelerate{};
    bool brake{};
    bool left{};
    bool right{};
    bool playback_active{};
    bool paused{};
    bool return_to_frontend{};
    viper::PhysicsStepAccumulator physics_step_accumulator{};
    viper::PhysicsStepSchedule last_physics_step_schedule{};
    std::vector<PlayerPhysicsTickContext> player_physics_ticks_this_update;
    std::uint64_t player_outer_update_count{};
    bool physics_step_clock_initialized{};
    std::uint32_t physics_step_count{};
    std::uint32_t pcar_callback_step_count{};
    bool pcar_timestamp_backend_callback_requested{};
    std::uint32_t playback_clock_milliseconds{};
    // The native binding evaluator consumes a post-poll device snapshot. The
    // DirectInput service supplies the original 256-byte keyboard snapshot;
    // Win32 key events remain a fallback and support deterministic UI smokes.
    viper::DirectInputKeyboardService directinput_keyboard;
    std::array<std::uint8_t, 256> keyboard_button_states{};
    std::array<std::uint8_t, 256> sampled_control_input_states{};
    std::array<std::uint8_t, 256> window_message_button_states{};
    bool directinput_keyboard_state_read{};
    bool directinput_up_pressed{};
    std::array<viper::RaceInputBinding, viper::race_loop_binding_slot_count> control_bindings{{
        {1U, 0xcbU}, {1U, 0xcdU}, {1U, 0xc8U}, {1U, 0xd0U}}};
    viper::RaceLoop43B290State control_update_state{};
    viper::PcarCallbackGlobals pcar_callback_globals{};
    viper::PcarScalarFields player_pcar_control_prefix{};
    viper::PcarCallbackControlResult player_pcar_callback_result{};
    viper::PcarConstructorControls player_pcar_constructor_controls{};
    float control_steering_smoothing{1.0F};
    float control_steering_range{1.0F};
    float control_steering_speed{1.0F};
    float control_throttle_range{1.0F};
    float control_throttle_sensitivity{1.0F};
    float control_throttle_speed{1.0F};
    float control_brake_range{1.0F};
    float control_brake_sensitivity{1.0F};
    float control_brake_speed{1.0F};
    bool control_game_pad{};
    bool control_squared_steering_gate_0x5093fc{};
    float game_throttle_boost_0x4ecc60{1.0F};
    viper::RaceLoop43B290Plan last_control_update{};
    HWND window{};
    std::wstring title_base;
    std::uint32_t target_laps{1U};
    ULONGLONG last_title_update{};
    ULONGLONG last_tick{};

    bool prepare_player_constructor_record(
        const viper::CarFileResourcePayloads& car_resources,
        const std::span<const std::byte> player_setup,
        const std::optional<std::uint32_t> serialized_stream_offset,
        const viper::CarTrackConstructionPlan& construction,
        const std::string_view model_name)
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
        if (!serialized_stream_offset.has_value()) return false;
        const auto name_plan = viper::plan_car_file_model_name(model_name);
        if (!name_plan.input_complete || name_plan.local_stack_copy_overflow ||
            name_plan.runtime_record_copy_overflow) return false;
        player_runtime_car_record.assign(
            viper::CarFileRuntimeMapping::runtime_record_size, std::byte{0});
        if (!viper::apply_car_track_runtime_record(player_runtime_car_record,
                preparation, name_plan, construction)) {
            player_runtime_car_record.clear();
            return false;
        }
        player_task_record_stream.emplace(*serialized_stream_offset);
        const auto appended = player_task_record_stream->append_record(
            player_runtime_car_record);
        if (appended.status !=
                viper::PhysicsTaskRecordSerializationStatus::complete ||
            appended.records.size() != 1U) {
            player_runtime_car_record.clear();
            player_task_record_stream.reset();
            return false;
        }
        const auto player_task_record_frame = player_task_record_stream->snapshot();
        const auto decoded_task_stream = viper::read_physics_task_record_stream(
            player_task_record_frame.bytes);
        if (decoded_task_stream.status !=
                viper::PhysicsTaskRecordStreamReadStatus::complete ||
            decoded_task_stream.records.size() != 1U ||
            decoded_task_stream.records[0].record_tag != 0x50434152U ||
            decoded_task_stream.records[0].cursor_offset != *serialized_stream_offset ||
            decoded_task_stream.records[0].record_size != player_runtime_car_record.size() ||
            decoded_task_stream.records[0].record_bytes_offset >
            player_task_record_frame.bytes.size() ||
            player_runtime_car_record.size() > player_task_record_frame.bytes.size() -
                decoded_task_stream.records[0].record_bytes_offset ||
            !std::equal(player_runtime_car_record.begin(), player_runtime_car_record.end(),
                player_task_record_frame.bytes.begin() + static_cast<std::ptrdiff_t>(
                    decoded_task_stream.records[0].record_bytes_offset))) {
            player_runtime_car_record.clear();
            player_task_record_stream.reset();
            return false;
        }
        return true;
    }

    bool initialize_player_constructor_inputs(
        const std::span<const std::byte> serialized_player_record,
        const std::span<const viper::ModelVertex> model_vertices,
        const std::array<viper::TireDefinition, 4>& tire_definitions,
        const std::optional<std::uint32_t> serialized_stream_offset,
        const viper::CarTrackConstructionPlan& construction)
    {
        bool host_fdiv_compatibility = false;
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_ARM64)
#else
        // The x86 compatibility probe is not yet wired into this runtime.
        return false;
#endif
        if (!serialized_stream_offset.has_value() ||
            serialized_player_record.size() !=
                viper::CarFileRuntimeMapping::runtime_record_size) return false;
        player_runtime_car_record.assign(serialized_player_record.begin(),
            serialized_player_record.end());
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
        if (!viper::materialize_ncar_body_constructor(
                player_runtime_state.body_object, *player_body_constructor_plan)) {
            player_runtime_car_record.clear();
            player_body_constructor_plan.reset();
            return false;
        }
        const auto constructor_subobjects =
            viper::plan_ncar_constructor_subobject_defaults(
                read_u32_le(player_runtime_car_record.data() + 0x194U));
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object,
                constructor_subobjects.embedded_curve_selector_writes) ||
            !viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object,
                constructor_subobjects.source_record_identity_writes)) {
            player_runtime_car_record.clear();
            player_body_constructor_plan.reset();
            return false;
        }
        const auto callback_setup =
            viper::initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
                player_runtime_car_record);
        if (!callback_setup.has_value()) {
            player_runtime_car_record.clear();
            player_body_constructor_plan.reset();
            return false;
        }
        // 0x433AA0 initializes the smoother accumulator; 0x433C00 derives
        // +0xDCC from the mapped runtime record; the PCAR constructor defaults
        // +0xBE4 to zero. These are the inputs consumed by 0x4330F0.
        const std::array<viper::NcarObjectFieldWrite, 3> callback_control_defaults{{
            {0x500U, 4U, 0U},
            {0xdccU, 4U, std::bit_cast<std::uint32_t>(callback_setup->angle_0xdcc)},
            {0xbe4U, 1U, 0U},
        }};
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object, callback_control_defaults)) {
            player_runtime_car_record.clear();
            player_body_constructor_plan.reset();
            return false;
        }
        // The derived ACAR constructor configures its embedded curve and
        // selector from the same mapped runtime car record. Persist the
        // numeric fields consumed by the recovered PCAR callback branch.
        const auto curve_source = viper::read_car_curve_source_record(
            player_runtime_car_record);
        if (!curve_source.has_value()) return false;
        const auto configured_curve = viper::configure_acar_update_curve(
            0U, *curve_source, host_fdiv_compatibility);
        const auto runtime_word = [&](const std::uint32_t offset) {
            std::uint32_t value{};
            for (std::uint32_t byte = 0; byte < 4U; ++byte) {
                value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                    player_runtime_car_record[offset + byte])) << (byte * 8U);
            }
            return value;
        };
        const auto runtime_float = [&](const std::uint32_t offset) {
            return std::bit_cast<float>(runtime_word(offset));
        };
        std::array<viper::NcarObjectFieldWrite, 25> embedded_numeric_writes{};
        std::size_t embedded_write_count{};
        const auto write_embedded = [&](const std::uint32_t offset,
                                        const std::uint8_t width,
                                        const std::uint32_t bits) {
            embedded_numeric_writes[embedded_write_count++] = {offset, width, bits};
        };
        const auto write_curve_float = [&](const std::uint32_t offset,
                                           const float value) {
            write_embedded(offset, 4U, std::bit_cast<std::uint32_t>(value));
        };
        write_curve_float(0xc04U, configured_curve.value_0x08);
        write_curve_float(0xc08U, configured_curve.reciprocal_0x0c);
        write_curve_float(0xc0cU, configured_curve.derived_value_0x30);
        write_curve_float(0xc10U, configured_curve.scoring.subtract_scale_0x14);
        write_curve_float(0xc30U, configured_curve.scoring.gate_value_0x34);
        write_curve_float(0xc34U, configured_curve.scoring.blend_value_0x38);
        write_curve_float(0xc38U, configured_curve.scoring.cutoff_0x3c);
        write_embedded(0xc3cU, 1U, configured_curve.scoring.mode_0x40);
        write_embedded(0xc3dU, 1U, configured_curve.scoring.mode_0x41);
        write_curve_float(0xc40U, configured_curve.scoring.quadratic_0x44);
        write_curve_float(0xc44U, configured_curve.scoring.linear_0x48);
        write_curve_float(0xc48U, configured_curve.scoring.intercept_0x4c);
        write_curve_float(0xc4cU, configured_curve.scoring.coefficient_0x50);
        write_curve_float(0xc50U, configured_curve.scoring.offset_0x54);
        write_curve_float(0xc54U, configured_curve.scoring.penalty_0x58);

        constexpr float selector_epsilon = std::bit_cast<float>(0x34000000U);
        constexpr float selector_scale = std::bit_cast<float>(0x3facfe73U);
        constexpr float selector_denominator = std::bit_cast<float>(0x3e587910U);
        const auto raw_profile = runtime_float(0xc8U);
        const auto selector_profile = std::fabs(raw_profile) > selector_epsilon
            ? raw_profile * selector_scale
            : std::bit_cast<float>(0x3b03126fU);
        write_curve_float(0xca0U, selector_denominator);
        write_curve_float(0xca4U, 1.0F / selector_denominator);
        write_curve_float(0xcacU, selector_profile);
        std::uint32_t selector_count{};
        for (; selector_count < 7U; ++selector_count) {
            if (!(runtime_float(0x94U + selector_count * 4U) > 0.01F)) break;
        }
        write_embedded(0xcb8U, 4U, selector_count);
        for (std::uint32_t index = 0; index < selector_count; ++index) {
            write_embedded(0xcc4U + index * 4U, 4U,
                runtime_word(0x94U + index * 4U));
        }
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object,
                std::span<const viper::NcarObjectFieldWrite>(
                    embedded_numeric_writes.data(), embedded_write_count))) {
            player_runtime_car_record.clear();
            player_body_constructor_plan.reset();
            return false;
        }
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
        // 0x42FF90 writes +0x50C = 1.0 after calling 0x43E490. Its four
        // wheel objects occupy +0x554 + 0x1A4*i and are initialized by
        // 0x4419A0. Retain those exact constructor projections in the player
        // session without treating them as a live force/integration state.
        player_runtime_state.constructor_field_0x50c = 1.0F;
        const std::array<viper::NcarObjectFieldWrite, 2> derived_body_writes{{
            {0x50cU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
            {0x000U, 4U, 0x004c8188U},
        }};
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object, derived_body_writes)) {
            player_runtime_car_record.clear();
            player_runtime_state = {};
            return false;
        }
        if (!player_pcar_constructor_controls.game_mode_0x4f4.has_value() ||
            !player_pcar_constructor_controls.value_0xd74.has_value() ||
            !player_pcar_constructor_controls.wheel_helper_value_0x6b4.has_value()) {
            player_runtime_car_record.clear();
            player_runtime_state = {};
            return false;
        }
        const std::array<viper::NcarObjectFieldWrite, 6> pcar_control_writes{{
            {0xec8U, 1U, static_cast<std::uint8_t>(
                player_pcar_constructor_controls.auto_shifting)},
            {0xec9U, 1U, static_cast<std::uint8_t>(
                player_pcar_constructor_controls.auto_clutch)},
            {0x534U, 1U, static_cast<std::uint8_t>(
                player_pcar_constructor_controls.auto_clutch_0x534)},
            {0x4f4U, 4U, *player_pcar_constructor_controls.game_mode_0x4f4},
            {0x4f8U, 4U, std::bit_cast<std::uint32_t>(
                player_pcar_constructor_controls.yaw_control)},
            {0xd74U, 1U, *player_pcar_constructor_controls.value_0xd74},
        }};
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object, pcar_control_writes)) {
            player_runtime_car_record.clear();
            player_runtime_state = {};
            return false;
        }
        for (std::size_t wheel = 0;
             wheel < player_runtime_state.wheel_constructor_plans.size(); ++wheel) {
            const auto wheel_offset = static_cast<std::uint32_t>(
                0x554U + 0x1a4U * static_cast<std::uint32_t>(wheel));
            auto plan = viper::plan_ncar_wheel_constructor(
                wheel_offset, viper::NcarWheelConstructorObservation{});
            if (!plan.unknown_fill_offsets.empty()) {
                player_runtime_car_record.clear();
                player_runtime_state = {};
                return false;
            }
            if (!viper::materialize_ncar_wheel_constructor(
                    player_runtime_state.wheel_objects[wheel], wheel_offset, plan)) {
                player_runtime_car_record.clear();
                player_runtime_state = {};
                return false;
            }
            const auto& core = player_wheel_core_initialization->wheels[wheel];
            if (!core.has_value()) {
                player_runtime_car_record.clear();
                player_runtime_state = {};
                return false;
            }
            if (!viper::apply_ncar_wheel_core_initialization(
                    player_runtime_state.wheel_objects[wheel], wheel_offset, *core)) {
                player_runtime_car_record.clear();
                player_runtime_state = {};
                return false;
            }
            const std::array<viper::NcarObjectFieldWrite, 3> pcar_wheel_control_writes{{
                {wheel_offset + 0x160U, 1U,
                    *player_pcar_constructor_controls.wheel_helper_value_0x6b4},
                {wheel_offset + 0x15cU, 4U,
                    player_pcar_constructor_controls.wheel_traction_0x6b0[wheel]},
                {wheel_offset + 0x164U, 4U,
                    player_pcar_constructor_controls.wheel_abs_0x6b8[wheel]},
            }};
            if (!viper::apply_ncar_wheel_object_writes(
                    player_runtime_state.wheel_objects[wheel], wheel_offset,
                    pcar_wheel_control_writes) ||
                !viper::apply_ncar_body_object_writes(
                    player_runtime_state.body_object, pcar_wheel_control_writes)) {
                player_runtime_car_record.clear();
                player_runtime_state = {};
                return false;
            }
            player_runtime_state.wheel_constructor_plans[wheel] = std::move(plan);
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
        player_runtime_state.placement_transform = construction.transform;
        if (!std::all_of(player_runtime_state.placement_transform.begin(),
                player_runtime_state.placement_transform.end(),
                [](const float value) { return std::isfinite(value); })) {
            player_runtime_state = {};
            return false;
        }
        // PCAR constructor helper 0x433C00 copies runtime-record +0x18..+0x44
        // verbatim into body +0x38..+0x64 (basis followed by world position).
        // Keep the same transform in the byte-backed body owner instead of
        // leaving only the scene/render-side copy initialized.
        if (!viper::copy_ncar_runtime_record_transform_to_body(
                player_runtime_state.body_object,
                player_runtime_state.placement_transform)) {
            player_runtime_state = {};
            return false;
        }
        // PCAR constructor 0x42FF90 calls 0x433AA0 after the placement
        // handoff. Reuse the initializer's evidence-backed direct-write
        // prefix, which includes the exact +0xE44 timestamp sentinel.
        const auto pcar_initializer = viper::plan_ncar_object_initialization({});
        const auto e44_initializer = std::find_if(
            pcar_initializer.writes_before_backend_refresh.begin(),
            pcar_initializer.writes_before_backend_refresh.end(),
            [](const viper::NcarObjectFieldWrite& write) {
                return write.offset == 0xe44U && write.width_bytes == 4U;
            });
        if (e44_initializer ==
                pcar_initializer.writes_before_backend_refresh.end() ||
            !viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object,
                pcar_initializer.writes_before_backend_refresh)) {
            player_runtime_state = {};
            return false;
        }
        player_runtime_state.constructor_field_0xe44 =
            std::bit_cast<float>(e44_initializer->value_bits);
        for (std::size_t wheel = 0; wheel < player_runtime_state
                .initial_wheel_world_positions.size(); ++wheel) {
            const auto& core = player_wheel_core_initialization->wheels[wheel];
            if (!core.has_value()) {
                player_runtime_state = {};
                return false;
            }
            // 0x441C10 stores wheel position relative to its selected vertex;
            // the parent transform then maps the reconstructed local point.
            const std::array<float, 3> local{
                core->position_relative_to_selected_vertex[0] +
                    core->selected_model_vertex[0],
                core->position_relative_to_selected_vertex[1] +
                    core->selected_model_vertex[1],
                core->position_relative_to_selected_vertex[2] +
                    core->selected_model_vertex[2]};
            const auto& t = player_runtime_state.placement_transform;
            auto& world = player_runtime_state.initial_wheel_world_positions[wheel];
            world = {
                t[0] * local[0] + t[3] * local[1] + t[6] * local[2] + t[9],
                t[1] * local[0] + t[4] * local[1] + t[7] * local[2] + t[10],
                t[2] * local[0] + t[5] * local[1] + t[8] * local[2] + t[11]};
            if (!std::all_of(world.begin(), world.end(),
                    [](const float value) { return std::isfinite(value); })) {
                player_runtime_state = {};
                return false;
            }
        }
        player_runtime_state.constructor_outputs_materialized = true;
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
        player_constructor_inputs_ready =
            player_runtime_state.constructor_outputs_materialized;
        return true;
    }

    std::size_t prepare_ai_runtime_car_records(
        const viper::SingleRaceAISetupPlan* const setups,
        const viper::CarFileResourcePayloads& ai_car_resources,
        const std::span<const std::uint32_t> configured_car_record_tags)
    {
        if (setups == nullptr) return 0U;
        std::size_t prepared_count = 0U;
        for (const auto& setup : setups->ai_setups) {
            const auto car_index = setup.entrant_index;
            if (!setup.input_complete || car_index >= configured_car_record_tags.size() ||
                configured_car_record_tags[car_index] != 0x41434152U) continue;
            const auto* construction = graphics.car_placement_plan(car_index);
            if (construction == nullptr || construction->record_tag != 0x41434152U ||
                construction->model_name != "viper") continue;

            std::array<std::byte, 0x8cU> setup_payload{};
            for (std::size_t word = 0; word < setup.setup.setup_words.size(); ++word) {
                const auto value = setup.setup.setup_words[word];
                for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
                    setup_payload[word * sizeof(value) + byte] =
                        static_cast<std::byte>((value >> (byte * 8U)) & 0xffU);
                }
            }
            const auto preparation =
                viper::prepare_car_file_runtime_mapping_from_setup_payload(
                    ai_car_resources, setup_payload, {}, false, false);
            if (!preparation.input_complete || !preparation.mapping.has_value())
                continue;
            const auto name_plan = viper::plan_car_file_model_name("viper");
            if (!name_plan.input_complete || name_plan.local_stack_copy_overflow ||
                name_plan.runtime_record_copy_overflow) continue;
            std::vector<std::byte> runtime_record(
                viper::CarFileRuntimeMapping::runtime_record_size, std::byte{0});
            if (!viper::apply_car_track_runtime_record(runtime_record, preparation,
                    name_plan, *construction) || runtime_record.size() != 0x1e4U ||
                read_u32_le(runtime_record.data()) != 0x41434152U ||
                read_u32_le(runtime_record.data() + 4U) != runtime_record.size())
                continue;
            configured_car_task_records.push_back({car_index,
                std::move(runtime_record)});
            ++prepared_count;
        }
        return prepared_count;
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

    void update_player_wheel_contacts_from_body()
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
        const auto read_body_float = [this](const std::uint32_t offset)
            -> std::optional<float> {
            const auto bits = viper::read_ncar_body_object_u32(
                player_runtime_state.body_object, offset);
            if (!bits.has_value()) return std::nullopt;
            return std::bit_cast<float>(*bits);
        };
        std::array<float, 9> body_basis{};
        for (std::size_t index = 0U; index < body_basis.size(); ++index) {
            const auto value = read_body_float(0x38U +
                static_cast<std::uint32_t>(index * sizeof(float)));
            if (!value) {
                for (auto& state : player_wheel_contact_states)
                    state.contact_active_0x168 = 0U;
                return;
            }
            body_basis[index] = *value;
        }
        const auto body_x = read_body_float(0x5cU);
        const auto body_y = read_body_float(0x60U);
        const auto body_z = read_body_float(0x64U);
        if (!body_x || !body_y || !body_z) {
            for (auto& state : player_wheel_contact_states)
                state.contact_active_0x168 = 0U;
            return;
        }
        const viper::PhysicsVector3 body_position{*body_x, *body_y, *body_z};
        const viper::PhysicsVector3 body_up{
            body_basis[3], body_basis[4], body_basis[5]};
        for (std::size_t wheel = 0; wheel < player_wheel_contact_states.size(); ++wheel) {
            const auto& core = player_wheel_core_initialization->wheels[wheel];
            if (!core.has_value()) {
                player_wheel_contact_states[wheel].contact_active_0x168 = 0U;
                continue;
            }

            // The selected vertex is subtracted by native 0x441C10 when it
            // stores +0x04..+0x0C, then added back by 0x437BA0. Combine those
            // terms to recover the local anchor before applying the initialized
            // body basis and position.
            const viper::PhysicsVector3 local_anchor{
                core->position_relative_to_selected_vertex[0] +
                    core->selected_model_vertex[0],
                core->position_relative_to_selected_vertex[1] +
                    core->selected_model_vertex[1],
                core->position_relative_to_selected_vertex[2] +
                    core->selected_model_vertex[2]};
            const auto wheel_world_point = viper::transform_body_local_point({
                local_anchor, body_basis, body_position});

            // This terrain projection now starts from the PCAR's initialized
            // byte-backed pose and full basis. It remains a terrain sample,
            // not the native collision-world query or a force update.
            viper::WheelContactObservation observation{};
            observation.body_query_enabled = true;
            observation.reset_feature_hook_active = false;
            observation.contact_projection_origin = wheel_world_point;
            observation.contact_direction = body_up;
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

            const auto wheel_object_offset = static_cast<std::uint32_t>(
                0x554U + 0x1a4U * static_cast<std::uint32_t>(wheel));
            std::array<viper::NcarObjectFieldWrite, 5> writes{};
            std::size_t write_count = 0U;
            const auto& contact_state = player_wheel_contact_states[wheel];
            writes[write_count++] = {wheel_object_offset + 0x19cU, 4U,
                std::bit_cast<std::uint32_t>(contact_state.contact_feature_0x19c)};
            writes[write_count++] = {wheel_object_offset + 0x168U, 1U,
                contact_state.contact_active_0x168};
            if (player_wheel_contact_results[wheel].reached_contact_callback_0x443f00) {
                writes[write_count++] = {wheel_object_offset + 0x130U, 4U,
                    std::bit_cast<std::uint32_t>(contact_state.contact_point_0x130.x)};
                writes[write_count++] = {wheel_object_offset + 0x134U, 4U,
                    std::bit_cast<std::uint32_t>(contact_state.contact_point_0x130.y)};
                writes[write_count++] = {wheel_object_offset + 0x138U, 4U,
                    std::bit_cast<std::uint32_t>(contact_state.contact_point_0x130.z)};
            }
            if (!viper::apply_ncar_wheel_object_writes(
                    player_runtime_state.wheel_objects[wheel], wheel_object_offset,
                    std::span<const viper::NcarObjectFieldWrite>(writes.data(),
                        write_count))) {
                player_constructor_inputs_ready = false;
                return;
            }
        }
    }

    void capture_player_tick_physics_state(PlayerPhysicsTickContext& tick) const
    {
        const auto kinematics_diagnostic =
            viper::read_ncar_body_integration_kinematics_input_diagnostic(
                player_runtime_state.body_object);
        tick.body_kinematics_input_ready =
            kinematics_diagnostic.input.has_value();
        tick.body_kinematics_non_finite =
            kinematics_diagnostic.contains_non_finite_value;
        tick.body_kinematics_reader_inventory_mismatch =
            kinematics_diagnostic.reader_inventory_mismatch;
        tick.body_kinematics_missing_float_count =
            kinematics_diagnostic.missing_float_count;
        tick.body_kinematics_missing_float_offsets =
            kinematics_diagnostic.missing_float_offsets;
        tick.body_integrator_timer_ramp_source_ready =
            kinematics_diagnostic.integrator_timer_ramp_source_ready;
        tick.body_integrator_timer_known_fields_finite =
            kinematics_diagnostic.integrator_timer_known_fields_finite;
        tick.body_integrator_timer_missing_count =
            kinematics_diagnostic.missing_integrator_timer_count;
        tick.body_integrator_timer_missing_offsets =
            kinematics_diagnostic.missing_integrator_timer_offsets;
        constexpr std::array<std::uint32_t, 25> offsets{
            0x38U, 0x3cU, 0x40U, 0x44U, 0x48U, 0x4cU,
            0x50U, 0x54U, 0x58U, 0x5cU, 0x60U, 0x64U,
            0x234U, 0x238U, 0x23cU, 0x240U, 0x244U, 0x248U,
            0x24cU, 0x250U, 0x254U, 0x258U, 0x25cU, 0x260U,
            0xe44U};
        for (std::size_t index = 0U; index < offsets.size(); ++index) {
            const auto bits = viper::read_ncar_body_object_u32(
                player_runtime_state.body_object, offsets[index]);
            if (bits.has_value())
                tick.body_state_after_tick[index] = std::bit_cast<float>(*bits);
        }
        for (std::size_t wheel = 0U; wheel < player_wheel_contact_states.size();
             ++wheel) {
            tick.wheel_contact_active_after_tick[wheel] =
                player_wheel_contact_states[wheel].contact_active_0x168;
            if (player_wheel_terrain_queries_succeeded[wheel]) {
                tick.wheel_contact_depth_after_tick[wheel] =
                    player_wheel_contact_results[wheel].projected_depth;
            }
        }
    }

    bool initialize_opponents()
    {
        if (route.size() < 2U || terrain == nullptr) return false;
        physics_step_accumulator = {};
        last_physics_step_schedule = {};
        physics_step_clock_initialized = false;
        physics_step_count = 0U;
        pcar_callback_step_count = 0U;
        playback_clock_milliseconds = 0U;
        player_simulated_pose.reset();
        opponent_motions.resize(graphics.car_count());
        opponent_ai_states.assign(graphics.car_count(), {});
        last_opponent_ai_decisions.assign(graphics.car_count(), {});
        opponent_simulated_poses.resize(graphics.car_count());
        std::fill(opponent_simulated_poses.begin(), opponent_simulated_poses.end(),
            std::nullopt);
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
            // AI starts from the same stationary grid assumption as the
            // player. The route policy supplies runtime-owned throttle/brake
            // decisions instead of an entrant-index speed seed.
            ai.speed = 0.0F;
            viper::initialize_race_progression(ai.race, ai.route_segment);
        }
        return true;
    }

    void set_paused(const bool value) noexcept
    {
        if (paused == value) return;
        paused = value;
        keyboard_button_states.fill(0U);
        window_message_button_states.fill(0U);
        left = right = accelerate = brake = false;
        refresh_keyboard_controls(*this);
        audio.set_paused(paused);
        if (!paused) {
            // The wall-clock frame and native fixed-step scheduler both need
            // fresh anchors after a pause; otherwise the scheduler replays the
            // entire paused interval as catch-up work.
            last_tick = GetTickCount64();
            physics_step_clock_initialized = false;
            physics_step_accumulator = {};
        }
    }

    bool advance(float delta_seconds, const std::int32_t observed_clock_milliseconds,
                 const bool update_graphics = true,
                 const bool simulation_enabled = true)
    {
        if (route.size() < 2U || terrain == nullptr) return false;
        if (!simulation_enabled) {
            audio.set_paused(true);
            refresh_title();
            return true;
        }
        audio.set_paused(false);
        if (!update_native_control_state(delta_seconds)) return false;
        std::int32_t scheduler_clock = observed_clock_milliseconds;
        if (playback_active) {
            const auto elapsed_milliseconds = static_cast<std::uint32_t>(
                std::max(0.0F, delta_seconds) * 1000.0F + 0.5F);
            playback_clock_milliseconds += elapsed_milliseconds;
            scheduler_clock = std::bit_cast<std::int32_t>(playback_clock_milliseconds);
        }
        if (!physics_step_clock_initialized) {
            physics_step_accumulator = {};
            physics_step_accumulator.clock_anchor_0x04 = scheduler_clock;
            physics_step_accumulator.recapture_anchor_0x09 = true;
            physics_step_clock_initialized = true;
        }
        viper::PhysicsStepSchedulerInput scheduler_input{};
        scheduler_input.scheduler_mode = 1;
        // Ordinary unpaused quick-race uses the elapsed-time scheduler path.
        // The human entrant selector is a pointer, so 0x49BE40 is false and
        // the recovered scheduler caps catch-up at two steps.
        scheduler_input.gate_0x508a40 = false;
        scheduler_input.gate_0x508760 = false;
        scheduler_input.cap_to_four = false;
        scheduler_input.simulation_tick_low_byte = static_cast<std::uint8_t>(
            physics_step_count);
        scheduler_input.clock_now = scheduler_clock;
        scheduler_input.task_simulation_time =
            static_cast<float>(physics_step_count) * 0.016F;
        const auto schedule = viper::schedule_physics_steps(
            physics_step_accumulator, scheduler_input);
        last_physics_step_schedule = schedule;
        player_physics_ticks_this_update.clear();
        last_opponent_ai_decisions.assign(opponent_motions.size(), {});
        const auto outer_update_index = player_outer_update_count++;
        bool lap_completed = false;
        for (std::int32_t step = 0; step < schedule.steps; ++step) {
            // Every opponent in this fixed step decides from the same traffic
            // snapshot. The player occupies its rendered entrant slot.
            auto route_participant_snapshot = opponent_motions;
            route_participant_snapshot[player_car_index] = motion;
            PlayerPhysicsTickContext tick_context{};
            tick_context.input_provenance = playback_active
                ? PhysicsTickInputProvenance::logical_trace_actions
                : PhysicsTickInputProvenance::merged_live_keyboard;
            tick_context.keyboard_snapshot = sampled_control_input_states;
            tick_context.controls = control_update_state;
            tick_context.pcar_callback_globals = pcar_callback_globals;
            tick_context.scheduler_clock_milliseconds = scheduler_clock;
            tick_context.tick_index = physics_step_count;
            tick_context.scheduled_step_index =
                static_cast<std::uint32_t>(step);
            tick_context.scheduled_step_count =
                static_cast<std::uint32_t>(schedule.steps);
            tick_context.outer_update_index = outer_update_index;
            // Native dispatch consumes the current timer before incrementing it.
            tick_context.simulation_time_seconds =
                static_cast<float>(physics_step_count) * 0.016F;
            player_physics_ticks_this_update.push_back(tick_context);
            update_player_wheel_contacts_from_body();
            if (!dispatch_player_pcar_callback_projection(tick_context)) return false;
            capture_player_tick_physics_state(
                player_physics_ticks_this_update.back());
            auto candidate_motion = motion;
            const auto route_result = viper::advance_route_motion(candidate_motion, route,
                tick_context.controls.steering.value_0x509358 < -0.2F,
                tick_context.controls.steering.value_0x509358 > 0.2F,
                tick_context.controls.analog_0x5093f4.value,
                tick_context.controls.analog_0x509320.value,
                0.016F, target_laps,
                tick_context.controls.controls.flag_0x5093f0 != 0U);
            if (!route_result.valid && !route_result.finished) return false;
            if (route_result.valid) {
                const auto surface = viper::sample_terrain_resource(
                    *terrain, {route_result.x, 1000.0f, route_result.z}, true);
                if (!surface.query_succeeded) return true;
                if (!viper::commit_route_motion_after_terrain_query(
                        motion, candidate_motion, surface.query_succeeded)) return true;
                player_simulated_pose = std::array<float, 4>{route_result.x,
                    surface.sampled_position[1], route_result.z, route_result.yaw};
            } else if (route_result.finished) {
                motion = candidate_motion;
            }
            lap_completed = lap_completed || route_result.lap_completed;

            for (std::size_t car = 0; car < opponent_motions.size(); ++car) {
                if (car == player_car_index) continue;
                auto& ai = opponent_motions[car];
                const auto decision = viper::decide_route_opponent_action(
                    opponent_ai_states[car], route, route_participant_snapshot, car);
                if (!decision.valid) continue;
                auto candidate_ai = ai;
                const auto ai_result = viper::advance_route_motion(candidate_ai, route,
                    decision.steer_left, decision.steer_right,
                    decision.throttle, decision.brake, 0.016F, target_laps);
                if (!ai_result.valid) continue;
                const auto ai_surface = viper::sample_terrain_resource(
                    *terrain, {ai_result.x, 1000.0f, ai_result.z}, true);
                if (!ai_surface.query_succeeded ||
                    !viper::commit_route_motion_after_terrain_query(
                        ai, candidate_ai, ai_surface.query_succeeded)) continue;
                opponent_ai_states[car] = decision.next_state;
                last_opponent_ai_decisions[car] = decision;
                opponent_simulated_poses[car] = std::array<float, 4>{ai_result.x,
                    ai_surface.sampled_position[1], ai_result.z, ai_result.yaw};
            }
            ++physics_step_count;
        }
        if (lap_completed) refresh_title(true);
        if (update_graphics) {
            for (std::size_t car = 0; car < graphics.car_count(); ++car) {
                const auto position = graphics.car_position(car);
                pose_buffer[car] = {position[0], graphics.car_height(car), position[1],
                    graphics.car_yaw(car)};
            }
            for (std::size_t car = 0; car < opponent_motions.size(); ++car) {
                if (car == player_car_index) continue;
                if (opponent_simulated_poses[car].has_value())
                    pose_buffer[car] = *opponent_simulated_poses[car];
            }
            if (player_simulated_pose.has_value())
                pose_buffer[player_car_index] = *player_simulated_pose;
        }
        if (update_graphics && !graphics.set_car_poses(pose_buffer)) return false;
        audio.update(motion.race.finished ? 0.0F : motion.speed,
            !motion.race.finished && accelerate);
        refresh_title();
        return true;
    }

    bool update_native_control_state(const float delta_seconds)
    {
        directinput_keyboard_state_read = false;
        directinput_up_pressed = false;
        if (!playback_active) {
            keyboard_button_states = window_message_button_states;
            viper::DirectInputKeyboardService::KeyboardState native_keyboard{};
            directinput_keyboard_state_read =
                directinput_keyboard.poll(native_keyboard);
            if (directinput_keyboard_state_read) {
                directinput_up_pressed =
                    (native_keyboard[0xc8U] & 0x80U) != 0U;
                for (std::size_t key = 0; key < keyboard_button_states.size(); ++key) {
                    if ((native_keyboard[key] & 0x80U) != 0U)
                        keyboard_button_states[key] = 1U;
                }
            }
            refresh_keyboard_controls(*this);
        }
        viper::RaceLoop43B290Observation observation{};
        observation.simulation_delta = delta_seconds;
        std::copy(control_bindings.begin(), control_bindings.end(),
            observation.bindings.begin());
        observation.force_ramped_path_0x5093a0 = control_game_pad;
        observation.steering_smoothing_0x509428 = control_steering_smoothing;
        observation.steering_gain_0x509390 = control_steering_range;
        observation.steering_response_rate_0x5093a8 = control_steering_speed;
        observation.analog_0x5093f4_scale_0x50934c = control_throttle_range;
        observation.analog_0x5093f4_bias_0x509370 = control_throttle_sensitivity;
        observation.analog_0x5093f4_rate_0x509350 = control_throttle_speed;
        observation.analog_0x509320_scale_0x509330 = control_brake_range;
        observation.analog_0x509320_bias_0x509380 = control_brake_sensitivity;
        observation.analog_0x509320_rate_0x509348 = control_brake_speed;

        auto input = viper::RaceInputBindingObservation{};
        if (playback_active) {
            // Trace files store logical actions. Give those actions isolated
            // synthetic selectors so duplicate user key bindings cannot
            // change a previously recorded schedule during playback.
            observation.bindings[0] = {1U, 0U};
            observation.bindings[1] = {1U, 1U};
            observation.bindings[2] = {1U, 2U};
            observation.bindings[3] = {1U, 3U};
            input.device_button_states[0] = static_cast<std::uint8_t>(left);
            input.device_button_states[1] = static_cast<std::uint8_t>(right);
            input.device_button_states[2] = static_cast<std::uint8_t>(accelerate);
            input.device_button_states[3] = static_cast<std::uint8_t>(brake);
        } else {
            input.device_button_states = keyboard_button_states;
        }
        sampled_control_input_states = input.device_button_states;
        observation.post_refresh_inputs = {input, input};
        last_control_update = viper::update_race_loop_43b290(
            control_update_state, observation);

        // The native PCAR callback begins by projecting these four shared
        // input globals into the object's scalar fields. The secondary
        // profile multiplier is currently held at the observed startup
        // default; its settings source is not part of this runtime yet.
        pcar_callback_globals.value_0x509320 =
            control_update_state.analog_0x509320.value;
        pcar_callback_globals.value_0x509344 =
            control_update_state.controls.difference_0x509344;
        pcar_callback_globals.value_0x509340 =
            control_update_state.controls.difference_0x509340;
        pcar_callback_globals.value_0x5093b8 =
            control_update_state.controls.value_0x5093b8;
        pcar_callback_globals.value_0x509358 =
            control_update_state.steering.value_0x509358;
        pcar_callback_globals.value_0x5093f4 =
            control_update_state.analog_0x5093f4.value;
        pcar_callback_globals.value_0x5093c8 =
            control_update_state.controls.value_0x5093c8;
        pcar_callback_globals.value_0x5093bc = static_cast<std::uint32_t>(
            control_update_state.controls.counter_0x5093bc);
        pcar_callback_globals.gate_0x509360 =
            control_update_state.controls.flag_0x509360 != 0U;
        pcar_callback_globals.gate_0x5093cc =
            control_update_state.controls.flag_0x5093cc != 0U;
        pcar_callback_globals.gate_0x5093f0 =
            control_update_state.controls.flag_0x5093f0 != 0U;
        pcar_callback_globals.gate_0x5093f8 =
            control_update_state.controls.flag_0x5093f8 != 0U;
        pcar_callback_globals.gate_0x509420 =
            control_update_state.controls.flag_0x509420 != 0U;
        pcar_callback_globals.gate_0x5093fc =
            control_squared_steering_gate_0x5093fc;
        return true;
    }

    bool update_player_pcar_callback(
        const PlayerPhysicsTickContext& tick_context)
    {
        const auto body_word = [&](const std::uint32_t offset) {
            return viper::read_ncar_body_object_u32(
                player_runtime_state.body_object, offset);
        };
        const auto body_float = [&](const std::uint32_t offset)
            -> std::optional<float> {
            const auto bits = body_word(offset);
            if (!bits) return std::nullopt;
            return std::bit_cast<float>(*bits);
        };
        const auto body_u8 = [&](const std::uint32_t offset)
            -> std::optional<std::uint8_t> {
            return viper::read_ncar_body_object_u8(
                player_runtime_state.body_object, offset);
        };
        viper::PcarCallbackControlObservation callback_observation{};
        callback_observation.globals = tick_context.pcar_callback_globals;
        callback_observation.multiplier.override_gate_0x4d10fc = true;
        callback_observation.multiplier.configured_multiplier_0x4ecc60 =
            game_throttle_boost_0x4ecc60;
        // The human race setup stores an entrant-list pointer in this global;
        // 0x49BE40 therefore returns false (the pointer is not integer one).
        callback_observation.multiplier.selector_is_one_0x49be40 = false;
        for (std::size_t axis = 0; axis < 3U; ++axis) {
            const auto velocity = body_float(0x234U +
                static_cast<std::uint32_t>(axis * 4U));
            const auto basis = body_float(0x50U +
                static_cast<std::uint32_t>(axis * 4U));
            if (!velocity || !basis) return false;
            callback_observation.body_velocity_0x234[axis] = *velocity;
            callback_observation.body_basis_0x50[axis] = *basis;
        }
        const auto smoother_value = body_float(0x500U);
        const auto smoother_multiplier = body_float(0xdccU);
        const auto smoother_mode = body_u8(0xbe4U);
        if (!smoother_value || !smoother_multiplier || !smoother_mode)
            return false;
        viper::PcarCallbackControlState callback_state{};
        callback_state.smoother.value_0x500 = *smoother_value;
        callback_state.smoother.multiplier_0xdcc = *smoother_multiplier;
        callback_state.smoother.mode_0xbe4 = *smoother_mode != 0U;
        callback_state.scalar_prefix = player_pcar_control_prefix;
        // The 0x5093FC square-magnitude gate has no session source yet and
        // therefore remains at its zero-initialized startup observation.
        player_pcar_callback_result = viper::advance_pcar_callback_control_prefix(
            callback_state, callback_observation);
        player_pcar_control_prefix = callback_state.scalar_prefix;
        std::array<viper::NcarObjectFieldWrite, 7> pcar_callback_writes{{
            {0x500U, 4U, std::bit_cast<std::uint32_t>(
                callback_state.smoother.value_0x500)},
            {0xde0U, 4U, std::bit_cast<std::uint32_t>(
                player_pcar_control_prefix.value_0xde0)},
            {0xde4U, 4U, std::bit_cast<std::uint32_t>(
                player_pcar_control_prefix.value_0xde4)},
            {0xde8U, 4U, std::bit_cast<std::uint32_t>(
                player_pcar_control_prefix.value_0xde8)},
            {0xdecU, 4U, std::bit_cast<std::uint32_t>(
                player_pcar_control_prefix.value_0xdec)},
        }};
        if (callback_state.smoother.mode_0xbe4) {
            pcar_callback_writes[5] = {0x978U, 4U,
                std::bit_cast<std::uint32_t>(callback_state.smoother.output_0x978)};
            pcar_callback_writes[6] = {0xb1cU, 4U,
                std::bit_cast<std::uint32_t>(callback_state.smoother.output_0xb1c)};
        } else {
            pcar_callback_writes[5] = {0x630U, 4U,
                std::bit_cast<std::uint32_t>(callback_state.smoother.output_0x630)};
            pcar_callback_writes[6] = {0x7d4U, 4U,
                std::bit_cast<std::uint32_t>(callback_state.smoother.output_0x7d4)};
        }
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object, pcar_callback_writes)) {
            return false;
        }

        const auto auto_shift = viper::read_ncar_body_object_u8(
            player_runtime_state.body_object, 0xec8U);
        const auto auto_clutch = viper::read_ncar_body_object_u8(
            player_runtime_state.body_object, 0xec9U);
        const auto branch_value_54 = body_float(0x54U);
        const auto branch_value_c7c = body_float(0xc7cU);
        const auto branch_cbc = body_word(0xcbcU);
        const auto branch_d64 = body_word(0xd64U);
        const auto branch_c34 = body_float(0xc34U);
        if (auto_shift && auto_clutch && branch_value_54 && branch_value_c7c &&
            branch_cbc && branch_d64 && branch_c34) {
            std::array<float, 7> selector_entries{};
            const auto selector_count_bits = body_word(0xcb8U);
            if (!selector_count_bits || *selector_count_bits > selector_entries.size())
                return false;
            bool selector_entries_complete = true;
            for (std::uint32_t index = 0; index < *selector_count_bits; ++index) {
                const auto entry = body_float(0xcc4U + index * 4U);
                if (!entry) {
                    selector_entries_complete = false;
                    break;
                }
                selector_entries[index] = *entry;
            }
            // 0x43F910's setup denominator is the child's +0x08 at parent
            // +0xCA0. 0x43FDD0's selection denominator is child +0x18,
            // parent +0xCB0; keep these distinct.
            const auto selector_denominator = body_float(0xcb0U);
            const auto selector_floor = body_word(0xcb4U);
            const auto selector_base = body_float(0xcc0U);
            const auto selector_policy = body_word(0xd00U);
            const std::array<std::optional<float>, 10> curve_values{{
                body_float(0xc10U), body_float(0xc30U), body_float(0xc34U),
                body_float(0xc38U), body_float(0xc40U), body_float(0xc44U),
                body_float(0xc48U), body_float(0xc4cU), body_float(0xc50U),
                body_float(0xc54U)}};
            const auto curve_mode_40 = viper::read_ncar_body_object_u8(
                player_runtime_state.body_object, 0xc3cU);
            const auto curve_mode_41 = viper::read_ncar_body_object_u8(
                player_runtime_state.body_object, 0xc3dU);
            if (selector_entries_complete && selector_denominator && selector_floor &&
                selector_base && selector_policy && curve_mode_40 && curve_mode_41 &&
                std::all_of(curve_values.begin(), curve_values.end(),
                    [](const auto& value) { return value.has_value(); })) {
                viper::PcarCallbackBranchFields branch{};
                branch.scalars = player_pcar_control_prefix;
                branch.value_0x54 = *branch_value_54;
                branch.value_0xec8 = *auto_shift;
                branch.value_0xec9 = *auto_clutch;
                branch.value_0xc7c = *branch_value_c7c;
                branch.value_0xcbc = *branch_cbc;
                branch.value_0xd64 = *branch_d64;
                branch.value_0xc34 = *branch_c34;
                const auto saved_flags = body_word(0x4U);
                if (!saved_flags) return false;
                branch.value_0x4 = *saved_flags;
                viper::PcarCallbackBranchObservation branch_observation{};
                branch_observation.globals = tick_context.pcar_callback_globals;
                branch_observation.selector_0xc98 = viper::EmbeddedSelectionFields{
                    *selector_denominator,
                    std::bit_cast<std::int32_t>(*selector_floor),
                    static_cast<int>(*selector_count_bits),
                    std::span<const float>(selector_entries.data(),
                        static_cast<std::size_t>(*selector_count_bits)),
                    std::bit_cast<std::int32_t>(*branch_cbc),
                    *selector_base,
                    *selector_policy != 0U};
                branch_observation.curve_0xbfc = viper::UpdateCurveFields{
                    *curve_values[0], *curve_values[1], *curve_values[2],
                    *curve_values[3], *curve_mode_40, *curve_mode_41,
                    *curve_values[4], *curve_values[5], *curve_values[6],
                    *curve_values[7], *curve_values[8], *curve_values[9]};
                const auto branch_result = viper::advance_pcar_callback_branch(
                    branch, branch_observation);
                if (!branch_result.complete) return false;
                std::array<viper::NcarObjectFieldWrite, 6> branch_writes{{
                    {0xcbcU, 4U, branch.value_0xcbc},
                    {0xd64U, 4U, branch.value_0xd64},
                    {0x54U, 4U, std::bit_cast<std::uint32_t>(branch.value_0x54)},
                    {0xdecU, 4U, std::bit_cast<std::uint32_t>(branch.scalars.value_0xdec)},
                    {0x4U, 4U, branch.value_0x4},
                    {0xde4U, 4U, std::bit_cast<std::uint32_t>(branch.scalars.value_0xde4)},
                }};
                if (!viper::apply_ncar_body_object_writes(
                        player_runtime_state.body_object, branch_writes)) return false;
                player_pcar_control_prefix = branch.scalars;
                if (branch_result.output_0x5093bc) {
                    pcar_callback_globals.value_0x5093bc =
                        *branch_result.output_0x5093bc;
                    control_update_state.controls.counter_0x5093bc =
                        std::bit_cast<std::int32_t>(*branch_result.output_0x5093bc);
                }
            }
        }

        // 0x425790 reads DAT_00508774 * 0.016. The recovered outer task
        // loop calls 0x421050 before incrementing DAT_00508774, so this
        // callback observes the current pre-increment fixed-step count.
        // Preserve the timestamp latch write; the requested backend vtable
        // +0x3C call still has no session-owned target/backend implementation.
        const auto timestamp_bits = body_word(0xeccU);
        pcar_timestamp_backend_callback_requested = false;
        // The native false-gate branch clears +0xECC without reading it.
        // Preserve that known store even while constructor state is unknown;
        // the enabled branch still requires the original latch value.
        if (timestamp_bits.has_value() ||
            !pcar_callback_globals.gate_0x509360) {
            const auto timestamp = viper::update_pcar_timestamp(
                timestamp_bits.value_or(0U),
                pcar_callback_globals.gate_0x509360,
                tick_context.simulation_time_seconds);
            pcar_timestamp_backend_callback_requested =
                timestamp.request_backend_callback;
            const viper::NcarObjectFieldWrite timestamp_write{
                0xeccU, 4U, timestamp.timestamp_bits_0xecc};
            if (!viper::apply_ncar_body_object_writes(
                    player_runtime_state.body_object,
                    std::span<const viper::NcarObjectFieldWrite>(
                        &timestamp_write, 1U))) return false;
        }

        // The 0x43867F linked callback always refreshes body +0xE28 from
        // gate global +0x5093F8. Target mutation is disabled in this model
        // until the native enable globals and ordinal target lookup are
        // session-owned; its unconditional cache store is still exact.
        auto linked_target_fields = viper::NcarLinkedTargetFields{};
        std::uint8_t linked_gate_cache{};
        const std::array<std::uint32_t, 12> unused_linked_state{};
        const std::array<float, 3> unused_linked_vector{};
        const auto linked_callback = viper::run_ncar_linked_callback(
            linked_target_fields, linked_gate_cache,
            static_cast<std::uint8_t>(pcar_callback_globals.gate_0x5093f8),
            false, 0, false, unused_linked_state, unused_linked_vector,
            unused_linked_state, tick_context.simulation_time_seconds, false);
        if (linked_callback.called_target_update) return false;
        const viper::NcarObjectFieldWrite linked_gate_write{
            0xe28U, 1U, linked_gate_cache};
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object,
                std::span<const viper::NcarObjectFieldWrite>(
                    &linked_gate_write, 1U))) return false;

        // 0x43869C calls 0x43B880 after the linked callback.
        // Its pushes map object +0xEB0, +0xEA8, +0xEAC to globals
        // +0x509384, +0x509388, +0x5093A4 respectively. Preserve the raw
        // DWORDs and only advance this global snapshot when every source byte
        // is owned by the session object.
        const auto callback_global_0x509384 = body_word(0xeb0U);
        const auto callback_global_0x509388 = body_word(0xea8U);
        const auto callback_global_0x5093a4 = body_word(0xeacU);
        if (callback_global_0x509384 && callback_global_0x509388 &&
            callback_global_0x5093a4) {
            viper::set_pcar_callback_globals_0x43b880(
                pcar_callback_globals, *callback_global_0x509384,
                *callback_global_0x509388, *callback_global_0x5093a4);
        }

        // Preserve the native shared-global getter stores that precede the
        // late callback tail: 0x43BA00 -> +0x4F0 (float), then 0x43BA20
        // -> +0x4EC (byte). The raw 0x43B880 global stores are applied above.
        const std::array<viper::NcarObjectFieldWrite, 2> callback_cache_writes{{
            {0x4f0U, 4U, std::bit_cast<std::uint32_t>(
                pcar_callback_globals.value_0x509344)},
            {0x4ecU, 1U, static_cast<std::uint32_t>(
                pcar_callback_globals.gate_0x509420)},
        }};
        if (!viper::apply_ncar_body_object_writes(
                player_runtime_state.body_object, callback_cache_writes)) {
            return false;
        }

        // These stores are the observed late PCAR callback tail. The
        // 0x434A80 target mutation remains disabled because its enable and
        // lookup inputs are not observed in the live session.
        const auto tail_c3c = body_u8(0xc3cU);
        const auto tail_c3d = body_u8(0xc3dU);
        const auto tail_c7c = body_float(0xc7cU);
        const auto tail_mode = body_word(0x4fcU);
        const auto tail_de0 = body_float(0xde0U);
        const auto tail_de4 = body_float(0xde4U);
        const auto tail_be4 = body_u8(0xbe4U);
        const auto tail_508 = body_float(0x508U);
        if (tail_c3c && tail_c3d && tail_c7c && tail_mode && tail_de0 &&
            tail_de4 && tail_be4 && tail_508) {
            viper::PcarCallbackTailFields tail{};
            tail.value_0xc3c = *tail_c3c;
            tail.gate_0xc3d = *tail_c3d;
            tail.curve_gate_0x41 = *tail_c3d;
            tail.value_0xc7c = *tail_c7c;
            tail.mode_0x4fc = *tail_mode;
            tail.value_0xde0 = *tail_de0;
            tail.value_0xde4 = *tail_de4;
            tail.mode_0xbe4 = *tail_be4 != 0U;
            tail.value_0x508 = *tail_508;
            const auto tail_result = viper::advance_pcar_callback_tail(
                tail, pcar_callback_globals.value_0x509340);
            std::array<viper::NcarObjectFieldWrite, 4> tail_writes{};
            std::size_t tail_write_count = 0U;
            if (tail_result.set_byte_0xc3c) {
                tail_writes[tail_write_count++] = {0xc3cU, 1U, tail.value_0xc3c};
            }
            if (tail_result.reset_0xde0) {
                tail_writes[tail_write_count++] = {0xde0U, 4U,
                    std::bit_cast<std::uint32_t>(tail.value_0xde0)};
            }
            if (tail_result.floored_0xde4) {
                tail_writes[tail_write_count++] = {0xde4U, 4U,
                    std::bit_cast<std::uint32_t>(tail.value_0xde4)};
            }
            if (tail_result.called_value_helper_0x434b20) {
                tail_writes[tail_write_count++] = {0x508U, 4U,
                    std::bit_cast<std::uint32_t>(tail.value_0x508)};
            }
            if (tail_write_count != 0U && !viper::apply_ncar_body_object_writes(
                    player_runtime_state.body_object,
                    std::span<const viper::NcarObjectFieldWrite>(
                        tail_writes.data(), tail_write_count))) return false;
        }

        // The current route motion model accepts digital inputs. Preserve the
        // recovered analog values in session state and trace; this threshold
        // is only the adapter into that provisional route API.
        left = control_update_state.steering.value_0x509358 < -0.2F;
        right = control_update_state.steering.value_0x509358 > 0.2F;
        accelerate = control_update_state.analog_0x5093f4.value > 0.2F;
        brake = control_update_state.analog_0x509320.value > 0.2F;
        return true;
    }

    bool dispatch_player_pcar_callback_projection(
        const PlayerPhysicsTickContext& tick_context)
    {
        if (!physics_task_runtime_collection.has_value() ||
            !player_pcar_task_binding.has_value()) return false;
        auto& entries = physics_task_runtime_collection->entries;
        const auto target_index =
            player_pcar_task_binding->stream_record_index;
        if (target_index >= entries.size()) return false;

        // The original task array walks records in stream order. The PCAR
        // vtable's +0x2C gate is a fixed true return, so project the known
        // part of its +0x08 callback at that entry. Other active task
        // callbacks remain explicitly unavailable and are not synthesized.
        for (std::size_t index = 0U; index <= target_index; ++index) {
            auto& entry = entries[index];
            if (entry.stream_record_index != index) return false;
            if (index != target_index) continue;
            if (entry.record_tag != 0x50434152U ||
                entry.stream_cursor !=
                    player_pcar_task_binding->serialized_stream_cursor ||
                entry.status !=
                    viper::PhysicsTaskRuntimeEntryStatus::
                        active_callback_unavailable ||
                entry.known_callback_projection_steps ==
                    std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
            if (!update_player_pcar_callback(tick_context)) return false;
            ++entry.known_callback_projection_steps;
            ++pcar_callback_step_count;
            return true;
        }
        return false;
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
                if (!standing.player) {
                    if (standing.participant_index < participant_names.size() &&
                        !participant_names[standing.participant_index].empty()) {
                        results += participant_names[standing.participant_index] + " ";
                    } else {
                        results += std::to_string(standing.participant_index + 1U) + " ";
                    }
                }
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
        if (paused) status += "\nPAUSED\nP: RESUME / ESC: EXIT";
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
    if (state.participant_names.size() >
            std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("participant count exceeds the native session field");
    }
    // 0x43B030 resets these CONTROL values to 1.0, then reads saved overrides.
    state.control_steering_smoothing =
        options.real("CONTROL", "steer_sensitivity").value_or(1.0F);
    state.control_steering_range =
        options.real("CONTROL", "steer_range").value_or(1.0F);
    state.control_steering_speed =
        options.real("CONTROL", "steer_speed").value_or(1.0F);
    state.control_throttle_range =
        options.real("CONTROL", "throttle_range").value_or(1.0F);
    state.control_throttle_sensitivity =
        options.real("CONTROL", "throttle_sensitivity").value_or(1.0F);
    state.control_throttle_speed =
        options.real("CONTROL", "throttle_speed").value_or(1.0F);
    state.control_brake_range =
        options.real("CONTROL", "brake_range").value_or(1.0F);
    state.control_brake_sensitivity =
        options.real("CONTROL", "brake_sensitivity").value_or(1.0F);
    state.control_brake_speed =
        options.real("CONTROL", "brake_speed").value_or(1.0F);
    state.control_game_pad =
        options.boolean("CONTROL", "game_pad").value_or(false);
    state.game_throttle_boost_0x4ecc60 =
        options.real("GAME", "throttle_boost").value_or(1.0F);
    state.control_squared_steering_gate_0x5093fc =
        options.boolean("CONTROL", "nonlinear_steering").value_or(false);

    constexpr std::array<std::pair<std::string_view, std::size_t>, 13> action_slots{{
        {"steer_left", 0U}, {"steer_right", 1U}, {"throttle", 2U},
        {"braking", 3U}, {"e_brake", 4U}, {"reverse", 5U},
        {"clutch", 6U}, {"airlift", 9U}, {"look_left", 10U},
        {"look_right", 11U}, {"look_back", 12U}, {"downshift", 13U},
        {"upshift", 14U}}};
    for (const auto& [action_name, slot] : action_slots) {
        const auto configured = options.get("CONTROL", action_name);
        if (configured) {
            const auto key = parse_keyboard_binding(*configured);
            if (!key) {
                state.control_bindings[slot] = {};
                continue;
            }
            const auto selector = directinput_key_selector(*key);
            if (!selector)
                throw std::runtime_error("could not map keyboard binding to a DirectInput scan code: " +
                    std::string(*configured));
            state.control_bindings[slot] = {1U, *selector};
        }
    }
    const auto native_steering_binding_kind = [](const std::uint32_t kind) {
        return kind == 1U || kind == 3U || kind == 4U || kind == 6U;
    };
    if (state.control_game_pad ||
        native_steering_binding_kind(state.control_bindings[0].kind) ||
        native_steering_binding_kind(state.control_bindings[1].kind)) {
        state.control_squared_steering_gate_0x5093fc = false;
    }
    const auto constructor_helper_0xd74 =
        viper::evaluate_pcar_constructor_helper_0x43ba90(
            state.control_game_pad, state.control_bindings[2].kind);
    state.player_pcar_constructor_controls =
        viper::initialize_pcar_constructor_controls(
            options.boolean("CONTROL", "traction_control"),
            options.boolean("CONTROL", "abs_braking"),
            options.boolean("CONTROL", "auto_shifting"),
            options.boolean("CONTROL", "auto_clutch"),
            options.integer("CONTROL", "yaw_control"),
            static_cast<std::uint32_t>(state.participant_names.size()),
            constructor_helper_0xd74);
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
    state.window_message_button_states[*selector] =
        static_cast<std::uint8_t>(pressed);
    state.keyboard_button_states[*selector] =
        static_cast<std::uint8_t>(pressed);
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
        if (wparam == 'P' && state && !state->playback_active &&
            !state->results_ready() && (lparam & (1LL << 30)) == 0) {
            state->set_paused(!state->paused);
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
            state->window_message_button_states.fill(0U);
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
               const std::optional<std::filesystem::path>& record_physics_tick_trace,
               const std::vector<TraceInputFrame>* playback_trace,
               const bool hold_results_for_frontend,
               const std::size_t player_car_index,
               const std::vector<std::string>& participant_names,
               const std::span<const std::uint32_t> configured_car_record_tags,
               const viper::SingleRaceAISetupPlan* const selected_ai_setups,
               const viper::CarFileResourcePayloads& ai_car_resources,
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
               const std::span<const viper::ResourceBundle* const> model_bundles,
               const bool force_default_placement,
               const viper::GameOptions& options,
               const viper::ReplayFile* const loaded_replay)
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
    if (loaded_replay) {
        state.loaded_replay = *loaded_replay;
        std::cout << "Retained validated native replay container: "
                  << state.loaded_replay->frame_count << " frames, timestamp/index "
                  << state.loaded_replay->timestamp_or_index << ", "
                  << state.loaded_replay->payload.size()
                  << " opaque payload bytes. Playback is not enabled.\n";
    }
    state.terrain = &terrain;
    state.participant_names = participant_names;
    state.physics_task_manifest = viper::plan_track_physics_task_manifest(
        track_objects, configured_car_record_tags, player_car_index);
    const auto serialized_stream_offset =
        state.physics_task_manifest->player_stream_offset;
    if (serialized_stream_offset.has_value()) {
        std::cout << "Inherited constructor argument 2 uses the ordered world-task "
                  << "manifest offset 0x" << std::hex << *serialized_stream_offset
                  << std::dec << " after " << *state.physics_task_manifest->player_entry_index
                  << " preceding records";
        if (state.physics_task_manifest->status !=
            viper::TrackPhysicsTaskManifestStatus::complete) {
            std::cout << " (later manifest entries remain unresolved)";
        }
        std::cout << ". ";
    } else {
        std::cout << "Inherited constructor argument 2 is unresolved in the ordered "
                  << "world-task manifest";
        if (state.physics_task_manifest->failed_track_object_row.has_value()) {
            std::cout << " at track.obt row "
                      << *state.physics_task_manifest->failed_track_object_row;
        }
        std::cout << ". ";
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
    const auto* player_placement =
        state.graphics.car_placement_plan(player_car_index);
    if (player_placement == nullptr ||
        !state.prepare_player_constructor_record(player_car_resources,
            player_car_setup, serialized_stream_offset, *player_placement,
            car_name)) {
        std::cerr << "Could not prepare the mapped PCAR task record.\n";
        DestroyWindow(hwnd);
        return 1;
    }
    std::cout << "Prepared a candidate PCAR record from the active CARF setup and native "
              << "placement plan: " << state.player_runtime_car_record.size()
              << " bytes, framed at physics-task cursor 0x" << std::hex
              << state.player_task_record_stream->snapshot().records[0].cursor_offset
              << std::dec << ".\n";
    state.configured_car_task_records.push_back({player_car_index,
        state.player_runtime_car_record});
    const auto prepared_ai_record_count = state.prepare_ai_runtime_car_records(
        selected_ai_setups, ai_car_resources, configured_car_record_tags);
    // Ordinary initialization sets the first gate byte and clears the second,
    // so 0x40D2F0 returns false and the static parser is enabled. The selector
    // region is zero-initialized in the shipped image; no indirect runtime
    // mutation is currently modeled. The x64 host uses host division because
    // the original x86 FDIV compatibility path is not implemented here.
    const auto materialized_track_records =
        viper::materialize_track_object_task_records(track_objects, terrain,
            model_bundles, 0U, true, false);
    const auto ordered_track_records = viper::plan_ordered_track_task_records(
        materialized_track_records, *state.physics_task_manifest,
        state.configured_car_task_records);
    std::cout << "Materialized " << prepared_ai_record_count
              << " selected AI ACAR runtime records and "
              << materialized_track_records.rows.size()
              << " track.obt rows; ordered manifest join is "
              << (ordered_track_records.complete ? "complete" : "incomplete");
    if (ordered_track_records.failed_manifest_entry.has_value())
        std::cout << " at entry " << *ordered_track_records.failed_manifest_entry;
    bool player_constructor_ready = false;
    if (ordered_track_records.complete) {
        const auto ordered_player_record = std::find_if(
            ordered_track_records.records.begin(), ordered_track_records.records.end(),
            [&](const auto& record) {
                return record.configured_car_index == player_car_index &&
                    record.record_tag == 0x50434152U;
            });
        if (ordered_player_record == ordered_track_records.records.end() ||
            ordered_player_record->record_bytes.size() !=
                viper::CarFileRuntimeMapping::runtime_record_size) {
            std::cerr << "The ordered stream has no complete PCAR for the selected player.\n";
            DestroyWindow(hwnd);
            return 1;
        }
        // The ordered join supplies the native car ordinal at +0x194. Keep the
        // expected player record identical to the bytes that will be serialized.
        state.player_runtime_car_record = ordered_player_record->record_bytes;
        state.full_task_record_stream.emplace(
            state.physics_task_manifest->initial_cursor);
        bool stream_ok = true;
        for (const auto& record : ordered_track_records.records) {
            const auto appended = state.full_task_record_stream->append_record(
                record.record_bytes);
            if (appended.status !=
                viper::PhysicsTaskRecordSerializationStatus::complete) {
                stream_ok = false;
                break;
            }
        }
        const auto stream = state.full_task_record_stream->snapshot();
        stream_ok = stream_ok && stream.status ==
            viper::PhysicsTaskRecordSerializationStatus::complete &&
            stream.initial_cursor == state.physics_task_manifest->initial_cursor &&
            state.physics_task_manifest->final_cursor.has_value() &&
            stream.final_cursor == *state.physics_task_manifest->final_cursor &&
            stream.records.size() == state.physics_task_manifest->entries.size();
        for (std::size_t index = 0U;
             stream_ok && index < ordered_track_records.records.size(); ++index) {
            const auto& ordered = ordered_track_records.records[index];
            const auto& serialized = stream.records[index];
            stream_ok = serialized.record_tag == ordered.record_tag &&
                serialized.cursor_offset == ordered.cursor_offset &&
                serialized.record_size == ordered.record_bytes.size();
        }
        if (stream_ok) {
            state.full_task_record_readback =
                viper::read_physics_task_record_stream(stream.bytes);
            const auto& decoded = *state.full_task_record_readback;
            stream_ok = decoded.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::complete &&
                decoded.consumed_bytes == stream.bytes.size() &&
                decoded.records.size() == ordered_track_records.records.size() &&
                decoded.final_cursor == stream.final_cursor;
            if (stream_ok) {
                const auto inventory = viper::inventory_physics_task_objects(
                    decoded, stream.bytes);
                stream_ok = inventory.status ==
                    viper::PhysicsTaskObjectInventoryStatus::complete;
                if (stream_ok)
                    state.physics_task_object_inventory = inventory.entries;
            }
            if (stream_ok) {
                auto runtime_collection =
                    viper::materialize_physics_task_runtime_collection(
                        decoded, stream.bytes, false);
                stream_ok = runtime_collection.status ==
                    viper::PhysicsTaskRuntimeCollectionStatus::complete;
                if (stream_ok)
                    state.physics_task_runtime_collection =
                        std::move(runtime_collection);
            }
            if (stream_ok) {
                const auto bindings = viper::bind_configured_car_task_records_to_stream(
                    ordered_track_records, decoded, stream.bytes);
                stream_ok = bindings.status ==
                    viper::ConfiguredCarTaskStreamBindingStatus::complete;
                if (stream_ok)
                    state.configured_car_stream_bindings = bindings.bindings;
            }
            if (stream_ok) {
                state.ai_acar_curve_sources.clear();
                for (const auto& binding : state.configured_car_stream_bindings) {
                    if (binding.record_tag != 0x41434152U) continue;
                    if (binding.record_bytes_offset > stream.bytes.size() ||
                        binding.record_size > stream.bytes.size() -
                            binding.record_bytes_offset) {
                        stream_ok = false;
                        break;
                    }
                    const auto record_bytes = std::span<const std::byte>(stream.bytes)
                        .subspan(binding.record_bytes_offset, binding.record_size);
                    const auto curve_source =
                        viper::read_car_curve_source_record(record_bytes);
                    if (!curve_source.has_value()) {
                        stream_ok = false;
                        break;
                    }
                    state.ai_acar_curve_sources.push_back({binding, *curve_source});
                }
            }
            for (std::size_t index = 0U;
                 stream_ok && index < decoded.records.size(); ++index) {
                const auto& ordered = ordered_track_records.records[index];
                const auto& readback = decoded.records[index];
                stream_ok = readback.record_tag == ordered.record_tag &&
                    readback.cursor_offset == ordered.cursor_offset &&
                    readback.record_size == ordered.record_bytes.size();
            }
            if (stream_ok && serialized_stream_offset.has_value()) {
                const auto binding = viper::bind_player_pcar_task_record(
                    decoded, stream.bytes, state.player_runtime_car_record,
                    *serialized_stream_offset);
                stream_ok = binding.status ==
                        viper::PlayerPcarTaskBindingStatus::complete &&
                    binding.binding.has_value();
                if (stream_ok) {
                    state.player_pcar_task_binding = *binding.binding;
                    const auto player_record_bytes =
                        std::span<const std::byte>(stream.bytes).subspan(
                            binding.binding->record_bytes_offset,
                            binding.binding->record_size);
                    stream_ok = player_placement != nullptr &&
                        state.initialize_player_constructor_inputs(
                            player_record_bytes, player_model.vertices, wheel_tires,
                            binding.binding->serialized_stream_cursor,
                            *player_placement);
                }
            } else {
                stream_ok = false;
            }
        }
        std::size_t retained_checkpoint_count = 0U;
        std::size_t retained_inactive_stat_count = 0U;
        std::size_t retained_obstacle_pose_count = 0U;
        std::size_t retained_obstacle_geometry_count = 0U;
        std::size_t unsupported_stat_variant_count = 0U;
        std::size_t unavailable_active_callback_count = 0U;
        if (stream_ok && state.physics_task_runtime_collection.has_value()) {
            state.inactive_stat_task_states.clear();
            for (const auto& entry :
                 state.physics_task_runtime_collection->entries) {
                switch (entry.status) {
                case viper::PhysicsTaskRuntimeEntryStatus::inert_checkpoint_retained:
                    ++retained_checkpoint_count;
                    break;
                case viper::PhysicsTaskRuntimeEntryStatus::inactive_stat_retained:
                    if (entry.stat.has_value())
                        state.inactive_stat_task_states.push_back(*entry.stat);
                    ++retained_inactive_stat_count;
                    break;
                case viper::PhysicsTaskRuntimeEntryStatus::unsupported_stat_variant:
                    ++unsupported_stat_variant_count;
                    break;
                case viper::PhysicsTaskRuntimeEntryStatus::
                        obstacle_cached_pose_retained:
                    if (!entry.obstacle.has_value() ||
                        !entry.obstacle->geometry_records.empty()) {
                        stream_ok = false;
                        break;
                    }
                    ++retained_obstacle_pose_count;
                    break;
                case viper::PhysicsTaskRuntimeEntryStatus::
                        obstacle_shape_geometry_retained:
                    if (!entry.obstacle.has_value() ||
                        entry.obstacle->geometry_records.size() != 6U) {
                        stream_ok = false;
                        break;
                    }
                    ++retained_obstacle_pose_count;
                    ++retained_obstacle_geometry_count;
                    break;
                case viper::PhysicsTaskRuntimeEntryStatus::active_callback_unavailable:
                    break;
                }
                if (entry.callback_behavior_unavailable)
                    ++unavailable_active_callback_count;
            }
        } else {
            stream_ok = false;
        }
        if (stream_ok) {
            std::cout << "; serialized " << stream.records.size()
                      << " ordered records and decoded readback from cursor 0x" << std::hex
                      << stream.initial_cursor << " through 0x" << stream.final_cursor
                      << std::dec << "; bound player PCAR record index "
                      << state.player_pcar_task_binding->stream_record_index
                      << " bound at cursor 0x" << std::hex
                      << state.player_pcar_task_binding->serialized_stream_cursor
                      << std::dec << "; initialized player constructor state from the "
                      << "bound stream record (body scale "
                      << state.player_body_constructor_plan
                          ->scaled_dimensions_0x1f0_to_0x1fc[0]
                      << ", inverse mass " << *state.player_body_inverse_mass_0x200
                      << ", four wheel anchors/core setups and tire responses); retained "
                      << state.ai_acar_curve_sources.size()
                      << " AI ACAR curve source records; materialized "
                      << retained_checkpoint_count
                      << " inert CHKP records and "
                      << retained_inactive_stat_count
                      << " inactive STAT task states and "
                      << retained_obstacle_pose_count
                      << " OBST cached poses (including "
                      << retained_obstacle_geometry_count
                      << " with selector-2 geometry); "
                      << unsupported_stat_variant_count
                      << " unsupported STAT variants and "
                      << unavailable_active_callback_count
                      << " active callback records remain unavailable";
        } else {
            state.full_task_record_stream.reset();
            state.full_task_record_readback.reset();
            state.physics_task_object_inventory.clear();
            state.physics_task_runtime_collection.reset();
            state.inactive_stat_task_states.clear();
            state.configured_car_stream_bindings.clear();
            state.ai_acar_curve_sources.clear();
            state.player_pcar_task_binding.reset();
            std::cout << "; full stream serialization validation failed";
        }
        player_constructor_ready = stream_ok &&
            state.player_constructor_inputs_ready;
    }
    std::cout << ".\n";
    if (!player_constructor_ready) {
        std::cerr << "Could not bind and initialize the player constructor state from the "
                  << "complete ordered task stream.\n";
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
    std::ofstream physics_tick_trace;
    if (record_physics_tick_trace) {
        physics_tick_trace.open(*record_physics_tick_trace,
            std::ios::out | std::ios::trunc);
        if (!physics_tick_trace) {
            std::cerr << "Could not create physics tick trace: "
                      << record_physics_tick_trace->string() << '\n';
            DestroyWindow(hwnd);
            return 1;
        }
        physics_tick_trace << "outer_update,scheduled_step_index,scheduled_step_count"
            ",tick_index,scheduler_clock_ms,simulation_time,input_provenance"
            ",steering,throttle,brake,control_value_5093b8,control_flag_5093f0"
            ",control_value_5093c8,control_flag_5093cc,control_flag_5093f8"
            ",control_flag_509360,control_difference_509344,control_flag_509420"
            ",control_difference_509340,control_shift_latch_509354"
            ",control_counter_5093bc,keyboard_snapshot_hex"
            ",body_basis_0,body_basis_1,body_basis_2,body_basis_3,body_basis_4"
            ",body_basis_5,body_basis_6,body_basis_7,body_basis_8"
            ",body_pos_x,body_pos_y,body_pos_z,body_vel_x,body_vel_y,body_vel_z"
            ",body_ang_x,body_ang_y,body_ang_z,body_force_x,body_force_y,body_force_z"
            ",body_moment_x,body_moment_y,body_moment_z,body_timer_0xe44"
            ",body_kinematics_input_ready,body_kinematics_non_finite"
            ",body_kinematics_reader_inventory_mismatch"
            ",body_kinematics_missing_float_count";
        for (std::size_t lane = 0U; lane < 44U; ++lane)
            physics_tick_trace << ",body_kinematics_missing_offset_" << lane;
        physics_tick_trace << ",body_integrator_timer_ramp_source_ready"
            ",body_integrator_timer_known_fields_finite"
            ",body_integrator_timer_missing_count";
        for (std::size_t lane = 0U; lane < 3U; ++lane)
            physics_tick_trace << ",body_integrator_timer_missing_offset_" << lane;
        for (std::size_t wheel = 0U; wheel < 4U; ++wheel) {
            physics_tick_trace << ",wheel" << wheel << "_contact_active"
                << ",wheel" << wheel << "_projected_depth";
        }
        physics_tick_trace << '\n';
        physics_tick_trace << std::setprecision(9);
        if (!physics_tick_trace) {
            std::cerr << "Could not write physics tick trace header.\n";
            DestroyWindow(hwnd);
            return 1;
        }
    }
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
            trace << ",wheel" << wheel << "_query_x,wheel" << wheel
                  << "_query_y,wheel" << wheel << "_query_z";
        }
        for (std::size_t wheel = 0; wheel < state.player_wheel_contact_states.size(); ++wheel) {
            trace << ",wheel" << wheel << "_surface_x,wheel" << wheel
                  << "_surface_y,wheel" << wheel << "_surface_z,wheel" << wheel
                  << "_normal_x,wheel" << wheel << "_normal_y,wheel" << wheel
                  << "_normal_z";
        }
        trace << ",control_0x509358,control_0x5093f4,control_0x509320"
              << ",control_0x5093b8"
              << ",control_0x5093f0,control_0x5093c8,control_0x5093cc"
              << ",control_0x5093f8,control_0x509360,control_0x509344"
              << ",control_0x509420,control_0x509340"
              << ",control_0x509354,control_0x5093bc"
              << ",pcar_prefix_0xde0,pcar_prefix_0xde4"
              << ",pcar_prefix_0xde8,pcar_prefix_0xdec"
              << ",pcar_traction_control,pcar_abs_braking"
              << ",pcar_auto_shifting,pcar_auto_clutch,pcar_yaw_control"
              << ",pcar_body_0x4f4_entrant_count"
              << ",pcar_body_0xd74";
        for (std::size_t wheel = 0U; wheel < 4U; ++wheel)
            trace << ",wheel" << wheel << "_constructor_0x6b4";
        trace << ",pcar_body_0xde0,pcar_body_0xde4"
              << ",pcar_body_0xde8,pcar_body_0xdec"
              << ",pcar_velocity_projection,pcar_scaled_smoother_request"
              << ",pcar_body_0x500,pcar_body_0xdcc,pcar_body_0xbe4"
              << ",pcar_body_0x630,pcar_body_0x7d4"
              << ",pcar_body_0x978,pcar_body_0xb1c"
              << ",pcar_body_0x4f0,pcar_body_0x4ec"
              << ",pcar_body_0xecc,pcar_timestamp_backend_callback_requested"
              << ",pcar_gate_0x5093fc,pcar_0x40d270_multiplier"
              << ",physics_steps_scheduled,physics_step_count"
              << ",pcar_callback_step_count"
              << ",pcar_callback_task_stream_index"
              << ",pcar_task_projection_dispatch_steps"
              << ",physics_simulation_time,physics_catchup_limited";
        trace << ",directinput_keyboard_state_read,directinput_up_pressed,paused"
              << ",physics_tick_input_provenance,physics_tick_clock_ms"
              << ",physics_tick_index,physics_tick_scheduled_step_index"
              << ",physics_tick_scheduled_step_count,physics_tick_simulation_time"
              << ",physics_tick_steering,physics_tick_throttle,physics_tick_brake";
        for (std::size_t car = 0U; car < state.opponent_motions.size(); ++car) {
            if (car == player_car_index) continue;
            trace << ",car" << car << "_ai_valid,car" << car
                  << "_ai_throttle,car" << car << "_ai_brake,car" << car
                  << "_ai_target_speed,car" << car << "_ai_leader,car" << car
                  << "_ai_gap";
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
                  << " parsed SFX0 PCM engine resources ("
                  << state.audio.band_count() << " ESFX bands).\n";
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    if (!state.directinput_keyboard.initialize(hwnd)) {
        std::cerr << "DirectInput keyboard initialization failed; using Win32 key-message fallback.\n";
    }
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
                !state.advance(delta_seconds,
                    std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(now)),
                    render_frame, !state.paused)) {
                std::cerr << "Vehicle pose update failed.\n";
                DestroyWindow(hwnd);
                return 1;
            }
            state.last_tick = now;
            if (record_physics_tick_trace) {
                for (const auto& tick : state.player_physics_ticks_this_update) {
                    const auto& controls = tick.controls.controls;
                    physics_tick_trace << tick.outer_update_index << ','
                        << tick.scheduled_step_index << ','
                        << tick.scheduled_step_count << ','
                        << tick.tick_index << ','
                        << tick.scheduler_clock_milliseconds << ','
                        << tick.simulation_time_seconds << ','
                        << static_cast<unsigned>(tick.input_provenance) << ','
                        << tick.controls.steering.value_0x509358 << ','
                        << tick.controls.analog_0x5093f4.value << ','
                        << tick.controls.analog_0x509320.value << ','
                        << controls.value_0x5093b8 << ','
                        << static_cast<unsigned>(controls.flag_0x5093f0) << ','
                        << controls.value_0x5093c8 << ','
                        << static_cast<unsigned>(controls.flag_0x5093cc) << ','
                        << static_cast<unsigned>(controls.flag_0x5093f8) << ','
                        << static_cast<unsigned>(controls.flag_0x509360) << ','
                        << controls.difference_0x509344 << ','
                        << static_cast<unsigned>(controls.flag_0x509420) << ','
                        << controls.difference_0x509340 << ','
                        << static_cast<unsigned>(controls.transition_latch_0x509354)
                        << ',' << controls.counter_0x5093bc << ',';
                    for (const auto key_state : tick.keyboard_snapshot) {
                        physics_tick_trace << std::hex << std::setw(2)
                            << std::setfill('0')
                            << static_cast<unsigned>(key_state);
                    }
                    for (const auto& value : tick.body_state_after_tick) {
                        physics_tick_trace << ',';
                        if (value.has_value()) physics_tick_trace << *value;
                    }
                    physics_tick_trace << std::dec << ','
                        << tick.body_kinematics_input_ready
                        << ',' << tick.body_kinematics_non_finite
                        << ',' << tick.body_kinematics_reader_inventory_mismatch
                        << ',' << tick.body_kinematics_missing_float_count;
                    for (std::size_t lane = 0U; lane < 44U; ++lane) {
                        physics_tick_trace << ',';
                        if (lane < tick.body_kinematics_missing_float_count)
                            physics_tick_trace << std::hex
                                << tick.body_kinematics_missing_float_offsets[lane]
                                << std::dec;
                    }
                    physics_tick_trace << ','
                        << tick.body_integrator_timer_ramp_source_ready
                        << ',' << tick.body_integrator_timer_known_fields_finite
                        << ',' << tick.body_integrator_timer_missing_count;
                    for (std::size_t lane = 0U; lane < 3U; ++lane) {
                        physics_tick_trace << ',';
                        if (lane < tick.body_integrator_timer_missing_count)
                            physics_tick_trace << std::hex
                                << tick.body_integrator_timer_missing_offsets[lane]
                                << std::dec;
                    }
                    for (std::size_t wheel = 0U; wheel < 4U; ++wheel) {
                        physics_tick_trace << ',';
                        if (tick.wheel_contact_active_after_tick[wheel]) {
                            physics_tick_trace << static_cast<unsigned>(
                                *tick.wheel_contact_active_after_tick[wheel]);
                        }
                        physics_tick_trace << ',';
                        if (tick.wheel_contact_depth_after_tick[wheel]) {
                            physics_tick_trace <<
                                *tick.wheel_contact_depth_after_tick[wheel];
                        }
                    }
                    physics_tick_trace << std::dec << std::setfill(' ') << '\n';
                    if (!physics_tick_trace) {
                        std::cerr << "Could not write physics tick trace.\n";
                        DestroyWindow(hwnd);
                        return 1;
                    }
                }
                physics_tick_trace.flush();
            }
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
                    trace << ',' << observation.contact_projection_origin.x
                          << ',' << observation.contact_projection_origin.y
                          << ',' << observation.contact_projection_origin.z;
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
                trace << ',' << state.control_update_state.steering.value_0x509358
                      << ',' << state.control_update_state.analog_0x5093f4.value
                      << ',' << state.control_update_state.analog_0x509320.value
                      << ',' << state.control_update_state.controls.value_0x5093b8;
                const auto& live_controls = state.control_update_state.controls;
                trace << ',' << static_cast<unsigned>(live_controls.flag_0x5093f0)
                      << ',' << live_controls.value_0x5093c8
                      << ',' << static_cast<unsigned>(live_controls.flag_0x5093cc)
                      << ',' << static_cast<unsigned>(live_controls.flag_0x5093f8)
                      << ',' << static_cast<unsigned>(live_controls.flag_0x509360)
                      << ',' << live_controls.difference_0x509344
                      << ',' << static_cast<unsigned>(live_controls.flag_0x509420)
                      << ',' << live_controls.difference_0x509340
                      << ',' << static_cast<unsigned>(live_controls.transition_latch_0x509354)
                      << ',' << live_controls.counter_0x5093bc
                      << ',' << state.player_pcar_control_prefix.value_0xde0
                      << ',' << state.player_pcar_control_prefix.value_0xde4
                      << ',' << state.player_pcar_control_prefix.value_0xde8
                      << ',' << state.player_pcar_control_prefix.value_0xdec
                      << ',' << state.player_pcar_constructor_controls.traction_control
                      << ',' << state.player_pcar_constructor_controls.abs_braking
                      << ',' << state.player_pcar_constructor_controls.auto_shifting
                      << ',' << state.player_pcar_constructor_controls.auto_clutch_0x534
                      << ',' << state.player_pcar_constructor_controls.yaw_control;
                if (state.player_pcar_constructor_controls.game_mode_0x4f4.has_value())
                    trace << ',' << *state.player_pcar_constructor_controls.game_mode_0x4f4;
                else
                    trace << ',';
                const auto constructor_helper_byte = viper::read_ncar_body_object_u8(
                    state.player_runtime_state.body_object, 0xd74U);
                if (constructor_helper_byte.has_value())
                    trace << ',' << static_cast<unsigned>(*constructor_helper_byte);
                else
                    trace << ',';
                for (const auto& wheel : state.player_runtime_state.wheel_objects) {
                    const auto helper_flag = viper::read_ncar_wheel_object_u8(
                        wheel, 0x160U);
                    if (helper_flag.has_value())
                        trace << ',' << static_cast<unsigned>(*helper_flag);
                    else
                        trace << ',';
                }
                for (const auto offset : {0xde0U, 0xde4U, 0xde8U, 0xdecU}) {
                    const auto bits = viper::read_ncar_body_object_u32(
                        state.player_runtime_state.body_object, offset);
                    if (bits.has_value()) trace << ',' << std::bit_cast<float>(*bits);
                    else trace << ',';
                }
                trace << ',' << state.player_pcar_callback_result
                    .absolute_velocity_projection
                      << ',' << state.player_pcar_callback_result
                    .scaled_smoother_request;
                for (const auto offset : {0x500U, 0xdccU}) {
                    const auto bits = viper::read_ncar_body_object_u32(
                        state.player_runtime_state.body_object, offset);
                    if (bits.has_value()) trace << ',' << std::bit_cast<float>(*bits);
                    else trace << ',';
                }
                const auto smoother_mode = viper::read_ncar_body_object_u8(
                    state.player_runtime_state.body_object, 0xbe4U);
                if (smoother_mode.has_value())
                    trace << ',' << static_cast<unsigned>(*smoother_mode);
                else trace << ',';
                for (const auto offset : {0x630U, 0x7d4U, 0x978U, 0xb1cU}) {
                    const auto bits = viper::read_ncar_body_object_u32(
                        state.player_runtime_state.body_object, offset);
                    if (bits.has_value()) trace << ',' << std::bit_cast<float>(*bits);
                    else trace << ',';
                }
                const auto shared_float_cache = viper::read_ncar_body_object_u32(
                    state.player_runtime_state.body_object, 0x4f0U);
                if (shared_float_cache.has_value())
                    trace << ',' << std::bit_cast<float>(*shared_float_cache);
                else trace << ',';
                const auto shared_byte_cache = viper::read_ncar_body_object_u8(
                    state.player_runtime_state.body_object, 0x4ecU);
                if (shared_byte_cache.has_value())
                    trace << ',' << static_cast<unsigned>(*shared_byte_cache);
                else trace << ',';
                const auto timestamp_cache = viper::read_ncar_body_object_u32(
                    state.player_runtime_state.body_object, 0xeccU);
                if (timestamp_cache.has_value())
                    trace << ',' << std::bit_cast<float>(*timestamp_cache);
                else trace << ',';
                trace << ',' << state.pcar_timestamp_backend_callback_requested;
                trace << ',' << state.pcar_callback_globals.gate_0x5093fc;
                trace << ',' << state.player_pcar_callback_result
                    .callback_multiplier.value;
                trace << ',' << state.last_physics_step_schedule.steps
                      << ',' << state.physics_step_count
                      << ',' << state.pcar_callback_step_count;
                if (state.player_pcar_task_binding.has_value() &&
                    state.physics_task_runtime_collection.has_value() &&
                    state.player_pcar_task_binding->stream_record_index <
                        state.physics_task_runtime_collection->entries.size()) {
                    const auto task_index =
                        state.player_pcar_task_binding->stream_record_index;
                    trace << ',' << task_index << ','
                          << state.physics_task_runtime_collection->entries[
                                task_index].known_callback_projection_steps;
                } else {
                    trace << ",,";
                }
                trace << ',' << static_cast<float>(state.physics_step_count) * 0.016F
                      << ',' << state.last_physics_step_schedule.catchup_limited
                      << ',' << state.directinput_keyboard_state_read
                      << ',' << state.directinput_up_pressed
                      << ',' << state.paused;
                if (!state.player_physics_ticks_this_update.empty()) {
                    const auto& tick = state.player_physics_ticks_this_update.back();
                    trace << ',' << static_cast<unsigned>(tick.input_provenance)
                          << ',' << tick.scheduler_clock_milliseconds
                          << ',' << tick.tick_index
                          << ',' << tick.scheduled_step_index
                          << ',' << tick.scheduled_step_count
                          << ',' << tick.simulation_time_seconds
                          << ',' << tick.controls.steering.value_0x509358
                          << ',' << tick.controls.analog_0x5093f4.value
                          << ',' << tick.controls.analog_0x509320.value;
                } else {
                    trace << ",,,,,,,,";
                }
                for (std::size_t car = 0U; car < state.opponent_motions.size(); ++car) {
                    if (car == player_car_index) continue;
                    const auto& decision = state.last_opponent_ai_decisions[car];
                    trace << ',' << decision.valid << ',' << decision.throttle << ','
                        << decision.brake << ',' << decision.target_speed << ',';
                    if (decision.leader_index !=
                        std::numeric_limits<std::size_t>::max())
                        trace << decision.leader_index;
                    trace << ',' << decision.distance_to_leader;
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
                     const viper::ReplayFile* loaded_replay,
                     const bool hold_results_for_frontend,
                     const std::uint32_t selected_laps,
                     const std::optional<std::uint32_t> selected_opponent_strength = {})
{
    const auto config_path = options.data_dir / "options.def";
    viper::GameOptions configured_options;
    std::error_code config_error;
    if (std::filesystem::is_regular_file(config_path, config_error) && !config_error)
        configured_options = viper::GameOptions::load(config_path);
    if (selected_opponent_strength && *selected_opponent_strength <
            opponent_strength_names.size())
        configured_options.set_integer("GAME", "opponent_strength",
            static_cast<int>(*selected_opponent_strength));
    const auto setup_config_directory = options.setup_config_directory
        ? *options.setup_config_directory
        : resolve_setup_config_directory(options.data_dir);
    const auto setup_preference = viper::load_setup_selection_preference(
        setup_config_directory);
    if (!setup_preference.error.empty())
        std::cerr << "Ignoring setup preference: " << setup_preference.error << '\n';
    if (setup_preference.selection) {
        for (std::size_t index = 0; index < supported_control_names.size(); ++index) {
            const auto& binding = setup_preference.selection->control_bindings[index];
            if (binding)
                configured_options.set_string("CONTROL",
                    std::string(supported_control_names[index]),
                    binding->empty() ? std::string{} : *binding);
        }
    }
    auto assets = load_assets(options.data_dir, track, car,
        std::move(configured_options), setup_config_directory);
    if (assets.player_setup_version_warning())
        std::cerr << "Warning: player CSU version is not 1; native loader continues with its first 0x8c bytes\n";
    report_start_grid(assets);
    if (options.validate_assets) return 0;
    const auto driver_order = viper::initialize_single_race_driver_order();
    constexpr std::array<std::string_view, viper::ai_driver_group_count> ai_name_profiles{
        "Easy", "Intermediate", "Hard", "Career1", "Career2", "Career3",
        "Career4", "Career5"};
    const auto opponent_strength = assets.options()
        .integer("GAME", "opponent_strength").value_or(0);
    std::array<std::string, viper::ai_driver_slots_per_group> ai_name_storage{};
    std::array<std::string_view, viper::ai_driver_slots_per_group> ai_names{};
    if (opponent_strength >= 0 && opponent_strength <
            static_cast<int>(ai_name_profiles.size())) {
        const auto group_index = static_cast<std::size_t>(opponent_strength);
        const auto active_count = assets.ai_drivers().active_counts[group_index];
        for (std::size_t slot = 0U; slot < ai_names.size() && slot < active_count; ++slot) {
            const auto key = std::string("AIDriverName:") +
                std::string(ai_name_profiles[group_index]) + ":Driver" +
                std::to_string(slot);
            const auto localized_name = assets.language().find(key);
            if (localized_name.has_value()) {
                ai_name_storage[slot] = *localized_name;
                ai_names[slot] = ai_name_storage[slot];
            }
        }
    }
    viper::SingleRaceEntrantObservation entrant_observation{};
    entrant_observation.configured_car_count = assets.options()
        .integer("GAME", "car_count").value_or(8);
    entrant_observation.driver_order_values = driver_order;
    entrant_observation.ai_names_by_slot = ai_names;
    entrant_observation.opponent_strength_value = opponent_strength;
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
    std::vector<std::string> participant_names;
    std::vector<std::uint32_t> configured_car_record_tags;
    participant_names.reserve(entrants.records.size());
    configured_car_record_tags.reserve(entrants.records.size());
    for (const auto& record : entrants.records) {
        participant_names.push_back(record.display_name.value_or(std::string{}));
        if (record.marker_value == 0U) {
            configured_car_record_tags.push_back(0x50434152U);
        } else if (record.marker_value == 1U) {
            configured_car_record_tags.push_back(0x41434152U);
        } else {
            throw std::runtime_error(
                "single-race entrant has an unsupported physics car marker");
        }
    }
    std::optional<viper::SingleRaceAISetupPlan> selected_ai_setups;
    if (entrants.participant_count > 1) {
        if (!assets.track_index())
            throw std::runtime_error("selected track has no matching tracks.tab row for AI setup");
        if (opponent_strength < 0 || opponent_strength >=
                static_cast<int>(viper::ai_driver_group_count)) {
            std::cerr << "AI setup is unresolved: GAME.opponent_strength is outside "
                         "the eight native AIDR profiles.\n";
        } else {
            // The single-race state is zeroed by 0x45C5B0. Both native
            // single-race initializers call 0x40A5D0 without writing its
            // tuning byte at +0xCD1; 0x41A000 consumes that byte as mode 0.
            constexpr std::uint8_t single_race_tuning_mode = 0U;
            viper::SingleRaceAISetupObservation ai_observation{};
            ai_observation.entrants = &entrants;
            ai_observation.drivers = &assets.ai_drivers();
            ai_observation.driver_group =
                static_cast<viper::AIDriverGroup>(opponent_strength);
            ai_observation.track_index = *assets.track_index();
            ai_observation.tuning_mode = single_race_tuning_mode;
            ai_observation.base_setup_payload = assets.ai_vehicle_setup();
            ai_observation.configuration_data_available = true;
            ai_observation.loaded_car_file = assets.ai_car().car_file;
            ai_observation.compatibility_division_helper_enabled = false;
            selected_ai_setups = viper::prepare_single_race_ai_setups(ai_observation);
            const auto complete_setup_records = static_cast<std::size_t>(
                std::count_if(selected_ai_setups->ai_setups.begin(),
                    selected_ai_setups->ai_setups.end(),
                    [](const auto& record) { return record.input_complete; }));
            std::cout << "Prepared " << selected_ai_setups->ai_setups.size()
                      << " AI setup records for selected AIDR profile "
                      << opponent_strength << " and native single-race tuning mode 0 ("
                      << complete_setup_records
                      << " numerically complete with the current FDIV model)\n";
        }
    }
    return run_window(options.software, options.smoke_frames, options.audio_smoke,
        options.record_trace, options.record_physics_tick_trace,
        playback_trace, hold_results_for_frontend,
        *entrants.player_record_index,
        participant_names,
        configured_car_record_tags,
        selected_ai_setups ? &*selected_ai_setups : nullptr,
        assets.ai_car(),
        std::clamp(selected_laps, 1U, 99U),
        assets.track_name(), assets.car_name(), assets.terrain(),
        assets.ideal_line(), assets.player_model(), assets.ai_model(),
        assets.track_objects(),
        assets.bundle(std::string(assets.car_name()) + ".car"),
        assets.player_car(), assets.player_car_setup(), assets.wheel_tires(),
        std::array<const viper::ResourceBundle*, 7>{
            &assets.bundle(std::string(assets.track_name()) + ".trk"),
            &assets.bundle(std::string(assets.car_name()) + ".car"),
            &assets.bundle("viper.car"), &assets.bundle("common.res"),
            &assets.bundle("race.res"), &assets.bundle("ui.res"),
            &assets.bundle("drivers.res")},
        options.grid_mode, assets.options(), loaded_replay);
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) { usage(std::cout); return 0; }
        const auto data_directory = resolve_data_directory(options);
        std::optional<std::vector<TraceInputFrame>> playback_trace;
        if (options.play_trace) playback_trace = load_input_trace(*options.play_trace);
        std::optional<viper::ReplayFile> loaded_replay;
        if (options.load_replay) {
            auto replay_result = viper::read_replay_file(*options.load_replay);
            if (!replay_result)
                throw std::runtime_error("could not load replay: " + replay_result.error);
            loaded_replay = std::move(replay_result.replay);
        }
        const auto config_path = data_directory / "options.def";
        viper::GameOptions configured_options;
        std::error_code config_error;
        if (std::filesystem::is_regular_file(config_path, config_error) && !config_error)
            configured_options = viper::GameOptions::load(config_path);
        auto track = options.track
            ? *options.track
            : configured_track_stem(data_directory, configured_options);
        const auto car = options.car
            ? *options.car
            : configured_car_stem(data_directory, configured_options);
        const bool interactive_frontend = !options.validate_assets &&
            !options.track && !options.car && !options.smoke_frames &&
            !options.audio_smoke && !options.record_trace &&
            !options.record_physics_tick_trace;
        if (!interactive_frontend && track == "random")
            track = resolve_random_track_choice(
                selectable_track_stems(data_directory));
        auto selected_track = track;
        auto selected_car = car;
        auto selected_laps = static_cast<std::uint32_t>(std::clamp(
            configured_options.integer("GAME", "laps").value_or(1), 1, 99));
        auto selected_opponent_strength = static_cast<std::uint32_t>(std::clamp(
            configured_options.integer("GAME", "opponent_strength").value_or(0),
            0, static_cast<int>(viper::ai_driver_group_count - 1U)));
        auto setup_config_directory = options.setup_config_directory
            ? *options.setup_config_directory
            : resolve_setup_config_directory(data_directory);
        std::array<std::string, 13> default_control_bindings{};
        for (std::size_t index = 0; index < supported_control_names.size(); ++index) {
            if (const auto value = configured_options.get(
                    "CONTROL", supported_control_names[index]))
                default_control_bindings[index] = std::string(*value);
        }
        auto selected_control_bindings = default_control_bindings;
        std::vector<std::string> setup_tracks;
        std::vector<std::string> setup_cars;
        if (interactive_frontend) {
            setup_tracks = selectable_track_stems(data_directory);
            setup_cars = selectable_car_stems(data_directory);
            const auto saved = viper::load_setup_selection_preference(
                setup_config_directory);
            if (!saved.error.empty())
                std::cerr << "Ignoring setup preference: " << saved.error << '\n';
            const auto indices = viper::resolve_setup_selection_indices(
                setup_tracks, setup_cars, saved.selection, selected_track, selected_car);
            selected_track = setup_tracks[indices.track];
            selected_car = setup_cars[indices.car];
            if (saved.selection && saved.selection->laps)
                selected_laps = std::clamp(*saved.selection->laps, 1U, 99U);
            if (saved.selection && saved.selection->opponent_strength)
                selected_opponent_strength = *saved.selection->opponent_strength;
            if (saved.selection) {
                for (std::size_t index = 0; index < selected_control_bindings.size(); ++index)
                    if (saved.selection->control_bindings[index])
                        selected_control_bindings[index] =
                            *saved.selection->control_bindings[index];
            }
        }
        while (true) {
            if (interactive_frontend) {
                const auto selection = show_setup_frontend(setup_tracks, setup_cars,
                    selected_track, selected_car, selected_laps,
                    selected_opponent_strength,
                    selected_control_bindings, default_control_bindings,
                    setup_config_directory);
                if (!selection) return 0;
                selected_track = selection->track;
                selected_car = selection->car;
                selected_laps = selection->laps;
                selected_opponent_strength = selection->opponent_strength;
                selected_control_bindings = selection->control_bindings;
                std::array<std::optional<std::string>, 13> saved_bindings{};
                for (std::size_t index = 0; index < saved_bindings.size(); ++index)
                    saved_bindings[index] = selected_control_bindings[index];
                const auto saved = viper::save_setup_selection_preference(
                    setup_config_directory,
                    {selected_track, selected_car, selected_laps,
                        selected_opponent_strength, saved_bindings});
                if (!saved.saved)
                    std::cerr << "Could not persist setup selection: "
                              << saved.error << '\n';
            }
            auto runtime_options = options;
            runtime_options.data_dir = data_directory;
            const auto result = run_selected_race(runtime_options, selected_track,
                selected_car, playback_trace ? &*playback_trace : nullptr,
                loaded_replay ? &*loaded_replay : nullptr,
                interactive_frontend, selected_laps,
                interactive_frontend
                    ? std::optional<std::uint32_t>(selected_opponent_strength)
                    : std::nullopt);
            if (result != 10) return result;
            if (!interactive_frontend) return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
