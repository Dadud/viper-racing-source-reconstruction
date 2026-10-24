#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace viper {

inline constexpr std::size_t ai_driver_group_count = 8;
inline constexpr std::size_t ai_driver_slots_per_group = 16;
inline constexpr std::size_t ai_driver_record_bytes = 0x220;
inline constexpr std::size_t ai_driver_group_bytes = 0x2204;
inline constexpr std::size_t ai_driver_resource_bytes =
    ai_driver_group_count * ai_driver_group_bytes;

enum class AIDriverGroup : std::size_t {
    easy,
    medium,
    hard,
    career1,
    career2,
    career3,
    career4,
    career5,
};

// Raw version-2 AIDR records. Record fields are deliberately unlabeled: the
// native code indexes each record at a 0x220-byte stride, but field semantics
// are not yet sufficiently recovered to expose typed values.
struct AIDriverResource {
    using Record = std::array<std::byte, ai_driver_record_bytes>;
    std::array<std::uint32_t, ai_driver_group_count> active_counts{};
    std::array<std::array<Record, ai_driver_slots_per_group>, ai_driver_group_count>
        records{};
};

// Parses the shipped AIDR v2 layout (eight 0x2204-byte groups). The native
// client also accepts a legacy v2 payload size; that conversion format is not
// included in this parser yet.
[[nodiscard]] std::optional<AIDriverResource> parse_ai_driver_resource(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

} // namespace viper
