#include "viper/physics_task_stream.hpp"

#include <limits>

namespace viper {

std::optional<std::uint32_t> physics_task_stream_advance_for_tag(
    const std::uint32_t record_tag) noexcept {
    switch (record_tag) {
    // Entries recovered from the lookup table at 0x4D4890, read by 0x4209E0.
    case 0x00434152U: // RAC\0
        return 0x18CU;
    case 0x47434152U: // GCAR
        return 0x190U;
    case 0x50434152U: // PCAR
    case 0x41434152U: // ACAR
    case 0x4E434152U: // NCAR
        return 0x18CU;
    case 0x53544154U: // STAT: scene record has no physics-task cursor advance
        return 0U;
    case 0x4F425354U: // OBST
    case 0x4C424F57U: // WOBL
    case 0x43484B50U: // CHKP
    case 0x42414C4CU: // BALL
        return 0x34U;
    default:
        return std::nullopt;
    }
}

PhysicsTaskStreamPlan plan_physics_task_stream_offsets(
    const std::span<const std::uint32_t> ordered_record_tags,
    const std::uint32_t initial_cursor) {
    PhysicsTaskStreamPlan plan;
    plan.initial_cursor = initial_cursor;
    plan.final_cursor = initial_cursor;
    plan.records.reserve(ordered_record_tags.size());

    for (std::size_t index = 0U; index < ordered_record_tags.size(); ++index) {
        const auto tag = ordered_record_tags[index];
        const auto advance = physics_task_stream_advance_for_tag(tag);
        if (!advance.has_value()) {
            plan.status = PhysicsTaskStreamPlanStatus::unknown_record_tag;
            plan.failed_record_index = index;
            return plan;
        }
        if (*advance > std::numeric_limits<std::uint32_t>::max()
                - plan.final_cursor) {
            plan.status = PhysicsTaskStreamPlanStatus::cursor_overflow;
            plan.failed_record_index = index;
            return plan;
        }
        plan.records.push_back({tag, plan.final_cursor, *advance});
        plan.final_cursor += *advance;
    }

    return plan;
}

} // namespace viper
