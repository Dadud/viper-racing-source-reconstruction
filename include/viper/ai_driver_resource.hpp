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
inline constexpr std::size_t ai_driver_track_profile_offset = 0x90;
inline constexpr std::size_t ai_driver_track_profile_bytes = 0x18;
inline constexpr std::size_t ai_driver_track_count = 8;
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

struct AIDriverTrackTuning {
    // Native fields are retained by record offset until their gameplay meaning
    // is established: the initializer reads these exact five binary32 values.
    float record_scale_0x20{};
    float record_scale_0x6c{};
    float track_value_0x90{};
    float track_value_0x94{};
    float track_value_0x9c{};
};

// Parses the shipped AIDR v2 layout (eight 0x2204-byte groups). The native
// client also accepts a legacy v2 payload size; that conversion format is not
// included in this parser yet.
[[nodiscard]] std::optional<AIDriverResource> parse_ai_driver_resource(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

[[nodiscard]] const AIDriverResource::Record* find_ai_driver_record(
    const AIDriverResource& resource,
    AIDriverGroup group,
    std::size_t driver_index) noexcept;

// Mirrors 0x41A000's row address: track_index + mode*8, with 24-byte rows
// beginning at record +0x90. Only the two observed modes and eight tracks are
// accepted, preventing the native unchecked address from escaping the record.
[[nodiscard]] std::optional<AIDriverTrackTuning> select_ai_driver_track_tuning(
    const AIDriverResource::Record& record,
    std::uint32_t track_index,
    std::uint8_t mode);

} // namespace viper
