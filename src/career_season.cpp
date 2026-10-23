#include "viper/career_season.hpp"

#include <algorithm>
#include <limits>

namespace viper {

CareerSeasonData decode_career_season_data(
    const std::span<const std::uint32_t> resource_words)
{
    CareerSeasonData data{};
    if (resource_words.size() < career_season_header_word_count) {
        return data;
    }

    data.header_complete = true;
    const auto declared_count = resource_words[0];
    data.declared_event_count.emplace(declared_count);
    for (std::size_t index = 0; index < career_season_money_value_count; ++index) {
        data.header_money_values[index] = resource_words[index + 1];
    }

    const auto max_records = (resource_words.size() -
        career_season_header_word_count) / career_season_event_word_count;
    const auto decoded_count = std::min<std::size_t>(declared_count, max_records);
    data.events.reserve(decoded_count);
    for (std::size_t event = 0; event < decoded_count; ++event) {
        const auto first_word = career_season_header_word_count +
            event * career_season_event_word_count;
        CareerSeasonEventRecord record{};
        for (std::size_t index = 0; index < record.raw_words.size(); ++index) {
            record.raw_words[index] = resource_words[first_word + index];
        }
        for (std::size_t index = 0; index < record.track_name_field.size(); ++index) {
            const auto word = record.raw_words[index / 4U];
            record.track_name_field[index] = static_cast<char>(
                (word >> ((index % 4U) * 8U)) & 0xffU);
            if (record.track_name_field[index] == '\0') break;
            record.track_name.push_back(record.track_name_field[index]);
        }
        record.field_0x10_low_byte = static_cast<std::uint8_t>(
            record.raw_words[4] & 0xffU);
        record.field_0x14 = record.raw_words[5];
        for (std::size_t index = 0; index < record.money_values.size(); ++index) {
            record.money_values[index] = record.raw_words[
                career_season_event_money_word_offset + index];
            record.points_by_placement[index] = record.raw_words[
                career_season_event_money_word_offset +
                career_season_money_value_count + index];
        }
        data.events.push_back(record);
    }

    data.records_complete = decoded_count == declared_count;
    if (data.records_complete) {
        const auto expected_words = career_season_header_word_count +
            decoded_count * career_season_event_word_count;
        data.trailing_word_count = resource_words.size() - expected_words;
        data.exact_payload_size = data.trailing_word_count == 0;
    }
    return data;
}

CareerSeasonParsePlan plan_career_season_parse(
    const CareerSeasonParseObservation& observation)
{
    constexpr std::size_t header_money_count = 8;
    constexpr std::size_t first_event_money_word = 0x0f;
    constexpr std::size_t event_stride_words = 0x16;
    CareerSeasonParsePlan plan{};

    if (observation.resource_words.size() < header_money_count + 1) {
        plan.input_complete = false;
        return plan;
    }

    const auto event_count = observation.resource_words[0];
    plan.declared_event_count.emplace(event_count);
    std::size_t required_words = header_money_count + 1;
    if (event_count != 0) {
        const auto max_count = (std::numeric_limits<std::size_t>::max() -
            (first_event_money_word + header_money_count)) /
            event_stride_words;
        if (event_count - 1 > max_count) {
            plan.input_complete = false;
            return plan;
        }
        const auto last_record_start = first_event_money_word +
            static_cast<std::size_t>(event_count - 1) * event_stride_words;
        required_words = last_record_start + header_money_count;
    }

    const auto available_words = observation.resource_words.size();
    plan.input_complete = available_words >= required_words;
    const auto parsed_events = event_count == 0
        ? std::size_t{0}
        : std::min<std::size_t>(event_count,
            available_words >= first_event_money_word + header_money_count
                ? 1 + (available_words - first_event_money_word -
                    header_money_count) / event_stride_words
                : 0);

    const auto expected_label_count = header_money_count +
        parsed_events * header_money_count;
    plan.checked_money_labels.reserve(expected_label_count);
    const auto append_label = [&](const std::size_t word_index) {
        CareerSeasonMoneyLabelPlan label{};
        label.resource_word_index = word_index;
        label.money_value = observation.resource_words[word_index];
        const auto lookup_index = plan.checked_money_labels.size();
        if (lookup_index < observation.money_label_lookup_results.size() &&
            observation.money_label_lookup_results[lookup_index] <= 1) {
            const bool found = observation.money_label_lookup_results[lookup_index] != 0;
            label.label_found.emplace(found);
            label.panic_reporter_called = !found;
            plan.panic_reporter_call_count += label.panic_reporter_called ? 1U : 0U;
        } else {
            plan.input_complete = false;
        }
        plan.checked_money_labels.push_back(label);
    };

    for (std::size_t index = 1; index <= header_money_count; ++index) {
        append_label(index);
    }
    for (std::size_t event = 0; event < parsed_events; ++event) {
        const auto record_start = first_event_money_word +
            event * event_stride_words;
        for (std::size_t prize = 0; prize < header_money_count; ++prize) {
            append_label(record_start + prize);
        }
    }
    return plan;
}

} // namespace viper
