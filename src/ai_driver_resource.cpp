#include "viper/ai_driver_resource.hpp"

#include <algorithm>

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

} // namespace viper
