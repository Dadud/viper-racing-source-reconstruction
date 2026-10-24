#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

// Return the class-specific cursor advance used by 0x4209E0 for a serialized
// physics-task record tag. This is distinct from the number of bytes copied
// into the task buffer and from the constructed object's allocation size.
[[nodiscard]] std::optional<std::uint32_t>
physics_task_stream_advance_for_tag(std::uint32_t record_tag) noexcept;

struct PhysicsTaskStreamRecordOffset {
    std::uint32_t record_tag{};
    std::uint32_t stream_offset{};
    std::uint32_t cursor_advance{};
};

enum class PhysicsTaskStreamPlanStatus {
    complete,
    unknown_record_tag,
    cursor_overflow,
};

struct PhysicsTaskStreamPlan {
    PhysicsTaskStreamPlanStatus status{PhysicsTaskStreamPlanStatus::complete};
    std::uint32_t initial_cursor{};
    std::uint32_t final_cursor{};
    std::vector<PhysicsTaskStreamRecordOffset> records;
    std::optional<std::size_t> failed_record_index;
};

// Folds records in their already-observed serialization order. The initial
// native cursor and record order are explicit inputs; this function does not
// infer either from track.obt lines or from object allocation sizes.
[[nodiscard]] PhysicsTaskStreamPlan plan_physics_task_stream_offsets(
    std::span<const std::uint32_t> ordered_record_tags,
    std::uint32_t initial_cursor = 0U);

} // namespace viper
