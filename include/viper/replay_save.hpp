#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace viper {

enum class ReplaySaveExit {
    incomplete,
    replay_not_visible,
    dialog_cancelled,
    file_open_failed,
    saved,
};

struct ReplaySaveObservation {
    bool replay_visible_0x4d0668{};
    // Initial one-time label mask at 0x4EC52C. The callback sets bits 0x1,
    // 0x2, 0x4, and 0x8 as it initializes the four controls.
    std::optional<std::uint8_t> label_mask_0x4ec52c;
    // Comparisons against 0x4EC548 and 0x4EC7E0 before showing the dialog.
    std::array<std::optional<bool>, 2> pre_dialog_cache_refresh_needed{};
    // Result of the Save dialog helper at 0x474770.
    std::optional<bool> dialog_accepted;
    // Whether 0x4112A0 returned a valid file-handle registry index.
    std::optional<bool> file_opened;
    std::optional<std::uint32_t> payload_word_count_0x508a58;
    std::optional<std::uint32_t> payload_tail_bytes_0x508a3c;
    // Comparisons against 0x4EB7E8 and 0x4EB820 on the open-failure path.
    std::array<std::optional<bool>, 2> open_failure_cache_refresh_needed{};
};

struct ReplaySaveCacheRefresh {
    std::uint32_t object_address{};
    bool performed{};
};

struct ReplaySaveLabelRegistration {
    std::uint8_t mask_bit{};
    std::uint32_t destination_address{};
    std::uint32_t string_address{};
    std::uint32_t cleanup_callback{};
};

struct ReplaySavePayloadWrite {
    // Zero denotes the serializer's stack-local header buffer.
    std::uint32_t source_address{};
    std::optional<std::uint32_t> byte_count;
};

struct ReplaySavePlan {
    bool input_complete{true};
    ReplaySaveExit exit{ReplaySaveExit::incomplete};
    std::uint8_t label_mask_after{};
    bool replay_torn_down{};
    bool save_dialog_called{};
    bool file_open_attempted{};
    bool record_write_call_made{};
    std::uint32_t record_bytes{};
    bool replay_payload_serializer_called{};
    std::uint32_t dialog_initial_directory_address{0x004ec820U};
    std::uint32_t dialog_path_buffer_bytes{0x104U};
    std::uint32_t file_access_mask{0x40000000U};
    std::uint32_t file_share_mode{1U};
    std::uint32_t file_creation_disposition{2U};
    std::uint32_t file_attributes{0x80U};
    std::uint32_t payload_magic{};
    std::uint32_t payload_version{};
    std::vector<ReplaySavePayloadWrite> payload_writes;
    bool file_handle_closed{};
    bool open_failure_dialog_shown{};
    std::vector<ReplaySaveCacheRefresh> cache_refreshes;
    std::vector<ReplaySaveLabelRegistration> label_registrations;
    std::vector<std::uint32_t> calls;
};

// Models the save-button callback beginning at 0x407C60. Dialog, file-open,
// and cache comparison outcomes are observations; WriteFile results are
// ignored by the original callback and therefore are not fabricated here.
[[nodiscard]] ReplaySavePlan plan_replay_save(
    const ReplaySaveObservation& observation);

} // namespace viper
