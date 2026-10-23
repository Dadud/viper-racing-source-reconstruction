#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

enum class SingleRaceSettingKey : std::uint8_t {
    field,
    realism,
    damage_on,
    is_reversed,
    time_of_day,
    weather,
    race_type,
    opponent_strength,
    car_count,
};

struct SingleRaceSettingWrite {
    SingleRaceSettingKey key{};
    std::uint32_t raw_value{};
    std::uint8_t value_width_bytes{};
    std::uint32_t helper{};
};

struct SingleRaceSettingsObservation {
    std::uint32_t field_at_0x0c{};
    std::uint32_t realism_at_0x00{};
    std::uint8_t damage_on_at_0x24{};
    std::uint8_t reversed_at_0x25{};
    std::uint32_t time_of_day_at_0x04{};
    std::uint32_t weather_at_0x08{};
    std::uint32_t race_type_at_0x10{};
    std::uint32_t word_at_0x18{};
    std::uint32_t word_at_0x1c{};
    std::uint32_t opponent_strength_at_0x20{};
    std::uint32_t car_count{};
};

struct SingleRaceSetupScreenObservation {
    std::int32_t final_ui_result{};
    std::optional<std::string_view> selected_car_name;
    std::optional<std::string_view> selected_track_name;
    // Value returned in EAX by 0x413BD0's selected timer backend.
    std::optional<std::int32_t> timer_source_value;
    // First DWORD of the track collection returned by 0x406590.
    std::optional<std::int32_t> track_collection_count;
    // String returned by 0x4065A0 at the computed random-track index.
    std::optional<std::string_view> indexed_track_name;
    SingleRaceSettingsObservation settings;
};

struct SingleRaceSetupScreenPlan {
    bool accepted{};
    bool input_complete{true};
    bool alternate_result_helper_0x4813f0_called{};
    bool car_output_write_reached{};
    std::optional<std::string> car_output;
    bool random_track_branch{};
    bool random_track_division_fault{};
    std::optional<std::int32_t> random_track_index;
    bool track_output_write_reachability_unknown{};
    bool track_output_write_reached{};
    std::optional<std::string> track_output;
    bool word_at_0x1c_cleared{};
    std::optional<std::uint32_t> word_at_0x1c_after;
    std::optional<bool> setting_writeback_reached{true};
    std::vector<SingleRaceSettingWrite> setting_writes;
};

// Projects 0x480C80's accepted selection outputs, random-track branch, and
// ordered settings writeback. Timer backend and collection contents are inputs.
[[nodiscard]] SingleRaceSetupScreenPlan plan_single_race_setup_screen(
    const SingleRaceSetupScreenObservation& observation);

} // namespace viper
