#include "viper/asset_catalog.hpp"
#include "viper/career_season.hpp"
#include "viper/language_resource.hpp"
#include "viper/model_resource.hpp"
#include "viper/resource_bundle.hpp"
#include "viper/race_scene_render_flow.hpp"
#include "viper/scene_overlay.hpp"
#include "viper/terrain_surface.hpp"
#include "viper/texture_resource.hpp"
#include "viper/track_graphics_resource.hpp"
#include "viper/vehicle_setup.hpp"
#include "viper/world_track_objects.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view kVersion = "0.1.0";

void print_usage(std::ostream& output)
{
    output << "Viper Racing reconstruction tools " << kVersion << '\n'
           << "Usage: viper_reconstruction [--data-dir PATH] [--list-assets]\n"
           << "       viper_reconstruction --list-resources BUNDLE_PATH\n"
           << "       viper_reconstruction --inspect-tire BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-car BUNDLE_PATH CAR_STEM\n"
           << "       viper_reconstruction --inspect-model BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-season BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-stamp BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-image BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-texture BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-grf BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-wobble-models TRACK_BUNDLE GRAF_RESOURCE\n"
           << "       viper_reconstruction --inspect-grf-view BUNDLE_PATH RESOURCE_NAME X Y Z FX FY FZ [DETAIL_LEVEL]\n"
           << "       viper_reconstruction --inspect-font BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-text-table BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-terrain BUNDLE_PATH\n"
           << "       viper_reconstruction --inspect-terrain BUNDLE_PATH --terrain-sample X Y Z\n"
           << "       viper_reconstruction --inspect-language LNG_PATH [KEY]\n"
           << "       viper_reconstruction --help | --version\n\n"
           << "Asset files stay outside this source repository. Set VIPER_DATA_DIR\n"
           << "or pass --data-dir to inspect an extracted game data directory.\n";
}

std::filesystem::path data_directory_from_environment()
{
    const char* value = std::getenv("VIPER_DATA_DIR");
    return value ? std::filesystem::path(value) : std::filesystem::path{};
}

int inspect_assets(const viper::AssetCatalog& catalog)
{
    const auto files = catalog.enumerate();
    std::uintmax_t total_bytes = 0;
    for (const auto& file : files) {
        if (file.size_bytes > std::numeric_limits<std::uintmax_t>::max() - total_bytes) {
            throw std::overflow_error("asset size total overflow");
        }
        total_bytes += file.size_bytes;
        std::cout << file.relative_path.generic_string() << '\t'
                  << file.size_bytes << '\n';
    }
    std::cerr << files.size() << " files, " << total_bytes << " bytes under "
              << catalog.root().string() << '\n';
    return 0;
}

int inspect_resource_bundle(const std::filesystem::path& path)
{
    const auto bundle = viper::ResourceBundle::open(path);
    for (const auto& entry : bundle.entries()) {
        std::cout << entry.name << '\t' << "0x" << std::hex << std::setw(8)
                  << std::setfill('0') << entry.type_code << '\t' << "0x"
                  << std::setw(8) << entry.attributes << std::dec << '\t'
                  << entry.payload_size << '\n';
    }
    std::cerr << bundle.entries().size() << " resources in " << path.string()
              << " (" << bundle.buffered_resource_count()
              << " buffered before mapped resources)\n";
    return 0;
}

int inspect_language_resource(
    const std::filesystem::path& path,
    const std::optional<std::string_view> key)
{
    const auto language = viper::load_language_resource(path);
    std::cout << "language\t" << language.language_name << '\n'
              << "entry_count\t" << language.entries.size() << '\n';
    if (key.has_value()) {
        const auto value = language.find(*key);
        if (!value.has_value()) {
            std::cerr << "language key not found: " << *key << '\n';
            return 1;
        }
        std::cout << "key\t" << *key << '\n'
                  << "value\t" << *value << '\n';
        return 0;
    }
    for (const auto& entry : language.entries) {
        std::cout << entry.key << '\t' << entry.value << '\n';
    }
    return 0;
}

int inspect_tire(const std::filesystem::path& path, const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto payload = bundle.find_payload(
        resource_name, viper::resource_type_code('T', 'I', 'R', 'E'));
    if (!payload) {
        throw std::runtime_error("TIRE resource not found: " + std::string(resource_name));
    }
    const auto tire = viper::parse_tire_resource(*payload);
    if (!tire) {
        throw std::runtime_error("TIRE resource has an unsupported payload size: " +
                                 std::to_string(payload->size()));
    }

    std::cout << "label\t" << tire->label << '\n'
              << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (std::size_t i = 0; i < tire->parameters.size(); ++i) {
        std::cout << "parameter_" << std::setw(2) << std::setfill('0') << i
                  << '\t' << tire->parameters[i] << '\n';
    }
    return 0;
}

int inspect_season(const std::filesystem::path& path,
                   const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto payload = bundle.find_payload(
        resource_name, viper::resource_type_code('S', 'E', 'A', 'S'));
    if (!payload) {
        throw std::runtime_error("SEAS resource not found: " + std::string(resource_name));
    }
    if (payload->size() % sizeof(std::uint32_t) != 0) {
        throw std::runtime_error("SEAS payload length is not DWORD-aligned");
    }

    std::vector<std::uint32_t> words(payload->size() / sizeof(std::uint32_t));
    for (std::size_t word = 0; word < words.size(); ++word) {
        const auto offset = word * sizeof(std::uint32_t);
        words[word] = static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>((*payload)[offset])) |
            (static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>((*payload)[offset + 1])) << 8U) |
            (static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>((*payload)[offset + 2])) << 16U) |
            (static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>((*payload)[offset + 3])) << 24U);
    }

    const auto data = viper::decode_career_season_data(words);
    std::cout << "header_complete\t" << data.header_complete << '\n'
              << "records_complete\t" << data.records_complete << '\n'
              << "exact_payload_size\t" << data.exact_payload_size << '\n'
              << "declared_events\t";
    if (data.declared_event_count) std::cout << *data.declared_event_count;
    else std::cout << "unknown";
    std::cout << '\n';

    if (data.header_complete) {
        std::cout << "header_money_values";
        for (const auto value : data.header_money_values) std::cout << '\t' << value;
        std::cout << '\n';
    }
    for (std::size_t index = 0; index < data.events.size(); ++index) {
        const auto& event = data.events[index];
        std::cout << "event_" << index << "_track\t" << event.track_name << '\n'
                  << "event_" << index << "_field_0x10_low_byte\t"
                  << static_cast<unsigned>(event.field_0x10_low_byte) << '\n'
                  << "event_" << index << "_field_0x14\t" << event.field_0x14 << '\n'
                  << "event_" << index << "_money_values";
        for (const auto value : event.money_values) std::cout << '\t' << value;
        std::cout << '\n' << "event_" << index << "_points_by_placement";
        for (const auto value : event.points_by_placement) std::cout << '\t' << value;
        std::cout << "\n" << "event_" << index << "_raw_words";
        for (const auto value : event.raw_words) std::cout << '\t' << value;
        std::cout << '\n';
    }
    std::cout << "trailing_words\t" << data.trailing_word_count << '\n';
    return data.header_complete && data.records_complete ? 0 : 2;
}

int inspect_car(const std::filesystem::path& path, const std::string_view car_stem)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto resources = viper::parse_car_file_resources(bundle, car_stem, true);
    std::array<float, 3> camera_point{-0.3F, 0.8F, 0.0F};
    bool camera_point_from_cockpit = false;
    if (const auto* cockpit = bundle.find("cockpit.tab",
            viper::TrackObjectTextTable::bats_resource_type_code)) {
        const auto table = viper::parse_track_object_text_table(bundle.payload(*cockpit));
        if (table) {
            if (const auto point = viper::parse_cockpit_camera_point(*table)) {
                camera_point = *point;
                camera_point_from_cockpit = true;
            }
        }
    }
    std::cout << "bundle\t" << path.string() << '\n'
              << "camera_point_source\t"
              << (camera_point_from_cockpit ? "cockpit.tab" : "native fallback") << '\n'
              << "camera_point\t" << camera_point[0] << '\t'
              << camera_point[1] << '\t' << camera_point[2] << '\n'
              << "car_resource\t" << resources.names.car_file_resource_name << '\n'
              << "car_payload\t";
    if (resources.car_file) {
        std::cout << "version " << resources.car_file->resource_version << ", "
                  << resources.car_file->source_payload_size << " bytes, copied "
                  << viper::CarFilePayload::payload_size << " bytes\n";
    } else {
        std::cout << "missing or unsupported\n";
    }
    std::cout << "upgrade_resource\t" << resources.names.upgrade_resource_name << '\n'
              << "upgrade_payload\t";
    if (!resources.upgrades) {
        std::cout << "missing\n";
    } else {
        std::cout << "version " << resources.upgrades->resource_version << ", "
                  << resources.upgrades->input_complete << " complete, "
                  << resources.upgrades->upgrade_sets.size() << " sets\n";
        for (std::size_t i = 0; i < resources.upgrades->upgrade_sets.size(); ++i) {
            std::cout << "upgrade_set_" << i << '\t'
                      << resources.upgrades->upgrade_sets[i].entries.size()
                      << " entries\n";
        }
    }
    return 0;
}

int inspect_model(const std::filesystem::path& path,
                  const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(
        resource_name, viper::ModelResource::resource_type_code);
    if (entry == nullptr) {
        throw std::runtime_error("FNIM model resource not found: " +
                                 std::string(resource_name));
    }
    const auto model = viper::parse_model_resource(
        entry->attributes, bundle.payload(*entry));
    if (!model) throw std::runtime_error("FNIM model resource is malformed or unsupported");
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "version\t" << model->resource_version << '\n'
              << "vertices\t" << model->vertices.size() << '\n'
              << "groups\t" << model->material_groups.size() << '\n';
    for (std::size_t index = 0; index < model->material_groups.size(); ++index) {
        const auto& group = model->material_groups[index];
        std::cout << "group_" << index << "_material\t"
                  << (group.material_name.empty() ? "<empty>" : group.material_name) << '\n'
                  << "group_" << index << "_vertex_range\t"
                  << group.first_vertex << ':' << group.end_vertex << '\n'
                  << "group_" << index << "_triangle_range\t"
                  << group.first_triangle << ':' << group.end_triangle << '\n'
                  << "group_" << index << "_index_count\t"
                  << group.local_indices.size() << '\n';
    }
    return 0;
}

int inspect_track_graphics(const std::filesystem::path& path,
                           const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(resource_name,
        viper::TrackGraphicsResourcePayload::resource_type_code);
    if (entry == nullptr) {
        throw std::runtime_error("GRAF resource not found: " +
            std::string(resource_name));
    }
    const auto graphics = viper::parse_track_graphics_resource(
        entry->attributes, bundle.payload(*entry));
    if (!graphics) {
        throw std::runtime_error("GRAF resource is malformed, unsupported, or contains no recognized chunks");
    }
    const auto summary = viper::summarize_track_graphics(*graphics);
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "version\t" << graphics->resource_version << '\n'
              << "payload_bytes\t" << graphics->source_payload_size << '\n'
              << "chunks\t" << summary.chunk_count << '\n'
              << "corners\t" << summary.corner_count << '\n'
              << "faces\t" << summary.face_count << '\n'
              << "materials\t" << summary.material_count << '\n'
              << "empty_material_names\t" << summary.empty_material_name_count << '\n'
              << "facing_models\t" << summary.facing_model_count << '\n'
              << "opaque_bytes\t" << summary.opaque_bytes << '\n'
              << "scene_graph_complete\t" << (graphics->scene_graph_complete ? 1 : 0) << '\n'
              << "scene_graph_geometry_matched\t" << (graphics->scene_graph_geometry_matched ? 1 : 0) << '\n'
              << "scene_nodes\t" << summary.scene_node_count << '\n'
              << "scene_geometry_nodes\t" << summary.scene_geometry_node_count << '\n'
              << "scene_lod_nodes\t" << summary.scene_lod_node_count << '\n'
              << "scene_facing_nodes\t" << summary.scene_facing_node_count << '\n'
              << "scene_geometry_chunks_matched\t" << summary.scene_geometry_chunks_matched << '\n'
              << "facing_registry_entries\t" << summary.facing_registry_entry_count << '\n'
              << "facing_registry_duplicate_ids\t" << summary.facing_registry_duplicate_id_count << '\n'
              << "facing_registry_invalid_ids\t" << summary.facing_registry_invalid_id_count << '\n';
    for (std::size_t chunk_index = 0; chunk_index < graphics->chunks.size(); ++chunk_index) {
        const auto& chunk = graphics->chunks[chunk_index];
        for (std::size_t material_index = 0;
             material_index < chunk.materials.size(); ++material_index) {
            const auto& material = chunk.materials[material_index];
            std::cout << "chunk_" << chunk_index << "_material_" << material_index
                      << '\t' << (material.texture_name.empty()
                          ? "<empty>" : material.texture_name)
                      << "\tcorners " << material.first_corner << ':'
                      << material.end_corner << "\tfaces " << material.first_face
                      << ':' << material.end_face << '\n';
        }
    }
    return 0;
}

int inspect_track_graphics_view(const std::filesystem::path& path,
                                const std::string_view resource_name,
                                const std::array<float, 3>& camera_position,
                                const std::array<float, 3>& camera_forward,
                                const float detail_level)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(resource_name,
        viper::TrackGraphicsResourcePayload::resource_type_code);
    if (entry == nullptr)
        throw std::runtime_error("GRAF resource not found: " + std::string(resource_name));
    const auto graphics = viper::parse_track_graphics_resource(
        entry->attributes, bundle.payload(*entry));
    if (!graphics)
        throw std::runtime_error("GRAF resource is malformed or unsupported");

    const float detail_factor = 2.0F - detail_level;
    const viper::TrackGraphicsCamera camera{camera_position, camera_forward,
        1000.0F, 4.0F, detail_factor * detail_factor};
    const auto selection = viper::select_track_graphics_nodes(*graphics, camera);
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "selection_complete\t" << (selection.traversal_complete ? 1 : 0) << '\n'
              << "visited_nodes\t" << selection.visited_node_count << '\n'
              << "visited_type_1\t" << selection.visited_type_counts[1] << '\n'
              << "visited_type_2\t" << selection.visited_type_counts[2] << '\n'
              << "visited_type_3\t" << selection.visited_type_counts[3] << '\n'
              << "visited_type_4\t" << selection.visited_type_counts[4] << '\n'
              << "ordinary_nodes\t" << selection.ordinary_node_offsets.size() << '\n'
              << "facing_mode_1_nodes\t" << selection.facing_mode_1_node_offsets.size() << '\n'
              << "facing_mode_2_nodes\t" << selection.facing_mode_2_node_offsets.size() << '\n'
              << "registered_mode_4_nodes\t" << selection.registered_mode_4_node_offsets.size() << '\n';
    return selection.traversal_complete ? 0 : 2;
}

int inspect_wobble_model_bindings(const std::filesystem::path& path,
                                  const std::string_view graf_resource_name)
{
    constexpr auto bats_type = viper::TrackObjectTextTable::bats_resource_type_code;
    constexpr auto graf_type = viper::TrackGraphicsResourcePayload::resource_type_code;
    const auto bundle = viper::ResourceBundle::open(path);
    const auto bats_payload = bundle.find_payload("track.obt", bats_type);
    const auto* graf_entry = bundle.find(graf_resource_name, graf_type);
    if (!bats_payload || graf_entry == nullptr)
        throw std::runtime_error("track.obt or requested GRAF resource is missing");
    const auto table = viper::parse_track_object_text_table(*bats_payload);
    const auto graphics = viper::parse_track_graphics_resource(
        graf_entry->attributes, bundle.payload(*graf_entry));
    if (!table || !graphics)
        throw std::runtime_error("track object table or GRAF payload is malformed");

    std::size_t wobble_count = 0U;
    std::size_t resolved_count = 0U;
    std::size_t unresolved_count = 0U;
    std::size_t malformed_count = 0U;
    std::size_t duplicate_id_count = 0U;
    std::unordered_set<std::int32_t> seen_model_ids;
    std::cout << "bundle\t" << path.string() << '\n'
              << "graf_resource\t" << graf_resource_name << '\n';
    for (std::size_t row = 1U; row < table->line_count; ++row) {
        const auto raw_line = viper::track_object_text_line(*table, row);
        if (!raw_line) throw std::runtime_error("track.obt line is out of bounds");
        const auto directive = viper::classify_track_directive(*raw_line);
        if (directive.kind == viper::TrackDirectiveKind::end) break;
        if (directive.kind != viper::TrackDirectiveKind::object) continue;

        std::array<char, 256> object_type{};
        int model_id = 0;
        const std::string line(directive.object_line);
        const int converted = std::sscanf(line.c_str(), "obj wobble %255s %d",
            object_type.data(), &model_id);
        if (converted == 0) continue;
        ++wobble_count;
        const auto record = viper::make_wobble_track_record({object_type.data(),
            static_cast<std::uint32_t>(converted), model_id});
        if (!record) {
            ++malformed_count;
            std::cout << "row_" << row << "\tmalformed\t" << directive.object_line << '\n';
            continue;
        }
        if (!seen_model_ids.insert(record->parsed_parameter_0x18).second)
            ++duplicate_id_count;
        const auto node_offset = viper::lookup_registered_facing_node(
            *graphics, record->parsed_parameter_0x18);
        if (!node_offset) {
            ++unresolved_count;
            std::cout << "row_" << row << "\tunresolved_id\t"
                      << record->parsed_parameter_0x18 << '\n';
            continue;
        }
        const auto node = std::find_if(graphics->scene_nodes.begin(),
            graphics->scene_nodes.end(), [node_offset](const auto& candidate) {
                return candidate.source_offset == *node_offset;
            });
        if (node == graphics->scene_nodes.end() || !node->parsed_chunk_index ||
            *node->parsed_chunk_index >= graphics->chunks.size()) {
            ++unresolved_count;
            std::cout << "row_" << row << "\tregistry_node_without_mesh\t"
                      << record->parsed_parameter_0x18 << '\t' << *node_offset << '\n';
            continue;
        }
        ++resolved_count;
        const auto& center = graphics->chunks[*node->parsed_chunk_index].center;
        std::cout << "row_" << row << "\tresolved\t"
                  << record->parsed_parameter_0x18 << '\t' << *node_offset << '\t'
                  << center[0] << '\t' << center[1] << '\t' << center[2] << '\n';
    }
    std::cout << "wobble_rows\t" << wobble_count << '\n'
              << "resolved_models\t" << resolved_count << '\n'
              << "unresolved_models\t" << unresolved_count << '\n'
              << "duplicate_model_ids\t" << duplicate_id_count << '\n'
              << "malformed_rows\t" << malformed_count << '\n';
    return 0;
}

float parse_finite_float(const std::string_view text, const std::string_view label)
{
    std::size_t parsed = 0U;
    const auto value = std::stof(std::string(text), &parsed);
    if (parsed != text.size() || !std::isfinite(value))
        throw std::invalid_argument(std::string(label) + " must be a finite number");
    return value;
}

int inspect_terrain(const std::filesystem::path& path,
                    const std::optional<std::array<float, 3>>& sample_position)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto terrain = viper::parse_track_terrain_resource(bundle);
    if (!terrain) {
        throw std::runtime_error("track.bpp TPPB v2 resource is missing or malformed");
    }
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\ttrack.bpp\n"
              << "version\t" << terrain->resource_version << '\n'
              << "planes\t" << terrain->planes.size() << '\n'
              << "partition_nodes\t" << terrain->partition_nodes.size() << '\n'
              << "root_node_index\t" << terrain->root_node_index << '\n';
    if (sample_position) {
        const auto sample = viper::sample_terrain_resource(
            *terrain, *sample_position, true);
        std::cout << "sample_x\t" << (*sample_position)[0] << '\n'
                  << "sample_y\t" << (*sample_position)[1] << '\n'
                  << "sample_z\t" << (*sample_position)[2] << '\n'
                  << "query_succeeded\t" << sample.query_succeeded << '\n'
                  << "selected_plane_index\t" << sample.selected_plane_index << '\n'
                  << "quantized_lookup_x\t" << sample.quantized_lookup_position[0] << '\n'
                  << "quantized_lookup_z\t" << sample.quantized_lookup_position[2] << '\n';
        if (sample.query_succeeded) {
            std::cout << "terrain_point_x\t" << sample.sampled_position[0] << '\n'
                      << "terrain_point_y\t" << sample.sampled_position[1] << '\n'
                      << "terrain_point_z\t" << sample.sampled_position[2] << '\n'
                      << "plane_normal_x\t" << sample.plane_normal[0] << '\n'
                      << "plane_normal_y\t" << sample.plane_normal[1] << '\n'
                      << "plane_normal_z\t" << sample.plane_normal[2] << '\n'
                      << "surface_value_0x34\t" << sample.surface_value_0x34 << '\n';
        }
    }
    return 0;
}

int inspect_stamp(const std::filesystem::path& path,
                  const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(
        resource_name, viper::resource_type_code('S', 'T', 'M', 'P'));
    if (entry == nullptr) {
        throw std::runtime_error("STMP resource not found: " +
                                 std::string(resource_name));
    }
    const auto stamp = viper::parse_stamp_payload(bundle.payload(*entry), true);
    if (!stamp) {
        throw std::runtime_error("STMP resource is shorter than its observed header");
    }
    const auto invalid_targets = std::count_if(
        stamp->pointer_entries.begin(), stamp->pointer_entries.end(),
        [](const auto& pointer) { return !pointer.target_within_payload; });
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "attributes\t0x" << std::hex << std::setw(8)
              << std::setfill('0') << entry->attributes << std::dec << '\n'
              << "payload_bytes\t" << entry->payload_size << '\n'
              << "factor_0x04\t" << stamp->factor_0x04 << '\n'
              << "factor_0x10\t" << stamp->factor_0x10 << '\n'
              << "pointer_entry_count\t" << stamp->pointer_entry_count << '\n'
              << "embedded_data_offset\t" << stamp->embedded_data_offset << '\n'
              << "pointer_table_complete\t" << stamp->pointer_table_complete << '\n'
              << "fixups_projected\t" << stamp->pointer_offsets_relocated << '\n'
              << "out_of_payload_targets\t" << invalid_targets << '\n';
    return stamp->pointer_table_complete && invalid_targets == 0 ? 0 : 2;
}

int inspect_font(const std::filesystem::path& path,
                 const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(
        resource_name, viper::resource_type_code('S', 'T', 'M', 'P'));
    if (entry == nullptr) {
        throw std::runtime_error("STMP font resource not found: " +
                                 std::string(resource_name));
    }
    const auto font = viper::parse_stamp_font_index(bundle.payload(*entry));
    if (!font || !font->glyph_table_complete ||
        !font->header_metrics_complete) {
        throw std::runtime_error(
            "STMP font metrics or glyph table are missing or truncated");
    }
    std::size_t populated_count = 0;
    for (const auto& character : font->characters) {
        if (character.populated) ++populated_count;
    }
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "glyph_record_count\t" << font->glyph_record_count << '\n'
              << "font_height_0x04\t" << font->font_height_0x04 << '\n'
              << "header_metric_0x08\t" << font->header_metric_0x08 << '\n'
              << "header_metric_0x0c\t" << font->header_metric_0x0c << '\n'
              << "layout_top_metric_0x44dbc0\t"
              << font->header_metric_0x0c << '\n'
              << "layout_bottom_metric_0x44dc00\t"
              << std::bit_cast<std::int32_t>(
                     static_cast<std::uint32_t>(font->font_height_0x04) -
                     static_cast<std::uint32_t>(font->header_metric_0x0c))
              << '\n'
              << "populated_character_count\t" << populated_count << '\n';
    for (std::size_t code = 0; code < font->characters.size(); ++code) {
        const auto& character = font->characters[code];
        if (!character.populated) continue;
        std::cout << "character\t" << code << '\t'
                  << character.glyph_index << '\t'
                  << character.metric_0x204 << '\t'
                  << character.metric_0x404 << '\n';
    }
    return 0;
}

int inspect_stamp_image(const std::filesystem::path& path,
                        const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(
        resource_name, viper::resource_type_code('S', 'T', 'M', 'P'));
    if (entry == nullptr) {
        throw std::runtime_error("STMP image resource not found: " +
                                 std::string(resource_name));
    }
    const auto image = viper::parse_stamp_image(bundle.payload(*entry), true);
    if (!image.has_value()) {
        throw std::runtime_error("STMP image header or palette is truncated");
    }
    std::size_t encoded_bytes = 0;
    for (const auto& frame : image->encoded_rows) {
        for (const auto& row : frame) encoded_bytes += row.size();
    }
    const auto first_frame_for_mode = [&](const std::uint8_t mode) {
        viper::SceneDashboardRasterObservation raster_observation{};
        raster_observation.renderer_mode = mode;
        raster_observation.clip_right = image->width;
        raster_observation.clip_bottom = image->height;
        raster_observation.framebuffer_pitch_bytes =
            image->width * (mode == 5U ? 4 : 2);
        return viper::raster_scene_stamp_image(
            {true, 0x0044D0C0U, 0, 0, 0},
            {true, *image, raster_observation});
    };
    const auto first_frame_mode4 = first_frame_for_mode(4U);
    const auto first_frame_mode5 = first_frame_for_mode(5U);
    std::optional<std::size_t> status_source_bytes_mode4;
    std::optional<std::size_t> status_source_bytes_mode5;
    if (entry->name == "status.stp") {
        const auto prepare_source = [&](const std::uint8_t mode) {
            viper::RaceLoopSharedOverlayObservation observation{};
            observation.state0_bitmap_blit.source_bitmap_mode = mode;
            observation.state0_bitmap_blit.bitmap_width = image->width;
            observation.state0_bitmap_blit.bitmap_height = image->height;
            observation.state0_bitmap_blit.bitmap_pitch_bytes =
                image->width * (mode == 5U ? 4 : 2);
            const auto complete =
                viper::populate_state0_dashboard_background_source_from_bundle(
                    observation, bundle);
            return std::pair{complete,
                observation.state0_bitmap_source_pixels.size()};
        };
        const auto mode4_source = prepare_source(4U);
        const auto mode5_source = prepare_source(5U);
        if (mode4_source.first) {
            status_source_bytes_mode4 = mode4_source.second;
        }
        if (mode5_source.first) {
            status_source_bytes_mode5 = mode5_source.second;
        }
    }
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "width\t" << image->width << '\n'
              << "height\t" << image->height << '\n'
              << "origin_x\t" << image->origin_x << '\n'
              << "origin_y\t" << image->origin_y << '\n'
              << "frame_count\t" << image->frame_count << '\n'
              << "pointer_table_complete\t"
              << image->pointer_table_complete << '\n'
              << "rows_complete\t" << image->rows_complete << '\n'
              << "encoded_row_bytes\t" << encoded_bytes << '\n'
              << "frame_0_palette_pixels_mode4\t"
              << first_frame_mode4.raster.pixel_writes.size() << '\n'
              << "frame_0_raster_complete_mode4\t"
              << first_frame_mode4.input_complete << '\n'
              << "frame_0_palette_pixels_mode5\t"
              << first_frame_mode5.raster.pixel_writes.size() << '\n'
              << "frame_0_raster_complete_mode5\t"
              << first_frame_mode5.input_complete << '\n';
    if (entry->name == "status.stp") {
        std::cout << "status_source_bytes_mode4\t"
                  << (status_source_bytes_mode4.has_value()
                          ? *status_source_bytes_mode4
                          : 0U)
                  << '\n'
                  << "status_source_bytes_mode5\t"
                  << (status_source_bytes_mode5.has_value()
                          ? *status_source_bytes_mode5
                          : 0U)
                  << '\n';
    }
    return image->pointer_table_complete && image->rows_complete &&
            first_frame_mode4.input_complete &&
            first_frame_mode5.input_complete &&
            (entry->name != "status.stp" ||
             (status_source_bytes_mode4.has_value() &&
              status_source_bytes_mode5.has_value()))
        ? 0
        : 2;
}

int inspect_texture(const std::filesystem::path& path,
                    const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto* entry = bundle.find(
        resource_name, viper::resource_type_code('T', 'E', 'X', ' '));
    if (entry == nullptr) {
        throw std::runtime_error("TEX texture resource not found: " +
                                 std::string(resource_name));
    }
    const auto texture = viper::parse_texture_payload(bundle.payload(*entry));
    if (!texture) {
        throw std::runtime_error(
            "TEX payload is malformed or uses an unsupported pixel variant");
    }
    std::size_t transparent_pixels = 0U;
    for (std::size_t pixel = 0U; pixel < texture->rgba.size() / 4U; ++pixel) {
        if (texture->rgba[pixel * 4U + 3U] == 0U) ++transparent_pixels;
    }
    std::cout << "bundle\t" << path.string() << '\n'
              << "resource\t" << entry->name << '\n'
              << "payload_bytes\t" << entry->payload_size << '\n'
              << "flags\t0x" << std::hex << static_cast<unsigned>(texture->flags)
              << std::dec << '\n'
              << "geometry_usage\t"
              << static_cast<unsigned>(texture->geometry_usage) << '\n'
              << "wrap\t" << static_cast<unsigned>(texture->wrap) << '\n'
              << "color_key\t0x" << std::hex << texture->color_key << std::dec << '\n'
              << "mip_count\t" << texture->mip_count << '\n'
              << "width\t" << texture->width << '\n'
              << "height\t" << texture->height << '\n'
              << "decoded_rgba_bytes\t" << texture->rgba.size() << '\n'
              << "transparent_pixels\t" << transparent_pixels << '\n';
    return 0;
}

int inspect_text_table(const std::filesystem::path& path,
                       const std::string_view resource_name)
{
    const auto bundle = viper::ResourceBundle::open(path);
    const auto payload = bundle.find_payload(
        resource_name, viper::TrackObjectTextTable::bats_resource_type_code);
    if (!payload) {
        throw std::runtime_error("STAB text-table resource not found: " +
                                 std::string(resource_name));
    }
    const auto table = viper::parse_track_object_text_table(*payload);
    if (!table) {
        throw std::runtime_error("STAB text-table payload is invalid: " +
                                 std::string(resource_name));
    }
    std::cout << "line_count\t" << table->line_count << '\n'
              << "line_stride\t" << table->line_stride << '\n';
    for (std::size_t row = 0; row < table->line_count; ++row) {
        std::cout << "row_" << row;
        for (std::size_t column = 0; column < table->line_offsets.size(); ++column) {
            const auto text = viper::track_object_text_line(*table, row, column);
            std::cout << '\t' << (text ? *text : std::string_view{});
        }
        std::cout << '\n';
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        auto data_directory = data_directory_from_environment();
        bool list_assets = false;
        std::optional<std::filesystem::path> resource_bundle_path;
        std::optional<std::filesystem::path> language_resource_path;
        std::optional<std::string> language_key;
        std::optional<std::string> tire_resource_name;
        std::optional<std::string> season_resource_name;
        std::optional<std::string> stamp_resource_name;
        std::optional<std::string> stamp_image_resource_name;
        std::optional<std::string> texture_resource_name;
        std::optional<std::string> font_resource_name;
        std::optional<std::string> text_table_resource_name;
        std::optional<std::string> car_stem;
        std::optional<std::string> model_resource_name;
        std::optional<std::string> grf_resource_name;
        std::optional<std::string> wobble_graf_resource_name;
        std::optional<std::array<float, 6>> grf_view_camera;
        float grf_view_detail_level = 1.0F;
        bool inspect_track_terrain = false;
        std::optional<std::array<float, 3>> terrain_sample_position;

        for (int i = 1; i < argc; ++i) {
            const std::string_view argument(argv[i]);
            if (argument == "--help" || argument == "-h") {
                print_usage(std::cout);
                return 0;
            }
            if (argument == "--version") {
                std::cout << kVersion << '\n';
                return 0;
            }
            if (argument == "--list-assets") {
                list_assets = true;
                continue;
            }
            if (argument == "--list-resources") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--list-resources requires a bundle path");
                }
                resource_bundle_path = argv[++i];
                continue;
            }
            if (argument == "--inspect-language") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-language requires an LNG path");
                }
                language_resource_path = argv[++i];
                if (i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
                    language_key = argv[++i];
                }
                continue;
            }
            if (argument == "--inspect-tire") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-tire requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                tire_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-season") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-season requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                season_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-stamp") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-stamp requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                stamp_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-image") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-image requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                stamp_image_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-texture") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-texture requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                texture_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-font") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-font requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                font_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-text-table") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-text-table requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                text_table_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-car") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-car requires a bundle path and car stem");
                }
                resource_bundle_path = argv[++i];
                car_stem = argv[++i];
                continue;
            }
            if (argument == "--inspect-model") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-model requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                model_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-grf") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-grf requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                grf_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-wobble-models") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-wobble-models requires a track bundle and GRAF resource name");
                }
                resource_bundle_path = argv[++i];
                wobble_graf_resource_name = argv[++i];
                continue;
            }
            if (argument == "--inspect-grf-view") {
                if (i + 8 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-grf-view requires bundle, resource, camera position, and forward vector");
                }
                resource_bundle_path = argv[++i];
                grf_resource_name = argv[++i];
                std::array<float, 6> view{};
                constexpr std::array<std::string_view, 6> labels{
                    "camera X", "camera Y", "camera Z",
                    "forward X", "forward Y", "forward Z"};
                for (std::size_t index = 0; index < view.size(); ++index)
                    view[index] = parse_finite_float(argv[++i], labels[index]);
                grf_view_camera = view;
                if (i + 1 < argc &&
                    !std::string_view(argv[i + 1]).starts_with("--"))
                    grf_view_detail_level = parse_finite_float(argv[++i],
                        "detail level");
                continue;
            }
            if (argument == "--inspect-terrain") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-terrain requires a bundle path");
                }
                resource_bundle_path = argv[++i];
                inspect_track_terrain = true;
                continue;
            }
            if (argument == "--terrain-sample") {
                if (i + 3 >= argc) {
                    throw std::invalid_argument(
                        "--terrain-sample requires X, Y, and Z coordinates");
                }
                terrain_sample_position = std::array<float, 3>{
                    parse_finite_float(argv[++i], "terrain sample X"),
                    parse_finite_float(argv[++i], "terrain sample Y"),
                    parse_finite_float(argv[++i], "terrain sample Z")};
                continue;
            }
            if (argument == "--data-dir") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument("--data-dir requires a path");
                }
                data_directory = argv[++i];
                continue;
            }
            throw std::invalid_argument("unknown option: " + std::string(argument));
        }

        if (terrain_sample_position && !inspect_track_terrain) {
            throw std::invalid_argument(
                "--terrain-sample requires --inspect-terrain BUNDLE_PATH");
        }
        if (list_assets && resource_bundle_path) {
            throw std::invalid_argument("--list-assets and --list-resources are separate actions");
        }
        if (language_resource_path.has_value()) {
            if (list_assets || resource_bundle_path.has_value()) {
                throw std::invalid_argument(
                    "language and resource-bundle inspection are separate actions");
            }
            return inspect_language_resource(*language_resource_path,
                language_key.has_value()
                    ? std::optional<std::string_view>(*language_key)
                    : std::nullopt);
        }
        if (resource_bundle_path) {
            if (list_assets) {
                throw std::invalid_argument("asset and resource inspection are separate actions");
            }
            const auto inspection_count = static_cast<unsigned>(tire_resource_name.has_value()) +
                static_cast<unsigned>(season_resource_name.has_value()) +
                static_cast<unsigned>(stamp_resource_name.has_value()) +
                static_cast<unsigned>(stamp_image_resource_name.has_value()) +
                static_cast<unsigned>(texture_resource_name.has_value()) +
                static_cast<unsigned>(font_resource_name.has_value()) +
                static_cast<unsigned>(text_table_resource_name.has_value()) +
                static_cast<unsigned>(car_stem.has_value()) +
                static_cast<unsigned>(model_resource_name.has_value()) +
                static_cast<unsigned>(grf_resource_name.has_value()) +
                static_cast<unsigned>(wobble_graf_resource_name.has_value()) +
                static_cast<unsigned>(inspect_track_terrain);
            if (inspection_count > 1U) {
                throw std::invalid_argument(
                    "choose one resource inspection action per invocation");
            }
            if (tire_resource_name) {
                return inspect_tire(*resource_bundle_path, *tire_resource_name);
            }
            if (season_resource_name) {
                return inspect_season(*resource_bundle_path, *season_resource_name);
            }
            if (stamp_resource_name) {
                return inspect_stamp(*resource_bundle_path, *stamp_resource_name);
            }
            if (stamp_image_resource_name) {
                return inspect_stamp_image(
                    *resource_bundle_path, *stamp_image_resource_name);
            }
            if (texture_resource_name) {
                return inspect_texture(*resource_bundle_path,
                                       *texture_resource_name);
            }
            if (font_resource_name) {
                return inspect_font(*resource_bundle_path, *font_resource_name);
            }
            if (text_table_resource_name) {
                return inspect_text_table(*resource_bundle_path,
                    *text_table_resource_name);
            }
            if (car_stem) {
                return inspect_car(*resource_bundle_path, *car_stem);
            }
            if (model_resource_name) {
                return inspect_model(*resource_bundle_path, *model_resource_name);
            }
            if (grf_resource_name) {
                if (grf_view_camera) {
                    return inspect_track_graphics_view(*resource_bundle_path,
                        *grf_resource_name,
                        {(*grf_view_camera)[0], (*grf_view_camera)[1],
                            (*grf_view_camera)[2]},
                        {(*grf_view_camera)[3], (*grf_view_camera)[4],
                            (*grf_view_camera)[5]}, grf_view_detail_level);
                }
                return inspect_track_graphics(*resource_bundle_path,
                    *grf_resource_name);
            }
            if (wobble_graf_resource_name) {
                return inspect_wobble_model_bindings(*resource_bundle_path,
                    *wobble_graf_resource_name);
            }
            if (inspect_track_terrain) {
                return inspect_terrain(*resource_bundle_path,
                    terrain_sample_position);
            }
            if (terrain_sample_position) {
                throw std::invalid_argument(
                    "--terrain-sample requires --inspect-terrain");
            }
            return inspect_resource_bundle(*resource_bundle_path);
        }
        if (!list_assets) {
            print_usage(std::cout);
            return 0;
        }
        if (data_directory.empty()) {
            throw std::invalid_argument(
                "pass --data-dir or set VIPER_DATA_DIR before --list-assets");
        }
        return inspect_assets(viper::AssetCatalog::open(data_directory));
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }
}
