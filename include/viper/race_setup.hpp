#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "viper/scene_overlay.hpp"

namespace viper {

enum class RaceSetupTraceOperationKind {
    direct_call,
    invoke_caller_callback,
    copy_race_results,
};

struct RaceSetupTraceOperation {
    RaceSetupTraceOperationKind kind{};
    std::uint32_t address{};
};

struct RaceSetupTraceOptions {
    // 0x401EA0 skips its CONTROL/race_camera lookup when DAT_004CF5B0 == 4.
    bool read_race_camera_setting{};
    // First 0x41B020 result: zero selects 0x4020E0; nonzero selects 0x41AB00.
    bool use_4020e0_loop{};
    // Second 0x41B020 result controls the post-cleanup completion callback.
    bool post_cleanup_mode_is_zero{};
    // A non-null param_3 replaces the default 0x40B3F0 completion routine.
    bool caller_callback_present{};
    // A non-null param_2 requests the race-result copy block.
    bool copy_race_results{};
};

struct RaceSetupResultSource {
    std::int32_t field_0x04{};
    std::int32_t field_0x0c{};
    std::int32_t associated_field_0x50{};
};

struct RaceSetupEntryRecord {
    char selector_0x41{};
    std::int32_t field_0x04{};
    std::int32_t field_0x0c{};
};

// Returns the observed outer call order in 0x401EA0. Addresses identify
// callees only; this trace does not emulate their subsystem effects.
[[nodiscard]] std::vector<RaceSetupTraceOperation> trace_race_setup(
    const RaceSetupTraceOptions& options);

// Models the identifier search in 0x4243C0 over records with a 0x48-byte
// stride. The binary returns null when the character selector is absent.
[[nodiscard]] std::optional<RaceSetupEntryRecord> find_race_setup_entry(
    std::span<const RaceSetupEntryRecord> records,
    char selector) noexcept;

// Models the populated three-DWORD row written for each result source by
// 0x401EA0. The first word stays zero unless source field +0x0C is positive.
[[nodiscard]] std::vector<std::array<std::int32_t, 3>> project_race_setup_results(
    std::span<const RaceSetupResultSource> sources);

struct RaceSetupResultsObservation {
    std::int32_t result_count{};
    // One result from 0x4243C0 for each positive loop iteration.
    std::vector<std::optional<RaceSetupEntryRecord>> indexed_entries;
    // One 0x45DFB0(+0x50) result for each entry whose +0x0c value is positive.
    std::vector<std::optional<std::int32_t>> associated_fields_0x50;
};

struct RaceSetupResultsPlan {
    bool input_complete{true};
    std::array<std::int32_t, 49> zeroed_result_buffer{};
    std::vector<std::array<std::int32_t, 3>> rows;
    std::vector<std::int8_t> selector_bytes;
    std::optional<std::int32_t> result_count_written;
    bool missing_entry_pointer_crash_possible{};
    bool missing_associated_record_crash_possible{};
    bool result_buffer_overrun{};
    std::uint64_t output_dword_count{};
};

// Models the param_2 result block of 0x401EA0, including its 49-DWORD zero
// fill and unbounded three-DWORD row loop. Indexed lookups remain observations.
[[nodiscard]] RaceSetupResultsPlan project_race_setup_result_buffer(
    const RaceSetupResultsObservation& observation);

struct RaceSetupObservation {
    std::int32_t global_0x4cf5b0{};
    std::int32_t global_0x4cf5b4_before{};
    // Value in the default-initialized local after 0x46AF00 reads the INI key.
    std::optional<std::int32_t> race_camera_setting_after_read;
    std::optional<bool> first_mode_byte_is_zero;
    std::optional<bool> post_cleanup_mode_byte_is_zero;
    bool caller_callback_present{};
    bool copy_race_results{};
    std::optional<RaceSetupResultsObservation> results;
    // DAT_004EB184 as observed immediately before the final 0x46AF50 write.
    std::optional<std::int32_t> camera_setting_to_persist;
};

struct RaceSetupPlan {
    bool input_complete{true};
    bool race_camera_setting_read{};
    std::optional<std::int32_t> camera_setting_written_to_0x4cf5b4;
    std::optional<std::int32_t> camera_setting_written_to_0x4eb184;
    std::optional<std::int32_t> camera_setting_passed_to_race_initialization;
    std::optional<std::int32_t> camera_setting_persisted;
    std::vector<RaceSetupTraceOperation> call_trace;
    std::optional<SceneDashboardInitializationPlan> dashboard_initialization;
    std::optional<RaceSetupResultsPlan> result_buffer;
};

// Composes the setting, mode, callback, and result-buffer control flow of
// 0x401EA0. Callee-owned mutations and the final global value remain inputs.
[[nodiscard]] RaceSetupPlan plan_race_setup(
    const RaceSetupObservation& observation);

} // namespace viper
