#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace viper {

enum class ReplayFileLoadExit {
    incomplete,
    header_read_failed,
    bad_cookie,
    bad_format_version,
    incompatible_word_count,
    payload_too_large,
    frame_count_out_of_range,
    packet_size_mismatch,
    buffer_read_failed,
    loaded,
};

struct ReplayFileLoadObservation {
    std::optional<bool> header_read;
    std::optional<std::uint32_t> magic;
    std::optional<std::uint32_t> format_version;
    std::optional<std::int32_t> timestamp_or_index_word;
    std::optional<std::int32_t> frame_count;
    std::optional<std::uint32_t> base_packet_size;
    std::optional<std::uint32_t> file_word_count;
    std::optional<std::uint32_t> payload_bytes;
    std::optional<std::int32_t> expected_word_count_0x508a58;
    std::optional<std::uint32_t> expected_packet_size_0x508ad0;
    std::optional<bool> frame_table_read;
    std::optional<bool> payload_read;
};

struct ReplayFileLoadPlan {
    bool input_complete{true};
    ReplayFileLoadExit exit{ReplayFileLoadExit::incomplete};
    bool replay_loaded_flag_cleared_at_entry{true};
    std::uint32_t header_bytes{0x1cU};
    std::uint32_t frame_table_bytes{};
    std::optional<std::uint32_t> payload_bytes;
    std::optional<std::uint32_t> loaded_timestamp_or_index_word;
    std::optional<std::int32_t> loaded_frame_count;
    bool replay_loaded_flag_set{};
    std::vector<std::uint32_t> error_string_addresses;
    std::vector<std::uint32_t> calls;
};

// Projects the replay-file parser at 0x426790. File reads and header values
// are observations; the helper performs no real file or engine operations.
[[nodiscard]] ReplayFileLoadPlan plan_replay_file_load(
    const ReplayFileLoadObservation& observation);

} // namespace viper
