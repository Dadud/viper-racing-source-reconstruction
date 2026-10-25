#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace viper {

struct SetupSelectionPreference {
    std::string track;
    std::string car;
    std::optional<std::uint32_t> laps;
    std::optional<std::uint32_t> opponent_strength;
};

struct SetupPreferenceLoadResult {
    std::optional<SetupSelectionPreference> selection;
    std::filesystem::path path;
    bool file_missing{};
    std::string error;
};

struct SetupPreferenceSaveResult {
    bool saved{};
    std::filesystem::path path;
    std::string error;
};

struct SetupSelectionIndices {
    std::size_t track{};
    std::size_t car{};
};

// Reads reconstruction-owned selector state from Config without modifying
// the original GameData/options.def file.
[[nodiscard]] SetupPreferenceLoadResult load_setup_selection_preference(
    const std::filesystem::path& config_directory);

// Atomically replaces Config/viper-racing-setup.cfg after validating both
// resource stems.
[[nodiscard]] SetupPreferenceSaveResult save_setup_selection_preference(
    const std::filesystem::path& config_directory,
    const SetupSelectionPreference& selection);

// Resolves saved stems independently against the currently installed lists.
// Missing or stale entries retain their options.def-derived initial indices.
[[nodiscard]] SetupSelectionIndices resolve_setup_selection_indices(
    std::span<const std::string> tracks,
    std::span<const std::string> cars,
    const std::optional<SetupSelectionPreference>& saved,
    std::string_view initial_track,
    std::string_view initial_car) noexcept;

} // namespace viper
