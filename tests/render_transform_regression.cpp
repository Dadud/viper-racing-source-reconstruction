#include "viper/render_transform.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

bool close(const float left, const float right) noexcept
{
    return std::abs(left - right) <= 1.0e-5F;
}

} // namespace

int main()
{
    constexpr float yaw = 0.63F;
    const auto yaw_transform = viper::make_yaw_render_transform(
        13.0F, 2.0F, -7.0F, yaw);
    const std::array<float, 3> local_point{1.25F, 0.4F, -2.0F};
    const auto yaw_projected = viper::transform_vehicle_point_xz(
        yaw_transform, local_point);
    const auto expected_yaw_x = 13.0F + local_point[0] * std::cos(yaw) +
        local_point[2] * std::sin(yaw);
    const auto expected_yaw_z = -7.0F - local_point[0] * std::sin(yaw) +
        local_point[2] * std::cos(yaw);
    require(close(yaw_projected.x, expected_yaw_x) &&
            close(yaw_projected.z, expected_yaw_z),
        "yaw-only projection changed from the previous renderer transform");
    require(close(viper::render_transform_yaw(yaw_transform), yaw),
        "yaw extraction changed for a yaw-only body transform");
    const auto yaw_world = viper::transform_vehicle_point_xyz(
        yaw_transform, local_point);
    require(close(yaw_world.x, expected_yaw_x) &&
            close(yaw_world.y, 2.4F) &&
            close(yaw_world.z, expected_yaw_z),
        "full XYZ transform did not preserve vertical translation and scale");

    const viper::BodyRenderTransform pitched_and_rolled{
        0.8F, 0.1F, 0.6F,
        -0.2F, 0.9F, 0.3F,
        -0.55F, -0.35F, 0.76F,
        10.0F, 20.0F, 30.0F};
    const auto full_basis = viper::transform_vehicle_point_xz(
        pitched_and_rolled, {2.0F, 3.0F, 4.0F});
    require(close(full_basis.x, 8.8F) && close(full_basis.z, 35.14F),
        "full body basis did not transform local FNIM XYZ into world X/Z");
    const auto full_basis_xyz = viper::transform_vehicle_point_xyz(
        pitched_and_rolled, {2.0F, 3.0F, 4.0F});
    require(close(full_basis_xyz.x, 8.8F) &&
            close(full_basis_xyz.y, 21.5F) &&
            close(full_basis_xyz.z, 35.14F),
        "full body basis did not transform all local FNIM coordinates");

    const auto ignores_local_height =
        viper::make_yaw_render_transform(0.0F, 0.0F, 0.0F, yaw);
    const auto flat_projection = viper::transform_vehicle_point_xyz(
        ignores_local_height, {0.0F, 5.0F, 0.0F});
    require(close(flat_projection.x, 0.0F) &&
            close(flat_projection.y, 5.0F) &&
            close(flat_projection.z, 0.0F),
        "yaw-only transform did not preserve local height");

    std::cout << "Native 3x3 transform preserves full XYZ across yaw, pitch, and roll.\n";
    return 0;
}
