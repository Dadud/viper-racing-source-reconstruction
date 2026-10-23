#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace viper {

struct CareerSeasonEventRecord;
struct RaceSetupResultsPlan;

inline constexpr std::size_t career_recent_result_count = 8;
inline constexpr std::size_t career_history_row_count = 4;
inline constexpr std::size_t career_progression_row_count = 8;
inline constexpr std::size_t career_progression_row_stride_dwords = 0x22;
inline constexpr std::size_t career_progression_table_dword_count = 0x110;
inline constexpr std::size_t career_event_column_count = 0x20;

// The binary stores eight raw result dwords for each career index. The
// gameplay meaning of each numeric value is not established.
struct CareerRecentResults {
    std::array<std::int32_t, career_recent_result_count> values{};
};

// Models 0x4AA410: fill the first zero slot, or shift a full row left and
// append the new raw value at the end.
void append_career_recent_result(
    CareerRecentResults& results,
    std::int32_t value) noexcept;

struct CareerProgressionTailState {
    std::int32_t history_row_index{};
    std::int32_t raw_stage_selector{};
    std::int32_t event_cursor{};
    std::int32_t event_limit{};
    std::uint32_t completed_stage_count{};
    std::uint32_t funds_total{};
    std::uint32_t event_loop_count{};
    std::array<CareerRecentResults, career_history_row_count> stage_history{};
    std::array<std::uint32_t, career_progression_table_dword_count>
        progression_table{};
    bool advanced_flag{};
};

enum class CareerProgressionTailStatus {
    no_completion_signal,
    more_events_pending,
    completed_stage,
    invalid_history_row,
};

struct CareerProgressionTailResult {
    CareerProgressionTailStatus status{
        CareerProgressionTailStatus::no_completion_signal};
    bool post_event_results_screen_reached{};
    bool history_row_loader_reached{};
};

// Models the guarded event-cursor advance at the start of 0x4AA5E0.
// transition_ready and advance_input represent the two upstream conditions.
[[nodiscard]] bool advance_career_event_from_input(
    CareerProgressionTailState& state,
    bool transition_ready,
    bool advance_input) noexcept;

struct CareerEventResultRow {
    std::array<std::int32_t, 3> words{};
};

enum class CareerEventRankingStatus {
    applied,
    invalid_order,
    invalid_event_column,
};

struct CareerEventRankingResult {
    CareerEventRankingStatus status{CareerEventRankingStatus::invalid_order};
    std::uint32_t competitor_seven_reward{};
};

struct CareerEventRankingObservation {
    // The eight numeric entries from 0x564A18, in the order consumed by
    // 0x4AA5E0 and 0x4AC290.
    std::array<std::int32_t, career_progression_row_count> competitor_order{};
    // Eight 12-byte result rows; dword 1 is the placement consumed by the code.
    std::array<CareerEventResultRow, career_progression_row_count> results{};
    std::array<std::uint32_t, 8> points_by_placement{};
    // Index 0 is unused; indices 1 through 8 are payout values.
    std::array<std::uint32_t, 9> payout_by_placement{};
    std::size_t event_column{};
};

struct CareerEntrantOrderObservation {
    std::array<float, career_progression_row_count> estimated_seconds{};
    std::array<std::int32_t, career_progression_row_count> competitor_order{};
};

// Models the pre-race estimate loop in 0x4AA5E0. The first seven inputs are
// the selected AI profile times; baseline_seconds and player_seconds come
// from separate race setup fields.
// Random samples are explicit observations of FUN_004193F0(201), one per
// competitor. Values outside [0, 200] are accepted as supplied observations.
[[nodiscard]] CareerEntrantOrderObservation estimate_career_entrant_order(
    const std::array<float, career_progression_row_count>& profile_seconds,
    float baseline_seconds,
    float player_seconds,
    const std::array<std::int32_t, career_progression_row_count>& random_samples) noexcept;

// Builds the ranking tables from a decoded SEAS event while keeping the
// observed race order/results as runtime inputs.
[[nodiscard]] CareerEventRankingObservation make_career_event_ranking_observation(
    const CareerSeasonEventRecord& season_event,
    const std::array<std::int32_t, career_progression_row_count>& competitor_order,
    const std::array<CareerEventResultRow, career_progression_row_count>& results,
    std::size_t event_column) noexcept;

// Composes the eight 0x401EA0 result rows with a decoded season event. Returns
// no observation unless the bounded race result projection contains exactly
// the eight rows consumed by the career ranking code.
[[nodiscard]] std::optional<CareerEventRankingObservation>
make_career_event_ranking_observation_from_race_setup(
    const CareerSeasonEventRecord& season_event,
    const std::array<std::int32_t, career_progression_row_count>& competitor_order,
    const RaceSetupResultsPlan& race_results,
    std::size_t event_column) noexcept;

// Models the completion suffix in 0x4AA5E0. Inputs for the upstream event
// dispatcher and reward table are explicit observations.
[[nodiscard]] CareerProgressionTailResult finish_career_stage(
    CareerProgressionTailState& state,
    bool stage_completion_signal,
    std::uint32_t stage_reward) noexcept;

// Models the placement payout and per-event standings writes in 0x4AA5E0.
// The binary's upstream order, result rows, and point/payout tables are inputs.
[[nodiscard]] CareerEventRankingResult apply_career_event_ranking(
    CareerProgressionTailState& state,
    const CareerEventRankingObservation& observation) noexcept;

// Models the eight-row 0x4AC020 aggregate score sort, including the observed
// Visual Studio shortsort tie behavior, and stores each row's resulting rank.
void rank_career_progression_rows(CareerProgressionTailState& state) noexcept;

} // namespace viper
