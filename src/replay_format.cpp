#include "viper/replay_format.hpp"

#include <bit>

namespace viper {
namespace {

constexpr std::uint32_t kErrorHelper = 0x00410ea0U;
constexpr std::uint32_t kCannotReadHeader = 0x004d5040U;
constexpr std::uint32_t kBadCookie = 0x004d5054U;
constexpr std::uint32_t kBadFormatVersion = 0x004d5068U;
constexpr std::uint32_t kBadWordCount = 0x004d508cU;
constexpr std::uint32_t kPayloadTooLarge = 0x004d50c0U;
constexpr std::uint32_t kBadFrameCount = 0x004d50e8U;
constexpr std::uint32_t kPacketSizeMismatch = 0x004d5100U;
constexpr std::uint32_t kCannotReadBuffer = 0x004d5124U;

void report_error(ReplayFileLoadPlan& plan, const std::uint32_t message)
{
    plan.calls.push_back(kErrorHelper);
    plan.error_string_addresses.push_back(message);
}

} // namespace

ReplayFileLoadPlan plan_replay_file_load(
    const ReplayFileLoadObservation& observation)
{
    ReplayFileLoadPlan result{};
    result.calls = {0x415090U, 0x4150a0U, 0x4257d0U, 0x411560U};

    if (!observation.header_read.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.header_read) {
        report_error(result, kCannotReadHeader);
        result.exit = ReplayFileLoadExit::header_read_failed;
        return result;
    }

    if (!observation.magic.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (*observation.magic != 0x5245504cU) {
        report_error(result, kBadCookie);
        result.exit = ReplayFileLoadExit::bad_cookie;
        return result;
    }

    if (!observation.format_version.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (*observation.format_version != 6U) {
        report_error(result, kBadFormatVersion);
        result.exit = ReplayFileLoadExit::bad_format_version;
        return result;
    }

    if (!observation.file_word_count.has_value() ||
        !observation.expected_word_count_0x508a58.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (*observation.file_word_count != std::bit_cast<std::uint32_t>(
            *observation.expected_word_count_0x508a58)) {
        report_error(result, kBadWordCount);
        result.exit = ReplayFileLoadExit::incompatible_word_count;
        return result;
    }

    if (!observation.payload_bytes.has_value()) {
        result.input_complete = false;
        return result;
    }
    result.payload_bytes = *observation.payload_bytes;
    if (std::bit_cast<std::int32_t>(*observation.payload_bytes) > 0x200000) {
        report_error(result, kPayloadTooLarge);
        result.exit = ReplayFileLoadExit::payload_too_large;
        return result;
    }

    if (!observation.timestamp_or_index_word.has_value() ||
        !observation.frame_count.has_value() ||
        !observation.base_packet_size.has_value()) {
        result.input_complete = false;
        return result;
    }
    result.loaded_timestamp_or_index_word =
        *observation.timestamp_or_index_word;
    result.loaded_frame_count = *observation.frame_count;

    // 0x426670 and 0x426940 derive the replay cursor from the file fields.
    result.calls.insert(result.calls.end(), {0x426670U, 0x426940U});
    if (*observation.frame_count < 0 ||
        *observation.frame_count >
            *observation.expected_word_count_0x508a58) {
        report_error(result, kBadFrameCount);
        result.exit = ReplayFileLoadExit::frame_count_out_of_range;
        return result;
    }

    if (!observation.expected_packet_size_0x508ad0.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (*observation.base_packet_size !=
        *observation.expected_packet_size_0x508ad0) {
        report_error(result, kPacketSizeMismatch);
        report_error(result, kCannotReadBuffer);
        result.exit = ReplayFileLoadExit::packet_size_mismatch;
        return result;
    }

    const auto word_count = static_cast<std::uint32_t>(
        *observation.expected_word_count_0x508a58);
    result.frame_table_bytes = word_count * 4U;
    result.calls.push_back(0x411560U);
    if (!observation.frame_table_read.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.frame_table_read) {
        report_error(result, kCannotReadBuffer);
        result.exit = ReplayFileLoadExit::buffer_read_failed;
        return result;
    }

    result.calls.push_back(0x411560U);
    if (!observation.payload_read.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.payload_read) {
        report_error(result, kCannotReadBuffer);
        result.exit = ReplayFileLoadExit::buffer_read_failed;
        return result;
    }

    result.calls.push_back(0x426580U);
    result.replay_loaded_flag_set = true;
    result.exit = ReplayFileLoadExit::loaded;
    return result;
}

} // namespace viper
