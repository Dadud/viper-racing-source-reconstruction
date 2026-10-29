#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace viper {

// Lossless representation of the observed Viper Racing v6 replay container.
// The fixed engine record and variable tail are opaque and preserved verbatim.
struct ReplayFile {
    std::array<std::uint8_t, 0xcd4> engine_record{};
    std::int32_t timestamp_or_index{};
    std::int32_t frame_count{};
    std::uint32_t base_packet_size{575};
    std::vector<std::uint32_t> frame_words;
    std::vector<std::uint8_t> payload;
};

struct ReplayFileResult {
    ReplayFile replay;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

// Validates the fixed replay engine record used by the native replay loader.
// This is separate from the lossless container codec so arbitrary records can
// still be preserved and inspected without claiming they are playable.
[[nodiscard]] bool validate_replay_engine_record(
    std::span<const std::uint8_t> engine_record,
    std::string& error);

[[nodiscard]] ReplayFileResult decode_replay_file(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::vector<std::uint8_t> encode_replay_file(const ReplayFile& replay,
                                                           std::string& error);
[[nodiscard]] ReplayFileResult read_replay_file(const std::filesystem::path& path);
[[nodiscard]] bool write_replay_file(const std::filesystem::path& path,
                                     const ReplayFile& replay,
                                     std::string& error);

} // namespace viper
