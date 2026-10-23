#include "viper/legacy_allocator.hpp"

#include <bit>

namespace viper {

LegacyAllocationPlan plan_legacy_allocation(
    const std::uint32_t payload_bytes,
    const bool vxd_handle_available,
    const bool vxd_allocator_enabled,
    const bool vxd_allocation_succeeded) noexcept
{
    const auto bytes = payload_bytes + 4U;
    const bool use_vxd = vxd_handle_available
        && vxd_allocator_enabled
        && vxd_allocation_succeeded;
    const auto marker_bits = use_vxd ? (0U - bytes) : bytes;
    return {
        bytes,
        std::bit_cast<std::int32_t>(marker_bits),
        0xa3U,
        use_vxd,
    };
}

} // namespace viper
