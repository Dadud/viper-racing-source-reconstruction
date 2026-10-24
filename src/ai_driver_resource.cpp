#include "viper/ai_driver_resource.hpp"

#include <algorithm>
#include <bit>

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

float read_f32_le(const std::span<const std::byte> bytes,
                  const std::size_t offset) noexcept
{
    return std::bit_cast<float>(read_u32_le(bytes, offset));
}

} // namespace

std::optional<AIDriverResource> parse_ai_driver_resource(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    if (resource_version != 2U || payload.size() != ai_driver_resource_bytes)
        return std::nullopt;

    AIDriverResource resource;
    for (std::size_t group = 0; group < ai_driver_group_count; ++group) {
        const auto group_offset = group * ai_driver_group_bytes;
        const auto count = read_u32_le(payload, group_offset +
            ai_driver_slots_per_group * ai_driver_record_bytes);
        if (count > ai_driver_slots_per_group) return std::nullopt;
        resource.active_counts[group] = count;
        for (std::size_t slot = 0; slot < ai_driver_slots_per_group; ++slot) {
            const auto record_offset = group_offset + slot * ai_driver_record_bytes;
            std::copy_n(payload.begin() + static_cast<std::ptrdiff_t>(record_offset),
                ai_driver_record_bytes, resource.records[group][slot].begin());
        }
    }
    return resource;
}

const AIDriverResource::Record* find_ai_driver_record(
    const AIDriverResource& resource,
    const AIDriverGroup group,
    const std::size_t driver_index) noexcept
{
    const auto group_index = static_cast<std::size_t>(group);
    if (group_index >= ai_driver_group_count ||
        driver_index >= ai_driver_slots_per_group ||
        driver_index >= resource.active_counts[group_index]) {
        return nullptr;
    }
    return &resource.records[group_index][driver_index];
}

std::optional<AIDriverTrackTuning> select_ai_driver_track_tuning(
    const AIDriverResource::Record& record,
    const std::uint32_t track_index,
    const std::uint8_t mode)
{
    if (track_index >= ai_driver_track_count || mode > 1U) return std::nullopt;
    const auto row = static_cast<std::size_t>(track_index) +
        static_cast<std::size_t>(mode) * ai_driver_track_count;
    const auto row_offset = ai_driver_track_profile_offset +
        row * ai_driver_track_profile_bytes;
    const auto bytes = std::span<const std::byte>(record);
    return AIDriverTrackTuning{
        read_f32_le(bytes, 0x20U),
        read_f32_le(bytes, 0x6cU),
        read_f32_le(bytes, row_offset),
        read_f32_le(bytes, row_offset + 4U),
        read_f32_le(bytes, row_offset + 0x0cU),
    };
}

} // namespace viper
