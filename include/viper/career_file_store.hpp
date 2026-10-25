#pragma once

#include "viper/career_file.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace viper {

struct CareerSlotLoadResult {
    CareerFileLoadResult file;
    std::filesystem::path path;
    std::string error;
};

struct CareerSlotSaveResult {
    bool saved{};
    std::filesystem::path path;
    std::string error;
};

struct CareerSlotDeleteResult {
    bool deleted{};
    bool already_missing{};
    std::filesystem::path path;
    std::string error;
};

// Reads a native career slot from Config/career/careerN.dat and applies the
// reconstructed header, XOR, and checksum validation.
[[nodiscard]] CareerSlotLoadResult load_career_slot(
    const std::filesystem::path& config_directory, int slot);

// Writes a complete native-format career image. The destination is replaced
// only after the new image has been fully written to a sibling temporary file.
[[nodiscard]] CareerSlotSaveResult save_career_slot(
    const std::filesystem::path& config_directory, int slot,
    std::span<const std::uint8_t> decoded_payload);

// Creates a new slot using the observed NewCareer payload initializer, then
// persists it through the native career file encoder. Settings remain raw
// values because their UI/native encodings have not been established.
[[nodiscard]] CareerSlotSaveResult create_career_slot(
    const std::filesystem::path& config_directory, int slot,
    std::span<const std::uint8_t> player_name_with_terminator,
    std::uint32_t raw_setting_at_0x10,
    std::uint8_t raw_setting_at_0x14);

// Removes one native career slot from Config/career/careerN.dat. A missing
// slot is reported separately and is not treated as a filesystem error.
[[nodiscard]] CareerSlotDeleteResult delete_career_slot(
    const std::filesystem::path& config_directory, int slot);

} // namespace viper
