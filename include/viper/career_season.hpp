#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace viper {

inline constexpr std::size_t career_season_header_word_count = 9;
inline constexpr std::size_t career_season_event_word_count = 0x16;
inline constexpr std::size_t career_season_event_money_word_offset = 6;
inline constexpr std::size_t career_season_money_value_count = 8;

struct CareerSeasonEventRecord {
    std::array<std::uint32_t, career_season_event_word_count> raw_words{};
    std::array<char, 8> track_name_field{};
    std::string track_name;
    std::uint8_t field_0x10_low_byte{};
    std::uint32_t field_0x14{};
    std::array<std::uint32_t, career_season_money_value_count> money_values{};
    std::array<std::uint32_t, career_season_money_value_count> points_by_placement{};
};

struct CareerSeasonData {
    bool header_complete{};
    bool records_complete{};
    bool exact_payload_size{};
    std::optional<std::uint32_t> declared_event_count;
    std::array<std::uint32_t, career_season_money_value_count> header_money_values{};
    std::vector<CareerSeasonEventRecord> events;
    std::size_t trailing_word_count{};
};

// Decodes the observed 9-DWORD header and raw 22-DWORD event records without
// assigning meanings to fields other than track, payouts, and placement points.
[[nodiscard]] CareerSeasonData decode_career_season_data(
    std::span<const std::uint32_t> resource_words);

struct CareerSeasonParseObservation {
    // DWORD view of the decoded SEAS resource returned by 0x417CB0.
    std::span<const std::uint32_t> resource_words;
    // 0x418C70 result for each Career:Money:<id> lookup, in emitted order.
    // Values other than 0 or 1 are treated as unavailable observations.
    std::span<const std::uint8_t> money_label_lookup_results;
};

struct CareerSeasonMoneyLabelPlan {
    std::size_t resource_word_index{};
    std::uint32_t money_value{};
    std::optional<bool> label_found;
    bool panic_reporter_called{};
};

struct CareerSeasonParsePlan {
    bool input_complete{true};
    std::optional<std::uint32_t> declared_event_count;
    std::vector<CareerSeasonMoneyLabelPlan> checked_money_labels;
    std::size_t panic_reporter_call_count{};
};

// Projects the label checks made by 0x4ADB00 after it loads a SEAS payload.
// The shipped parser's unchecked indexing is represented as incomplete input
// when the supplied DWORD span cannot contain its declared records.
[[nodiscard]] CareerSeasonParsePlan plan_career_season_parse(
    const CareerSeasonParseObservation& observation);

} // namespace viper
