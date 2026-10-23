#include "viper/matrix_quaternion.hpp"

#include <cmath>
#include <cstddef>

namespace viper {

MatrixQuaternionResult matrix3x3_to_quaternion(
    const std::array<float, 9>& matrix,
    const bool fdiv_compatibility_enabled) noexcept
{
    MatrixQuaternionResult result{};
    result.fdiv_compatibility_branch = fdiv_compatibility_enabled;

    const auto trace = static_cast<long double>(matrix[0]) +
        static_cast<long double>(matrix[4]) +
        static_cast<long double>(matrix[8]);
    if (trace > 0.0L) {
        result.positive_trace_branch = true;
        const auto root = std::sqrt(trace + 1.0L);
        const auto half_inverse_root = 0.5L / root;
        result.quaternion[3] = static_cast<float>(0.5L * root);
        result.quaternion[0] = static_cast<float>(
            (static_cast<long double>(matrix[5]) - matrix[7]) *
            half_inverse_root);
        result.quaternion[1] = static_cast<float>(
            (static_cast<long double>(matrix[6]) - matrix[2]) *
            half_inverse_root);
        result.quaternion[2] = static_cast<float>(
            (static_cast<long double>(matrix[1]) - matrix[3]) *
            half_inverse_root);
        return result;
    }

    auto axis = matrix[0] < matrix[4] ? std::size_t{1} : std::size_t{0};
    if (matrix[axis * 4] < matrix[8]) {
        axis = 2;
    }
    const auto next = (axis + 1) % 3;
    const auto final = (next + 1) % 3;
    result.diagonal_branch_axis = static_cast<std::int8_t>(axis);

    const auto root = std::sqrt(
        static_cast<long double>(matrix[axis * 4]) -
        (static_cast<long double>(matrix[final * 4]) +
         static_cast<long double>(matrix[next * 4])) + 1.0L);
    const auto half_inverse_root = 0.5L / root;
    result.quaternion[axis] = static_cast<float>(0.5L * root);
    result.quaternion[3] = static_cast<float>(
        (static_cast<long double>(matrix[final + next * 3]) -
         static_cast<long double>(matrix[final * 3 + next])) *
        half_inverse_root);
    result.quaternion[next] = static_cast<float>(
        (static_cast<long double>(matrix[axis * 3 + next]) +
         static_cast<long double>(matrix[next * 3 + axis])) *
        half_inverse_root);
    result.quaternion[final] = static_cast<float>(
        (static_cast<long double>(matrix[axis + final * 3]) +
         static_cast<long double>(matrix[axis * 3 + final])) *
        half_inverse_root);
    return result;
}

QuaternionSlerpResult quaternion_slerp(
    const std::array<float, 4>& source,
    const std::array<float, 4>& target,
    const float amount,
    const bool fdiv_compatibility_enabled) noexcept
{
    QuaternionSlerpResult result{};
    result.fdiv_compatibility_branch = fdiv_compatibility_enabled;
    auto from = source;
    auto to = target;
    auto dot = static_cast<float>(
        static_cast<double>(from[2]) * to[2] +
        static_cast<double>(to[3]) * from[3] +
        static_cast<double>(to[1]) * from[1] +
        static_cast<double>(to[0]) * from[0]);
    if (dot < 0.0F) {
        for (auto& component : from) {
            component = -component;
        }
        dot = -dot;
        result.source_negated = true;
    }

    const auto sine_squared = std::fabs(1.0 -
        static_cast<double>(dot) * static_cast<double>(dot));
    const auto sine = static_cast<float>(std::sqrt(sine_squared));
    if (std::fabs(sine) < 1.1920929e-7F) {
        result.quaternion = from;
        result.near_parallel_copy = true;
        return result;
    }

    const auto angle = std::atan2(static_cast<long double>(sine),
                                  static_cast<long double>(dot));
    const auto reciprocal_sine = 1.0L / static_cast<long double>(sine);
    const auto source_weight = std::sin(
        (1.0L - static_cast<long double>(amount)) * angle) * reciprocal_sine;
    const auto target_weight = std::sin(
        angle * static_cast<long double>(amount)) * reciprocal_sine;
    for (std::size_t component = 0; component < result.quaternion.size(); ++component) {
        result.quaternion[component] = static_cast<float>(
            static_cast<long double>(from[component]) * source_weight +
            static_cast<long double>(to[component]) * target_weight);
    }
    const auto x = static_cast<long double>(result.quaternion[0]);
    const auto y = static_cast<long double>(result.quaternion[1]);
    const auto z = static_cast<long double>(result.quaternion[2]);
    const auto w = static_cast<long double>(result.quaternion[3]);
    const auto norm_squared = x * x + w * w + z * z + y * y;
    const auto inverse_norm = 1.0L / std::sqrt(norm_squared);
    for (auto& component : result.quaternion) {
        component = static_cast<float>(
            static_cast<long double>(component) * inverse_norm);
    }
    return result;
}

std::array<float, 9> quaternion_to_matrix3x3(
    const std::array<float, 4>& quaternion) noexcept
{
    const auto x = static_cast<long double>(quaternion[0]);
    const auto y = static_cast<long double>(quaternion[1]);
    const auto z = static_cast<long double>(quaternion[2]);
    const auto w = static_cast<long double>(quaternion[3]);
    const auto twice_x = static_cast<float>(x * 2.0L);
    const auto twice_y = static_cast<float>(y * 2.0L);
    const auto twice_z = static_cast<float>(z * 2.0L);
    const auto twice_x_times_x = static_cast<float>(x * 2.0L * x);
    const auto twice_z_times_z = static_cast<float>(z * 2.0L * z);
    const auto twice_wx = static_cast<float>(w * x * 2.0L);
    return {
        static_cast<float>(1.0L - (
            static_cast<long double>(twice_y) * y +
            static_cast<long double>(twice_z) * z)),
        static_cast<float>(x * twice_y - w * twice_z),
        static_cast<float>(x * twice_z + w * twice_y),
        static_cast<float>(w * twice_z + x * twice_y),
        static_cast<float>(1.0L - (
            static_cast<long double>(twice_x_times_x) +
            static_cast<long double>(twice_z_times_z))),
        static_cast<float>(y * twice_z - w * twice_y),
        static_cast<float>(x * twice_z - w * twice_y),
        static_cast<float>(static_cast<long double>(twice_wx) + y * twice_z),
        static_cast<float>(1.0L - (
            static_cast<long double>(twice_y) * y +
            static_cast<long double>(twice_x_times_x))),
    };
}

} // namespace viper
