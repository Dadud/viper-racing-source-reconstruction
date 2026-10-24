#include "viper/ai_driver_resource.hpp"
#include "viper/ai_notes_resource.hpp"

#include <cstddef>
#include <cstdint>
#include <bit>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void write_u16_le(std::vector<std::byte>& bytes, const std::size_t offset,
                  const std::uint16_t value)
{
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void write_u32_le(std::vector<std::byte>& bytes, const std::size_t offset,
                  const std::uint32_t value)
{
    for (std::size_t i = 0; i < 4U; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8U)) & 0xffU);
}

void write_f32_le(std::vector<std::byte>& bytes, const std::size_t offset,
                  const float value)
{
    write_u32_le(bytes, offset, std::bit_cast<std::uint32_t>(value));
}

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void test_ai_driver_resource()
{
    using namespace viper;
    std::vector<std::byte> payload(ai_driver_resource_bytes);
    write_u32_le(payload, 0x2200U, 3U);
    write_u32_le(payload, 7U * ai_driver_group_bytes + 0x2200U, 1U);
    const auto retained_record = 7U * ai_driver_group_bytes;
    payload[retained_record + 17U] = std::byte{0x5a};
    const auto selected_record = 2U * ai_driver_record_bytes;
    write_f32_le(payload, selected_record + 0x20U, 2.0F);
    write_f32_le(payload, selected_record + 0x6cU, 3.0F);
    constexpr std::size_t selected_row = ai_driver_track_profile_offset +
        (3U + ai_driver_track_count) * ai_driver_track_profile_bytes;
    write_f32_le(payload, selected_record + selected_row, 4.0F);
    write_f32_le(payload, selected_record + selected_row + 4U, 5.0F);
    write_f32_le(payload, selected_record + selected_row + 0x0cU, 6.0F);

    const auto parsed = parse_ai_driver_resource(2U, payload);
    require(parsed.has_value(), "valid AIDR v2 resource rejected");
    require(parsed->active_counts[0] == 3U && parsed->active_counts[7] == 1U,
            "AIDR group count mismatch");
    require(parsed->records[7][0][17] == std::byte{0x5a},
            "AIDR raw record bytes were not preserved");
    const auto* driver = find_ai_driver_record(*parsed, AIDriverGroup::easy, 2U);
    require(driver != nullptr, "active AIDR driver lookup failed");
    const auto tuning = select_ai_driver_track_tuning(*driver, 3U, 1U);
    require(tuning.has_value() && tuning->record_scale_0x20 == 2.0F &&
            tuning->record_scale_0x6c == 3.0F &&
            tuning->track_value_0x90 == 4.0F &&
            tuning->track_value_0x94 == 5.0F &&
            tuning->track_value_0x9c == 6.0F,
            "native AIDR track-row indexing mismatch");
    require(find_ai_driver_record(*parsed, AIDriverGroup::easy, 3U) == nullptr,
            "inactive AIDR driver slot accepted");
    require(!select_ai_driver_track_tuning(*driver, 8U, 0U) &&
            !select_ai_driver_track_tuning(*driver, 0U, 2U),
            "out-of-range AIDR tuning row accepted");
    require(!parse_ai_driver_resource(1U, payload),
            "unsupported AIDR version accepted");
    require(!parse_ai_driver_resource(2U,
        std::span<const std::byte>(payload).first(payload.size() - 1U)),
        "truncated AIDR payload accepted");

    write_u32_le(payload, 5U * ai_driver_group_bytes + 0x2200U, 17U);
    require(!parse_ai_driver_resource(2U, payload),
            "AIDR group count beyond its 16 records accepted");
}

std::vector<std::byte> make_note_payload()
{
    using namespace viper;
    std::vector<std::byte> payload(AINoteResource::header_bytes + 16U);
    write_u32_le(payload, 0x00U, 7U);
    write_u32_le(payload, 0x04U, 0x40U);
    write_u32_le(payload, 0x08U, 12U);
    write_u32_le(payload, 0x0cU, 2U);
    write_u32_le(payload, 0x10U, 7U);
    constexpr std::string_view car = "ViPeR";
    for (std::size_t i = 0; i < car.size(); ++i)
        payload[0x14U + i] = static_cast<std::byte>(car[i]);
    write_u16_le(payload, 0x34U, 0xa1b2U);
    write_u16_le(payload, 0x36U, 0xc3d4U);
    write_u32_le(payload, 0x38U, 2U);
    write_u32_le(payload, 0x3cU, 16U);
    for (std::size_t i = AINoteResource::header_bytes; i < payload.size(); ++i)
        payload[i] = static_cast<std::byte>(i & 0xffU);
    return payload;
}

void test_ai_note_resource()
{
    using namespace viper;
    auto payload = make_note_payload();
    const auto parsed = parse_ai_note_resource(7U, payload);
    require(parsed.has_value(), "valid ADNT v7 resource rejected");
    require(parsed->track_index == 12U && parsed->entrant_group == 2U &&
            parsed->difficulty == 7U && parsed->car_stem == "ViPeR",
            "ADNT lookup header mismatch");
    require(parsed->redundant_check == 0xa1b2U &&
            parsed->ideal_line_check == 0xc3d4U &&
            parsed->ideal_line_node_count == 2U &&
            parsed->node_payload.size() == 16U,
            "ADNT check and payload fields mismatch");

    const AINoteLookupKey key{2U, 7U, 12U, "viper"};
    require(ai_note_matches_lookup_key(*parsed, key),
            "case-insensitive ADNT key match failed");
    require(make_ai_note_path(key, AINotePathKind::track_sidecar) ==
            ".trk27cvipr.dnt", "native track sidecar path mismatch");
    require(make_ai_note_path(key, AINotePathKind::notes_directory) ==
            "*notes\\27cvipr.dnt", "native notes path mismatch");
    const AINoteLookupKey gt_key{2U, 7U, 12U, "vipergt"};
    require(make_ai_note_path(gt_key, AINotePathKind::track_sidecar) ==
            ".trk27cvgt.dnt", "vipergt note-name normalization mismatch");
    require(!make_ai_note_path(AINoteLookupKey{7U, 7U, 12U, "viper"},
            AINotePathKind::track_sidecar), "invalid entrant group accepted");
    require(!make_ai_note_path(AINoteLookupKey{2U, 8U, 12U, "viper"},
            AINotePathKind::track_sidecar), "invalid difficulty accepted");
    require(!ai_note_matches_lookup_key(*parsed,
            AINoteLookupKey{2U, 7U, 11U, "viper"}),
            "mismatched ADNT track accepted");

    require(!parse_ai_note_resource(6U, payload),
            "unsupported ADNT resource version accepted");
    write_u32_le(payload, 0U, 6U);
    require(!parse_ai_note_resource(7U, payload),
            "unsupported ADNT internal version accepted");
    write_u32_le(payload, 0U, 7U);
    write_u32_le(payload, 0x38U, 0U);
    require(!parse_ai_note_resource(7U, payload),
            "zero-node ADNT resource accepted");
    write_u32_le(payload, 0x38U, 3U);
    write_u32_le(payload, 0x3cU, 23U);
    payload.resize(AINoteResource::header_bytes + 23U);
    require(!parse_ai_note_resource(7U, payload),
            "ADNT payload with an invalid node-size ratio accepted");
}

} // namespace

int main()
{
    try {
        test_ai_driver_resource();
        test_ai_note_resource();
        std::cout << "AI resource regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
