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

struct WorldVehiclePoint {
    float x{};
    float y{};
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

[[nodiscard]] inline WorldVehiclePoint transform_vehicle_point_xyz(
    const BodyRenderTransform& transform,
    const std::array<float, 3>& local_point) noexcept
{
    // FNIM points are row vectors. Retain the native matrix convention used
    // by the body integrator and preserve all three world-space coordinates.
    return {
        transform[9] + local_point[0] * transform[0] +
            local_point[1] * transform[3] + local_point[2] * transform[6],
        transform[10] + local_point[0] * transform[1] +
            local_point[1] * transform[4] + local_point[2] * transform[7],
        transform[11] + local_point[0] * transform[2] +
            local_point[1] * transform[5] + local_point[2] * transform[8],
    };
}

[[nodiscard]] inline ProjectedVehiclePoint transform_vehicle_point_xz(
    const BodyRenderTransform& transform,
    const std::array<float, 3>& local_point) noexcept
{
    const auto world = transform_vehicle_point_xyz(transform, local_point);
    return {world.x, world.z};
}

[[nodiscard]] inline float render_transform_yaw(
    const BodyRenderTransform& transform) noexcept
{
    return std::atan2(transform[6], transform[0]);
}

} // namespace viper
