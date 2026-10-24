#include "viper/asset_catalog.hpp"
#include "viper/career_season.hpp"
#include "viper/language_resource.hpp"
#include "viper/resource_bundle.hpp"
#include "viper/race_scene_render_flow.hpp"
#include "viper/scene_overlay.hpp"
#include "viper/terrain_surface.hpp"
#include "viper/vehicle_setup.hpp"
#include "viper/world_track_objects.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
           << "       viper_reconstruction --inspect-season BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-stamp BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-image BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-font BUNDLE_PATH RESOURCE_NAME\n"
           << "       viper_reconstruction --inspect-terrain BUNDLE_PATH\n"
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
    std::cout << "bundle\t" << path.string() << '\n'
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

int inspect_terrain(const std::filesystem::path& path)
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
        std::optional<std::string> font_resource_name;
        std::optional<std::string> car_stem;
        bool inspect_track_terrain = false;

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
            if (argument == "--inspect-font") {
                if (i + 2 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-font requires a bundle path and resource name");
                }
                resource_bundle_path = argv[++i];
                font_resource_name = argv[++i];
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
            if (argument == "--inspect-terrain") {
                if (i + 1 >= argc) {
                    throw std::invalid_argument(
                        "--inspect-terrain requires a bundle path");
                }
                resource_bundle_path = argv[++i];
                inspect_track_terrain = true;
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
                static_cast<unsigned>(font_resource_name.has_value()) +
                static_cast<unsigned>(car_stem.has_value()) +
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
            if (font_resource_name) {
                return inspect_font(*resource_bundle_path, *font_resource_name);
            }
            if (car_stem) {
                return inspect_car(*resource_bundle_path, *car_stem);
            }
            if (inspect_track_terrain) {
                return inspect_terrain(*resource_bundle_path);
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
