#include "viper/career_progression.hpp"
#include "viper/career_season.hpp"
#include "viper/race_setup.hpp"

#include <utility>

namespace viper {

bool advance_career_event_from_input(
    CareerProgressionTailState& state,
    const bool transition_ready,
    const bool advance_input) noexcept
{
    if (!transition_ready || !advance_input || state.event_limit <= 0 ||
        state.event_cursor < 0 ||
        state.event_cursor >= state.event_limit - 1) {
        return false;
    }

    ++state.event_cursor;
    ++state.event_loop_count;
    rank_career_progression_rows(state);
    return true;
}

CareerEntrantOrderObservation estimate_career_entrant_order(
    const std::array<float, career_progression_row_count>& profile_seconds,
    const float baseline_seconds,
    const float player_seconds,
    const std::array<std::int32_t, career_progression_row_count>& random_samples) noexcept
{
    CareerEntrantOrderObservation observation{};
    for (std::size_t competitor = 0;
         competitor < career_progression_row_count;
         ++competitor) {
        observation.competitor_order[competitor] =
            static_cast<std::int32_t>(competitor);
        const auto jitter =
            static_cast<float>(random_samples[competitor] - 100) * 0.0001F + 1.0F;
        if (competitor == career_progression_row_count - 1) {
            observation.estimated_seconds[competitor] = player_seconds;
        } else if (static_cast<std::int32_t>(profile_seconds[competitor]) > 0) {
            observation.estimated_seconds[competitor] =
                (baseline_seconds + profile_seconds[competitor]) * jitter;
        } else {
            observation.estimated_seconds[competitor] = 300.0F;
        }
    }

    // The binary compares every later estimate to the current slot and swaps
    // immediately when it finds a smaller one. Equal values do not trigger a
    // swap, though a smaller later value can move an equal pair past each other.
    for (std::size_t first = 0; first + 1 < career_progression_row_count; ++first) {
        for (std::size_t later = first + 1;
             later < career_progression_row_count;
             ++later) {
            if (observation.estimated_seconds[later] <
                observation.estimated_seconds[first]) {
                std::swap(observation.estimated_seconds[first],
                          observation.estimated_seconds[later]);
                std::swap(observation.competitor_order[first],
                          observation.competitor_order[later]);
            }
        }
    }
    return observation;
}

CareerEventRankingObservation make_career_event_ranking_observation(
    const CareerSeasonEventRecord& season_event,
    const std::array<std::int32_t, career_progression_row_count>& competitor_order,
    const std::array<CareerEventResultRow, career_progression_row_count>& results,
    const std::size_t event_column) noexcept
{
    CareerEventRankingObservation observation{};
    observation.competitor_order = competitor_order;
    observation.results = results;
    observation.points_by_placement = season_event.points_by_placement;
    for (std::size_t placement = 1; placement < observation.payout_by_placement.size();
         ++placement) {
        observation.payout_by_placement[placement] =
            season_event.money_values[placement - 1];
    }
    observation.event_column = event_column;
    return observation;
}

std::optional<CareerEventRankingObservation>
make_career_event_ranking_observation_from_race_setup(
    const CareerSeasonEventRecord& season_event,
    const std::array<std::int32_t, career_progression_row_count>& competitor_order,
    const RaceSetupResultsPlan& race_results,
    const std::size_t event_column) noexcept
{
    if (!race_results.input_complete ||
        race_results.result_count_written !=
            static_cast<std::int32_t>(career_progression_row_count) ||
        race_results.rows.size() != career_progression_row_count) {
        return std::nullopt;
    }

    std::array<CareerEventResultRow, career_progression_row_count> results{};
    for (std::size_t row = 0; row < results.size(); ++row) {
        results[row].words = race_results.rows[row];
    }
    return make_career_event_ranking_observation(
        season_event, competitor_order, results, event_column);
}

void append_career_recent_result(
    CareerRecentResults& results,
    const std::int32_t value) noexcept
{
    for (auto& existing_value : results.values) {
        if (existing_value == 0) {
            existing_value = value;
            return;
        }
    }

    for (std::size_t index = 1; index < results.values.size(); ++index) {
        results.values[index - 1] = results.values[index];
    }
    results.values.back() = value;
}

CareerProgressionTailResult finish_career_stage(
    CareerProgressionTailState& state,
    const bool stage_completion_signal,
    const std::uint32_t stage_reward) noexcept
{
    CareerProgressionTailResult result{};
    state.advanced_flag = false;
    if (!stage_completion_signal) {
        result.status = CareerProgressionTailStatus::no_completion_signal;
        return result;
    }

    if (state.event_cursor < state.event_limit) {
        result.status = CareerProgressionTailStatus::more_events_pending;
        result.post_event_results_screen_reached = true;
        return result;
    }

    if (state.history_row_index < 0 ||
        static_cast<std::size_t>(state.history_row_index) >=
            state.stage_history.size()) {
        result.status = CareerProgressionTailStatus::invalid_history_row;
        return result;
    }

    const std::int32_t raw_result_code =
        state.raw_stage_selector == 0 ? 1 :
        state.raw_stage_selector == 1 ? 2 :
        state.raw_stage_selector == 2 ? 3 : 4;
    auto& history = state.stage_history[
        static_cast<std::size_t>(state.history_row_index)];
    append_career_recent_result(history, raw_result_code);
    state.funds_total += stage_reward;
    result.post_event_results_screen_reached = true;

    state.progression_table.fill(0);
    if (state.raw_stage_selector == 0 &&
        state.history_row_index + 1 <
            static_cast<std::int32_t>(state.stage_history.size())) {
        ++state.history_row_index;
        result.history_row_loader_reached = true;

        for (std::size_t row = 0; row < career_progression_row_count; ++row) {
            state.progression_table[
                row * career_progression_row_stride_dwords] =
                static_cast<std::uint32_t>(row);
        }
        append_career_recent_result(
            state.stage_history[static_cast<std::size_t>(state.history_row_index)],
            5);
        state.advanced_flag = true;
    }

    ++state.completed_stage_count;
    state.event_cursor = 0;
    state.event_loop_count = 0;
    for (std::size_t row = 0; row < career_progression_row_count; ++row) {
        state.progression_table[
            row * career_progression_row_stride_dwords + 1] = 0;
    }

    result.status = CareerProgressionTailStatus::completed_stage;
    return result;
}

CareerEventRankingResult apply_career_event_ranking(
    CareerProgressionTailState& state,
    const CareerEventRankingObservation& observation) noexcept
{
    CareerEventRankingResult result{};
    if (observation.event_column >= career_event_column_count) {
        result.status = CareerEventRankingStatus::invalid_event_column;
        return result;
    }

    std::size_t competitor_seven_position = career_progression_row_count;
    for (std::size_t position = 0;
         position < observation.competitor_order.size();
         ++position) {
        const auto competitor = observation.competitor_order[position];
        if (competitor < 0 ||
            static_cast<std::size_t>(competitor) >=
                career_progression_row_count) {
            result.status = CareerEventRankingStatus::invalid_order;
            return result;
        }
        if (competitor == 7) {
            // 0x4AC290 scans the whole array without breaking, so duplicates
            // select the final matching position.
            competitor_seven_position = position;
        }
    }
    if (competitor_seven_position == career_progression_row_count) {
        // The binary would index one row before the result buffer here. Keep
        // malformed observations inside the portable model boundary.
        result.status = CareerEventRankingStatus::invalid_order;
        return result;
    }

    const auto competitor_seven_placement = observation.results[
        competitor_seven_position].words[1];
    if (competitor_seven_placement > 0 &&
        competitor_seven_placement < 9) {
        result.competitor_seven_reward = observation.payout_by_placement[
            static_cast<std::size_t>(competitor_seven_placement)];
        state.funds_total += result.competitor_seven_reward;
    }

    for (std::size_t position = 0;
         position < observation.competitor_order.size();
         ++position) {
        const auto competitor = static_cast<std::size_t>(
            observation.competitor_order[position]);
        const auto placement = observation.results[position].words[1];
        if (placement > 0 && placement < 9) {
            state.progression_table[
                competitor * career_progression_row_stride_dwords + 1] +=
                observation.points_by_placement[
                    static_cast<std::size_t>(placement - 1)];
        }
        state.progression_table[
            competitor * career_progression_row_stride_dwords + 2 +
            observation.event_column] = static_cast<std::uint32_t>(placement);
    }

    result.status = CareerEventRankingStatus::applied;
    return result;
}

void rank_career_progression_rows(CareerProgressionTailState& state) noexcept
{
    std::array<std::size_t, career_progression_row_count> order{};
    for (std::size_t row = 0; row < order.size(); ++row) {
        order[row] = row;
    }

    // The eight-element call takes the shortsort path in the shared VS 1998
    // sort helper. It selects the first strictly greater item, then always
    // swaps that item with the end of the remaining range.
    for (std::size_t end = order.size() - 1; end > 0; --end) {
        std::size_t selected = 0;
        for (std::size_t candidate = 1; candidate <= end; ++candidate) {
            const auto candidate_score = static_cast<std::int32_t>(
                state.progression_table[
                    order[candidate] * career_progression_row_stride_dwords + 1]);
            const auto current_selected_score = static_cast<std::int32_t>(
                state.progression_table[
                    order[selected] * career_progression_row_stride_dwords + 1]);
            if (candidate_score > current_selected_score) {
                selected = candidate;
            }
        }
        const auto displaced = order[selected];
        order[selected] = order[end];
        order[end] = displaced;
    }

    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        state.progression_table[
            order[rank] * career_progression_row_stride_dwords] =
            static_cast<std::uint32_t>(rank);
    }
}

} // namespace viper
