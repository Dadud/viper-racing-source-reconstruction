#pragma once

#include <cstdint>

namespace viper {

struct LegacyAllocationPlan {
    std::uint32_t bytes_with_header{};
    std::int32_t size_marker{};
    std::uint8_t fill_byte{0xa3};
    bool use_vexed_device{};
};

// Reconstructs the allocation-path choice in 0x414170 for ordinary requests
// whose payload plus four-byte header fits in a positive signed 32-bit size.
// Failed VxD allocation attempts fall through to the process heap path.
[[nodiscard]] LegacyAllocationPlan plan_legacy_allocation(
    std::uint32_t payload_bytes,
    bool vxd_handle_available,
    bool vxd_allocator_enabled,
    bool vxd_allocation_succeeded) noexcept;

} // namespace viper
