#include "viper/game_assets.hpp"

#include <array>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <system_error>

namespace viper {
namespace {

constexpr std::array<std::string_view, 3> kSharedBundleNames{
    "common.res", "ui.res", "race.res"};

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
                            const std::string_view car)
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

    for (std::size_t i = 0; i < assets.bundle_names_.size(); ++i) {
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
    constexpr std::uint32_t car_setup_type = 0x43435330U;
    const auto& car_bundle = assets.bundle(assets.bundle_names_[3]);
    const auto* car_setup_entry = car_bundle.find(
        assets.car_name_ + ".ccs", car_setup_type);
    if (car_setup_entry == nullptr)
        throw std::runtime_error(assets.car_name_ +
            " human car setup resource is missing");
    const auto car_setup_payload = car_bundle.payload(*car_setup_entry);
    if (car_setup_entry->attributes != 2U || car_setup_payload.size() !=
            assets.player_car_setup_.size())
        throw std::runtime_error(assets.car_name_ +
            " human car setup resource has an unsupported version or size");
    std::copy(car_setup_payload.begin(), car_setup_payload.end(),
        assets.player_car_setup_.begin());
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
