#include "viper/single_race_setup_screen.hpp"

#include <bit>
#include <cstddef>
#include <limits>

namespace viper {
namespace {

std::string c_string_prefix(const std::string_view value)
{
    return std::string(value.substr(0, value.find('\0')));
}

bool begins_with_random_case_insensitive(const std::string_view value)
{
    constexpr std::string_view random_word = "random";
    const auto nul = value.find('\0');
    const auto c_string = value.substr(0, nul);
    if (c_string.size() < random_word.size()) {
        return false;
    }
    for (std::size_t index = 0; index < random_word.size(); ++index) {
        auto character = c_string[index];
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
        if (character != random_word[index]) {
            return false;
        }
    }
    return true;
}

} // namespace

SingleRaceSetupScreenPlan plan_single_race_setup_screen(
    const SingleRaceSetupScreenObservation& observation)
{
    SingleRaceSetupScreenPlan result{};
    result.accepted = observation.final_ui_result == -2;
    result.alternate_result_helper_0x4813f0_called = !result.accepted;

    if (result.accepted) {
        result.car_output_write_reached = true;
        if (observation.selected_car_name.has_value()) {
            result.car_output = c_string_prefix(
                *observation.selected_car_name);
        } else {
            result.input_complete = false;
        }

        if (!observation.selected_track_name.has_value()) {
            result.input_complete = false;
            result.track_output_write_reachability_unknown = true;
            result.setting_writeback_reached = std::nullopt;
            return result;
        } else if (!begins_with_random_case_insensitive(
                       *observation.selected_track_name)) {
            result.track_output_write_reached = true;
            result.track_output = c_string_prefix(
                *observation.selected_track_name);
        } else {
            result.random_track_branch = true;
            if (!observation.timer_source_value.has_value() ||
                !observation.track_collection_count.has_value()) {
                result.input_complete = false;
                result.track_output_write_reachability_unknown = true;
                result.setting_writeback_reached = std::nullopt;
                return result;
            } else {
                const auto track_count_word =
                    static_cast<std::uint32_t>(
                        *observation.track_collection_count);
                const auto divisor = std::bit_cast<std::int32_t>(
                    track_count_word - 2U);
                const auto timer_value = *observation.timer_source_value;
                if (divisor == 0 ||
                    (timer_value == std::numeric_limits<std::int32_t>::min() &&
                     divisor == -1)) {
                    result.random_track_division_fault = true;
                    result.input_complete = false;
                    result.setting_writeback_reached = std::nullopt;
                    return result;
                } else {
                    const auto index = timer_value % divisor;
                    result.random_track_index = index;
                    result.track_output_write_reached = true;
                    if (!observation.indexed_track_name.has_value()) {
                        result.input_complete = false;
                    } else {
                        result.track_output = c_string_prefix(
                            *observation.indexed_track_name);
                    }
                }
            }
        }
    }

    auto word_at_0x1c = observation.settings.word_at_0x1c;
    if (observation.settings.word_at_0x18 != 0U) {
        word_at_0x1c = 0;
        result.word_at_0x1c_cleared = true;
    }
    result.word_at_0x1c_after = word_at_0x1c;

    result.setting_writes = {
        {SingleRaceSettingKey::field,
         observation.settings.field_at_0x0c, 4U, 0x0046af50U},
        {SingleRaceSettingKey::realism,
         observation.settings.realism_at_0x00, 4U, 0x0046af50U},
        {SingleRaceSettingKey::damage_on,
         observation.settings.damage_on_at_0x24, 1U, 0x0046afe0U},
        {SingleRaceSettingKey::is_reversed,
         observation.settings.reversed_at_0x25, 1U, 0x0046afe0U},
        {SingleRaceSettingKey::time_of_day,
         observation.settings.time_of_day_at_0x04, 4U, 0x0046af50U},
        {SingleRaceSettingKey::weather,
         observation.settings.weather_at_0x08, 4U, 0x0046af50U},
        {SingleRaceSettingKey::race_type,
         observation.settings.race_type_at_0x10, 4U, 0x0046af50U},
        {SingleRaceSettingKey::opponent_strength,
         observation.settings.opponent_strength_at_0x20, 4U, 0x0046af50U},
        {SingleRaceSettingKey::car_count,
         observation.settings.car_count, 4U, 0x0046af50U},
    };
    return result;
}

} // namespace viper
