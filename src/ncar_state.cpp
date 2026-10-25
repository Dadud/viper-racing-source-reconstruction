#include "viper/ncar_state.hpp"

#include "viper/legacy_fdiv.hpp"
#include "viper/matrix_quaternion.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace viper {
namespace {

struct DerivativeCoefficients {
    long double at_sample_0{};
    long double at_sample_1{};
    long double at_sample_2{};
};

std::uint32_t read_u32_le(const std::byte* const bytes, const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U);
}

std::uint16_t read_u16_le(const std::byte* const bytes, const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U);
}

void write_u32_le(std::byte* const bytes, const std::uint32_t value) noexcept
{
    for (unsigned i = 0; i < 4; ++i) {
        bytes[i] = static_cast<std::byte>((value >> (i * 8U)) & 0xffU);
    }
}

void write_u16_le(std::byte* const bytes, const std::uint16_t value) noexcept
{
    bytes[0] = static_cast<std::byte>(value & 0xffU);
    bytes[1] = static_cast<std::byte>((value >> 8U) & 0xffU);
}

std::int32_t ftol_nearest_approximation(const long double value) noexcept
{
    const long double rounded = std::nearbyint(value);
    if (!std::isfinite(rounded)
        || rounded < static_cast<long double>(std::numeric_limits<std::int32_t>::min())
        || rounded > static_cast<long double>(std::numeric_limits<std::int32_t>::max())) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(rounded);
}

DerivativeCoefficients derivative_coefficients(
    const NcarSampleWindow& samples,
    const bool fdiv_compatibility_enabled) noexcept
{
    const auto as_float = [](const std::uint32_t bits) noexcept {
        return std::bit_cast<float>(bits);
    };
    const long double x0 = as_float(samples.records[0].sample_time_bits_0x04);
    const long double x1 = as_float(samples.records[1].sample_time_bits_0x04);
    const long double x2 = as_float(samples.records[2].sample_time_bits_0x04);
    return {
        divide_with_vc98_fdiv_compatibility(
            ((2.0L * x0 - x1) - x2), (x0 - x1) * (x0 - x2),
            fdiv_compatibility_enabled),
        divide_with_vc98_fdiv_compatibility(
            (x0 - x2), (x1 - x0) * (x1 - x2),
            fdiv_compatibility_enabled),
        divide_with_vc98_fdiv_compatibility(
            (x0 - x1), (x2 - x0) * (x2 - x1),
            fdiv_compatibility_enabled),
    };
}

DerivativeCoefficients history_derivative_coefficients(
    const NcarSampleWindow& samples,
    const bool fdiv_compatibility_enabled) noexcept
{
    const auto as_float = [](const std::uint32_t bits) noexcept {
        return std::bit_cast<float>(bits);
    };
    const long double x0 = as_float(samples.records[0].sample_time_bits_0x04);
    const long double x1 = as_float(samples.records[1].sample_time_bits_0x04);
    const long double x2 = as_float(samples.records[2].sample_time_bits_0x04);

    // 0x439520 stores these two differences to float stack locals at 0x4395E3
    // and 0x4395E7, then reuses those rounded values for all three axes.
    const long double delta_0_1 = static_cast<float>(x0 - x1);
    const long double delta_0_2 = static_cast<float>(x0 - x2);
    return {
        divide_with_vc98_fdiv_compatibility(
            ((2.0L * x0 - x1) - x2), delta_0_1 * delta_0_2,
            fdiv_compatibility_enabled),
        divide_with_vc98_fdiv_compatibility(
            delta_0_2, (x1 - x0) * (x1 - x2),
            fdiv_compatibility_enabled),
        divide_with_vc98_fdiv_compatibility(
            delta_0_1, (x2 - x0) * (x2 - x1),
            fdiv_compatibility_enabled),
    };
}

template <std::size_t RecordBytes>
std::array<float, 12> axis_angle_record_matrix(
    const std::array<std::uint8_t, RecordBytes>& record,
    const bool fdiv_compatibility_enabled) noexcept
{
    constexpr long double angle_scale = 0.0003834952L;
    constexpr long double angle_bias = 6.2831855L;
    std::array<long double, 3> rotation_vector{};
    for (std::size_t axis = 0; axis < rotation_vector.size(); ++axis) {
        const auto offset = axis * 2U;
        const auto encoded = static_cast<std::uint16_t>(record[offset]) |
            static_cast<std::uint16_t>(record[offset + 1U] << 8U);
        rotation_vector[axis] = static_cast<float>(
            static_cast<float>(encoded) * static_cast<float>(angle_scale) -
            static_cast<float>(angle_bias));
    }

    const auto x = rotation_vector[0];
    const auto y = rotation_vector[1];
    const auto z = rotation_vector[2];
    const auto angle = static_cast<float>(std::sqrt(x * x + y * y + z * z));
    std::array<float, 9> matrix{};
    if (angle <= 0.0001F) {
        matrix = {1.0F, 0.0F, 0.0F,
                  0.0F, 1.0F, 0.0F,
                  0.0F, 0.0F, 1.0F};
    } else {
        const long double inverse = divide_with_vc98_fdiv_compatibility(
            1.0L, angle, fdiv_compatibility_enabled);
        const long double nx = x * inverse;
        const long double ny = y * inverse;
        const long double nz = z * inverse;
        const long double sine = std::sin(static_cast<long double>(angle));
        const long double cosine = std::cos(static_cast<long double>(angle));
        const long double one_minus_cosine = 1.0L - cosine;
        matrix = {
            static_cast<float>(nx * nx * one_minus_cosine + cosine),
            static_cast<float>(nx * ny * one_minus_cosine + nz * sine),
            static_cast<float>(nx * nz * one_minus_cosine - ny * sine),
            static_cast<float>(nx * ny * one_minus_cosine - nz * sine),
            static_cast<float>(ny * ny * one_minus_cosine + cosine),
            static_cast<float>(ny * nz * one_minus_cosine + nx * sine),
            static_cast<float>(nx * nz * one_minus_cosine + ny * sine),
            static_cast<float>(ny * nz * one_minus_cosine - nx * sine),
            static_cast<float>(nz * nz * one_minus_cosine + cosine),
        };
    }

    std::array<float, 12> transform{};
    std::copy(matrix.begin(), matrix.end(), transform.begin());
    for (std::size_t axis = 0; axis < 3; ++axis) {
        transform[9U + axis] = std::bit_cast<float>(read_u32_le(
            reinterpret_cast<const std::byte*>(record.data()), 6U + axis * 4U));
    }
    return transform;
}

std::array<float, 12> interpolate_and_orthonormalize_transform(
    const std::array<float, 12>& first,
    const std::array<float, 12>& second,
    const float alpha,
    const bool fdiv_compatibility_enabled) noexcept
{
    std::array<float, 12> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<float>(
            (static_cast<long double>(second[index]) - first[index]) * alpha +
            first[index]);
    }
    const auto normalize = [&](std::array<float, 3>& vector) noexcept {
        const long double length = std::sqrt(
            static_cast<long double>(vector[0]) * vector[0] +
            static_cast<long double>(vector[1]) * vector[1] +
            static_cast<long double>(vector[2]) * vector[2]);
        const long double inverse = divide_with_vc98_fdiv_compatibility(
            1.0L, length, fdiv_compatibility_enabled);
        for (auto& component : vector) {
            component = static_cast<float>(component * inverse);
        }
    };
    const auto cross = [](const std::array<float, 3>& left,
                          const std::array<float, 3>& right) noexcept {
        return std::array<float, 3>{
            left[1] * right[2] - right[1] * left[2],
            left[2] * right[0] - right[2] * left[0],
            left[0] * right[1] - right[0] * left[1]};
    };
    std::array<float, 3> first_row{result[0], result[1], result[2]};
    std::array<float, 3> discarded_second_row{result[3], result[4], result[5]};
    std::array<float, 3> third_row{result[6], result[7], result[8]};
    normalize(first_row);
    normalize(discarded_second_row);
    normalize(third_row);
    auto second_row = cross(third_row, first_row);
    normalize(first_row);
    normalize(second_row);
    third_row = cross(first_row, second_row);
    std::copy(first_row.begin(), first_row.end(), result.begin());
    std::copy(second_row.begin(), second_row.end(), result.begin() + 3);
    std::copy(third_row.begin(), third_row.end(), result.begin() + 6);
    return result;
}

} // namespace

bool accumulate_ncar_force_moment(
    NcarForceMomentAccumulator& accumulator,
    const std::array<float, 3>& vector,
    const std::array<float, 3>& application_point,
    const std::array<float, 3>& center) noexcept
{
    constexpr long double squared_length_threshold = 1.1920929e-7L;
    const auto x = static_cast<long double>(vector[0]);
    const auto y = static_cast<long double>(vector[1]);
    const auto z = static_cast<long double>(vector[2]);
    const auto squared_length = std::fabs(x * x + y * y + z * z);
    if (!(squared_length > squared_length_threshold)) {
        return false;
    }

    const auto arm_x = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[0]) - center[0]));
    const auto arm_y = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[1]) - center[1]));
    const auto arm_z = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[2]) - center[2]));
    accumulator.force_0x24c[0] = static_cast<float>(
        static_cast<long double>(accumulator.force_0x24c[0]) + x);
    accumulator.force_0x24c[1] = static_cast<float>(
        static_cast<long double>(accumulator.force_0x24c[1]) + y);
    accumulator.force_0x24c[2] = static_cast<float>(
        static_cast<long double>(accumulator.force_0x24c[2]) + z);
    const auto moment_x = static_cast<float>(arm_y * z - arm_z * y);
    const auto moment_y = static_cast<float>(arm_z * x - arm_x * z);
    const auto moment_z = static_cast<float>(arm_x * y - arm_y * x);
    accumulator.moment_0x258[0] = static_cast<float>(
        static_cast<long double>(accumulator.moment_0x258[0]) + moment_x);
    accumulator.moment_0x258[1] = static_cast<float>(
        static_cast<long double>(accumulator.moment_0x258[1]) + moment_y);
    accumulator.moment_0x258[2] = static_cast<float>(
        static_cast<long double>(accumulator.moment_0x258[2]) + moment_z);
    return true;
}

std::array<float, 3> transform_ncar_vector_at_point(
    const NcarPointTransformFields& body,
    const std::array<float, 3>& application_point) noexcept
{
    const auto arm_x = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[0]) - body.center_0x5c[0]));
    const auto arm_y = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[1]) - body.center_0x5c[1]));
    const auto arm_z = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[2]) - body.center_0x5c[2]));
    const auto wx = static_cast<long double>(body.angular_0x240[0]);
    const auto wy = static_cast<long double>(body.angular_0x240[1]);
    const auto wz = static_cast<long double>(body.angular_0x240[2]);
    const auto cross_x = static_cast<float>(wy * arm_z - wz * arm_y);
    const auto cross_y = static_cast<float>(wz * arm_x - wx * arm_z);
    const auto cross_z = static_cast<float>(wx * arm_y - wy * arm_x);
    return {
        static_cast<float>(static_cast<long double>(body.linear_0x234[0]) + cross_x),
        static_cast<float>(static_cast<long double>(body.linear_0x234[1]) + cross_y),
        static_cast<float>(static_cast<long double>(body.linear_0x234[2]) + cross_z),
    };
}

void accumulate_ncar_moment_vector(
    NcarForceMomentAccumulator& accumulator,
    const std::array<float, 3>& vector) noexcept
{
    for (std::size_t axis = 0; axis < vector.size(); ++axis) {
        accumulator.moment_0x258[axis] = static_cast<float>(
            static_cast<long double>(accumulator.moment_0x258[axis]) + vector[axis]);
    }
}

void accumulate_ncar_point_impulse(
    NcarPointImpulseAccumulator& accumulator,
    const std::array<float, 3>& impulse,
    const std::array<float, 3>& application_point,
    const std::array<float, 3>& center,
    const float duration) noexcept
{
    const auto arm_x = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[0]) - center[0]));
    const auto arm_y = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[1]) - center[1]));
    const auto arm_z = static_cast<long double>(static_cast<float>(
        static_cast<long double>(application_point[2]) - center[2]));
    const auto x = static_cast<long double>(impulse[0]);
    const auto y = static_cast<long double>(impulse[1]);
    const auto z = static_cast<long double>(impulse[2]);
    const auto dt = static_cast<long double>(duration);

    accumulator.duration_0x2ac = static_cast<float>(
        static_cast<long double>(accumulator.duration_0x2ac) + dt);
    accumulator.impulse_time_0x294[0] = static_cast<float>(
        static_cast<long double>(accumulator.impulse_time_0x294[0]) + x * dt);
    accumulator.impulse_time_0x294[1] = static_cast<float>(
        static_cast<long double>(accumulator.impulse_time_0x294[1]) + y * dt);
    accumulator.impulse_time_0x294[2] = static_cast<float>(
        static_cast<long double>(accumulator.impulse_time_0x294[2]) + z * dt);
    const auto moment_x = static_cast<float>(z * arm_y - y * arm_z);
    const auto moment_y = static_cast<float>(x * arm_z - z * arm_x);
    const auto moment_z = static_cast<float>(y * arm_x - x * arm_y);
    accumulator.moment_time_0x2a0[0] = static_cast<float>(
        static_cast<long double>(accumulator.moment_time_0x2a0[0]) +
        static_cast<long double>(moment_x) * dt);
    accumulator.moment_time_0x2a0[1] = static_cast<float>(
        static_cast<long double>(accumulator.moment_time_0x2a0[1]) +
        static_cast<long double>(moment_y) * dt);
    accumulator.moment_time_0x2a0[2] = static_cast<float>(
        static_cast<long double>(accumulator.moment_time_0x2a0[2]) +
        static_cast<long double>(moment_z) * dt);
}

NcarIntegratorBookkeepingResult advance_ncar_integrator_bookkeeping(
    NcarIntegratorBookkeepingState& state,
    const NcarIntegratorBookkeepingInput& input) noexcept
{
    NcarIntegratorBookkeepingResult result{};
    auto counter_bits = std::bit_cast<std::uint32_t>(state.counter_0xd78) + 1U;
    state.counter_0xd78 = std::bit_cast<std::int32_t>(counter_bits);
    if (state.counter_0xd78 >= 8) {
        state.counter_0xd78 = 0;
        result.counter_reset_to_zero = true;
        result.substep_helper_0x4323e0_reached = true;
        result.substep_helper_argument =
            std::bit_cast<float>(std::uint32_t{0x3e03126fU});
    }

    state.sampled_time_0xd70 = input.time_callback_result;
    state.sum_0xeb0 = static_cast<float>(
        static_cast<long double>(input.addend_0x5f8) + input.addend_0x79c);
    state.copied_value_0xd64 = input.source_0xcbc;
    state.copied_value_0xd68 = input.source_0xcb4;
    return result;
}

NcarIntegratorChildControlResult setup_ncar_integrator_child_control(
    NcarIntegratorChildControl& child,
    const std::uint32_t value_0xdec,
    const std::uint32_t value_0xde0,
    const std::uint8_t mode_byte_0x534) noexcept
{
    child.value_at_child_0x64 = value_0xde0;
    child.value_at_child_0x5c = value_0xdec;
    return {true, mode_byte_0x534};
}

NcarAnimationControllerResult advance_ncar_animation_controller(
    NcarIntegratorChildControl& child,
    NcarAnimationControllerTargets& targets,
    const NcarAnimationControllerObservation& observation) noexcept
{
    NcarAnimationControllerResult result{};
    const auto as_float = [](const std::uint32_t bits) noexcept {
        return std::bit_cast<float>(bits);
    };
    const auto bits_of = [](const float value) noexcept {
        return std::bit_cast<std::uint32_t>(value);
    };
    auto child_argument = child.value_at_child_0x64;
    auto selected_value = child.selection_index_0x24 < 1
        ? child.selected_value_0x18
        : observation.selected_value_for_index;
    const auto callback_value = observation.callback_value_0x50;
    const auto child_limit = observation.child_value_0x3c;
    const auto upper_value = observation.child_value_0x34 + 1000.0F;

    if (observation.mode_byte == 0U) {
        child.previous_index_0x1c = child.selection_index_0x24;
        child.ramp_value_0x60 = as_float(child.value_at_child_0x5c);
        child.animation_state_0x68 = 0;
    } else {
        const auto index = child.selection_index_0x24;
        if (index < 2 && child.previous_index_0x1c != index &&
            child.animation_state_0x68 == 4) {
            child.animation_state_0x68 = 0;
            child.previous_index_0x1c = index;
        }
        if (observation.child_override_flag_0x41) {
            child.animation_state_0x68 = 0;
        }

        switch (child.animation_state_0x68) {
        case 0:
            child.ramp_value_0x60 = as_float(child.value_at_child_0x5c);
            if (index != child.previous_index_0x1c) {
                if (child.previous_index_0x1c < 1 || index < 1) {
                    child.ramp_value_0x60 = 0.0F;
                    child.animation_state_0x68 = 4;
                    child.previous_index_0x1c = index;
                } else {
                    child.animation_state_0x68 = 1;
                }
            }
            break;
        case 1: {
            child.ramp_value_0x60 -= 0.128F;
            if (child.previous_index_0x1c < index &&
                std::bit_cast<std::int32_t>(bits_of(child.ramp_value_0x60)) <
                    0x3f000000) {
                child.previous_index_0x1c = 0;
            }
            if (bits_of(child.ramp_value_0x60) > 0x80000000U) {
                child.ramp_value_0x60 = 0.0F;
                child.animation_state_0x68 = 3;
                if (child.previous_index_0x1c <= index) {
                    child.animation_state_0x68 = 2;
                }
            }
            child_argument = 0U;
            break;
        }
        case 2:
            child_argument = 0U;
            child.ramp_value_0x60 = 0.0F;
            child.animation_state_0x68 = 4;
            break;
        case 3: {
            child_argument = 0x3f800000U;
            child.previous_index_0x1c = 0;
            child.ramp_value_0x60 = 1.0F;
            const auto scaled_selection =
                static_cast<long double>(observation.callback_value_0x4c) *
                static_cast<long double>(selected_value);
            if (scaled_selection < static_cast<long double>(callback_value) ||
                static_cast<long double>(child_limit) <= scaled_selection) {
                child.ramp_value_0x60 = 0.0F;
                child.animation_state_0x68 = 4;
            }
            break;
        }
        case 4:
            child.previous_index_0x1c = index;
            child.ramp_value_0x60 += 0.128F;
            if (std::bit_cast<std::int32_t>(bits_of(child.ramp_value_0x60)) >
                0x3f800000) {
                child.ramp_value_0x60 = 1.0F;
                child.animation_state_0x68 = 0;
            }
            if (index > 1) {
                child_argument = 0U;
            }
            break;
        case 5:
            if (static_cast<long double>(child_limit) * 0.7L <= callback_value ||
                std::bit_cast<std::int32_t>(child_argument) < 0x3eccccce) {
                child.animation_state_0x68 = 4;
            } else if (callback_value <= upper_value) {
                child.ramp_value_0x60 = 0.0F;
            } else {
                child.ramp_value_0x60 = 0.2F;
            }
            break;
        default:
            break;
        }

        if (callback_value < upper_value && child.animation_state_0x68 != 3) {
            if (observation.fdiv_compatibility_enabled) {
                result.fdiv_compatibility_path_reached = true;
            }
            auto ratio =
                (static_cast<long double>(callback_value) - observation.child_value_0x34) /
                (static_cast<long double>(upper_value) - observation.child_value_0x34);
            if (ratio <= 0.0L) {
                ratio = 0.0L;
            }
            if (static_cast<long double>(child.ramp_value_0x60) <= ratio) {
                ratio = child.ramp_value_0x60;
            }
            child.ramp_value_0x60 = static_cast<float>(ratio);
        }
    }

    child.selected_value_0x18 = child.selection_index_0x24 < 1
        ? (child.selection_index_0x24 < 0
            ? -child.negative_index_value_0x2c
            : 0.0F)
        : observation.selected_value_for_index;
    child.enabled_0x58 = child.selection_index_0x24 == 0 ? 0U : 1U;
    targets.value_target_0x2c = child_argument;
    result.wrote_value_target_0x2c = true;
    result.child_value_callback_argument = child_argument;
    targets.ramp_target_0x00 = child.ramp_value_0x60;
    result.wrote_ramp_target_0x00 = true;
    result.child_ramp_setter_value = child.ramp_value_0x60;
    return result;
}

bool initialize_ncar_constructor_storage(
    const std::span<std::byte> object,
    const std::span<std::byte> sample_window,
    const std::uint32_t sample_window_address) noexcept
{
    if (object.size() < NcarConstructorLayout::object_size ||
        sample_window.size() < NcarConstructorLayout::sample_window_bytes ||
        sample_window_address == 0U) {
        return false;
    }

    const auto store_word = [&](const std::size_t offset,
                                const std::uint32_t value) noexcept {
        write_u32_le(object.data() + offset, value);
    };
    const auto store_byte = [&](const std::size_t offset,
                                const std::uint8_t value) noexcept {
        object[offset] = static_cast<std::byte>(value);
    };

    store_word(0x00U, NcarConstructorLayout::vtable_address);
    for (const auto offset : {0x538U, 0x53cU, 0x540U, 0x544U,
                              0x548U, 0x54cU, 0x550U}) {
        store_word(offset, 0U);
    }
    for (std::size_t index = 0; index < 4U; ++index) {
        store_byte(0x6f4U + index * 0x1a4U, 1U);
    }
    for (const auto offset : {0xeccU, 0xed8U, 0xee4U, 0xef0U}) {
        store_word(offset, 0U);
        store_word(offset + 4U, 0U);
        store_word(offset + 8U, 0U);
    }
    store_word(0xec0U, sample_window_address);
    store_word(0xec4U, sample_window_address);
    std::fill_n(sample_window.begin(), NcarConstructorLayout::sample_window_bytes,
                std::byte{0});
    store_word(0xec8U, 0U);
    store_word(0xf1cU, 0xc47a0000U);
    store_word(0xf24U, 0x3f800000U);
    store_word(0xf30U, 0U);
    store_byte(0xf20U, 1U);
    store_byte(0xf34U, 0U);

    constexpr std::array<std::uint32_t, 4> ramp_initial_bits{
        0x3ecccccdU, 0x3ecccccdU, 0x3dcccccdU, 0x3ecccccdU,
    };
    for (std::size_t index = 0; index < ramp_initial_bits.size(); ++index) {
        const auto offset = 0xefcU + index * sizeof(std::uint32_t);
        store_word(offset, ramp_initial_bits[index]);
        store_word(offset + 0x10U, ramp_initial_bits[index]);
    }
    return true;
}

NcarDestructorPlan plan_ncar_destruction(
    const NcarDestructorObservation& observation) noexcept
{
    NcarDestructorPlan plan;
    plan.manager_index_0x510 = observation.manager_index_0x510;
    const auto append = [&plan](const NcarDestructorAction action) noexcept {
        plan.actions[plan.action_count++] = action;
    };

    append(NcarDestructorAction::install_ncar_vtable);
    if (!observation.event_processor_active) {
        if (observation.global_delay_queue_present) {
            append(NcarDestructorAction::destroy_global_delay_queue);
            append(NcarDestructorAction::free_global_delay_queue);
        }
        append(NcarDestructorAction::clear_global_delay_queue);
    }
    if (observation.event_processor_active) {
        append(NcarDestructorAction::unregister_shared_manager_client);
    }
    if (observation.indexed_manager_entry_present) {
        append(NcarDestructorAction::release_indexed_manager_entry);
    }
    append(NcarDestructorAction::clear_indexed_manager_slot);
    append(NcarDestructorAction::release_sample_window);
    append(NcarDestructorAction::clear_sample_window_fields);
    append(NcarDestructorAction::call_base_car_destructor);
    return plan;
}

bool apply_ncar_destructor_field_writes(
    const std::span<std::byte> object) noexcept
{
    if (object.size() < NcarConstructorLayout::object_size) {
        return false;
    }
    write_u32_le(object.data(), NcarConstructorLayout::vtable_address);
    write_u32_le(object.data() + 0xec0U, 0U);
    write_u32_le(object.data() + 0xec4U, 0U);
    return true;
}

std::uint32_t expand_ncar_wrapped_tick(const std::uint16_t wrapped_tick,
                                      const std::uint32_t current_tick) noexcept
{
    std::uint32_t candidate = (current_tick & 0xffff0000U)
        | static_cast<std::uint32_t>(wrapped_tick);
    const auto delta = static_cast<std::int64_t>(candidate)
        - static_cast<std::int64_t>(current_tick);
    if (delta > 0x8000) {
        candidate -= 0x10000U;
    } else if (delta < -0x8000) {
        candidate += 0x10000U;
    }
    return candidate;
}

float ncar_seconds_from_tick(const std::uint32_t expanded_tick,
                             const std::uint32_t epoch_tick_origin,
                             const float epoch_seconds_origin) noexcept
{
    const auto tick_delta = std::bit_cast<std::int32_t>(expanded_tick - epoch_tick_origin);
    const long double seconds = static_cast<long double>(tick_delta)
            * static_cast<long double>(0.001F)
        + static_cast<long double>(epoch_seconds_origin);
    return static_cast<float>(seconds);
}

NcarRotationDecode decode_ncar_rotation16(
    const std::array<std::uint16_t, 3>& rotation,
    const bool fdiv_compatibility_enabled) noexcept
{
    // 0x49E090 multiplies the unsigned word by 40 * 2^-15, then subtracts 20.
    constexpr long double scale = 40.0L / 32768.0L;
    constexpr long double zero_threshold = 1.1920929e-7L;
    std::array<long double, 3> axis_angle{};
    for (std::size_t i = 0; i < axis_angle.size(); ++i) {
        const long double unrounded = static_cast<long double>(rotation[i]) * scale - 20.0L;
        axis_angle[i] = static_cast<long double>(static_cast<float>(unrounded));
    }

    const long double squared_magnitude = axis_angle[0] * axis_angle[0]
        + axis_angle[1] * axis_angle[1]
        + axis_angle[2] * axis_angle[2];
    if (std::abs(squared_magnitude) <= zero_threshold) {
        return {{0.0F, 0.0F, 0.0F, 1.0F}, true,
                fdiv_compatibility_enabled};
    }

    const long double magnitude = std::sqrt(squared_magnitude);
    const long double half_angle = 0.5L * magnitude;
    const long double sine = std::sin(half_angle);
    const long double cosine = std::cos(half_angle);
    NcarRotationDecode result{};
    for (std::size_t i = 0; i < axis_angle.size(); ++i) {
        result.quaternion[i] = static_cast<float>(axis_angle[i] / magnitude * sine);
    }
    result.quaternion[3] = static_cast<float>(cosine);
    result.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    return result;
}

NcarRotationEncode encode_ncar_rotation16(
    const std::array<float, 4>& quaternion,
    const bool fdiv_compatibility_enabled) noexcept
{
    constexpr long double zero_threshold = 1.1920929e-7L;
    constexpr long double lower_bound = -20.0L;
    constexpr long double upper_bound = 20.0L;
    const long double angle_x87 = std::acos(static_cast<long double>(quaternion[3]));
    const long double sine_x87 = std::sin(angle_x87);
    const float angle_spill = static_cast<float>(angle_x87);
    const float sine_spill = static_cast<float>(sine_x87);
    const long double full_angle = 2.0L * static_cast<long double>(angle_spill);
    const bool zero_sine_branch = std::abs(sine_x87) <= zero_threshold;

    NcarRotationEncode result{};
    result.zero_sine_branch = zero_sine_branch;
    result.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    for (std::size_t i = 0; i < result.rotation.size(); ++i) {
        long double axis_component = 0.0L;
        if (!zero_sine_branch) {
            axis_component = (static_cast<long double>(quaternion[i])
                / static_cast<long double>(sine_spill)) * full_angle;
        }
        const float axis_spill = static_cast<float>(axis_component);
        const long double clamped_axis = std::clamp(
            static_cast<long double>(axis_spill), lower_bound, upper_bound);
        const long double quantized =
            ((clamped_axis - lower_bound) / (upper_bound - lower_bound)) * 32768.0L;
        result.rotation[i] = static_cast<std::uint16_t>(
            ftol_nearest_approximation(quantized));
    }
    return result;
}

NcarPackedPayload24 encode_ncar_packed_payload24(
    const NcarSampleRecord& record,
    const std::uint32_t tick_origin,
    const float seconds_origin,
    const bool fdiv_compatibility_enabled) noexcept
{
    NcarPackedPayload24 result{};
    result.fdiv_compatibility_enabled = fdiv_compatibility_enabled;

    const float record_time = std::bit_cast<float>(record.sample_time_bits_0x04);
    const long double elapsed_tick_value =
        (static_cast<long double>(record_time) - static_cast<long double>(seconds_origin))
            * 1000.0L
        + 0.5F;
    const auto elapsed_ticks = ftol_nearest_approximation(elapsed_tick_value);
    const auto tick_bits = std::bit_cast<std::uint32_t>(elapsed_ticks) + tick_origin;
    write_u16_le(result.bytes.data(), static_cast<std::uint16_t>(tick_bits & 0xffffU));

    for (std::size_t axis = 0; axis < record.vector_bits_0x1c.size(); ++axis) {
        write_u32_le(result.bytes.data() + 2 + axis * 4, record.vector_bits_0x1c[axis]);
    }

    std::array<float, 4> quaternion{};
    for (std::size_t component = 0; component < quaternion.size(); ++component) {
        quaternion[component] = std::bit_cast<float>(record.rotation_bits_0x28[component]);
    }
    const auto encoded_rotation =
        encode_ncar_rotation16(quaternion, fdiv_compatibility_enabled);
    for (std::size_t component = 0; component < encoded_rotation.rotation.size(); ++component) {
        write_u16_le(result.bytes.data() + 14 + component * 2,
                     encoded_rotation.rotation[component]);
    }

    const float scaled_value = std::bit_cast<float>(record.scaled_byte_value_bits_0x18);
    std::uint8_t scaled_byte = 0xffU;
    if (!(scaled_value > 8000.0F)) {
        const long double quantized =
            (static_cast<long double>(scaled_value) / 8000.0L) * 255.0L;
        scaled_byte = static_cast<std::uint8_t>(
            ftol_nearest_approximation(quantized));
    }
    result.bytes[20] = static_cast<std::byte>(scaled_byte);

    const auto scalar_bits = record.signed_scalar_bits_0x0c;
    const float scalar_value = std::bit_cast<float>(scalar_bits);
    const std::uint8_t sign_code = scalar_bits > 0x80000000U ? 0x10U : 0U;
    const auto magnitude_code = static_cast<std::uint8_t>(ftol_nearest_approximation(
        std::abs(static_cast<long double>(scalar_value)) * 15.0L));
    const auto low_bits = static_cast<std::uint8_t>(record.small_integer_0x14 & 7U);
    result.bytes[21] = static_cast<std::byte>(
        low_bits | static_cast<std::uint8_t>((sign_code | magnitude_code) << 3U));

    std::uint8_t packed_flags = 0;
    const auto value_0x08 = std::bit_cast<std::int32_t>(record.value_bits_0x08);
    if (value_0x08 > 0x3c23d70a) {
        packed_flags |= 0x01U;
    }
    packed_flags |= static_cast<std::uint8_t>((record.flag_0x38 & 1U) << 1U);
    packed_flags |= static_cast<std::uint8_t>((record.flag_0x39 & 1U) << 2U);
    for (std::size_t flag = 0; flag < record.gate_bytes_0x3a.size(); ++flag) {
        if (record.gate_bytes_0x3a[flag] != 0) {
            packed_flags |= static_cast<std::uint8_t>(1U << (flag + 3U));
        }
    }
    result.bytes[22] = static_cast<std::byte>(packed_flags);
    result.unspecified_bit_mask[22] = static_cast<std::byte>(0x80U);
    result.unspecified_bit_mask[23] = static_cast<std::byte>(0xffU);
    return result;
}

NcarSampleProducerCaptureResult capture_ncar_sample_record(
    NcarSampleRecord& destination,
    NcarSampleProducerObjectState& body,
    const float current_time,
    const bool fdiv_compatibility_enabled) noexcept
{
    NcarSampleProducerCaptureResult result{};
    result.fdiv_compatibility_enabled = fdiv_compatibility_enabled;

    auto candidate_orientation = body.orientation_0x38;
    const auto normalize = [fdiv_compatibility_enabled](
        const std::array<float, 3>& vector,
        std::array<float, 3>& normalized) noexcept {
        const long double x = vector[0];
        const long double y = vector[1];
        const long double z = vector[2];
        const long double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > 0.0L) || !std::isfinite(length)) return false;
        const long double inverse = divide_with_vc98_fdiv_compatibility(
            1.0L, length, fdiv_compatibility_enabled);
        normalized = {static_cast<float>(x * inverse),
                      static_cast<float>(y * inverse),
                      static_cast<float>(z * inverse)};
        return std::isfinite(normalized[0]) && std::isfinite(normalized[1]) &&
            std::isfinite(normalized[2]);
    };

    // 0x422C90 normalizes the first basis vector, takes the cross product of
    // the original third and first vectors for the second, then recomputes the
    // third as their cross product.
    const std::array<float, 3> first_source{
        candidate_orientation[0], candidate_orientation[1], candidate_orientation[2]};
    const std::array<float, 3> third_source{
        candidate_orientation[6], candidate_orientation[7], candidate_orientation[8]};
    std::array<float, 3> first{};
    if (!normalize(first_source, first)) return result;
    const std::array<float, 3> second_source{
        third_source[1] * first_source[2] - third_source[2] * first_source[1],
        third_source[2] * first_source[0] - third_source[0] * first_source[2],
        third_source[0] * first_source[1] - third_source[1] * first_source[0]};
    std::array<float, 3> second{};
    if (!normalize(second_source, second)) return result;
    const std::array<float, 3> third{
        second[1] * first[2] - second[2] * first[1],
        second[2] * first[0] - second[0] * first[2],
        second[0] * first[1] - second[1] * first[0]};
    std::copy(first.begin(), first.end(), candidate_orientation.begin());
    std::copy(second.begin(), second.end(), candidate_orientation.begin() + 3);
    std::copy(third.begin(), third.end(), candidate_orientation.begin() + 6);

    // Inline matrix-to-quaternion conversion at 0x43E050 follows 0x422FD0's
    // row-major branch equations once 0x422C90 has rebuilt the basis.
    const auto converted = matrix3x3_to_quaternion(
        candidate_orientation, fdiv_compatibility_enabled);
    if (!std::all_of(converted.quaternion.begin(), converted.quaternion.end(),
            [](const float value) { return std::isfinite(value); })) {
        return result;
    }

    NcarSampleRecord captured = destination;
    const auto store_u32 = [&captured](const std::size_t offset,
                                       const std::uint32_t value) noexcept {
        write_u32_le(captured.raw_bytes.data() + offset, value);
    };
    const auto store_float = [&store_u32](const std::size_t offset,
                                          const float value) noexcept {
        store_u32(offset, std::bit_cast<std::uint32_t>(value));
    };

    captured.sample_time_bits_0x04 = std::bit_cast<std::uint32_t>(current_time);
    captured.value_bits_0x08 = std::bit_cast<std::uint32_t>(body.value_0xde4);
    captured.signed_scalar_bits_0x0c = std::bit_cast<std::uint32_t>(body.value_0x500);
    captured.value_bits_0x10 = body.value_0xc34;
    captured.small_integer_0x14 = body.value_0xd64;
    captured.scaled_byte_value_bits_0x18 = std::bit_cast<std::uint32_t>(body.value_0xd70);
    store_float(0x04, current_time);
    store_u32(0x08, captured.value_bits_0x08);
    store_u32(0x0c, captured.signed_scalar_bits_0x0c);
    store_u32(0x10, captured.value_bits_0x10);
    store_u32(0x14, captured.small_integer_0x14);
    store_u32(0x18, captured.scaled_byte_value_bits_0x18);

    for (std::size_t axis = 0; axis < body.position_0x5c.size(); ++axis) {
        captured.vector_bits_0x1c[axis] =
            std::bit_cast<std::uint32_t>(body.position_0x5c[axis]);
        store_float(0x1c + axis * 4U, body.position_0x5c[axis]);
    }
    for (std::size_t component = 0; component < converted.quaternion.size(); ++component) {
        captured.rotation_bits_0x28[component] =
            std::bit_cast<std::uint32_t>(converted.quaternion[component]);
        store_float(0x28 + component * 4U, converted.quaternion[component]);
    }
    captured.flag_0x38 = body.flag_0xe28;
    captured.flag_0x39 = body.flag_0xec0;
    captured.gate_bytes_0x3a = body.wheel_damage_0x574;
    captured.raw_bytes[0x38] = static_cast<std::byte>(captured.flag_0x38);
    captured.raw_bytes[0x39] = static_cast<std::byte>(captured.flag_0x39);
    for (std::size_t wheel = 0; wheel < captured.gate_bytes_0x3a.size(); ++wheel) {
        captured.raw_bytes[0x3a + wheel] =
            static_cast<std::byte>(captured.gate_bytes_0x3a[wheel]);
    }

    body.orientation_0x38 = candidate_orientation;
    body.flag_0xec0 = 0U;
    destination = captured;
    result.captured = true;
    result.orientation_normalized = true;
    return result;
}

NcarPackedPayloadApplyResult decode_ncar_packed_payload24(
    const std::span<const std::byte, 0x18> payload,
    const std::uint32_t current_tick,
    const std::uint32_t tick_origin,
    const float seconds_origin,
    NcarSampleRecord& destination,
    const bool fdiv_compatibility_enabled) noexcept
{
    NcarPackedPayloadApplyResult result{};
    const auto wrapped_tick = read_u16_le(payload.data(), 0);
    result.expanded_tick = expand_ncar_wrapped_tick(wrapped_tick, current_tick);
    destination.sample_time_bits_0x04 = std::bit_cast<std::uint32_t>(
        ncar_seconds_from_tick(result.expanded_tick, tick_origin, seconds_origin));
    write_u32_le(destination.raw_bytes.data() + 0x04, destination.sample_time_bits_0x04);

    const std::uint8_t scaled_byte = std::to_integer<std::uint8_t>(payload[0x14]);
    const long double scaled_value = static_cast<long double>(scaled_byte)
        * static_cast<long double>(0.003921569F) * 8000.0L;
    destination.scaled_byte_value_bits_0x18 = std::bit_cast<std::uint32_t>(
        static_cast<float>(scaled_value));
    write_u32_le(destination.raw_bytes.data() + 0x18,
                 destination.scaled_byte_value_bits_0x18);

    const std::uint8_t packed_scalar = std::to_integer<std::uint8_t>(payload[0x15]);
    const std::uint8_t scalar_magnitude = static_cast<std::uint8_t>(
        (packed_scalar & 0x78U) >> 3U);
    long double scalar_value = static_cast<long double>(scalar_magnitude)
        * static_cast<long double>(0.06666667F);
    if ((packed_scalar & 0x80U) != 0) {
        scalar_value = -scalar_value;
    }
    destination.signed_scalar_bits_0x0c = std::bit_cast<std::uint32_t>(
        static_cast<float>(scalar_value));
    write_u32_le(destination.raw_bytes.data() + 0x0c,
                 destination.signed_scalar_bits_0x0c);

    const std::uint8_t integer_value = static_cast<std::uint8_t>(packed_scalar & 7U);
    destination.small_integer_0x14 = integer_value == 7U
        ? 0xffffffffU
        : static_cast<std::uint32_t>(integer_value);
    write_u32_le(destination.raw_bytes.data() + 0x14, destination.small_integer_0x14);

    const std::uint8_t packed_flags = std::to_integer<std::uint8_t>(payload[0x16]);
    const float value_0x08 = (packed_flags & 1U) != 0 ? 0.5F : 0.0F;
    destination.value_bits_0x08 = std::bit_cast<std::uint32_t>(value_0x08);
    write_u32_le(destination.raw_bytes.data() + 0x08, destination.value_bits_0x08);
    destination.flag_0x38 = static_cast<std::uint8_t>((packed_flags >> 1U) & 1U);
    destination.flag_0x39 = static_cast<std::uint8_t>((packed_flags >> 2U) & 1U);
    destination.raw_bytes[0x38] = static_cast<std::byte>(destination.flag_0x38);
    destination.raw_bytes[0x39] = static_cast<std::byte>(destination.flag_0x39);
    for (std::size_t flag = 0; flag < destination.gate_bytes_0x3a.size(); ++flag) {
        destination.gate_bytes_0x3a[flag] =
            static_cast<std::uint8_t>((packed_flags >> (flag + 3U)) & 1U);
        destination.raw_bytes[0x3a + flag] =
            static_cast<std::byte>(destination.gate_bytes_0x3a[flag]);
    }

    for (std::size_t axis = 0; axis < destination.vector_bits_0x1c.size(); ++axis) {
        destination.vector_bits_0x1c[axis] = read_u32_le(payload.data(), 2 + axis * 4);
        write_u32_le(destination.raw_bytes.data() + 0x1c + axis * 4,
                     destination.vector_bits_0x1c[axis]);
    }

    std::array<std::uint16_t, 3> encoded_rotation{};
    for (std::size_t component = 0; component < encoded_rotation.size(); ++component) {
        encoded_rotation[component] = read_u16_le(payload.data(), 0x0e + component * 2);
    }
    result.rotation = decode_ncar_rotation16(encoded_rotation, fdiv_compatibility_enabled);
    for (std::size_t component = 0; component < result.rotation.quaternion.size(); ++component) {
        destination.rotation_bits_0x28[component] =
            std::bit_cast<std::uint32_t>(result.rotation.quaternion[component]);
        write_u32_le(destination.raw_bytes.data() + 0x28 + component * 4,
                     destination.rotation_bits_0x28[component]);
    }
    return result;
}

NcarSampleWindow decode_ncar_sample_window(
    const std::span<const std::byte, ncar_sample_window_bytes> bytes) noexcept
{
    NcarSampleWindow result{};
    for (std::size_t i = 0; i < result.records.size(); ++i) {
        const std::size_t base = i * ncar_sample_record_bytes;
        auto& record = result.records[i];
        for (std::size_t byte = 0; byte < record.raw_bytes.size(); ++byte) {
            record.raw_bytes[byte] = bytes[base + byte];
        }
        record.time_bits_0x00 = read_u32_le(bytes.data(), base);
        record.sample_time_bits_0x04 = read_u32_le(bytes.data(), base + 0x04);
        record.value_bits_0x08 = read_u32_le(bytes.data(), base + 0x08);
        record.signed_scalar_bits_0x0c = read_u32_le(bytes.data(), base + 0x0c);
        record.value_bits_0x10 = read_u32_le(bytes.data(), base + 0x10);
        record.small_integer_0x14 = read_u32_le(bytes.data(), base + 0x14);
        record.scaled_byte_value_bits_0x18 = read_u32_le(bytes.data(), base + 0x18);
        for (std::size_t axis = 0; axis < record.vector_bits_0x1c.size(); ++axis) {
            record.vector_bits_0x1c[axis] = read_u32_le(bytes.data(), base + 0x1c + axis * 4);
        }
        for (std::size_t component = 0; component < record.rotation_bits_0x28.size(); ++component) {
            record.rotation_bits_0x28[component] =
                read_u32_le(bytes.data(), base + 0x28 + component * 4);
        }
        record.flag_0x38 = std::to_integer<std::uint8_t>(bytes[base + 0x38]);
        record.flag_0x39 = std::to_integer<std::uint8_t>(bytes[base + 0x39]);
        for (std::size_t gate = 0; gate < record.gate_bytes_0x3a.size(); ++gate) {
            record.gate_bytes_0x3a[gate] =
                std::to_integer<std::uint8_t>(bytes[base + 0x3a + gate]);
        }
    }
    return result;
}

NcarSampleUpdate update_ncar_sample_buffer(
    NcarSampleBuffer& buffer,
    const std::span<const std::byte, ncar_sample_record_bytes> incoming,
    const float current_time) noexcept
{
    if (buffer.record_mode_0xec8 != 0) {
        const float existing_parameter =
            std::bit_cast<float>(read_u32_le(buffer.bytes.data(), 0x04));
        const float incoming_parameter =
            std::bit_cast<float>(read_u32_le(incoming.data(), 0x04));
        if (!(existing_parameter < incoming_parameter)) {
            return {false, false, buffer.record_mode_0xec8};
        }
    }

    std::memmove(buffer.bytes.data() + ncar_sample_record_bytes,
                 buffer.bytes.data(), 2 * ncar_sample_record_bytes);
    std::memcpy(buffer.bytes.data(), incoming.data(), ncar_sample_record_bytes);
    write_u32_le(buffer.bytes.data(), std::bit_cast<std::uint32_t>(current_time));

    if (buffer.record_mode_0xec8 > 2) {
        return {true, true, buffer.record_mode_0xec8};
    }
    ++buffer.record_mode_0xec8;
    return {true, false, buffer.record_mode_0xec8};
}

NcarSampleLinkedTargetResult update_ncar_sample_linked_target(
    NcarSampleBuffer& buffer,
    NcarLinkedTargetFields& target,
    const NcarSampleLinkedTargetObservation& observation) noexcept
{
    NcarSampleLinkedTargetResult result;
    result.sample_update = update_ncar_sample_buffer(
        buffer, observation.incoming_sample, observation.current_time);
    if (!result.sample_update.run_post_accept_helpers) {
        return result;
    }

    const auto samples = decode_ncar_sample_window(buffer.bytes);
    const auto& newest = samples.records[0];
    const auto& oldest = samples.records[2];
    result.linked_target_gate_passed = should_update_ncar_linked_target(
        buffer.record_mode_0xec8,
        newest.flag_0x38,
        oldest.flag_0x38,
        observation.target_lookup_succeeded);
    if (!result.linked_target_gate_passed) {
        return result;
    }

    const auto sample_parameter = std::bit_cast<float>(
        newest.sample_time_bits_0x04);
    result.target_update = update_ncar_linked_target(
        target,
        observation.source_state_0x38,
        observation.source_vector_0x234,
        observation.current_time,
        sample_parameter,
        observation.type_name_is_plane);
    return result;
}

void apply_ncar_callback_gate_bytes(
    NcarCallbackGateBytes& object_bytes,
    const NcarSampleRecord& sample) noexcept
{
    for (std::size_t index = 0; index < object_bytes.size(); ++index) {
        object_bytes[index] = sample.gate_bytes_0x3a[index] != 0 ? 1U : 0U;
    }
}

NcarCallbackQueueResult advance_ncar_callback_queue(
    NcarCallbackQueueState& state,
    NcarSampleBuffer& sample_buffer,
    TimedEventQueue& event_queue,
    const NcarCallbackQueueObservation& observation)
{
    NcarCallbackQueueResult result{};
    if (observation.event_processor_active_0x49be40) {
        return result;
    }

    constexpr auto payload_interval = std::bit_cast<float>(
        std::uint32_t{0x3D888889U});
    if (static_cast<long double>(observation.current_time) -
        static_cast<long double>(state.last_payload_time_0xf30) >
        static_cast<long double>(payload_interval)) {
        state.last_payload_time_0xf30 = observation.current_time;
        result.enqueue_branch_reached = true;
        result.enqueue_succeeded = enqueue_timed_event(
            event_queue, observation.payload_0x3e, 0,
            observation.queue_clock, observation.queue_clock_offset);
    }

    result.dequeue_attempted = true;
    result.dequeue = poll_timed_event(
        event_queue, observation.queue_clock, ncar_sample_record_bytes);
    result.copied_payload_has_sample_size = result.dequeue.consumed &&
        result.dequeue.copied &&
        result.dequeue.payload.size() == ncar_sample_record_bytes;
    if (result.copied_payload_has_sample_size) {
        const std::span<const std::byte, ncar_sample_record_bytes> incoming{
            result.dequeue.payload.data(), ncar_sample_record_bytes};
        result.sample_update = update_ncar_sample_buffer(
            sample_buffer, incoming, observation.current_time);
    }
    return result;
}

NcarSampleDerivative derive_ncar_sample_vector(
    const NcarSampleWindow& samples,
    const bool fdiv_compatibility_enabled) noexcept
{
    const auto as_float = [](const std::uint32_t bits) noexcept {
        return std::bit_cast<float>(bits);
    };
    const auto coefficients = derivative_coefficients(samples, fdiv_compatibility_enabled);

    NcarSampleDerivative result{};
    for (std::size_t axis = 0; axis < result.values.size(); ++axis) {
        const long double y0 = as_float(samples.records[0].vector_bits_0x1c[axis]);
        const long double y1 = as_float(samples.records[1].vector_bits_0x1c[axis]);
        const long double y2 = as_float(samples.records[2].vector_bits_0x1c[axis]);
        long double value = coefficients.at_sample_2 * y2;
        value += coefficients.at_sample_1 * y1;
        value += coefficients.at_sample_0 * y0;
        result.values[axis] = static_cast<float>(value);
    }
    return result;
}

NcarInterpolatedWheelPlan update_ncar_interpolated_wheel(
    const NcarInterpolatedWheelObservation& observation) noexcept
{
    const auto interpolate = [&](const std::size_t byte_index,
                                 const long double scale,
                                 const long double bias = 0.0L) noexcept {
        const long double current = observation.first_record[byte_index];
        const long double next = observation.second_record[byte_index];
        const long double alpha = observation.interpolation_fraction;
        return static_cast<float>(current * scale +
            (next - current) * scale * alpha + bias);
    };

    NcarInterpolatedWheelPlan result{};
    result.field_0xe8 = interpolate(2, 1.0L / 64.0L);
    result.angle_0xac = interpolate(0, 256.0L * 0.0003834952L,
        -6.2831855L);
    result.field_0x124 = interpolate(1, 200.0L / 128.0L, -100.0L);
    result.field_0x11c = interpolate(3, 10.0L / 128.0L);
    result.flags_0x120 = observation.first_record[4];
    result.flag_0x20 = (result.flags_0x120 & 0x40U) != 0 ? 1U : 0U;
    result.copied_body_velocity_0xbc = observation.body_velocity_0x234;
    return result;
}

NcarInterpolatedRecordSelectionPlan select_ncar_interpolated_records(
    const NcarInterpolatedRecordSelectionObservation& observation) noexcept
{
    NcarInterpolatedRecordSelectionPlan result{};
    if (observation.global_count_0x508a58 <= 0) {
        return result;
    }

    const auto frame_step = std::bit_cast<std::int32_t>(
        observation.global_time_bits_0x508774 -
        observation.object_value_bits_0xedc -
        observation.object_value_bits_0xee0);
    result.selection_parameters_valid = true;
    result.frame_step_param_3 = frame_step;

    // Signed C++ division and remainder match the x86 idiv behavior for these
    // nonzero positive divisors, including negative frame positions.
    const auto first_slot = (frame_step / 6) %
        observation.global_count_0x508a58;
    const auto second_slot = (first_slot + 1) %
        observation.global_count_0x508a58;
    result.first_slot_index = first_slot;
    result.second_slot_index = second_slot;
    if (!(second_slot < observation.object_limit_0xef8)) {
        return result;
    }

    result.interpolation_callback_0x427b10_reached = true;
    result.interpolation_fraction = static_cast<float>(frame_step % 6) *
        0.16666667F;

    const auto available_records = observation.records.size() /
        ncar_interpolated_record_bytes;
    if (observation.record_pointer_origin_index > available_records ||
        observation.record_pointer_origin_index > static_cast<std::size_t>(
            std::numeric_limits<std::int64_t>::max())) {
        return result;
    }
    const auto origin = static_cast<std::int64_t>(
        observation.record_pointer_origin_index);
    const auto first_storage_index = origin + first_slot;
    const auto second_storage_index = origin + second_slot;
    if (first_storage_index < 0 || second_storage_index < 0 ||
        first_storage_index >= static_cast<std::int64_t>(available_records) ||
        second_storage_index >= static_cast<std::int64_t>(available_records)) {
        return result;
    }

    result.record_window_valid = true;
    const auto* const records = observation.records.data();
    std::memcpy(result.first_record.data(),
        records + static_cast<std::size_t>(first_storage_index) *
            ncar_interpolated_record_bytes,
        ncar_interpolated_record_bytes);
    std::memcpy(result.second_record.data(),
        records + static_cast<std::size_t>(second_storage_index) *
            ncar_interpolated_record_bytes,
        ncar_interpolated_record_bytes);
    return result;
}

NcarObjectRegistrationPlan register_ncar_object(
    const NcarObjectRegistrationObservation& observation) noexcept
{
    NcarObjectRegistrationPlan result{};
    result.table = observation.table;
    if (observation.index < 0 ||
        observation.index >= static_cast<std::int32_t>(
            ncar_registered_object_count)) {
        result.out_of_table_write_unmodeled = true;
        return result;
    }

    result.index_in_table = true;
    result.table[static_cast<std::size_t>(observation.index)] =
        observation.object_pointer;
    result.table_write_applied = true;
    return result;
}

NcarObjectLookupPlan lookup_ncar_object(
    const NcarObjectLookupObservation& observation) noexcept
{
    NcarObjectLookupPlan result{};
    if (observation.index < 0 ||
        observation.index >= static_cast<std::int32_t>(
            ncar_registered_object_count)) {
        result.out_of_table_read_unmodeled = true;
        return result;
    }

    result.index_in_table = true;
    result.object_pointer = observation.table[
        static_cast<std::size_t>(observation.index)];
    return result;
}

NcarInterpolatedDispatchPlan dispatch_ncar_interpolated_object(
    const NcarInterpolatedDispatchObservation& observation) noexcept
{
    NcarInterpolatedDispatchPlan result{};
    result.indexed_target_table_byte_offset = 0xA4U +
        observation.object_callback_index_0x510 * 116U;
    std::uint32_t flags = observation.object_flags_0x04;
    if (observation.object_gate_0xed4 > 0 &&
        observation.selection.object_limit_0xef8 != 0) {
        flags |= 1U;
    }
    result.object_flags_after_gate_0x04 = flags;

    if (observation.indexed_target_pointer_nonnull &&
        observation.indexed_target_value_0x2c.has_value()) {
        result.indexed_target_stores_valid = true;
        result.indexed_target_write_0x04 =
            *observation.indexed_target_value_0x2c;
        result.indexed_target_write_0x08 = 0U;
    }
    if (!observation.indexed_target_pointer_nonnull) {
        result.indexed_target_null_dereference = true;
        return result;
    }

    if ((flags & 1U) == 0) {
        result.registered_transform_lookup_0x420930_called = true;
        if (!observation.registered_object_table_0x507d70.has_value()) {
            result.registered_object_table_observation_missing = true;
            return result;
        }
        result.registered_object_table_observation_available = true;
        result.registered_object_lookup = lookup_ncar_object(
            {*observation.registered_object_table_0x507d70,
                std::bit_cast<std::int32_t>(
                    observation.object_transform_index_0xec8)});
        if (!result.registered_object_lookup.index_in_table) {
            result.registered_object_out_of_table_access_unmodeled = true;
            return result;
        }
        if (*result.registered_object_lookup.object_pointer == 0U) {
            result.registered_transform_null_dereference = true;
            return result;
        }
        result.registered_transform_copy_attempted = true;
        if (observation.registered_transform_0x38.has_value()) {
            result.registered_transform_copy_valid = true;
            result.copied_transform_0x38 =
                *observation.registered_transform_0x38;
        }
    }

    if (observation.global_virtual_return_nonzero.has_value()) {
        result.virtual_callback_branch_known = true;
        if (*observation.global_virtual_return_nonzero) {
            flags &= ~3U;
        }
        if ((flags & 1U) != 0) {
            flags |= 2U;
            const auto frame_step = std::bit_cast<std::int32_t>(
                observation.selection.global_time_bits_0x508774 -
                observation.selection.object_value_bits_0xedc -
                observation.selection.object_value_bits_0xee0);
            result.interpolation_time_gate_passed = frame_step < 0 ||
                static_cast<std::int64_t>(
                    observation.object_frame_gate_0xed8) > frame_step;
            if (result.interpolation_time_gate_passed) {
                result.selector_0x427630_called = true;
                result.selection = select_ncar_interpolated_records(
                    observation.selection);
                if (result.selection.record_window_valid) {
                    auto update = observation.update;
                    update.object.first_record = result.selection.first_record;
                    update.object.second_record = result.selection.second_record;
                    update.object.interpolation_fraction =
                        result.selection.interpolation_fraction;
                    update.saved_object_flags_0x04 = flags;
                    result.interpolated_update =
                        update_ncar_interpolated_state(update);
                }
            }
        }
        result.object_flags_after_virtual_callback_0x04 = flags;
    }

    result.incremented_object_counter_0x10 = static_cast<std::uint16_t>(
        observation.object_counter_0x10 + 1U);
    result.tail_virtual_callback_0x38_called = true;
    return result;
}

NcarInterpolatedObjectPlan update_ncar_interpolated_object(
    const NcarInterpolatedObjectObservation& observation) noexcept
{
    const auto interpolate = [&](const std::size_t offset,
                                 const long double scale,
                                 const long double bias = 0.0L) noexcept {
        const long double current = observation.first_record[offset];
        const long double next = observation.second_record[offset];
        const long double alpha = observation.interpolation_fraction;
        return static_cast<float>(current * scale +
            (next - current) * scale * alpha + bias);
    };
    const auto read_flags = [](const auto& record) noexcept {
        return static_cast<std::uint16_t>(record[0x38]) |
            static_cast<std::uint16_t>(record[0x39] << 8U);
    };

    NcarInterpolatedObjectPlan result{};
    const auto current_flags = read_flags(observation.first_record);
    const auto next_flags = read_flags(observation.second_record);
    const bool use_next_endpoint = (next_flags & 0x200U) == 0;
    result.field_0xd70 = interpolate(0x1e, 14000.0L / 32768.0L, -2000.0L);
    result.field_0x500 = interpolate(0x20,
        256.0L * 0.00007324219L, -1.2L);
    const auto current_byte_0x3a = observation.first_record[0x3a];
    if (current_byte_0x3a == 0) {
        result.field_0x50c = 1.0F;
    } else {
        const long double current = current_byte_0x3a * (1.0L / 128.0L);
        const long double next = observation.second_record[0x3a] *
            (1.0L / 128.0L);
        result.field_0x50c = static_cast<float>(use_next_endpoint
            ? current + (next - current) * observation.interpolation_fraction
            : current);
    }
    result.setter_0xde0 = static_cast<float>(
        observation.first_record[0x21] * (1.0L / 128.0L) +
        (observation.second_record[0x21] -
         static_cast<long double>(observation.first_record[0x21])) *
         (1.0L / 128.0L) * observation.interpolation_fraction);
    result.setter_0xde4 = static_cast<float>(
        observation.first_record[0x22] * (1.0L / 128.0L) +
        (observation.second_record[0x22] -
         static_cast<long double>(observation.first_record[0x22])) *
         (1.0L / 128.0L) * observation.interpolation_fraction);
    result.combined_flags_0x04 = static_cast<std::uint16_t>(
        current_flags | next_flags);
    result.field_0xe28 = (current_flags & 0x100U) != 0;
    result.field_0xd68 = std::bit_cast<std::int8_t>(
        observation.first_record[0x23]);
    result.field_0xd64 = result.field_0xd68;
    long double speed_squared = 0.0L;
    for (std::size_t axis = 0; axis < result.body_velocity_0x234.size(); ++axis) {
        const auto offset = 0x12U + axis * sizeof(float);
        const long double current = std::bit_cast<float>(read_u32_le(
            reinterpret_cast<const std::byte*>(observation.first_record.data()), offset));
        const long double next = std::bit_cast<float>(read_u32_le(
            reinterpret_cast<const std::byte*>(observation.second_record.data()), offset));
        const long double velocity = current +
            (next - current) * observation.interpolation_fraction;
        result.body_velocity_0x234[axis] = static_cast<float>(velocity);
        result.copied_velocity_0xdf0[axis] = result.body_velocity_0x234[axis];
        speed_squared += velocity * velocity;
    }
    result.speed_magnitude_0xddc = static_cast<float>(std::sqrt(speed_squared));
    for (std::size_t wheel = 0; wheel < result.wheel_updates.size(); ++wheel) {
        NcarInterpolatedWheelObservation wheel_observation{};
        const auto record_offset = 0x24U + wheel * 5U;
        std::copy_n(observation.first_record.begin() +
            static_cast<std::ptrdiff_t>(record_offset),
            wheel_observation.first_record.size(),
            wheel_observation.first_record.begin());
        std::copy_n(observation.second_record.begin() +
            static_cast<std::ptrdiff_t>(record_offset),
            wheel_observation.second_record.size(),
            wheel_observation.second_record.begin());
        wheel_observation.interpolation_fraction =
            observation.interpolation_fraction;
        wheel_observation.body_velocity_0x234 = result.body_velocity_0x234;
        result.wheel_updates[wheel] =
            update_ncar_interpolated_wheel(wheel_observation);
    }
    const float signed_scale = observation.object_scalar_0xdcc * result.field_0x500 *
        (observation.object_flag_0xbe4_nonzero ? -1.0F : 1.0F);
    if (observation.object_flag_0xbe4_nonzero) {
        result.field_0x978 = signed_scale;
        result.field_0xb1c = signed_scale;
    } else {
        result.field_0x630 = signed_scale;
        result.field_0x7d4 = signed_scale;
    }
    result.reset_helper_0x433920_called =
        (result.combined_flags_0x04 & 0x20U) == 0 &&
        observation.object_reset_gate_0x48c_nonzero;
    if (result.reset_helper_0x433920_called) {
        result.reset_object_byte_0x48c = 0U;
        result.reset_object_byte_0x48d = 0U;
        result.reset_object_dwords_0x48e.fill(0U);
        result.reset_wheel_damage_bytes_0x574.fill(0U);
        result.reset_resource_group_passes = 5U;
        result.reset_backend_virtual_call_attempted = true;
        result.reset_backend_group = plan_ncar_reset_backend_group(
            observation.reset_backend_group);
    }
    result.rotation_endpoint_is_second_record = use_next_endpoint;
    const auto first_transform = axis_angle_record_matrix(
        observation.first_record, observation.fdiv_compatibility_enabled);
    const auto selected_transform = axis_angle_record_matrix(
        use_next_endpoint ? observation.second_record : observation.first_record,
        observation.fdiv_compatibility_enabled);
    result.transform_0x38_to_0x64 = interpolate_and_orthonormalize_transform(
        first_transform, selected_transform,
        observation.interpolation_fraction,
        observation.fdiv_compatibility_enabled);
    return result;
}

NcarInterpolatedUpdatePlan update_ncar_interpolated_state(
    const NcarInterpolatedUpdateObservation& observation) noexcept
{
    NcarInterpolatedUpdatePlan result{};
    result.object_update = update_ncar_interpolated_object(observation.object);
    result.restored_object_flags_0x04 = observation.saved_object_flags_0x04;

    const auto speed = [](const auto& record) noexcept {
        const auto* const bytes = reinterpret_cast<const std::byte*>(record.data());
        long double magnitude_squared = 0.0L;
        for (const std::size_t offset : {0x12U, 0x16U, 0x1aU}) {
            const long double component = std::bit_cast<float>(
                read_u32_le(bytes, offset));
            magnitude_squared += component * component;
        }
        return std::sqrt(magnitude_squared);
    };
    const long double first_speed = speed(observation.object.first_record);
    const long double second_speed = speed(observation.object.second_record);
    const long double alpha = observation.object.interpolation_fraction;
    result.interpolated_speed_0xed0 = static_cast<float>(
        second_speed * alpha + (1.0L - alpha) * first_speed);
    return result;
}

NcarHistoryDerivative derive_ncar_history_derivative(
    const NcarSampleWindow& samples,
    const NcarSampleDerivative& current,
    const NcarSampleDerivative& previous,
    const NcarSampleDerivative& older,
    const bool fdiv_compatibility_enabled) noexcept
{
    const auto coefficients = history_derivative_coefficients(
        samples, fdiv_compatibility_enabled);
    NcarHistoryDerivative result{};
    for (std::size_t axis = 0; axis < result.values.size(); ++axis) {
        long double value = coefficients.at_sample_0 * current.values[axis];
        value += coefficients.at_sample_2 * older.values[axis];
        value += coefficients.at_sample_1 * previous.values[axis];
        result.values[axis] = static_cast<float>(value);
    }

    const long double x = result.values[0];
    const long double y = result.values[1];
    const long double z = result.values[2];
    const long double gate_magnitude_squared = z * z + y * y + x * x;
    if (static_cast<long double>(96.23611F) < gate_magnitude_squared) {
        const long double sqrt_argument = x * x + z * z + y * y;
        const long double inverse_magnitude = divide_with_vc98_fdiv_compatibility(
            1.0L, std::sqrt(sqrt_argument), fdiv_compatibility_enabled);
        for (auto& value : result.values) {
            const float normalized = static_cast<float>(value * inverse_magnitude);
            value = normalized * 9.81F;
        }
        result.normalized_to_9_81 = true;
    }
    return result;
}

void advance_ncar_interpolation_ramp(NcarInterpolationRamp& ramp,
                                     const float first_parameter,
                                     const float second_parameter,
                                     const bool fdiv_compatibility_enabled) noexcept
{
    ramp.value_0xf24 -= 1.0F;
    ramp.rate_0xf28 = static_cast<float>(divide_with_vc98_fdiv_compatibility(
        1.0L - static_cast<long double>(ramp.value_0xf24),
        (static_cast<long double>(first_parameter) - second_parameter) * 60.0L,
        fdiv_compatibility_enabled));
}

NcarSampleDynamicsResult advance_ncar_sample_dynamics(
    NcarSampleDynamicsState& state,
    const NcarSampleWindow& samples,
    const bool fdiv_compatibility_enabled) noexcept
{
    // 0x439520 copies the six floats at +0xECC..+0xEE3 to +0xED8..+0xEEF
    // before replacing the current derivative triplet.
    state.older_derivative_0xee4 = state.previous_derivative_0xed8;
    state.previous_derivative_0xed8 = state.current_derivative_0xecc.values;

    state.current_derivative_0xecc = derive_ncar_sample_vector(
        samples, fdiv_compatibility_enabled);
    state.acceleration_0xef0 = derive_ncar_history_derivative(
        samples,
        state.current_derivative_0xecc,
        NcarSampleDerivative{state.previous_derivative_0xed8},
        NcarSampleDerivative{state.older_derivative_0xee4},
        fdiv_compatibility_enabled);

    const auto first_parameter = std::bit_cast<float>(
        samples.records[0].sample_time_bits_0x04);
    const auto second_parameter = std::bit_cast<float>(
        samples.records[1].sample_time_bits_0x04);
    advance_ncar_interpolation_ramp(
        state.interpolation_ramp, first_parameter, second_parameter,
        fdiv_compatibility_enabled);

    return {state.current_derivative_0xecc, state.acceleration_0xef0,
            fdiv_compatibility_enabled};
}

NcarRotationBlendResult advance_ncar_rotation_blend(
    NcarRotationBlendState& state,
    const NcarRotationBlendObservation& observation) noexcept
{
    NcarRotationBlendResult result{};
    result.fdiv_compatibility_branch = observation.fdiv_compatibility_enabled;

    const auto interpolate_pair = [&](const std::size_t older,
                                      const std::size_t newer,
                                      const std::size_t result_index) {
        const auto numerator = static_cast<double>(observation.now) -
            static_cast<double>(observation.sample_times[older]);
        const auto denominator = static_cast<double>(
            observation.sample_times[newer]) -
            static_cast<double>(observation.sample_times[older]);
        const auto fraction = static_cast<float>(numerator / denominator);
        result.sample_fractions[result_index] = fraction;
        const auto slerp = quaternion_slerp(
            observation.sample_quaternions[older],
            observation.sample_quaternions[newer], fraction,
            observation.fdiv_compatibility_enabled);
        result.pair_quaternions[result_index] = slerp.quaternion;
        result.pair_near_parallel_copy[result_index] =
            slerp.near_parallel_copy;
        if (result_index == 0U) {
            result.first_pair_source_negated = slerp.source_negated;
        } else {
            result.second_pair_source_negated = slerp.source_negated;
        }
    };

    // 0x43A080 first interpolates slot 1 toward slot 0, then slot 2 toward 1.
    interpolate_pair(1U, 0U, 0U);
    interpolate_pair(2U, 1U, 1U);
    const auto sample_slerp = quaternion_slerp(
        result.pair_quaternions[1], result.pair_quaternions[0],
        state.interpolation_fraction_0xf24,
        observation.fdiv_compatibility_enabled);
    result.interpolated_sample_quaternion = sample_slerp.quaternion;
    result.sample_pair_source_negated = sample_slerp.source_negated;
    result.sample_pair_near_parallel_copy = sample_slerp.near_parallel_copy;
    state.interpolation_fraction_0xf24 += state.interpolation_step_0xf28;

    std::uint32_t matching_slots = 0U;
    for (const auto slot_value : state.slot_value_0x19c) {
        if (slot_value == 10U) {
            ++matching_slots;
        }
    }
    result.slot_weight = static_cast<float>(matching_slots) * 0.2F;
    result.object_blend_weight = (1.0F - result.slot_weight) *
        state.interpolation_weight_0xefc;

    const auto matrix_conversion = matrix3x3_to_quaternion(
        state.matrix_0x38, observation.fdiv_compatibility_enabled);
    result.object_matrix_positive_trace_branch =
        matrix_conversion.positive_trace_branch;
    result.object_matrix_diagonal_branch_axis =
        matrix_conversion.diagonal_branch_axis;
    result.object_quaternion_before = matrix_conversion.quaternion;
    const auto object_slerp = quaternion_slerp(
        matrix_conversion.quaternion,
        result.interpolated_sample_quaternion,
        result.object_blend_weight,
        observation.fdiv_compatibility_enabled);
    result.object_quaternion_after = object_slerp.quaternion;
    result.object_blend_source_negated = object_slerp.source_negated;
    result.object_blend_near_parallel_copy = object_slerp.near_parallel_copy;
    result.output_matrix = quaternion_to_matrix3x3(object_slerp.quaternion);
    state.matrix_0x38 = result.output_matrix;
    return result;
}

bool should_update_ncar_linked_target(
    const int record_mode,
    const std::uint8_t current_gate_byte,
    const std::uint8_t prior_gate_byte,
    const bool target_present) noexcept
{
    return record_mode > 1
        && current_gate_byte != 0
        && current_gate_byte != prior_gate_byte
        && target_present;
}

NcarLinkedTargetUpdate update_ncar_linked_target(
    NcarLinkedTargetFields& target,
    const std::array<std::uint32_t, 12>& state_0x38,
    const std::array<float, 3>& vector_0x234,
    const float now,
    const float sample_parameter,
    const bool type_name_is_plane) noexcept
{
    target.cleared_vector_0x240.fill(0.0F);
    if (!(static_cast<double>(now) - 2.0 > target.last_update_time_0x478)) {
        return {};
    }

    target.copied_state_0x38 = state_0x38;
    target.last_update_time_0x478 = now;
    const double elapsed = static_cast<double>(now) - sample_parameter;
    if (!(elapsed >= 0.0) || elapsed > static_cast<double>(0.1F)) {
        return {true, false};
    }

    if (type_name_is_plane) {
        constexpr double speed_scale = 31.11111F;
        for (std::size_t axis = 0; axis < target.vector_0x234.size(); ++axis) {
            target.vector_0x234[axis] = static_cast<float>(
                static_cast<double>(vector_0x234[axis])
                + static_cast<double>(target.multiplier_0x50[axis]) * speed_scale);
            target.accumulator_0x5c[axis] -= target.subtractor_0x44[axis];
        }
        return {true, true};
    }

    const double factor = elapsed * static_cast<double>(31.11111F) + 3.5;
    for (std::size_t axis = 0; axis < target.accumulator_0x5c.size(); ++axis) {
        const float input = std::bit_cast<float>(state_0x38[6 + axis]);
        target.accumulator_0x5c[axis] = static_cast<float>(
            static_cast<double>(input) * factor + target.accumulator_0x5c[axis]);
    }
    target.accumulator_0x5c[1] += 0.5F;

    const double x = vector_0x234[0];
    const double y = vector_0x234[1];
    const double z = vector_0x234[2];
    const double squared_speed = x * x + (y * y + z * z);
    const double speed = std::sqrt(squared_speed) + static_cast<double>(31.11111F);
    for (std::size_t axis = 0; axis < target.vector_0x234.size(); ++axis) {
        target.vector_0x234[axis] = static_cast<float>(target.multiplier_0x50[axis] * speed);
    }
    return {true, true};
}

NcarLinkedCallbackResult run_ncar_linked_callback(
    NcarLinkedTargetFields& target,
    std::uint8_t& cached_gate_byte,
    const std::uint8_t gate_byte,
    const bool linked_update_enabled,
    const int global_mode,
    const bool target_present,
    const std::array<std::uint32_t, 12>& instance_state_0x38,
    const std::array<float, 3>& instance_vector_0x234,
    const std::array<std::uint32_t, 12>& global_state,
    const float now,
    const bool type_name_is_plane) noexcept
{
    NcarLinkedCallbackResult result{};
    if (linked_update_enabled && gate_byte != cached_gate_byte && gate_byte != 0
        && target_present) {
        const std::array<float, 3> zero_vector{};
        const auto& state = global_mode == 11 ? global_state : instance_state_0x38;
        const auto& vector = global_mode == 11 ? zero_vector : instance_vector_0x234;
        result.called_target_update = true;
        result.target_update = update_ncar_linked_target(
            target, state, vector, now, now, type_name_is_plane);
    }
    cached_gate_byte = gate_byte;
    return result;
}

void advance_ncar_timed_interpolation(
    NcarTimedInterpolation& fields,
    const float newest_sample_time,
    const float previous_sample_time,
    const float now) noexcept
{
    const float interval = (newest_sample_time - previous_sample_time) * 2.0F;
    const float ramp_duration = interval * 7.0F;
    const float elapsed = now - fields.interpolation_start_0xf1c;
    if (interval <= elapsed) {
        if (ramp_duration + interval <= elapsed) {
            fields.current_0xefc = fields.target_0xf0c;
        } else {
            const float factor = (elapsed - interval) / ramp_duration;
            for (std::size_t i = 0; i < fields.current_0xefc.size(); ++i) {
                fields.current_0xefc[i] = fields.target_0xf0c[i] * factor;
            }
        }
    }
    const float output_scale = 1.0F - fields.current_0xefc[0];
    for (auto& component : fields.output_0x240) {
        component *= output_scale;
    }
}

NcarResetResourceGroupsResult reset_ncar_resource_groups(
    const std::array<NcarResetResourceGroupInput, 5>& groups,
    std::array<std::uint8_t, 5>& resource_flags) noexcept
{
    NcarResetResourceGroupsResult result{};
    result.groups_visited = static_cast<std::uint8_t>(groups.size());
    for (std::size_t group = 0; group < groups.size(); ++group) {
        const auto& input = groups[group];
        const auto available = std::min({
            input.destination_records.size(),
            input.source_records.size(),
            input.destination_record_count});
        result.records_copied[group] = available;
        result.records_unprocessed[group] =
            input.destination_record_count - available;
        result.descriptor_pointer_writes_attempted[group] = true;
        result.descriptor_pointer_values[group] = input.descriptor_pointer_0x10;
        if (available != input.destination_record_count ||
            input.source_record_count < input.destination_record_count) {
            result.inputs_complete = false;
        }
        for (std::size_t record = 0; record < available; ++record) {
            std::copy_n(input.source_records[record].begin(), 3U,
                        input.destination_records[record].begin());
        }
        resource_flags[group] = 0U;
    }
    return result;
}

NcarResetSphereBoundsPlan derive_ncar_reset_sphere_geometry(
    const NcarResetSphereBoundsObservation& observation) noexcept
{
    NcarResetSphereBoundsPlan result{};
    if (!observation.extents_pointer_present.has_value()) {
        result.extents_pointer_unknown = true;
        return result;
    }
    if (!*observation.extents_pointer_present) {
        result.unusual_model_reported = true;
    } else if (observation.precomputed_model_bounds.has_value()) {
        const auto& bounds = *observation.precomputed_model_bounds;
        result.raw_bounds = {bounds.minimum[0], bounds.maximum[0],
            bounds.minimum[1], bounds.maximum[1],
            bounds.minimum[2], bounds.maximum[2]};
        result.inputs_complete = std::all_of(
            result.raw_bounds.begin(), result.raw_bounds.end(),
            [](const float value) { return std::isfinite(value); });
    } else {
        result.raw_bounds = {10000.0F, -10000.0F, 10000.0F, -10000.0F,
            10000.0F, -10000.0F};
        const auto observed_vertices = std::min(
            observation.vertex_count, observation.vertices.size());
        result.inputs_complete = observed_vertices == observation.vertex_count;
        for (std::size_t vertex = 0; vertex < observed_vertices; ++vertex) {
            const auto& coordinates = observation.vertices[vertex];
            for (std::size_t axis = 0; axis < 3U; ++axis) {
                result.raw_bounds[axis * 2U] = std::min(
                    result.raw_bounds[axis * 2U], coordinates[axis]);
                result.raw_bounds[axis * 2U + 1U] = std::max(
                    result.raw_bounds[axis * 2U + 1U], coordinates[axis]);
            }
        }
    }

    result.inputs_complete = result.inputs_complete &&
        std::isfinite(observation.car_record_0x58) &&
        std::isfinite(observation.car_record_0x5c) &&
        std::isfinite(observation.car_record_0x128);
    if (!result.inputs_complete) {
        return result;
    }

    auto max_x = result.raw_bounds[1];
    if (max_x >= 2.0F) {
        max_x = 2.0F;
    }
    const float car_half_width = observation.car_record_0x58 * 0.0254F;
    const float car_height_fraction = observation.car_record_0x5c * 0.01F;
    const float z_shift = (car_height_fraction - (1.0F - car_height_fraction)) *
        car_half_width * -0.5F;
    result.local_geometry.local_b0_after_body_shift = result.raw_bounds[2] +
        observation.car_record_0x128 * -0.0254F;
    result.local_geometry.local_bc_after_clamp = max_x;
    result.local_geometry.local_98_after_body_shift = result.raw_bounds[4] + z_shift;
    result.local_geometry.local_9c_after_body_shift = result.raw_bounds[5] + z_shift;
    return result;
}

std::array<std::array<float, 3>, 12> derive_ncar_reset_sphere_local_positions(
    const NcarResetSphereLocalGeometry& geometry) noexcept
{
    const float x_04 = geometry.local_bc_after_clamp - 0.4F;
    const float z_04 = geometry.local_9c_after_body_shift - 0.4F;
    const float z_middle = (geometry.local_9c_after_body_shift +
        geometry.local_98_after_body_shift) * 0.5F;
    const float z_middle_lower = (z_middle + z_04) * 0.5F;
    const float x_06 = geometry.local_bc_after_clamp - 0.6F;
    const float y_06 = geometry.local_b0_after_body_shift + 0.6F;
    const float x_05 = geometry.local_bc_after_clamp - 0.5F;
    const float y_05 = geometry.local_b0_after_body_shift + 0.5F;
    const float z_05 = geometry.local_98_after_body_shift + 0.5F;
    const float z_middle_upper = (z_05 + z_middle) * 0.5F;
    const float y_04 = geometry.local_b0_after_body_shift + 0.4F;
    return {{
        {-x_04, y_04, z_04}, {0.0F, y_04, z_04}, {x_04, y_04, z_04},
        {-x_04, y_04, z_middle_lower}, {0.0F, y_04, z_middle_lower},
        {x_04, y_04, z_middle_lower},
        {-x_06, y_06, z_middle}, {x_06, y_06, z_middle},
        {-x_05, y_05, z_middle_upper}, {x_05, y_05, z_middle_upper},
        {-x_04, y_05, z_05}, {x_05, y_05, z_05},
    }};
}

NcarResetBackendGroupPlan plan_ncar_reset_backend_group(
    const NcarResetBackendGroupObservation& observation)
{
    NcarResetBackendGroupPlan result{};
    result.group_virtual_0x04_attempted = true;
    result.child_count_0x4c = observation.child_count_0x4c;
    if (!observation.group_object_present.has_value()) {
        result.group_pointer_presence_unknown = true;
        return result;
    }
    if (!*observation.group_object_present) {
        result.group_pointer_fault = true;
        return result;
    }

    std::vector<NcarResetBackendGroupObservation::Child> generated_children;
    auto children = observation.children;
    auto constructor_geometry = observation.constructor_geometry;
    if (children.empty() && !constructor_geometry.has_value() &&
        observation.source_bounds.has_value()) {
        const auto bounds = derive_ncar_reset_sphere_geometry(
            *observation.source_bounds);
        result.geometry_inputs_incomplete = !bounds.inputs_complete;
        result.unusual_model_reported = bounds.unusual_model_reported;
        if (bounds.inputs_complete || bounds.unusual_model_reported) {
            constructor_geometry = bounds.local_geometry;
        }
    }
    if (children.empty() && constructor_geometry.has_value()) {
        const auto positions = derive_ncar_reset_sphere_local_positions(
            *constructor_geometry);
        result.constructor_child_tag_value_0x18 = 0x6dU;
        result.child_allocations_assumed_success = true;
        static constexpr std::array<float, 12> constructor_radii{
            0.4F, 0.4F, 0.4F, 0.4F, 0.4F, 0.4F,
            0.6F, 0.6F, 0.5F, 0.5F, 0.5F, 0.5F};
        generated_children.reserve(positions.size());
        for (std::size_t child = 0; child < positions.size(); ++child) {
            generated_children.push_back(
                {true, positions[child], positions[child],
                    constructor_radii[child]});
            // 0x42CD90 calls the child's virtual +0x1C setter with 0x6D;
            // its 0x42FB60 target stores that value at child +0x18.
            ++result.constructor_child_tag_writes_0x18;
        }
        // The group setter stores the same value at group +0x18, then
        // propagates it through each child's virtual +0x1C setter.
        result.group_tag_value_0x18 = 0x6dU;
        result.group_child_tag_propagations = positions.size();
        result.constructor_child_tag_writes_0x18 += positions.size();
        children = generated_children;
    }
    const auto observed_count = std::min(
        observation.child_count_0x4c,
        children.size());
    result.child_calls_unobserved =
        observation.child_count_0x4c - observed_count;
    if (!observation.parent_transform_present.has_value()) {
        result.parent_transform_presence_unknown = true;
    }
    for (std::size_t child = 0; child < observed_count; ++child) {
        ++result.child_entries_visited;
        const auto& child_observation = children[child];
        if (!child_observation.pointer_present) {
            result.null_child_pointer_fault = true;
            result.child_calls_unobserved += observed_count - child - 1U;
            break;
        }
        ++result.child_calls_attempted;
        if (child_observation.radius.has_value()) {
            const auto& local = child_observation.local_position_0x20;
            const auto bound = std::sqrt(local[0] * local[0] +
                local[1] * local[1] + local[2] * local[2]) +
                *child_observation.radius;
            result.group_radius_0x50 = std::max(result.group_radius_0x50, bound);
        } else {
            result.radius_inputs_incomplete = true;
        }
        if (!observation.parent_transform_present.has_value()) {
            continue;
        }
        if (!*observation.parent_transform_present) {
            result.null_parent_transform_fault = true;
            result.child_calls_unobserved += observed_count - child - 1U;
            break;
        }
        const auto& transform = observation.parent_transform_0x00_to_0x2c;
        const auto& point = child_observation.local_position_0x20;
        result.child_world_positions_0x2c.push_back({
            static_cast<float>(static_cast<long double>(transform[0]) * point[0] +
                static_cast<long double>(transform[3]) * point[1] +
                static_cast<long double>(transform[6]) * point[2] + transform[9]),
            static_cast<float>(static_cast<long double>(transform[1]) * point[0] +
                static_cast<long double>(transform[4]) * point[1] +
                static_cast<long double>(transform[7]) * point[2] + transform[10]),
            static_cast<float>(static_cast<long double>(transform[2]) * point[0] +
                static_cast<long double>(transform[5]) * point[1] +
                static_cast<long double>(transform[8]) * point[2] + transform[11])});
    }
    return result;
}

NcarBackendGroupRefreshPlan plan_ncar_backend_group_refresh(
    const NcarResetBackendGroupObservation& observation)
{
    NcarBackendGroupRefreshPlan result{};
    result.group_virtual_0x14_attempted = true;
    result.child_count_0x4c = observation.child_count_0x4c;
    if (!observation.group_object_present.has_value()) {
        result.group_pointer_presence_unknown = true;
        return result;
    }
    if (!*observation.group_object_present) {
        result.group_pointer_fault = true;
        return result;
    }
    std::vector<NcarResetBackendGroupObservation::Child> generated_children;
    auto children = observation.children;
    auto constructor_geometry = observation.constructor_geometry;
    if (children.empty() && !constructor_geometry.has_value() &&
        observation.source_bounds.has_value()) {
        const auto bounds = derive_ncar_reset_sphere_geometry(
            *observation.source_bounds);
        result.geometry_inputs_incomplete = !bounds.inputs_complete;
        result.unusual_model_reported = bounds.unusual_model_reported;
        if (bounds.inputs_complete || bounds.unusual_model_reported) {
            constructor_geometry = bounds.local_geometry;
        }
    }
    if (children.empty() && constructor_geometry.has_value()) {
        const auto positions = derive_ncar_reset_sphere_local_positions(
            *constructor_geometry);
        generated_children.reserve(positions.size());
        for (const auto& position : positions) {
            generated_children.push_back({true, position, position, std::nullopt});
        }
        children = generated_children;
        result.child_allocations_assumed_success = true;
    }
    const auto observed_count = std::min(
        observation.child_count_0x4c, children.size());
    result.child_calls_unobserved = observation.child_count_0x4c - observed_count;
    result.children.reserve(observed_count);
    for (std::size_t child = 0; child < observed_count; ++child) {
        ++result.child_entries_visited;
        const auto& input = children[child];
        NcarBackendChildRefreshEffect effect{};
        effect.virtual_0x14_attempted = true;
        if (!input.pointer_present) {
            effect.null_child_pointer_fault = true;
            result.child_calls_unobserved += observed_count - child - 1U;
            result.children.push_back(effect);
            break;
        }
        effect.byte_0x38_cleared = true;
        effect.byte_0x88_cleared = true;
        if (input.constructor_position_0x64.has_value()) {
            effect.restored_position_0x20 = input.constructor_position_0x64;
        } else {
            effect.constructor_position_unknown = true;
        }
        result.children.push_back(effect);
    }
    return result;
}

NcarObjectInitializationPlan plan_ncar_object_initialization(
    const NcarObjectInitializationObservation& observation)
{
    NcarObjectInitializationPlan result{};
    const auto write32 = [](std::vector<NcarObjectFieldWrite>& writes,
                            const std::uint32_t offset,
                            const std::uint32_t bits) {
        writes.push_back({offset, 4U, bits});
    };
    const auto write8 = [](std::vector<NcarObjectFieldWrite>& writes,
                           const std::uint32_t offset,
                           const std::uint8_t value) {
        writes.push_back({offset, 1U, value});
    };
    auto& before_refresh = result.writes_before_backend_refresh;
    write8(before_refresh, 0x0e3cU, 1U);
    write32(before_refresh, 0x0e08U, 0U);
    write32(before_refresh, 0x0e0cU, 0U);
    write32(before_refresh, 0x0e10U, 0U);
    write8(before_refresh, 0x0e3dU, 1U);
    write32(before_refresh, 0x0488U, 0U);
    write32(before_refresh, 0x0e68U, 0U);
    write8(before_refresh, 0x048cU, 0U);
    write32(before_refresh, 0x0e14U, 0x40a00000U);
    write32(before_refresh, 0x050cU, 0x3f800000U);
    write32(before_refresh, 0x0e44U, 0xc2c80000U);

    result.backend_group_refresh = plan_ncar_backend_group_refresh(
        observation.backend_group);

    auto& before_helper = result.writes_before_helper_0x4331e0;
    for (std::size_t index = 0; index <
         observation.saved_transform_0xe6c.size(); ++index) {
        write32(before_helper,
            0x38U + static_cast<std::uint32_t>(index * sizeof(float)),
            std::bit_cast<std::uint32_t>(observation.saved_transform_0xe6c[index]));
    }
    for (const std::uint32_t offset : {0x0eacU, 0x0ea8U, 0x0ea4U}) {
        write32(before_helper, offset, 0U);
    }
    write8(before_helper, 0x0e28U, 0U);
    for (const std::uint32_t offset : {0x0de0U, 0x0de4U, 0x0de8U,
             0x0508U, 0x0504U, 0x0500U, 0x0d70U, 0x0e38U,
             0x0eb0U, 0x0d64U}) {
        write32(before_helper, offset, 0U);
    }
    write32(before_helper, 0x0decU, 0x3f800000U);
    write32(before_helper, 0x0d68U, 0U);
    result.helper_0x4331e0_called = true;

    auto& helper_4331e0_writes = result.writes_from_helper_0x4331e0;
    write32(helper_4331e0_writes, 0x0cbcU, 0U);
    write32(helper_4331e0_writes, 0x0d64U, 0U);

    auto& after_helper = result.writes_after_helper_0x4331e0;
    for (const std::uint32_t offset : {0x0ddcU, 0x0e20U, 0x0e24U,
             0x04f0U}) {
        write32(after_helper, offset, 0U);
    }
    write8(after_helper, 0x04ecU, 0U);
    write8(after_helper, 0x0e48U, 0U);
    write8(after_helper, 0x0e49U, 0U);
    write32(after_helper, 0x0dfcU, 0U);
    write32(after_helper, 0x0e00U, 0U);
    write32(after_helper, 0x0e18U, std::bit_cast<std::uint32_t>(
        static_cast<float>(static_cast<double>(observation.source_value_0xe9c) *
                           3.79)));
    write32(after_helper, 0x0e1cU, std::bit_cast<std::uint32_t>(
        static_cast<float>(static_cast<double>(observation.source_value_0xea0) *
                           0.0010527777)));
    write32(after_helper, 0x0e04U, 0U);
    write32(after_helper, 0x0e30U, 0U);
    result.helper_0x4338c0_called = true;

    result.child_flags_written = std::min(
        observation.child_count_0x1ec,
        observation.child_flag_storage_count);
    result.child_flags_unobserved = observation.child_count_0x1ec -
        result.child_flags_written;
    auto& wheel_writes = result.wheel_reset_writes_0x441b90;
    for (std::size_t wheel = 0; wheel < 4U; ++wheel) {
        const auto base = 0x554U + static_cast<std::uint32_t>(wheel * 0x1a4U);
        write32(wheel_writes, base + 0x13cU, 0U);
        write32(wheel_writes, base + 0x124U, 0U);
        write8(wheel_writes, base + 0x168U, 0U);
        write8(wheel_writes, base + 0x020U, 0U);
        for (const std::uint32_t offset : {0x0e8U, 0x0dcU, 0x0a4U,
                 0x10cU, 0x16cU, 0x158U}) {
            write32(wheel_writes, base + offset, 0U);
        }
    }

    auto& reset_writes = result.reset_object_writes_0x433920;
    write8(reset_writes, 0x48dU, 0U);
    for (std::size_t index = 0; index < 8U; ++index) {
        write32(reset_writes,
            0x48eU + static_cast<std::uint32_t>(index * sizeof(std::uint32_t)),
            0U);
    }
    write8(reset_writes, 0x48cU, 0U);
    result.reset_resources = reset_ncar_resource_groups(
        observation.resource_groups, result.reset_resource_flags_0x15);
    for (std::size_t wheel = 0; wheel < 4U; ++wheel) {
        write8(reset_writes,
            0x574U + static_cast<std::uint32_t>(wheel * 0x1a4U), 0U);
    }
    result.reset_backend_group = plan_ncar_reset_backend_group(
        observation.backend_group);
    result.cleared_field_0xc0c = 0U;
    result.final_backend_group_refresh = plan_ncar_backend_group_refresh(
        observation.backend_group);
    return result;
}

NcarVertexMapPlan plan_ncar_vertex_maps(
    const std::array<NcarVertexMapGroupObservation, 5>& groups)
{
    NcarVertexMapPlan result{};
    const auto& base_group = groups[0];
    const auto observed_base_count = std::min(
        base_group.vertex_count, base_group.vertices.size());
    for (std::size_t group = 0; group < groups.size(); ++group) {
        const auto& input = groups[group];
        const auto observed_count = std::min(
            input.vertex_count, input.vertices.size());
        result.vertices_unobserved[group] = input.vertex_count - observed_count;
        result.group_inputs_complete[group] =
            observed_count == input.vertex_count &&
            (group == 0U || observed_base_count == base_group.vertex_count);
        auto& mapped = result.reference_indices[group];
        mapped.reserve(observed_count);
        for (std::size_t vertex = 0; vertex < observed_count; ++vertex) {
            if (group == 0U) {
                mapped.push_back(static_cast<std::uint16_t>(vertex));
                continue;
            }
            std::uint16_t nearest_index = 0U;
            long double nearest_distance = 100000000.0L;
            const auto& candidate = input.vertices[vertex];
            for (std::size_t base = 0; base < observed_base_count; ++base) {
                const auto& reference = base_group.vertices[base];
                const long double dx = static_cast<long double>(reference[0]) - candidate[0];
                const long double dy = static_cast<long double>(reference[1]) - candidate[1];
                const long double dz = static_cast<long double>(reference[2]) - candidate[2];
                const long double distance = dx * dx + dz * dz + dy * dy;
                if (distance < nearest_distance) {
                    nearest_distance = distance;
                    nearest_index = static_cast<std::uint16_t>(base & 0xffffU);
                }
            }
            mapped.push_back(nearest_index);
        }
        result.vertices_mapped[group] = mapped.size();
    }
    return result;
}

std::optional<NcarBodyConstructorObservation>
make_ncar_body_constructor_observation_from_runtime_record(
    const std::span<const std::byte> runtime_record,
    const std::optional<std::uint32_t> serialized_object_stream_offset,
    const bool fdiv_compatibility_enabled,
    const std::optional<std::uint32_t> compatibility_fill_bits) noexcept
{
    constexpr std::size_t required_size = 0x1e4U;
    if (runtime_record.size() < required_size) {
        return std::nullopt;
    }

    NcarBodyConstructorObservation observation{};
    observation.source_record_word_0x00 = read_u32_le(runtime_record.data(), 0U);
    observation.serialized_object_stream_offset = serialized_object_stream_offset;
    observation.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    observation.compatibility_fill_bits = compatibility_fill_bits;
    for (std::size_t index = 0; index < observation.source_values_0x08_to_0x14.size();
         ++index) {
        observation.source_values_0x08_to_0x14[index] = std::bit_cast<float>(
            read_u32_le(runtime_record.data(), 0x08U + index * sizeof(std::uint32_t)));
    }
    return observation;
}

NcarBodyConstructorPlan plan_ncar_body_constructor(
    const NcarBodyConstructorObservation& observation)
{
    NcarBodyConstructorPlan result{};
    result.initialized_subobjects_at_0x6c = 8U;
    result.initialized_subobjects_at_0x2b0 = 16U;
    result.helper_0x4375c0_called = true;
    result.helper_0x43e650_called = true;
    result.fdiv_compatibility_enabled = observation.fdiv_compatibility_enabled;
    const auto write32 = [](std::vector<NcarObjectFieldWrite>& writes,
                            const std::uint32_t offset,
                            const std::uint32_t bits) {
        writes.push_back({offset, 4U, bits});
    };
    const auto write8 = [](std::vector<NcarObjectFieldWrite>& writes,
                           const std::uint32_t offset,
                           const std::uint8_t value) {
        writes.push_back({offset, 1U, value});
    };
    const auto write16 = [](std::vector<NcarObjectFieldWrite>& writes,
                            const std::uint32_t offset,
                            const std::uint16_t value) {
        writes.push_back({offset, 2U, value});
    };

    auto& base_writes = result.inherited_base_writes_0x4375c0;
    write32(base_writes, 0x00U, 0x004c80c8U);
    for (const std::uint32_t offset : {0x28U, 0x2cU, 0x30U, 0x24U}) {
        write32(base_writes, offset, 0U);
    }
    write16(base_writes, 0x10U, 0U);
    write32(base_writes, 0x0cU, observation.source_record_word_0x00);
    write32(base_writes, 0x04U, 0U);
    if (observation.serialized_object_stream_offset.has_value()) {
        write32(base_writes, 0x08U, *observation.serialized_object_stream_offset);
    }
    write32(base_writes, 0x3cU, 0U);
    write32(base_writes, 0x40U, 0U);
    write32(base_writes, 0x38U, 0x3f800000U);
    write32(base_writes, 0x44U, 0U);
    write32(base_writes, 0x48U, 0x3f800000U);
    write32(base_writes, 0x4cU, 0U);
    write32(base_writes, 0x50U, 0U);
    write32(base_writes, 0x54U, 0U);
    write32(base_writes, 0x58U, 0x3f800000U);
    write32(base_writes, 0x5cU, 0U);
    write32(base_writes, 0x60U, 0U);
    write8(base_writes, 0x68U, 1U);
    write32(base_writes, 0x64U, 0U);

    auto& subobject_writes = result.subobject_writes_0x43f460;
    for (std::size_t subobject = 0; subobject < 8U; ++subobject) {
        const auto base = 0x6cU + static_cast<std::uint32_t>(subobject * 0x30U);
        write32(subobject_writes, base + 0x18U, 0x447a0000U);
        write8(subobject_writes, base + 0x0bU, 1U);
        write32(subobject_writes, base, 0U);
        write32(subobject_writes, base + 0x04U, 0U);
        write32(subobject_writes, base + 0x08U, 0U);
        write32(subobject_writes, base + 0x1cU, 0U);
    }

    auto& finalizer_writes = result.finalizer_writes_0x43e650;
    write16(finalizer_writes, 0x10U, 1U);
    for (const std::uint32_t offset : {0x234U, 0x238U, 0x23cU, 0x240U,
             0x244U, 0x248U, 0x24cU, 0x250U, 0x254U, 0x258U,
             0x25cU, 0x260U, 0x288U, 0x28cU, 0x290U, 0x294U,
             0x298U, 0x29cU, 0x2a0U, 0x2a4U, 0x2a8U, 0x2acU, 0x470U}) {
        write32(finalizer_writes, offset, 0U);
    }

    static constexpr std::array<long double, 4> scale{
        0.4545L, 0.042279862L, 0.042279862L, 0.042279862L};
    for (std::size_t field = 0; field < scale.size(); ++field) {
        result.scaled_dimensions_0x1f0_to_0x1fc[field] =
            static_cast<float>(static_cast<long double>(
                observation.source_values_0x08_to_0x14[field]) * scale[field]);
        result.reciprocal_dimensions_0x200_to_0x20c[field] =
            static_cast<float>(divide_with_vc98_fdiv_compatibility(
                1.0L,
                result.scaled_dimensions_0x1f0_to_0x1fc[field],
                observation.fdiv_compatibility_enabled));
    }

    std::optional<float> fill_value = 0.0F;
    if (observation.fdiv_compatibility_enabled) {
        if (observation.compatibility_fill_bits.has_value()) {
            fill_value = std::bit_cast<float>(
                *observation.compatibility_fill_bits);
        } else {
            fill_value.reset();
            result.fdiv_compatibility_fill_unknown = true;
        }
    }
    result.matrix_0x210 = {
        result.reciprocal_dimensions_0x200_to_0x20c[1], fill_value, fill_value,
        fill_value, result.reciprocal_dimensions_0x200_to_0x20c[2], fill_value,
        fill_value, fill_value, result.reciprocal_dimensions_0x200_to_0x20c[3]};
    result.copied_matrix_0x264 = result.matrix_0x210;
    return result;
}

bool apply_ncar_body_object_writes(
    NcarBodyRuntimeObjectState& destination,
    const std::span<const NcarObjectFieldWrite> writes) noexcept
{
    auto materialized = destination;
    for (const auto& write : writes) {
        if ((write.width_bytes != 1U && write.width_bytes != 2U &&
                write.width_bytes != 4U) ||
            static_cast<std::uint64_t>(write.offset) + write.width_bytes >
                ncar_body_runtime_object_bytes) {
            return false;
        }
        const auto offset = static_cast<std::size_t>(write.offset);
        for (std::size_t byte = 0; byte < write.width_bytes; ++byte) {
            materialized.bytes[offset + byte] = static_cast<std::byte>(
                (write.value_bits >> (byte * 8U)) & 0xffU);
            materialized.initialized_bytes[offset + byte] = 1U;
        }
    }
    destination = materialized;
    return true;
}

std::optional<std::uint32_t> read_ncar_body_object_u32(
    const NcarBodyRuntimeObjectState& source,
    const std::uint32_t offset) noexcept
{
    if (static_cast<std::uint64_t>(offset) + sizeof(std::uint32_t) >
        ncar_body_runtime_object_bytes) {
        return std::nullopt;
    }
    const auto start = static_cast<std::size_t>(offset);
    std::uint32_t value{};
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        if (source.initialized_bytes[start + byte] == 0U) return std::nullopt;
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
            source.bytes[start + byte])) << (byte * 8U);
    }
    return value;
}

std::optional<std::uint8_t> read_ncar_body_object_u8(
    const NcarBodyRuntimeObjectState& source,
    const std::uint32_t offset) noexcept
{
    if (offset >= ncar_body_runtime_object_bytes ||
        source.initialized_bytes[offset] == 0U) return std::nullopt;
    return std::to_integer<std::uint8_t>(source.bytes[offset]);
}

bool materialize_ncar_body_constructor(
    NcarBodyRuntimeObjectState& destination,
    const NcarBodyConstructorPlan& plan) noexcept
{
    if (!plan.helper_0x4375c0_called || !plan.helper_0x43e650_called ||
        plan.fdiv_compatibility_fill_unknown) {
        return false;
    }
    for (const auto& value : plan.matrix_0x210) {
        if (!value.has_value()) return false;
    }
    for (const auto& value : plan.copied_matrix_0x264) {
        if (!value.has_value()) return false;
    }

    std::vector<NcarObjectFieldWrite> writes;
    writes.reserve(plan.inherited_base_writes_0x4375c0.size() +
        plan.subobject_writes_0x43f460.size() +
        plan.finalizer_writes_0x43e650.size() + 4U + 4U + 18U + 3U);
    writes.insert(writes.end(), plan.inherited_base_writes_0x4375c0.begin(),
        plan.inherited_base_writes_0x4375c0.end());
    writes.insert(writes.end(), plan.subobject_writes_0x43f460.begin(),
        plan.subobject_writes_0x43f460.end());
    writes.push_back({0x00U, 4U, plan.vtable_address});
    writes.push_back({0x1ecU, 4U, plan.cleared_dword_0x1ec});
    writes.push_back({0x474U, 1U, plan.cleared_byte_0x474});
    for (std::size_t index = 0; index < plan.scaled_dimensions_0x1f0_to_0x1fc.size(); ++index) {
        writes.push_back({static_cast<std::uint32_t>(0x1f0U + index * 4U), 4U,
            std::bit_cast<std::uint32_t>(plan.scaled_dimensions_0x1f0_to_0x1fc[index])});
        writes.push_back({static_cast<std::uint32_t>(0x200U + index * 4U), 4U,
            std::bit_cast<std::uint32_t>(plan.reciprocal_dimensions_0x200_to_0x20c[index])});
        writes.push_back({static_cast<std::uint32_t>(0x210U + index * 4U), 4U,
            std::bit_cast<std::uint32_t>(*plan.matrix_0x210[index])});
        writes.push_back({static_cast<std::uint32_t>(0x264U + index * 4U), 4U,
            std::bit_cast<std::uint32_t>(*plan.copied_matrix_0x264[index])});
    }
    writes.insert(writes.end(), plan.finalizer_writes_0x43e650.begin(),
        plan.finalizer_writes_0x43e650.end());
    return apply_ncar_body_object_writes(destination, writes);
}

NcarConstructorSubobjectDefaultsPlan plan_ncar_constructor_subobject_defaults(
    const std::optional<std::uint32_t> source_record_word_0x65)
{
    NcarConstructorSubobjectDefaultsPlan result{};
    result.update_curve_subobjects_initialized = 1U;
    result.selector_subobjects_initialized = 1U;
    result.linked_state_subobjects_initialized = 3U;
    const auto write = [&](const std::uint32_t offset,
                           const std::uint8_t width,
                           const std::uint32_t bits) {
        const NcarObjectFieldWrite field{offset, width, bits};
        result.writes.push_back(field);
        if ((offset >= 0x0bfcU && offset < 0x0c98U) ||
            (offset >= 0x0c98U && offset < 0x0d04U)) {
            result.embedded_curve_selector_writes.push_back(field);
        }
    };

    // 0x440100 at parent +0xBFC. Its inherited 0x43F8F0 base is followed
    // by the curve vtable and selector/evaluator defaults.
    write(0x0bfcU, 4U, 0x004c8770U);
    // +0xC00 is a native pointer/link and must not be fabricated as a zero
    // DWORD by the host byte-backed object.
    for (const std::uint32_t offset : {0x0c04U, 0x0c08U,
             0x0c0cU, 0x0c10U, 0x0c14U}) {
        write(offset, 4U, 0U);
    }
    write(0x0bfcU, 4U, 0x004c8780U);
    write(0x0c14U, 4U, 0x43142000U); // +0x18
    write(0x0c18U, 4U, 0x478ca000U); // +0x1C, 72000
    write(0x0c1cU, 4U, 0x37690453U); // +0x20, 1/72000
    write(0x0c20U, 4U, 0x43910000U); // +0x24
    write(0x0c24U, 4U, 0U);          // +0x28
    write(0x0c30U, 4U, 0x43142000U); // +0x34
    write(0x0c34U, 4U, 0U);          // +0x38
    write(0x0c38U, 4U, 0x45bb8000U); // +0x3C, 6000
    write(0x0c3cU, 1U, 0U);          // +0x40
    write(0x0c3dU, 1U, 0U);          // +0x41
    write(0x0c64U, 4U, 0x43910000U); // +0x68

    // 0x43F7D0 at object +0xC7C: five zero DWORDs.
    for (const std::uint32_t offset : {0x0c7cU, 0x0c80U, 0x0c84U,
             0x0c88U, 0x0c94U}) {
        write(offset, 4U, 0U);
    }

    // 0x43FA30's 0x43F8F0 base writes, then its overrides at +0xC98.
    write(0x0c98U, 4U, 0x004c8770U);
    // +0xC9C is the selector's native parent pointer.
    for (const std::uint32_t offset : {0x0cacU, 0x0ca8U, 0x0ca4U,
             0x0ca0U}) {
        write(offset, 4U, 0U);
    }
    write(0x0c98U, 4U, 0x004c8778U);
    for (const std::uint32_t offset : {0x0cb0U, 0x0cb8U, 0x0cb4U,
             0x0cc0U, 0x0ce4U}) {
        write(offset, 4U, 0U);
    }
    write(0x0cf0U, 1U, 0U);
    write(0x0cf8U, 4U, 0U);
    write(0x0cf4U, 4U, 0U);
    write(0x0d00U, 4U, 4U);
    write(0x0cbcU, 4U, 0U);

    // 0x43F6D0 at +0xD04, +0xD24 and +0xD44.
    for (const std::uint32_t base : {0x0d04U, 0x0d24U, 0x0d44U}) {
        write(base, 4U, 0x004c8768U);
        for (const std::uint32_t offset : {0x08U, 0x04U, 0x1cU,
                 0x18U, 0x10U, 0x0cU, 0x14U}) {
            write(base + offset, 4U, 0U);
        }
    }

    // Remaining direct 0x42FF90 state initialization after vertex maps.
    write(0x0000U, 4U, 0x004c7f20U);
    write(0x0514U, 1U, 0U);
    write(0x04f4U, 4U, 2U);
    write(0x0534U, 1U, 0U);
    write(0x0d74U, 1U, 0U);
    write(0x04f8U, 4U, 0U);
    if (source_record_word_0x65.has_value()) {
        write(0x0510U, 4U, *source_record_word_0x65);
        write(0x0d78U, 4U, *source_record_word_0x65);
    } else {
        result.constructor_tail_source_unknown = true;
    }
    write(0x050cU, 4U, 0x3f800000U);
    for (std::uint32_t offset = 0x048eU; offset < 0x04aeU; offset += 4U) {
        write(offset, 4U, 0U);
    }
    write(0x0be4U, 1U, 0U);
    write(0x04fcU, 4U, 2U);
    write(0x0e40U, 4U, 0U);
    return result;
}

NcarWheelConstructorPlan plan_ncar_wheel_constructor(
    const std::uint32_t wheel_object_offset,
    const NcarWheelConstructorObservation& observation)
{
    NcarWheelConstructorPlan result{};
    result.fdiv_compatibility_enabled = observation.fdiv_compatibility_enabled;
    const auto write = [&](const std::uint32_t offset,
                           const std::uint8_t width,
                           const std::uint32_t bits) {
        result.writes.push_back({wheel_object_offset + offset, width, bits});
    };
    const auto write_fill32 = [&](const std::uint32_t offset) {
        if (!observation.fdiv_compatibility_enabled) {
            write(offset, 4U, 0U);
        } else if (observation.compatibility_fill_bits.has_value()) {
            write(offset, 4U, *observation.compatibility_fill_bits);
        } else {
            result.unknown_fill_offsets.push_back(wheel_object_offset + offset);
        }
    };
    const auto write_fill8 = [&](const std::uint32_t offset) {
        if (!observation.fdiv_compatibility_enabled) {
            write(offset, 1U, 0U);
        } else if (observation.compatibility_fill_bits.has_value()) {
            write(offset, 1U,
                *observation.compatibility_fill_bits & 0xffU);
        } else {
            result.unknown_fill_offsets.push_back(wheel_object_offset + offset);
        }
    };

    write(0x00U, 4U, 0x004c89c0U);
    write(0x194U, 4U, 0x43910000U);
    write(0x18cU, 4U, 0x44e10000U);
    const auto reciprocal = divide_with_vc98_fdiv_compatibility(
        1.0L, 1800.0L, observation.fdiv_compatibility_enabled);
    write(0x00U, 4U, 0x004c89c8U);

    write_fill8(0x020U);
    write_fill32(0x19cU);
    write_fill8(0x1a0U);
    for (const std::uint32_t offset : {0x08cU, 0x0e8U, 0x0e4U, 0x0e0U,
             0x158U, 0x128U, 0x0d8U, 0x17cU, 0x0d4U, 0x0dcU,
             0x0a4U, 0x10cU, 0x130U, 0x134U, 0x138U, 0x100U,
             0x104U, 0x108U, 0x110U, 0x198U, 0x118U, 0x170U,
             0x174U, 0x178U, 0x14cU}) {
        write_fill32(offset);
    }
    write(0x150U, 4U, 0x3f800000U);
    write_fill32(0x154U);
    for (const std::uint32_t offset : {0x0bcU, 0x0c0U, 0x0c4U,
             0x0c8U, 0x0ccU, 0x0d0U, 0x16cU, 0x0acU, 0x11cU}) {
        write_fill32(offset);
    }
    write_fill8(0x120U);
    write_fill8(0x121U);
    for (const std::uint32_t offset : {0x060U, 0x064U}) {
        write_fill32(offset);
    }
    write(0x05cU, 4U, 0x3f800000U);
    write(0x034U, 4U, 0x41200000U);
    write(0x038U, 4U, 0x3dcccccdU);
    write(0x114U, 4U, 0x3f800000U);
    write(0x190U, 4U, std::bit_cast<std::uint32_t>(
        static_cast<float>(reciprocal)));
    write(0x188U, 4U, 0x43060001U);
    for (const std::uint32_t offset : {0x124U, 0x13cU, 0x0b0U,
             0x0b4U}) {
        write_fill32(offset);
    }
    write(0x0b8U, 4U, 0x3f800000U);
    for (const std::uint32_t offset : {0x140U, 0x144U, 0x148U,
             0x004U, 0x008U, 0x00cU}) {
        write_fill32(offset);
    }
    write_fill8(0x160U);
    write_fill32(0x15cU);
    write_fill32(0x0a8U);
    write_fill8(0x168U);
    for (const std::uint32_t offset : {0x04cU, 0x0f4U, 0x0f8U,
             0x0fcU, 0x184U, 0x010U}) {
        write_fill32(offset);
    }
    return result;
}

bool apply_ncar_wheel_object_writes(
    NcarWheelRuntimeObjectState& destination,
    const std::uint32_t wheel_object_offset,
    const std::span<const NcarObjectFieldWrite> writes) noexcept
{
    auto materialized = destination;
    const auto object_begin = static_cast<std::uint64_t>(wheel_object_offset);
    const auto object_end = object_begin + ncar_wheel_object_bytes;
    for (const auto& write : writes) {
        if ((write.width_bytes != 1U && write.width_bytes != 4U) ||
            write.offset < object_begin ||
            static_cast<std::uint64_t>(write.offset) + write.width_bytes > object_end) {
            return false;
        }
        const auto local_offset = static_cast<std::size_t>(
            static_cast<std::uint64_t>(write.offset) - object_begin);
        for (std::size_t byte = 0; byte < write.width_bytes; ++byte) {
            materialized.bytes[local_offset + byte] = static_cast<std::byte>(
                (write.value_bits >> (byte * 8U)) & 0xffU);
            materialized.initialized_bytes[local_offset + byte] = 1U;
        }
    }
    destination = materialized;
    return true;
}

bool materialize_ncar_wheel_constructor(
    NcarWheelRuntimeObjectState& destination,
    const std::uint32_t wheel_object_offset,
    const NcarWheelConstructorPlan& plan) noexcept
{
    if (!plan.unknown_fill_offsets.empty()) return false;
    return apply_ncar_wheel_object_writes(destination, wheel_object_offset,
        plan.writes);
}

NcarSoundConstructorPlan plan_ncar_sound_constructor(
    const NcarSoundConstructorObservation& observation)
{
    NcarSoundConstructorPlan result{};
    constexpr std::array<std::uint32_t, 7> car_slots{
        0x0e50U, 0x0e58U, 0x0e5cU, 0x0e60U, 0x0e64U, 0x0e4cU, 0x0e54U};
    for (std::size_t i = 0; i < car_slots.size(); ++i) {
        if (observation.handles[i].has_value()) {
            result.car_writes.push_back({car_slots[i], 4U, *observation.handles[i]});
        } else {
            result.unresolved_handle_slots.push_back(car_slots[i]);
        }
    }

    // All six 0x46C080/0x46C030 objects receive the car's +0x510 owner at sound +0x10.
    for (std::size_t i = 1; i <= 6; ++i) {
        const auto sound = observation.handles[i];
        if (!sound.has_value() || *sound == 0U) {
            if (!sound.has_value()) {
                result.unresolved_sound_objects.push_back(car_slots[i]);
            }
            continue;
        }
        if (observation.owner_handle_0x510.has_value()) {
            result.sound_writes.push_back({*sound, 0x10U, 4U,
                *observation.owner_handle_0x510});
        } else {
            result.unresolved_sound_objects.push_back(car_slots[i]);
        }
    }

    const auto shift = observation.handles[6];
    if (!shift.has_value()) {
        result.unresolved_sound_objects.push_back(car_slots[5]);
    } else if (*shift != 0U) {
        if (observation.shift_gain_0x30_bits.has_value()) {
            if (*observation.shift_gain_0x30_bits != 0x3e4ccccdU) {
                result.sound_writes.push_back({*shift, 0x34U, 1U, 1U});
                result.sound_writes.push_back({*shift, 0x30U, 4U, 0x3e4ccccdU});
            }
        } else {
            result.unresolved_sound_objects.push_back(car_slots[5]);
        }
        if (observation.shift_pitch_0x08_bits.has_value()) {
            if (*observation.shift_pitch_0x08_bits != 0x3f19999aU) {
                result.sound_writes.push_back({*shift, 0x2cU, 1U, 1U});
                result.sound_writes.push_back({*shift, 0x08U, 4U, 0x3f19999aU});
            }
        } else {
            result.unresolved_sound_objects.push_back(car_slots[5]);
        }
    }
    return result;
}

NcarSampleCallbackEffect apply_ncar_sample_callback(
    NcarTimedCallbackState& state,
    const NcarSampleRecord& sample,
    const float now) noexcept
{
    NcarSampleCallbackEffect result{};
    result.virtual_method_0x40_reached = true;
    for (std::size_t axis = 0; axis < result.sample_vector_0x1c.size(); ++axis) {
        result.sample_vector_0x1c[axis] = std::bit_cast<float>(
            sample.vector_bits_0x1c[axis]);
    }
    const auto q0 = std::bit_cast<float>(sample.rotation_bits_0x28[0]);
    const auto q1 = std::bit_cast<float>(sample.rotation_bits_0x28[1]);
    const auto q2 = std::bit_cast<float>(sample.rotation_bits_0x28[2]);
    const auto q3 = std::bit_cast<float>(sample.rotation_bits_0x28[3]);
    const auto q0d = static_cast<double>(q0);
    const auto q1d = static_cast<double>(q1);
    const auto q2d = static_cast<double>(q2);
    const auto q3d = static_cast<double>(q3);
    result.callback_components[0] = static_cast<float>(
        q3d * (q1d * 2.0) + (q2d * 2.0) * q0d);
    result.callback_components[1] = static_cast<float>(
        1.0 - (q1d * (q1d * 2.0) + q0d * (q0d * 2.0)));

    auto& target = state.virtual_target;
    auto& virtual_effect = result.virtual_effect;
    virtual_effect.reached = true;
    virtual_effect.fdiv_compatibility_enabled =
        state.fdiv_compatibility_enabled;
    virtual_effect.leading_child_virtual_0x14_reached = true;
    if (target.render_group_pointer_present) {
        for (std::size_t pass = 0;
             pass < virtual_effect.render_group_virtual_0x14_passes;
             ++pass) {
            for (auto& child : target.render_children) {
                child.cached_position_bits_0x20 =
                    child.current_position_bits_0x64;
                child.dirty_flag_0x88 = 0U;
                ++virtual_effect.render_child_virtual_0x14_calls;
            }
        }
    } else {
        virtual_effect.invalid_render_group_pointer_dereference_reached = true;
    }
    target.orientation_0x38.fill(0.0F);
    const auto component_a = result.callback_components[0];
    const auto component_b = result.callback_components[1];
    const auto reciprocal_length = [&](const std::array<float, 3>& vector) {
        const auto squared = static_cast<long double>(vector[0]) * vector[0] +
            static_cast<long double>(vector[1]) * vector[1] +
            static_cast<long double>(vector[2]) * vector[2];
        const auto length = std::sqrt(squared);
        return divide_with_vc98_fdiv_compatibility(
            1.0L, length, state.fdiv_compatibility_enabled);
    };
    const auto inverse_first_length = reciprocal_length(
        {component_b, -component_a, 0.0F});
    const std::array<float, 3> first_basis{
        static_cast<float>(static_cast<long double>(component_b) *
                           inverse_first_length),
        static_cast<float>(0.0L * inverse_first_length),
        static_cast<float>(-static_cast<long double>(component_a) *
                           inverse_first_length)};
    const std::array<float, 3> raw_second_basis{
        first_basis[2] * 0.0F,
        first_basis[0] * component_b - first_basis[2] * component_a,
        first_basis[1] * component_a - first_basis[0] * 0.0F};
    const auto inverse_second_length = reciprocal_length(raw_second_basis);
    const std::array<float, 3> second_basis{
        static_cast<float>(static_cast<long double>(raw_second_basis[0]) *
                           inverse_second_length),
        static_cast<float>(static_cast<long double>(raw_second_basis[1]) *
                           inverse_second_length),
        static_cast<float>(static_cast<long double>(raw_second_basis[2]) *
                           inverse_second_length)};
    target.orientation_0x38 = {
        first_basis[0], first_basis[1], first_basis[2],
        second_basis[0], second_basis[1], second_basis[2],
        first_basis[1] * second_basis[2] - second_basis[1] * first_basis[2],
        first_basis[2] * second_basis[0] - second_basis[2] * first_basis[0],
        first_basis[0] * second_basis[1] - second_basis[0] * first_basis[1]};
    target.position_0x5c = result.sample_vector_0x1c;
    target.velocity_0x234.fill(0.0F);
    target.zeroed_field_0xd70 = 0U;
    target.timer_0x50c = 0.0F;
    target.callback_time_0xe44 = now;
    target.flag_0xe3c = true;
    target.flag_0xe3d = true;

    virtual_effect.embedded_zero_setter_reached = true;
    target.embedded_value_0xcbc = 0U;
    target.copied_value_0xd64 = target.embedded_value_0xcbc;
    virtual_effect.embedded_value_copied = true;

    virtual_effect.child_flags_update_reached = true;
    const auto child_flag_count = std::min<std::size_t>(
        target.child_count_0x1ec, target.child_flags_0x98.size());
    std::fill_n(target.child_flags_0x98.begin(), child_flag_count,
                std::uint8_t{1U});
    virtual_effect.child_flags_written = child_flag_count;
    virtual_effect.child_flag_span_covers_count =
        child_flag_count == target.child_count_0x1ec;
    virtual_effect.child_resource_update_reached = true;
    const auto resource_reset = reset_ncar_resource_groups(
        target.resource_groups, target.resource_flag_0x15);
    virtual_effect.resource_groups_visited = resource_reset.groups_visited;
    virtual_effect.resource_records_copied = resource_reset.records_copied;
    virtual_effect.resource_records_unprocessed =
        resource_reset.records_unprocessed;
    virtual_effect.resource_group_inputs_complete =
        resource_reset.inputs_complete;
    target.object_byte_0x48c = 0U;
    target.object_byte_0x48d = 0U;
    target.object_dwords_0x48e.fill(0U);
    target.callback_slot_flags_0x574.fill(0U);
    target.callback_slots_0x554.fill(NcarSampleCallbackSlotState{});
    virtual_effect.callback_slot_reset_helper_calls_reached = true;
    virtual_effect.callback_slot_clear_reached = true;
    target.field_0xc0c = 0U;
    virtual_effect.child_virtual_0x04_reached = true;
    virtual_effect.trailing_child_virtual_0x14_reached = true;
    virtual_effect.event_queue_append_reached = true;
    virtual_effect.event_payload = {target.manager_index_0x510, 0U};

    state.prior_time_0xf2c = now;
    result.callback_time_written = true;
    state.gate_0xf20 = true;
    result.gate_0xf20_set = true;
    return result;
}

NcarTimedCallbackResult advance_ncar_timed_callback(
    NcarTimedCallbackState& state,
    const NcarSampleRecord& sample,
    const float now) noexcept
{
    NcarTimedCallbackResult result{};
    if (!state.gate_0xf20 &&
        static_cast<double>(now) - static_cast<double>(state.prior_time_0xf2c) > 0.5) {
        result.main_chain_reached = true;
        result.interpolation_0x43a190_reached = true;
        result.prediction_0x439a10_reached = true;
        result.final_update_0x439db0_reached = true;
    }

    if (state.sample_flag_0x39) {
        result.sample_callback_0x439d30_reached = true;
        const auto effect = apply_ncar_sample_callback(state, sample, now);
        result.sample_virtual_method_0x40_reached =
            effect.virtual_method_0x40_reached;
        result.sample_vector_0x1c = effect.sample_vector_0x1c;
        result.callback_components = effect.callback_components;
        result.callback_time_written = effect.callback_time_written;
        result.gate_0xf20_set = effect.gate_0xf20_set;
        result.virtual_effect = effect.virtual_effect;
        state.sample_flag_0x39 = false;
        result.sample_flag_cleared = true;
    }
    return result;
}

NcarPositionPredictionResult advance_ncar_position_prediction(
    NcarPositionPredictionState& state,
    NcarTimedCallbackState& callback_state,
    const NcarSampleWindow& samples,
    const float now,
    const bool fdiv_compatibility_enabled) noexcept
{
    NcarPositionPredictionResult result{};
    result.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    callback_state.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    const auto as_float = [](const std::uint32_t bits) noexcept {
        return std::bit_cast<float>(bits);
    };
    const auto& newest = samples.records[0];
    const auto sample_time = static_cast<long double>(
        as_float(newest.sample_time_bits_0x04));
    const auto delta_time = static_cast<long double>(now) - sample_time;

    for (std::size_t axis = 0; axis < result.predicted_position.size(); ++axis) {
        const auto sample_position = static_cast<long double>(
            as_float(newest.vector_bits_0x1c[axis]));
        const auto velocity = static_cast<long double>(state.velocity_0x234[axis]);
        const auto acceleration = static_cast<long double>(
            state.acceleration_0xef0[axis]);
        result.predicted_position[axis] = static_cast<float>(
            ((acceleration * delta_time * 0.5L + velocity) * delta_time) +
            sample_position);
    }

    std::array<long double, 3> times{};
    for (std::size_t index = 0; index < times.size(); ++index) {
        times[index] = static_cast<long double>(as_float(
            samples.records[index].sample_time_bits_0x04));
    }
    const auto evaluation_time = static_cast<long double>(now) * 2.0L;
    const auto coefficient_0 =
        (evaluation_time - times[1] - times[2]) /
        ((times[0] - times[2]) * (times[0] - times[1]));
    const auto coefficient_1 =
        (evaluation_time - times[0] - times[2]) /
        ((times[1] - times[0]) * (times[1] - times[2]));
    const auto coefficient_2 =
        (evaluation_time - times[0] - times[1]) /
        ((times[2] - times[0]) * (times[2] - times[1]));
    for (std::size_t axis = 0; axis < result.sample_velocity.size(); ++axis) {
        const auto position_0 = static_cast<long double>(
            as_float(samples.records[0].vector_bits_0x1c[axis]));
        const auto position_1 = static_cast<long double>(
            as_float(samples.records[1].vector_bits_0x1c[axis]));
        const auto position_2 = static_cast<long double>(
            as_float(samples.records[2].vector_bits_0x1c[axis]));
        auto velocity = coefficient_0 * position_0;
        velocity += coefficient_2 * position_2;
        velocity += coefficient_1 * position_1;
        result.sample_velocity[axis] = static_cast<float>(velocity);
        result.position_error[axis] = state.position_0x5c[axis] -
            result.predicted_position[axis];
    }
    const auto error_x = static_cast<long double>(result.position_error[0]);
    const auto error_y = static_cast<long double>(result.position_error[1]);
    const auto error_z = static_cast<long double>(result.position_error[2]);
    const auto squared_position_error =
        error_x * error_x + error_z * error_z + error_y * error_y;
    result.squared_position_error = static_cast<float>(squared_position_error);

    result.reset_gate_passed =
        std::fabs(static_cast<double>(state.value_0xefc) -
                  static_cast<double>(state.value_0xf0c)) <= 1.1920929e-7 &&
        static_cast<long double>(83.72251F) <= squared_position_error;
    if (result.reset_gate_passed) {
        result.reset_callback_0x439d30_reached = true;
        result.reset_callback_effect = apply_ncar_sample_callback(
            callback_state, newest, now);
        return result;
    }

    state.position_0x5c = blend_ncar_vector(
        state.position_0x5c, result.predicted_position, state.blend_weights_0xf00);
    state.velocity_0x234 = blend_ncar_vector(
        state.velocity_0x234, result.sample_velocity, state.blend_weights_0xf00);
    result.position_blend_applied = true;
    result.velocity_blend_applied = true;
    return result;
}

NcarTimedFrameResult advance_ncar_timed_frame(
    NcarTimedFrameState& state,
    const NcarSampleWindow& samples,
    const float now,
    const bool fdiv_compatibility_enabled) noexcept
{
    NcarTimedFrameResult result{};
    state.callback.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    const auto as_float = [](const std::uint32_t bits) noexcept {
        return std::bit_cast<float>(bits);
    };
    const auto newest_time = as_float(samples.records[0].sample_time_bits_0x04);
    const auto previous_time = as_float(samples.records[1].sample_time_bits_0x04);
    const auto main_gate = !state.callback.gate_0xf20 &&
        static_cast<double>(now) -
            static_cast<double>(state.callback.prior_time_0xf2c) > 0.5;

    if (main_gate) {
        result.main_chain_reached = true;
        result.interpolation_0x43a190_reached = true;
        advance_ncar_timed_interpolation(
            state.interpolation, newest_time, previous_time, now);

        state.position.value_0xefc = state.interpolation.current_0xefc[0];
        state.position.value_0xf0c = state.interpolation.target_0xf0c[0];
        result.position = advance_ncar_position_prediction(
            state.position, state.callback, samples, now,
            fdiv_compatibility_enabled);

        NcarRotationBlendObservation rotation_observation{};
        rotation_observation.now = now;
        rotation_observation.fdiv_compatibility_enabled =
            fdiv_compatibility_enabled;
        for (std::size_t index = 0; index < samples.records.size(); ++index) {
            rotation_observation.sample_times[index] = as_float(
                samples.records[index].sample_time_bits_0x04);
            for (std::size_t component = 0;
                 component < rotation_observation.sample_quaternions[index].size();
                 ++component) {
                rotation_observation.sample_quaternions[index][component] =
                    as_float(samples.records[index].rotation_bits_0x28[component]);
            }
        }
        state.rotation.interpolation_weight_0xefc =
            state.interpolation.current_0xefc[0];
        result.rotation_0x439db0_reached = true;
        result.rotation = advance_ncar_rotation_blend(
            state.rotation, rotation_observation);
    }

    if (state.callback.sample_flag_0x39) {
        result.sample_callback_0x439d30_reached = true;
        result.sample_callback_effect = apply_ncar_sample_callback(
            state.callback, samples.records[0], now);
        state.callback.sample_flag_0x39 = false;
        result.sample_flag_cleared = true;
    }
    return result;
}

std::array<float, 3> blend_ncar_vector(
    const std::array<float, 3>& from,
    const std::array<float, 3>& to,
    const std::array<float, 3>& weights) noexcept
{
    std::array<float, 3> result{};
    for (std::size_t axis = 0; axis < result.size(); ++axis) {
        const auto weight = static_cast<long double>(weights[axis]);
        result[axis] = static_cast<float>(
            static_cast<long double>(to[axis]) * weight +
            (1.0L - weight) * static_cast<long double>(from[axis]));
    }
    return result;
}

bool ncar_elapsed_gate(const float now, const float record_time,
                       const int record_mode, const bool input_is_one) noexcept
{
    if (record_mode < 1 && !input_is_one) {
        return false;
    }
    return now - record_time > 2.0F;
}

bool ncar_half_second_elapsed(const float now, const float timestamp) noexcept
{
    return now - timestamp > 0.5F;
}

void decay_ncar_timer(NcarStateFields& fields) noexcept
{
    constexpr std::int32_t threshold_bits = 0x3c83126f;
    const auto timer_bits = std::bit_cast<std::int32_t>(fields.timer_0x50c);
    if (timer_bits > threshold_bits) {
        fields.timer_0x50c -= 0.016F;
    } else {
        fields.timer_0x50c = 0.0F;
    }
}

bool apply_ncar_integrator_entry_reset(NcarStateFields& state) noexcept
{
    if (state.reset_gate_0x4fc != 0) {
        return false;
    }
    state.field_0xdec = 0.0F;
    state.field_0xde4 = 1.0F;
    state.angular_x_0x234 = 0.0F;
    state.angular_z_0x23c = 0.0F;
    return true;
}

bool initialize_ncar_rotation(NcarStateFields& fields) noexcept
{
    decay_ncar_timer(fields);
    if (!fields.initialized_0xf34) {
        fields.angular_x_0x234 = 0.0F;
        fields.angular_y_0x238 = 0.0F;
        fields.angular_z_0x23c = 0.0F;
        fields.angular_w_0x240 = 0.0F;
        fields.angular_extra_0x244 = 0.0F;
        fields.angular_extra_0x248 = 0.0F;
        fields.initialized_0xf34 = true;
        fields.angular_y_0x238 = std::bit_cast<float>(std::uint32_t{0x414c0c4aU});
    } else {
        const auto value_bits = std::bit_cast<std::uint32_t>(
            fields.angular_y_0x238);
        if (value_bits > 0x80000000U) {
            return true;
        }
    }

    fields.field_0xde4 = 0.0F;
    fields.field_0xde0 = 0.0F;
    fields.field_0xdec = 0.0F;
    return false;
}

namespace {

struct NcarCallbackPrefix {
    NcarCallbackMainResult result{};
    bool mode_3_branch{};
};

NcarCallbackPrefix advance_ncar_callback_prefix(
    NcarStateFields& state,
    const NcarCallbackMainObservation& observation) noexcept
{
    NcarCallbackPrefix prefix{};
    auto& result = prefix.result;
    if (observation.input_time_gate_0x4390c0) {
        result.rotation_initializer_reached = true;
        result.rotation_initializer_returned_early =
            initialize_ncar_rotation(state);
        result.main_integrator_0x431150_reached =
            !result.rotation_initializer_returned_early;
    } else if (state.record_mode_0xec8 == 3) {
        prefix.mode_3_branch = true;
        result.mode_3_branch_reached = true;
        result.control_update_0x4330f0_reached = true;
        state.field_0xde8 = 0.0F;
        state.field_0xcbc = observation.current_sample.small_integer_0x14;
        state.field_0xd64 = state.field_0xcbc;
        state.field_0xdec = 0.0F;
        state.field_0xde4 = std::bit_cast<float>(
            observation.current_sample.value_bits_0x08);
        state.field_0xde0 = 0.0F;
        result.mode_3_setter_values_written = true;
        result.linked_callback_0x434a80_reached = true;
        result.main_integrator_0x431150_reached = true;
    }

    if (result.main_integrator_0x431150_reached) {
        result.integrator_prefix_reset_applied =
            apply_ncar_integrator_entry_reset(state);
    }
    return prefix;
}

void advance_ncar_callback_tail(
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    const NcarCallbackMainObservation& observation,
    const bool mode_3_branch,
    NcarCallbackMainResult& result) noexcept
{
    if (mode_3_branch) {
        apply_ncar_callback_gate_bytes(
            object_gate_bytes, observation.current_sample);
        result.gate_bytes_written = true;

        const auto sample_time = std::bit_cast<float>(
            observation.current_sample.time_bits_0x00);
        if (!ncar_half_second_elapsed(observation.current_time,
                                      sample_time)) {
            state.timer_0x50c = 1.0F;
            result.timer_set_to_one = true;
            result.acceleration_reset_0x43a130_reached = true;
        } else {
            decay_ncar_timer(state);
            result.timer_decay_0x439160_reached = true;
        }
    }

    if (!observation.event_processor_active_0x49be40) {
        state.timer_0x50c = std::bit_cast<float>(std::uint32_t{0x3F4CCCCDU});
        result.exit_timer_overridden_to_0_8 = true;
    }
}

} // namespace

NcarCallbackMainResult advance_ncar_callback_main(
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    const NcarCallbackMainObservation& observation) noexcept
{
    auto prefix = advance_ncar_callback_prefix(state, observation);
    advance_ncar_callback_tail(state, object_gate_bytes, observation,
                               prefix.mode_3_branch, prefix.result);
    return prefix.result;
}

NcarCallbackBookkeepingResult advance_ncar_callback_main_with_bookkeeping(
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    NcarIntegratorBookkeepingState& bookkeeping_state,
    const NcarCallbackMainObservation& observation,
    const NcarIntegratorBookkeepingInput& bookkeeping_input) noexcept
{
    NcarCallbackBookkeepingResult result{};
    auto prefix = advance_ncar_callback_prefix(state, observation);
    result.callback = prefix.result;
    if (result.callback.main_integrator_0x431150_reached) {
        result.integrator_bookkeeping_reached = true;
        result.integrator_bookkeeping = advance_ncar_integrator_bookkeeping(
            bookkeeping_state, bookkeeping_input);
    }
    advance_ncar_callback_tail(state, object_gate_bytes, observation,
                               prefix.mode_3_branch, result.callback);
    return result;
}

NcarFrameCallbackResult advance_ncar_frame_callback(
    NcarCallbackQueueState& queue_state,
    NcarSampleBuffer& sample_buffer,
    TimedEventQueue& event_queue,
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    NcarIntegratorBookkeepingState& bookkeeping_state,
    const NcarFrameCallbackObservation& observation)
{
    NcarFrameCallbackResult result{};
    result.queue = advance_ncar_callback_queue(
        queue_state, sample_buffer, event_queue, observation.queue);

    auto main_observation = observation.main;
    main_observation.current_time = observation.queue.current_time;
    main_observation.event_processor_active_0x49be40 =
        observation.queue.event_processor_active_0x49be40;
    result.main = advance_ncar_callback_main_with_bookkeeping(
        state, object_gate_bytes, bookkeeping_state, main_observation,
        observation.integrator_bookkeeping);
    return result;
}

} // namespace viper
