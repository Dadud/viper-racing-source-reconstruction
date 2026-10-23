#pragma once

#include <array>
#include <cstdint>

namespace viper {

struct MatrixQuaternionResult {
    std::array<float, 4> quaternion{};
    bool positive_trace_branch{};
    std::int8_t diagonal_branch_axis{-1};
    bool fdiv_compatibility_branch{};
};

struct QuaternionSlerpResult {
    std::array<float, 4> quaternion{};
    bool source_negated{};
    bool near_parallel_copy{};
    bool fdiv_compatibility_branch{};
};

// Reconstructs 0x422FD0's row-major 3x3 matrix conversion. Its caller supplies
// the global Pentium-FDIV compatibility flag; long-double arithmetic preserves
// the x87 intermediate path, while the host divide approximates its correction.
[[nodiscard]] MatrixQuaternionResult matrix3x3_to_quaternion(
    const std::array<float, 9>& matrix,
    bool fdiv_compatibility_enabled) noexcept;

// Models the shortest-arc spherical interpolation in 0x4C5F00. The optional
// Pentium-FDIV path is reported but its correction is not emulated.
[[nodiscard]] QuaternionSlerpResult quaternion_slerp(
    const std::array<float, 4>& source,
    const std::array<float, 4>& target,
    float amount,
    bool fdiv_compatibility_enabled) noexcept;

// Converts x/y/z/w quaternion values to the row-major matrix stores used by
// the NCAR rotation callback.
[[nodiscard]] std::array<float, 9> quaternion_to_matrix3x3(
    const std::array<float, 4>& quaternion) noexcept;

} // namespace viper
