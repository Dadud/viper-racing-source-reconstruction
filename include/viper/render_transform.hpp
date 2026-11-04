#pragma once

#include <array>
#include <cmath>

namespace viper {

// The reconstructed body transform uses the native 3x3 row-major basis and
// translation layout at offsets +0x38..+0x64.
using BodyRenderTransform = std::array<float, 12>;

struct ProjectedVehiclePoint {
    float x{};
    float z{};
};

[[nodiscard]] inline BodyRenderTransform make_yaw_render_transform(
    const float x, const float y, const float z, const float yaw) noexcept
{
    BodyRenderTransform transform{};
    const float sine = std::sin(yaw);
    const float cosine = std::cos(yaw);
    transform[0] = cosine;
    transform[2] = -sine;
    transform[4] = 1.0F;
    transform[6] = sine;
    transform[8] = cosine;
    transform[9] = x;
    transform[10] = y;
    transform[11] = z;
    return transform;
}

[[nodiscard]] inline ProjectedVehiclePoint transform_vehicle_point_xz(
    const BodyRenderTransform& transform,
    const std::array<float, 3>& local_point) noexcept
{
    // FNIM points are row vectors. Retain the native matrix convention used
    // by the body integrator, then project the world-space point onto X/Z.
    return {
        transform[9] + local_point[0] * transform[0] +
            local_point[1] * transform[3] + local_point[2] * transform[6],
        transform[11] + local_point[0] * transform[2] +
            local_point[1] * transform[5] + local_point[2] * transform[8],
    };
}

[[nodiscard]] inline float render_transform_yaw(
    const BodyRenderTransform& transform) noexcept
{
    return std::atan2(transform[6], transform[0]);
}

} // namespace viper
