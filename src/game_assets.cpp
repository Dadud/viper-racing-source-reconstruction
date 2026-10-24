#include "viper/game_assets.hpp"

#include <array>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace viper {
namespace {

constexpr std::array<std::string_view, 3> kSharedBundleNames{
    "common.res", "ui.res", "race.res"};
constexpr std::uint32_t kAIDriverResourceType = 0x41494472U;
constexpr std::uint32_t kTireResourceType = 0x54495245U;

std::string normalize_asset_stem(const std::string_view value, const char* label)
{
    if (value.empty()) throw std::runtime_error(std::string(label) + " name is empty");
    std::string result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        const bool upper = ch >= 'A' && ch <= 'Z';
        const bool lower = ch >= 'a' && ch <= 'z';
        const bool digit = ch >= '0' && ch <= '9';
        if (!upper && !lower && !digit && ch != '-' && ch != '_')
            throw std::runtime_error(std::string(label) +
                " name must contain only letters, digits, '-' or '_'");
        result.push_back(static_cast<char>(upper ? ch + ('a' - 'A') : ch));
    }
    return result;
}

bool ascii_equal_case_insensitive(const std::string_view left,
                                  const std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const auto a = static_cast<unsigned char>(left[i]);
        const auto b = static_cast<unsigned char>(right[i]);
        const auto lower = [](const unsigned char value) {
            return value >= 'A' && value <= 'Z'
                ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
        };
        if (lower(a) != lower(b)) return false;
    }
    return true;
}

} // namespace

GameAssets GameAssets::load(const std::filesystem::path& root,
                            const std::string_view track,
                            const std::string_view car,
                            std::optional<GameOptions> options,
                            std::filesystem::path setup_config_directory)
{
    if (root.empty()) throw std::runtime_error("asset directory is required");
    GameAssets assets;
    assets.root_ = root;
    assets.track_name_ = normalize_asset_stem(track, "track");
    assets.car_name_ = normalize_asset_stem(car, "car");
    for (std::size_t i = 0; i < kSharedBundleNames.size(); ++i)
        assets.bundle_names_[i] = kSharedBundleNames[i];
    assets.bundle_names_[3] = assets.car_name_ + ".car";
    assets.bundle_names_[4] = assets.track_name_ + ".trk";
    assets.bundle_names_[5] = "drivers.res";

    for (std::size_t i = 0; i < 6U; ++i) {
        const auto& bundle_name = assets.bundle_names_[i];
        const auto path = root / std::filesystem::path(bundle_name);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec) {
            throw std::runtime_error("missing required regular file: " + path.string());
        }
        try {
            assets.bundles_[i] = ResourceBundle::open(path);
        } catch (const std::exception& e) {
            throw std::runtime_error("invalid resource bundle " + path.string() +
                ": " + e.what());
        }
    }
    const auto& common_bundle = assets.bundle(assets.bundle_names_[0]);
    constexpr std::uint32_t tire_type = 0x54495245U;
    const auto* default_tire_entry = common_bundle.find("def_tire.tir", tire_type);
    if (default_tire_entry == nullptr)
        throw std::runtime_error("common.res default tire resource def_tire.tir is missing");
    const auto parsed_default_tire = parse_tire_resource(
        common_bundle.payload(*default_tire_entry));
    if (!parsed_default_tire)
        throw std::runtime_error("common.res default tire resource def_tire.tir is invalid");
    assets.default_tire_ = *parsed_default_tire;
    const auto& driver_bundle = assets.bundle(assets.bundle_names_[5]);
    const auto* ai_driver_entry = driver_bundle.find("aidriver.adr", kAIDriverResourceType);
    if (ai_driver_entry == nullptr)
        throw std::runtime_error("drivers.res AIDR resource aidriver.adr is missing");
    const auto ai_drivers = parse_ai_driver_resource(
        ai_driver_entry->attributes, driver_bundle.payload(*ai_driver_entry));
    if (!ai_drivers)
        throw std::runtime_error("drivers.res aidriver.adr has an unsupported version, size, or group count");
    assets.ai_drivers_ = *ai_drivers;
    assets.options_ = options ? std::move(*options) : GameOptions{};
    constexpr std::uint32_t car_setup_type = 0x43435330U;
    const auto& car_bundle = assets.bundle(assets.bundle_names_[3]);
    for (std::size_t wheel_index = 0;
         wheel_index < assets.wheel_tires_.size(); ++wheel_index) {
        const char axle_suffix = wheel_index < 2U ? 'f' : 'r';
        const auto tire_name = assets.car_name_ + axle_suffix + ".tir";
        const ResourceEntry* tire_entry = car_bundle.find(
            tire_name, kTireResourceType);
        const ResourceBundle* tire_bundle = &car_bundle;
        if (tire_entry == nullptr) {
            tire_entry = common_bundle.find("def_tire.tir", kTireResourceType);
            tire_bundle = &common_bundle;
        }
        if (tire_entry == nullptr) {
            throw std::runtime_error("required tire resource is missing: " +
                tire_name + " and common.res def_tire.tir");
        }
        if (tire_entry->attributes != 1U) {
            throw std::runtime_error("tire resource " + tire_entry->name +
                " has an unsupported version");
        }
        const auto tire = parse_tire_resource(tire_bundle->payload(*tire_entry));
        if (!tire) {
            throw std::runtime_error("tire resource " + tire_entry->name +
                " has an unsupported payload size");
        }
        assets.wheel_tires_[wheel_index] = *tire;
    }
    if (!setup_config_directory.empty()) {
        const auto player_name = assets.options_.get("GLOBAL", "player_name");
        if (player_name && !player_name->empty() && *player_name != "." &&
            *player_name != ".." &&
            std::all_of(player_name->begin(), player_name->end(), [](const char ch) {
                return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                    (ch >= '0' && ch <= '9') || ch == ' ' || ch == '_' ||
                    ch == '-' || ch == '.';
            })) {
            const auto setup_path = setup_config_directory / "setups" /
                (std::string(*player_name) + ".csu");
            std::error_code setup_error;
            const auto setup_size = std::filesystem::file_size(setup_path,
                setup_error);
            if (!setup_error && setup_size == HumanCarSetupFile::file_size) {
                std::array<std::byte, HumanCarSetupFile::file_size> setup_bytes{};
                std::ifstream setup_file(setup_path, std::ios::binary);
                setup_file.read(reinterpret_cast<char*>(setup_bytes.data()),
                    static_cast<std::streamsize>(setup_bytes.size()));
                if (setup_file.gcount() ==
                        static_cast<std::streamsize>(setup_bytes.size())) {
                    const auto parsed_setup = parse_human_car_setup_file(setup_bytes);
                    if (parsed_setup) {
                        assets.player_car_setup_ = parsed_setup->active_setup;
                        assets.player_setup_file_loaded_ = true;
                        assets.player_setup_version_warning_ =
                            parsed_setup->version_warning;
                    }
                }
            }
        }
    }
    if (!assets.player_setup_file_loaded_) {
        const ResourceEntry* car_setup_entry = car_bundle.find(
            assets.car_name_ + ".ccs", car_setup_type);
        const ResourceBundle* setup_bundle = &car_bundle;
        if (car_setup_entry == nullptr) {
            car_setup_entry = common_bundle.find("default.ccs", car_setup_type);
            setup_bundle = &common_bundle;
        }
        if (car_setup_entry == nullptr)
            throw std::runtime_error(assets.car_name_ +
                " human setup and common default.ccs resources are missing");
        const auto car_setup_payload = setup_bundle->payload(*car_setup_entry);
        if (car_setup_entry->attributes != 2U || car_setup_payload.size() !=
                assets.player_car_setup_.size())
            throw std::runtime_error("human setup resource " + car_setup_entry->name +
                " has an unsupported version or size");
        std::copy(car_setup_payload.begin(), car_setup_payload.end(),
            assets.player_car_setup_.begin());
    }
    const auto language_path = root / "english.lng";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(language_path, ec) || ec) {
        throw std::runtime_error("missing required regular file: " + language_path.string());
    }
    try {
        assets.language_ = load_language_resource(language_path);
    } catch (const std::exception& e) {
        throw std::runtime_error("invalid language resource " + language_path.string() + ": " + e.what());
    }
    const auto& track_bundle = assets.bundle(assets.bundle_names_[4]);
    const auto* track_objects_entry = track_bundle.find(
        "track.obt", TrackObjectTextTable::bats_resource_type_code);
    if (track_objects_entry == nullptr)
        throw std::runtime_error(assets.track_name_ +
            " track.obt world-object resource is missing");
    const auto track_objects = parse_track_object_text_table(
        track_bundle.payload(*track_objects_entry));
    if (!track_objects)
        throw std::runtime_error(assets.track_name_ +
            " track.obt world-object resource is invalid");
    assets.track_objects_ = *track_objects;
    const auto tracks_payload = common_bundle.find_payload(
        "tracks.tab", TrackObjectTextTable::bats_resource_type_code);
    if (tracks_payload) {
        const auto tracks = parse_track_object_text_table(*tracks_payload);
        if (tracks) {
            for (std::size_t index = 0; index < tracks->line_count; ++index) {
                const auto name = track_object_text_line(*tracks, index, 0);
                if (name && ascii_equal_case_insensitive(*name, assets.track_name_)) {
                    assets.track_index_ = static_cast<std::uint32_t>(index);
                    break;
                }
            }
        }
    }
    const auto* ai_setup_entry = track_bundle.find("aidef.ccs", car_setup_type);
    if (ai_setup_entry == nullptr)
        throw std::runtime_error(assets.track_name_ +
            " AI default setup resource aidef.ccs is missing");
    const auto ai_setup_payload = track_bundle.payload(*ai_setup_entry);
    if (ai_setup_entry->attributes != 2U || ai_setup_payload.size() !=
            assets.ai_vehicle_setup_.size())
        throw std::runtime_error(assets.track_name_ +
            " AI default setup resource has an unsupported version or size");
    std::copy(ai_setup_payload.begin(), ai_setup_payload.end(),
        assets.ai_vehicle_setup_.begin());
    const auto terrain = parse_track_terrain_resource(track_bundle);
    if (!terrain) {
        throw std::runtime_error(assets.track_name_ +
            " track.bpp terrain resource is missing or unsupported");
    }
    assets.terrain_ = *terrain;
    constexpr std::uint32_t ideal_line_type = 0x494c494eU;
    const auto* ideal_line_entry = track_bundle.find("track.ild", ideal_line_type);
    if (ideal_line_entry == nullptr) {
        throw std::runtime_error(assets.track_name_ +
            " track.ild ideal-line resource is missing");
    }
    const auto ideal_line_payload = parse_ideal_line_resource(
        ideal_line_entry->attributes,
        track_bundle.payload(*ideal_line_entry));
    if (!ideal_line_payload) {
        throw std::runtime_error(assets.track_name_ +
            " track.ild ideal-line resource is invalid");
    }
    const auto normalized_ideal_line = normalize_ideal_line_resource(*ideal_line_payload, 0U);
    if (!normalized_ideal_line) {
        throw std::runtime_error(assets.track_name_ +
            " track.ild could not be normalized");
    }
    assets.ideal_line_ = *normalized_ideal_line;
    assets.player_car_ = parse_car_file_resources(car_bundle, assets.car_name_, true);
    if (!assets.player_car_.car_file) {
        throw std::runtime_error(assets.car_name_ +
            " CARF version 6 resource is missing or invalid");
    }
    const ResourceBundle* ai_car_bundle = &car_bundle;
    if (assets.car_name_ != "viper") {
        const auto ai_car_path = root / "viper.car";
        std::error_code ai_car_error;
        if (!std::filesystem::is_regular_file(ai_car_path, ai_car_error) || ai_car_error)
            throw std::runtime_error("missing required AI vehicle bundle: " + ai_car_path.string());
        assets.bundles_[6] = ResourceBundle::open(ai_car_path);
        assets.bundle_names_[6] = "viper.car";
        ai_car_bundle = &assets.bundles_[6];
    }
    assets.ai_car_ = parse_car_file_resources(*ai_car_bundle, "viper", false);
    if (!assets.ai_car_.car_file)
        throw std::runtime_error("viper.car AI CARF version 6 resource is missing or invalid");
    bool player_model_loaded = false;
    const auto expected_model = assets.car_name_ + "0.mod";
    for (const auto& entry : car_bundle.entries()) {
        if (entry.type_code != ModelResource::resource_type_code) continue;
        const auto model = parse_model_resource(entry.attributes,
            car_bundle.payload(entry));
        if (!model) throw std::runtime_error("invalid or unsupported FNIM model resource: " + entry.name);
        ++assets.model_resource_count_;
        if (ascii_equal_case_insensitive(entry.name, expected_model)) {
            assets.player_model_ = *model;
            player_model_loaded = true;
        }
    }
    if (!player_model_loaded)
        throw std::runtime_error(expected_model + " FNIM model resource is missing");
    if (assets.car_name_ == "viper") {
        assets.ai_model_ = assets.player_model_;
    } else {
        bool ai_model_loaded = false;
        for (const auto& entry : ai_car_bundle->entries()) {
            if (entry.type_code != ModelResource::resource_type_code ||
                !ascii_equal_case_insensitive(entry.name, "viper0.mod")) continue;
            const auto model = parse_model_resource(entry.attributes,
                ai_car_bundle->payload(entry));
            if (!model)
                throw std::runtime_error("invalid or unsupported AI Viper FNIM model resource: " + entry.name);
            assets.ai_model_ = *model;
            ai_model_loaded = true;
            break;
        }
        if (!ai_model_loaded)
            throw std::runtime_error("viper0.mod AI FNIM model resource is missing");
    }
    return assets;
}

const ResourceBundle& GameAssets::bundle(const std::string_view name) const
{
    for (std::size_t i = 0; i < bundle_names_.size(); ++i) {
        if (name == bundle_names_[i]) return bundles_[i];
    }
    throw std::out_of_range("unknown game asset bundle: " + std::string(name));
}

} // namespace viper
