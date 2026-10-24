#pragma once

#include "viper/language_resource.hpp"
#include "viper/game_options.hpp"
#include "viper/model_resource.hpp"
#include "viper/resource_bundle.hpp"
#include "viper/terrain_surface.hpp"
#include "viper/vehicle_setup.hpp"
#include "viper/world_track_objects.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace viper {

// Owns the startup assets used by the runtime for its full session lifetime.
class GameAssets {
public:
    [[nodiscard]] static GameAssets load(
        const std::filesystem::path& root,
        std::string_view track = "bemidji",
        std::string_view car = "viper",
        std::optional<GameOptions> options = std::nullopt);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] std::string_view track_name() const noexcept { return track_name_; }
    [[nodiscard]] std::string_view car_name() const noexcept { return car_name_; }
    [[nodiscard]] const ResourceBundle& bundle(std::string_view name) const;
    [[nodiscard]] const LanguageResource& language() const noexcept { return language_; }
    [[nodiscard]] const TerrainSurfaceResourcePayload& terrain() const noexcept { return terrain_; }
    [[nodiscard]] const IdealLineTrackData& ideal_line() const noexcept { return ideal_line_; }
    [[nodiscard]] const CarFileResourcePayloads& player_car() const noexcept { return player_car_; }
    [[nodiscard]] const ModelResource& player_model() const noexcept { return player_model_; }
    [[nodiscard]] const TireDefinition& default_tire() const noexcept { return default_tire_; }
    [[nodiscard]] const std::array<std::byte, 0x8c>& player_car_setup() const noexcept { return player_car_setup_; }
    [[nodiscard]] const GameOptions& options() const noexcept { return options_; }
    [[nodiscard]] std::size_t model_resource_count() const noexcept { return model_resource_count_; }

private:
    std::filesystem::path root_;
    std::array<ResourceBundle, 5> bundles_;
    std::array<std::string, 5> bundle_names_;
    std::string track_name_;
    std::string car_name_;
    LanguageResource language_;
    TerrainSurfaceResourcePayload terrain_;
    IdealLineTrackData ideal_line_;
    CarFileResourcePayloads player_car_;
    ModelResource player_model_;
    TireDefinition default_tire_;
    std::array<std::byte, 0x8c> player_car_setup_{};
    GameOptions options_;
    std::size_t model_resource_count_{};
};

} // namespace viper
