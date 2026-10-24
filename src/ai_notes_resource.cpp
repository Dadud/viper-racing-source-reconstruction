#include "viper/ai_notes_resource.hpp"

#include <algorithm>
#include <cstdio>

namespace viper {
namespace {

std::uint32_t read_u32_le(const std::span<const std::byte> bytes,
                          const std::size_t offset) noexcept
{
    return std::to_integer<std::uint32_t>(bytes[offset]) |
        (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

std::uint16_t read_u16_le(const std::span<const std::byte> bytes,
                          const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint16_t>(bytes[offset]) |
        (std::to_integer<std::uint16_t>(bytes[offset + 1U]) << 8U));
}

bool ascii_equal_case_insensitive(const std::string_view left,
                                  const std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto a = static_cast<unsigned char>(left[index]);
        const auto b = static_cast<unsigned char>(right[index]);
        const auto lower = [](const unsigned char value) {
            return value >= 'A' && value <= 'Z'
                ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
        };
        if (lower(a) != lower(b)) return false;
    }
    return true;
}

bool valid_car_stem(const std::string_view value) noexcept
{
    if (value.empty() || value.size() >= AINoteResource::car_name_bytes)
        return false;
    for (const unsigned char ch : value) {
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'))
            return false;
    }
    return true;
}

std::string normalize_note_car_stem(const std::string_view value)
{
    if (ascii_equal_case_insensitive(value, "viper")) return "vipr";
    if (ascii_equal_case_insensitive(value, "vipergt")) return "vgt";
    return std::string(value.substr(0U, 4U));
}

} // namespace

std::optional<std::string> make_ai_note_path(const AINoteLookupKey& key,
                                              const AINotePathKind kind)
{
    if (key.entrant_group > 6U || key.difficulty > 7U || key.track_index > 15U ||
        !valid_car_stem(key.car_stem)) {
        return std::nullopt;
    }
    const auto normalized_car = normalize_note_car_stem(key.car_stem);
    char formatted[3U * 8U + AINoteResource::car_name_bytes]{};
    const auto written = std::snprintf(formatted, sizeof(formatted), "%x%x%x%.*s",
        static_cast<unsigned int>(key.entrant_group),
        static_cast<unsigned int>(key.difficulty),
        static_cast<unsigned int>(key.track_index),
        static_cast<int>(normalized_car.size()), normalized_car.data());
    if (written < 0 || static_cast<std::size_t>(written) >= sizeof(formatted))
        return std::nullopt;

    std::string path;
    switch (kind) {
    case AINotePathKind::track_sidecar:
        path = ".trk";
        break;
    case AINotePathKind::notes_directory:
        path = "*notes\\";
        break;
    default:
        return std::nullopt;
    }
    path.append(formatted, static_cast<std::size_t>(written));
    path += ".dnt";
    return path;
}

std::optional<AINoteResource> parse_ai_note_resource(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    constexpr std::size_t car_name_offset = 0x14U;
    constexpr std::size_t redundant_check_offset = 0x34U;
    constexpr std::size_t line_check_offset = 0x36U;
    constexpr std::size_t line_count_offset = 0x38U;
    constexpr std::size_t node_bytes_offset = 0x3cU;
    if (resource_version != 7U || payload.size() < AINoteResource::header_bytes ||
        read_u32_le(payload, 0U) != 7U ||
        read_u32_le(payload, 4U) != AINoteResource::header_bytes) {
        return std::nullopt;
    }

    const auto node_count = read_u32_le(payload, line_count_offset);
    const auto node_bytes = read_u32_le(payload, node_bytes_offset);
    const auto available_node_bytes = payload.size() - AINoteResource::header_bytes;
    if (node_bytes != available_node_bytes || node_count == 0U ||
        node_bytes / node_count != 8U) {
        return std::nullopt;
    }

    const auto car_field = payload.subspan(car_name_offset,
        AINoteResource::car_name_bytes);
    const auto terminator = std::find(car_field.begin(), car_field.end(), std::byte{0});
    if (terminator == car_field.end()) return std::nullopt;
    const auto car_length = static_cast<std::size_t>(terminator - car_field.begin());
    if (car_length == 0U) return std::nullopt;

    AINoteResource note;
    note.track_index = read_u32_le(payload, 0x08U);
    note.entrant_group = read_u32_le(payload, 0x0cU);
    note.difficulty = read_u32_le(payload, 0x10U);
    note.car_stem.assign(reinterpret_cast<const char*>(car_field.data()), car_length);
    note.redundant_check = read_u16_le(payload, redundant_check_offset);
    note.ideal_line_check = read_u16_le(payload, line_check_offset);
    note.ideal_line_node_count = node_count;
    note.node_payload_bytes = node_bytes;
    note.node_payload.assign(payload.begin() +
        static_cast<std::ptrdiff_t>(AINoteResource::header_bytes), payload.end());
    return note;
}

bool ai_note_matches_lookup_key(const AINoteResource& note,
                                const AINoteLookupKey& key) noexcept
{
    return note.track_index == key.track_index &&
        note.entrant_group == key.entrant_group &&
        note.difficulty == key.difficulty &&
        ascii_equal_case_insensitive(note.car_stem, key.car_stem);
}

} // namespace viper
