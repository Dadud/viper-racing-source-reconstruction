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

// Reads a native career slot from Config/career/careerN.dat and applies the
// reconstructed header, XOR, and checksum validation.
[[nodiscard]] CareerSlotLoadResult load_career_slot(
    const std::filesystem::path& config_directory, int slot);

// Writes a complete native-format career image. The destination is replaced
// only after the new image has been fully written to a sibling temporary file.
[[nodiscard]] CareerSlotSaveResult save_career_slot(
    const std::filesystem::path& config_directory, int slot,
    std::span<const std::uint8_t> decoded_payload);

} // namespace viper
