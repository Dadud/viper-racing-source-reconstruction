#include "viper/game_assets.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <system_error>

namespace viper {
namespace {

constexpr std::array<std::string_view, 5> kBundleNames{
    "common.res", "ui.res", "race.res", "viper.car", "bemidji.trk"};

} // namespace

GameAssets GameAssets::load(const std::filesystem::path& root)
{
    if (root.empty()) throw std::runtime_error("asset directory is required");
    GameAssets assets;
    assets.root_ = root;
    for (std::size_t i = 0; i < kBundleNames.size(); ++i) {
        const auto path = root / std::filesystem::path(kBundleNames[i]);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec) {
            throw std::runtime_error("missing required regular file: " + path.string());
        }
        try {
            assets.bundles_[i] = ResourceBundle::open(path);
        } catch (const std::exception& e) {
            throw std::runtime_error("invalid resource bundle " + path.string() + ": " + e.what());
        }
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
    const auto terrain = parse_track_terrain_resource(assets.bundle("bemidji.trk"));
    if (!terrain) {
        throw std::runtime_error("Bemidji track.bpp terrain resource is missing or unsupported");
    }
    assets.terrain_ = *terrain;
    constexpr std::uint32_t ideal_line_type = 0x494c494eU;
    const auto* ideal_line_entry = assets.bundle("bemidji.trk").find("track.ild", ideal_line_type);
    if (ideal_line_entry == nullptr) {
        throw std::runtime_error("Bemidji track.ild ideal-line resource is missing");
    }
    const auto ideal_line_payload = parse_ideal_line_resource(
        ideal_line_entry->attributes,
        assets.bundle("bemidji.trk").payload(*ideal_line_entry));
    if (!ideal_line_payload) {
        throw std::runtime_error("Bemidji track.ild ideal-line resource is invalid");
    }
    const auto normalized_ideal_line = normalize_ideal_line_resource(*ideal_line_payload, 0U);
    if (!normalized_ideal_line) {
        throw std::runtime_error("Bemidji track.ild could not be normalized");
    }
    assets.ideal_line_ = *normalized_ideal_line;
    return assets;
}

const ResourceBundle& GameAssets::bundle(const std::string_view name) const
{
    for (std::size_t i = 0; i < kBundleNames.size(); ++i) {
        if (name == kBundleNames[i]) return bundles_[i];
    }
    throw std::out_of_range("unknown game asset bundle: " + std::string(name));
}

} // namespace viper
