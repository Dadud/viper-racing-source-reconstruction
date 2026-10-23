#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace viper {

struct EntityUpdateDispatchInput {
    // Result from virtual +0x0C after the unconditional +8 callback.
    bool update_enabled{};
    // Result from virtual +0x10. False dispatches +0x14 immediately;
    // true queues the object for the sorted second phase.
    bool defer_update{};
    // Result from virtual +0x20; used only for deferred objects.
    float deferred_order_key{};
};

struct EntityVirtualCall {
    std::size_t object_index{};
    std::uint8_t vtable_offset{};
};

// Reproduces the two-phase virtual callback order in 0x45BD20. Each object's
// input contains the outcomes of +0x0C, +0x10 and +0x20 callbacks.
[[nodiscard]] std::vector<EntityVirtualCall> entity_update_callback_order(
    std::span<const EntityUpdateDispatchInput> objects);

} // namespace viper
