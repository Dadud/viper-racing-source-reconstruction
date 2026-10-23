#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "viper/replay_format.hpp"

namespace viper {

enum class ReplayLoadExit {
    incomplete,
    modal_guard,
    dialog_cancelled,
    file_open_failed,
    data_read_failed,
    invalid_record,
    replay_start_failed,
    replay_started,
};

struct ReplayLoadObservation {
    bool replay_ui_guard_0x4ec78c{};
    bool current_replay_path_present_0x4ec5f0{};
    bool current_replay_visible_0x4d0668{};
    // Required only when the callback opens the file dialog.
    std::optional<bool> dialog_accepted;
    // Results from 0x411430, 0x411560, 0x45C590 and 0x426790, respectively.
    std::optional<bool> file_opened;
    std::optional<bool> record_read;
    std::optional<bool> record_valid;
    std::optional<bool> replay_started;
    // When present, derives replay_started from the observed 0x426790 checks.
    std::optional<ReplayFileLoadObservation> file_parser;
};

struct ReplayLoadPlan {
    bool input_complete{true};
    ReplayLoadExit exit{ReplayLoadExit::incomplete};
    bool record_buffer_read{};
    std::uint32_t record_buffer_bytes{};
    bool current_replay_cleared{};
    bool replay_record_installed{};
    bool replay_record_removed_after_start_failure{};
    bool file_handle_closed{};
    bool renderer_restored{};
    std::optional<ReplayFileLoadPlan> file_parse;
    std::vector<std::uint32_t> calls;
};

// Projects the load-button callback at 0x407760. File dialog, parser, read,
// validation, and replay-start outcomes are observations, not emulated APIs.
[[nodiscard]] ReplayLoadPlan plan_replay_load(
    const ReplayLoadObservation& observation);

} // namespace viper
