#include "viper/ai_driver_resource.hpp"
#include "viper/ai_notes_resource.hpp"

#include <cstddef>
#include <cstdint>
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

    const auto parsed = parse_ai_driver_resource(2U, payload);
    require(parsed.has_value(), "valid AIDR v2 resource rejected");
    require(parsed->active_counts[0] == 3U && parsed->active_counts[7] == 1U,
            "AIDR group count mismatch");
    require(parsed->records[7][0][17] == std::byte{0x5a},
            "AIDR raw record bytes were not preserved");
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
