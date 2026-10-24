#include <Windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "viper/game_assets.hpp"
#include "viper/race_participants.hpp"

#include <array>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <limits>
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
    std::optional<unsigned> smoke_frames;
};

void usage(std::ostream& out)
{
    out << "Viper Racing reconstruction runtime\n"
        << "Usage: viper_game [--data-dir PATH] [--config-dir PATH] [--track NAME] [--car NAME] [--validate-assets] [--smoke-frames N] [--software] [--help]\n"
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


std::string configured_car_stem(const std::filesystem::path& data_dir,
                                const viper::GameOptions& options)
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

    std::vector<std::string> stems;
    do {
        stems.push_back(std::filesystem::path(file.cFileName).stem().string());
    } while (FindNextFileW(search, &file));
    auto index = options.integer("GLOBAL", "car_no").value_or(0);
    // The original constructor resets a selector at or above the enumerated
    // car count to zero. The enumeration order is the native *.car order.
    if (index < 0)
        throw std::runtime_error("options.def car_no selects an empty car entry");
    if (static_cast<std::size_t>(index) >= stems.size()) index = 0;
    return stems[static_cast<std::size_t>(index)];
}

class D3DWindow {
public:
    bool create(HWND hwnd, bool software,
                const viper::TerrainSurfaceResourcePayload& terrain,
                const viper::IdealLineTrackData& ideal_line,
                const viper::ModelResource& player_model,
                const viper::ModelResource& ai_model,
                const std::size_t player_car_index,
                const viper::TrackObjectTextTable& track_objects)
    {
        try {
            ideal_line_points_ = extract_ideal_line_points(ideal_line);
            extract_terrain_points(terrain);
            player_model_ = &player_model;
            ai_model_ = &ai_model;
            player_car_index_ = player_car_index;
            const auto spawns = car_spawns(track_objects);
            if (spawns.empty()) throw std::runtime_error("track.obt has no usable obj car start position");
            car_poses_.reserve(spawns.size());
            for (const auto& spawn : spawns) {
                const float yaw = route_yaw(spawn[0], spawn[1]);
                const auto surface = viper::sample_terrain_resource(
                    terrain, {spawn[0], 1000.0f, spawn[1]}, true);
                if (!surface.query_succeeded)
                    throw std::runtime_error("track.obt car start position has no terrain surface");
                car_poses_.push_back({spawn[0], surface.sampled_position[1], spawn[1], yaw});
            }
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

    bool render()
    {
        if (!target_ || !context_ || !swap_chain_) return false;
        constexpr float color[]{0.035f, 0.055f, 0.075f, 1.0f};
        context_->OMSetRenderTargets(1, target_.GetAddressOf(), nullptr);
        D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width_),
                                static_cast<float>(height_), 0.0f, 1.0f};
        context_->RSSetViewports(1, &viewport);
        context_->ClearRenderTargetView(target_.Get(), color);
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
        return SUCCEEDED(swap_chain_->Present(1, 0));
    }

    bool set_car_pose(const std::size_t car_index, const float anchor_x, const float anchor_y,
                      const float anchor_z, const float yaw)
    {
        if (car_index >= car_poses_.size()) return false;
        car_poses_[car_index] = {anchor_x, anchor_y, anchor_z, yaw};
        return update_car_buffer();
    }

    std::array<float, 2> car_position(const std::size_t car_index) const
    {
        if (car_index >= car_poses_.size()) return {};
        return {car_poses_[car_index].x, car_poses_[car_index].z};
    }

private:
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
            "O VSMain(V v) { O o; o.p=float4(v.p,0.5,1); o.c=v.c; return o; }\n";
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
        return SUCCEEDED(device_->CreateRasterizerState(&raster, &surface_rasterizer_));
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

    float route_yaw(const float x, const float z) const
    {
        float nearest = std::numeric_limits<float>::infinity();
        float result = 0.0f;
        for (std::size_t index = 0; index < ideal_line_points_.size(); ++index) {
            const auto& a = ideal_line_points_[index];
            const auto& b = ideal_line_points_[(index + 1U) % ideal_line_points_.size()];
            const float dx = b[0] - a[0], dz = b[1] - a[1];
            const float length_sq = dx * dx + dz * dz;
            if (!(length_sq > 1.0e-8f)) continue;
            const float fraction = std::clamp(((x - a[0]) * dx + (z - a[1]) * dz) /
                length_sq, 0.0f, 1.0f);
            const float ex = x - (a[0] + dx * fraction);
            const float ez = z - (a[1] + dz * fraction);
            const float distance = ex * ex + ez * ez;
            if (distance < nearest) { nearest = distance; result = std::atan2(dx, dz); }
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
            "float4 VSMain(V v) : SV_POSITION { return float4(v.p, 0.0, 1.0); }\n";
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
};

struct WindowState {
    D3DWindow graphics;
    const viper::TerrainSurfaceResourcePayload* terrain{};
    std::vector<std::array<float, 2>> route;
    std::size_t player_car_index{};
    std::size_t route_segment{};
    float route_fraction{};
    float steering{};
    float speed{};
    bool accelerate{};
    bool brake{};
    bool left{};
    bool right{};
    // CONTROL order: steer_left, steer_right, throttle, braking.
    std::array<UINT, 4> control_keys{
        VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN};
    ULONGLONG last_tick{};

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
            if (distance < nearest) { nearest = distance; route_segment = index; route_fraction = fraction; }
        }
        if (!std::isfinite(nearest)) return false;
        const auto& a = route[route_segment];
        const auto& b = route[(route_segment + 1U) % route.size()];
        const float yaw = std::atan2(b[0] - a[0], b[1] - a[1]);
        const auto surface = viper::sample_terrain_resource(*terrain, {x, 1000.0f, z}, true);
        return surface.query_succeeded && graphics.set_car_pose(player_car_index,
            x, surface.sampled_position[1], z, yaw);
    }

    bool advance(float delta_seconds)
    {
        if (route.size() < 2U || terrain == nullptr) return false;
        delta_seconds = std::clamp(delta_seconds, 0.0f, 0.05f);
        const float input = static_cast<float>(right) - static_cast<float>(left);
        steering += (input - steering) * std::min(1.0f, delta_seconds * 5.0f);
        const float drive = static_cast<float>(accelerate) - static_cast<float>(brake);
        speed = std::clamp((speed + drive * 14.0f * delta_seconds) *
            std::pow(0.992f, delta_seconds * 60.0f), -8.0f, 38.0f);
        float distance = speed * delta_seconds;
        for (unsigned guard = 0; std::abs(distance) > 1.0e-5f && guard < 128U; ++guard) {
            const auto& a = route[route_segment];
            const auto& b = route[(route_segment + 1U) % route.size()];
            const float dx = b[0] - a[0], dz = b[1] - a[1];
            const float length = std::sqrt(dx * dx + dz * dz);
            if (!(length > 1.0e-5f)) { route_segment = (route_segment + 1U) % route.size(); route_fraction = 0.0f; continue; }
            const float remaining = (distance > 0.0f ? 1.0f - route_fraction : route_fraction) * length;
            if (std::abs(distance) < remaining) { route_fraction += distance / length; distance = 0.0f; }
            else {
                const bool forward = distance > 0.0f;
                distance += distance > 0.0f ? -remaining : remaining;
                route_segment = (route_segment + (forward ? 1U : route.size() - 1U)) % route.size();
                route_fraction = forward ? 0.0f : 1.0f;
            }
        }
        const auto& a = route[route_segment];
        const auto& b = route[(route_segment + 1U) % route.size()];
        const float dx = b[0] - a[0], dz = b[1] - a[1];
        const float tangent = std::atan2(dx, dz);
        const float route_x = a[0] + dx * route_fraction;
        const float route_z = a[1] + dz * route_fraction;
        const float lateral = steering * speed * std::abs(speed) * 0.0008f;
        const float x = route_x + std::cos(tangent) * lateral;
        const float z = route_z - std::sin(tangent) * lateral;
        const auto surface = viper::sample_terrain_resource(*terrain, {x, 1000.0f, z}, true);
        if (!surface.query_succeeded) { speed = 0.0f; return true; }
        return graphics.set_car_pose(player_car_index,
            x, surface.sampled_position[1], z, tangent + steering * 0.35f);
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

void load_keyboard_bindings(WindowState& state,
                            const viper::GameOptions& options)
{
    constexpr std::array<std::string_view, 4> action_names{
        "steer_left", "steer_right", "throttle", "braking"};
    for (std::size_t index = 0; index < action_names.size(); ++index) {
        const auto configured = options.get("CONTROL", action_names[index]);
        if (configured) {
            const auto key = parse_keyboard_binding(*configured);
            state.control_keys[index] = key.value_or(
                (std::numeric_limits<UINT>::max)());
        }
    }
}

void update_control_key(WindowState& state, const UINT key, const bool pressed)
{
    const auto matches = [&](const std::size_t index) {
        return state.control_keys[index] == key;
    };
    if (matches(0U)) state.left = pressed;
    if (matches(1U)) state.right = pressed;
    if (matches(2U)) state.accelerate = pressed;
    if (matches(3U)) state.brake = pressed;
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
        if (wparam == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        if (state && (lparam & (1LL << 30)) == 0) {
            update_control_key(*state, static_cast<UINT>(wparam), true);
        }
        return 0;
    case WM_KEYUP:
        if (state) {
            update_control_key(*state, static_cast<UINT>(wparam), false);
        }
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

int run_window(bool software, std::optional<unsigned> smoke_frames,
               const std::size_t player_car_index,
               const std::string_view track_name,
               const std::string_view car_name,
               const viper::TerrainSurfaceResourcePayload& terrain,
               const viper::IdealLineTrackData& ideal_line,
               const viper::ModelResource& player_model,
               const viper::ModelResource& ai_model,
               const viper::TrackObjectTextTable& track_objects,
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
    state.route = extract_ideal_line_points(ideal_line);
    state.player_car_index = player_car_index;
    load_keyboard_bindings(state, options);
    const auto title_utf8 = std::string("Viper Racing — ") + std::string(track_name) +
        " & " + std::string(car_name) + " Driving Prototype (WASD / Arrows)";
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
    if (!state.graphics.create(hwnd, software, terrain, ideal_line, player_model,
            ai_model, player_car_index, track_objects)) {
        std::cerr << "Could not initialize Direct3D 11. "
                  << (software ? "WARP initialization failed.\n" : "Try --software to use WARP.\n");
        DestroyWindow(hwnd);
        return 1;
    }
    const auto spawn = state.graphics.car_position(player_car_index);
    if (!state.place_on_route(spawn[0], spawn[1])) {
        std::cerr << "Could not align the grid start with the ideal line and terrain.\n";
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
            const ULONGLONG now = GetTickCount64();
            if (state.last_tick != 0U &&
                !state.advance(static_cast<float>(now - state.last_tick) / 1000.0f)) {
                std::cerr << "Vehicle pose update failed.\n";
                DestroyWindow(hwnd);
                return 1;
            }
            state.last_tick = now;
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

} // namespace

int wmain(int argc, wchar_t** argv)
{
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) { usage(std::cout); return 0; }
        if (options.data_dir.empty())
            throw std::runtime_error("pass --data-dir PATH or set VIPER_DATA_DIR");
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
                    // The helper-mode selector is not reconstructed. Use the
                    // explicitly modeled base division path for preparation.
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
        return run_window(options.software, options.smoke_frames,
            *entrants.player_record_index,
            assets.track_name(), assets.car_name(), assets.terrain(),
            assets.ideal_line(), assets.player_model(), assets.ai_model(), assets.track_objects(),
            assets.options());
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
