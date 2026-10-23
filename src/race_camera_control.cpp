#include "viper/race_camera_control.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace viper {
namespace {

constexpr std::array<float, 4> translation_speed{{
    1.0F, 0.25F, 10.0F, 0.05F,
}};
constexpr std::array<float, 4> rotation_speed{{
    0.05F, 0.0125F, 0.15F, 0.0025F,
}};

using Matrix3 = std::array<float, 9>;

[[nodiscard]] bool key_down(const RaceCameraKeyState& keys,
                            const std::uint8_t key) noexcept
{
    return keys.down[key];
}

[[nodiscard]] char ascii_lower(const char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

[[nodiscard]] bool ascii_case_equal(
    const std::string_view left,
    const std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) return false;
    }
    return true;
}

[[nodiscard]] Matrix3 rotation_x(const float angle) noexcept
{
    const auto c = static_cast<float>(std::cos(static_cast<long double>(angle)));
    const auto s = static_cast<float>(std::sin(static_cast<long double>(angle)));
    return {1.0F, 0.0F, 0.0F, 0.0F, c, s, 0.0F, -s, c};
}

[[nodiscard]] Matrix3 rotation_y(const float angle) noexcept
{
    const auto c = static_cast<float>(std::cos(static_cast<long double>(angle)));
    const auto s = static_cast<float>(std::sin(static_cast<long double>(angle)));
    return {c, 0.0F, -s, 0.0F, 1.0F, 0.0F, s, 0.0F, c};
}

[[nodiscard]] Matrix3 rotation_z(const float angle) noexcept
{
    const auto c = static_cast<float>(std::cos(static_cast<long double>(angle)));
    const auto s = static_cast<float>(std::sin(static_cast<long double>(angle)));
    return {c, s, 0.0F, -s, c, 0.0F, 0.0F, 0.0F, 1.0F};
}

[[nodiscard]] Matrix3 multiply(const Matrix3& left,
                              const Matrix3& right) noexcept
{
    Matrix3 product{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            const auto index = row * 3 + column;
            product[index] = static_cast<float>(
                static_cast<long double>(left[row * 3]) * right[column] +
                static_cast<long double>(left[row * 3 + 1]) *
                    right[3 + column] +
                static_cast<long double>(left[row * 3 + 2]) *
                    right[6 + column]);
        }
    }
    return product;
}

void orthonormalize_camera_basis(Matrix3& orientation) noexcept
{
    std::array<float, 3> forward{{orientation[0], orientation[1], orientation[2]}};
    std::array<float, 3> side{{
        orientation[7] * forward[2] - orientation[8] * forward[1],
        orientation[8] * forward[0] - orientation[6] * forward[2],
        orientation[6] * forward[1] - orientation[7] * forward[0],
    }};
    const auto forward_length = std::sqrt(
        static_cast<long double>(forward[0]) * forward[0] +
        static_cast<long double>(forward[1]) * forward[1] +
        static_cast<long double>(forward[2]) * forward[2]);
    const auto side_length = std::sqrt(
        static_cast<long double>(side[0]) * side[0] +
        static_cast<long double>(side[1]) * side[1] +
        static_cast<long double>(side[2]) * side[2]);
    for (auto& component : forward) {
        component = static_cast<float>(component / forward_length);
    }
    for (auto& component : side) {
        component = static_cast<float>(component / side_length);
    }
    const std::array<float, 3> up{{
        side[1] * forward[2] - side[2] * forward[1],
        side[2] * forward[0] - side[0] * forward[2],
        side[0] * forward[1] - side[1] * forward[0],
    }};
    orientation = {
        forward[0], forward[1], forward[2],
        side[0], side[1], side[2],
        up[0], up[1], up[2],
    };
}

void build_look_at_basis(Matrix3& orientation,
                         const std::array<float, 3>& delta) noexcept
{
    orientation = {
        delta[2], 0.0F, -delta[0],
        0.0F, 0.0F, 0.0F,
        delta[0], delta[1], delta[2],
    };
    orthonormalize_camera_basis(orientation);
}

[[nodiscard]] Matrix3 preset_axis_angle_rotation(
    const RaceCameraPresetRecord& record) noexcept
{
    constexpr float degrees_to_radians = 0.017453292F;
    const std::array<float, 3> axis_angle{{
        record.raw_scalar_0x10 * degrees_to_radians,
        record.raw_scalar_0x14 * degrees_to_radians,
        record.raw_scalar_0x18 * degrees_to_radians,
    }};
    const auto angle = static_cast<float>(std::sqrt(
        static_cast<long double>(axis_angle[0]) * axis_angle[0] +
        static_cast<long double>(axis_angle[1]) * axis_angle[1] +
        static_cast<long double>(axis_angle[2]) * axis_angle[2]));
    if (angle <= 0.0001F) {
        return {
            1.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
        };
    }

    const auto inverse_angle = 1.0L / static_cast<long double>(angle);
    const auto x = static_cast<long double>(axis_angle[0]) * inverse_angle;
    const auto y = static_cast<long double>(axis_angle[1]) * inverse_angle;
    const auto z = static_cast<long double>(axis_angle[2]) * inverse_angle;
    const auto sine = std::sin(static_cast<long double>(angle));
    const auto cosine = std::cos(static_cast<long double>(angle));
    const auto one_minus_cosine = 1.0L - cosine;
    return {
        static_cast<float>(x * x * one_minus_cosine + cosine),
        static_cast<float>(x * y * one_minus_cosine + z * sine),
        static_cast<float>(x * z * one_minus_cosine - y * sine),
        static_cast<float>(x * y * one_minus_cosine - z * sine),
        static_cast<float>(y * y * one_minus_cosine + cosine),
        static_cast<float>(y * z * one_minus_cosine + x * sine),
        static_cast<float>(x * z * one_minus_cosine + y * sine),
        static_cast<float>(y * z * one_minus_cosine - x * sine),
        static_cast<float>(z * z * one_minus_cosine + cosine),
    };
}

} // namespace

RaceCameraTableLoadResult load_race_camera_table(
    const std::span<RaceCameraPresetRecord> records,
    const bool camera_tab_opened,
    const std::span<const RaceCameraTableRowObservation> rows) noexcept
{
    RaceCameraTableLoadResult result{};
    if (!camera_tab_opened) return result;

    result.selector_count = rows.size();
    if (rows.size() > records.size()) {
        result.input_complete = false;
        return result;
    }

    for (std::size_t index = 0; index < rows.size(); ++index) {
        auto& record = records[index];
        const auto type_name = rows[index].type_name;
        if (ascii_case_equal(type_name, "fixed")) {
            record.raw_word_0x00 = 0;
        } else if (ascii_case_equal(type_name, "pan")) {
            record.raw_word_0x00 = 1;
        } else if (ascii_case_equal(type_name, "pan_zoom")) {
            record.raw_word_0x00 = 2;
        } else if (ascii_case_equal(type_name, "chase")) {
            record.raw_word_0x00 = 3;
        } else {
            ++result.unknown_type_panic_count;
        }

        const auto& values = rows[index].converted_values;
        record.position = {values[0], values[1], values[2]};
        record.raw_scalar_0x10 = values[3];
        record.raw_scalar_0x14 = values[4];
        record.raw_scalar_0x18 = values[5];
    }
    return result;
}

void store_race_camera_mode(RaceCameraModeState& state,
                            const std::int32_t mode) noexcept
{
    state.raw_mode_0x508758 = mode;
}

RaceCameraMode0Output update_race_camera_mode_0(
    RaceCameraMode0State& state,
    const RaceCameraMode0Input& input) noexcept
{
    constexpr std::uint32_t smoothing_initialized = 0x02U;
    constexpr std::uint32_t cosine_initialized = 0x04U;
    if ((state.initialization_flags_0x507e04 & smoothing_initialized) == 0) {
        state.smoothed_target_0x507d00 = {0.0F, 1.0F, 0.0F};
        state.initialization_flags_0x507e04 |= smoothing_initialized;
    }
    if ((state.initialization_flags_0x507e04 & cosine_initialized) == 0) {
        constexpr long double thirty_degrees = 0.5235987715423107L;
        state.cosine_30_degrees_0x507d44 = static_cast<float>(
            std::cos(thirty_degrees));
        state.initialization_flags_0x507e04 |= cosine_initialized;
    }

    const auto& vehicle = input.vehicle_transform_0x38;
    const auto& local_point = input.local_camera_point_0x47c;
    const std::array<float, 3> world_offset{{
        vehicle[0] * local_point[0] +
            vehicle[3] * local_point[1] +
            vehicle[6] * local_point[2],
        vehicle[1] * local_point[0] +
            vehicle[4] * local_point[1] +
            vehicle[7] * local_point[2],
        vehicle[2] * local_point[0] +
            vehicle[5] * local_point[1] +
            vehicle[8] * local_point[2],
    }};

    std::array<float, 3> target{{vehicle[3], vehicle[4], vehicle[5]}};
    if (input.ground_sample_found) {
        const auto projection =
            input.ground_sample[0] * vehicle[3] +
            input.ground_sample[2] * vehicle[5] +
            input.ground_sample[1] * vehicle[4];
        auto blend = (projection - state.cosine_30_degrees_0x507d44) /
            (1.0F - state.cosine_30_degrees_0x507d44);
        if (blend <= 0.0F) {
            blend = 0.0F;
        }
        for (std::size_t i = 0; i < target.size(); ++i) {
            target[i] = (1.0F - blend) * vehicle[3 + i] +
                blend * input.ground_sample[i];
        }
    }

    for (std::size_t i = 0; i < state.smoothed_target_0x507d00.size(); ++i) {
        state.smoothed_target_0x507d00[i] =
            target[i] * 0.050000012F +
            state.smoothed_target_0x507d00[i] * 0.95F;
    }

    std::array<float, 12> transform = vehicle;
    for (std::size_t i = 0; i < world_offset.size(); ++i) {
        transform[9 + i] += world_offset[i];
    }
    const std::array<float, 3> midpoint{{
        vehicle[3] * 0.5F + state.smoothed_target_0x507d00[0] * 0.5F,
        vehicle[4] * 0.5F + state.smoothed_target_0x507d00[1] * 0.5F,
        vehicle[5] * 0.5F + state.smoothed_target_0x507d00[2] * 0.5F,
    }};

    Matrix3 orientation{{
        vehicle[8] * midpoint[1] - midpoint[2] * vehicle[7],
        midpoint[2] * vehicle[6] - vehicle[2] * vehicle[8],
        vehicle[7] * midpoint[0] - vehicle[6] * midpoint[1],
        midpoint[0], midpoint[1], midpoint[2],
        0.0F, 0.0F, 0.0F,
    }};
    orientation[6] = midpoint[2] * orientation[1] -
        orientation[2] * midpoint[1];
    orientation[7] = orientation[2] * midpoint[0] -
        midpoint[2] * orientation[0];
    orientation[8] = midpoint[1] * orientation[0] -
        orientation[1] * midpoint[0];
    orthonormalize_camera_basis(orientation);
    for (std::size_t i = 0; i < orientation.size(); ++i) {
        transform[i] = orientation[i];
    }

    const auto trailing_scale = input.vehicle_offset_factor_0xea8 * -0.005F;
    for (std::size_t i = 0; i < world_offset.size(); ++i) {
        transform[9 + i] += vehicle[6 + i] * trailing_scale;
    }
    return {transform, input.ground_sample_found};
}

bool prepare_race_camera_mode_11_output(
    const RaceCameraModeState& mode,
    const RaceCameraControlState& camera,
    RaceCameraMode11Output& output) noexcept
{
    if (mode.raw_mode_0x508758 != 11) {
        return false;
    }

    for (std::size_t i = 0; i < camera.orientation.size(); ++i) {
        output.transform[i] = camera.orientation[i];
    }
    for (std::size_t i = 0; i < camera.position.size(); ++i) {
        output.transform[camera.orientation.size() + i] = camera.position[i];
    }
    output.velocity = {};
    return true;
}

bool prepare_race_camera_mode_8_or_9_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes8And9Input& input,
    RaceCameraModes8And9Output& output) noexcept
{
    if (mode.raw_mode_0x508758 != 8 && mode.raw_mode_0x508758 != 9) {
        return false;
    }

    constexpr float quarter_turn = 1.5707964F;
    const auto quarter_turn_rotation = rotation_x(quarter_turn);
    output.transform = {
        quarter_turn_rotation[0], quarter_turn_rotation[1],
        quarter_turn_rotation[2], quarter_turn_rotation[3],
        quarter_turn_rotation[4], quarter_turn_rotation[5],
        quarter_turn_rotation[6], quarter_turn_rotation[7],
        quarter_turn_rotation[8],
        input.camera_anchor_position[0],
        input.camera_anchor_position[1] +
            (mode.raw_mode_0x508758 == 8 ? 10.0F : 50.0F),
        input.camera_anchor_position[2],
    };
    output.velocity = input.car_velocity;
    output.velocity[1] = 0.0F;
    return true;
}

bool prepare_race_camera_mode_1_or_2_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes1And2Input& input,
    RaceCameraModes1And2Output& output) noexcept
{
    if (mode.raw_mode_0x508758 != 1 && mode.raw_mode_0x508758 != 2) {
        return false;
    }

    const std::array<float, 3> local_offset =
        mode.raw_mode_0x508758 == 1
            ? std::array<float, 3>{0.0F, 0.5F, -0.1F}
            : std::array<float, 3>{0.0F, 0.8F, -0.5F};
    const auto& orientation = input.vehicle_orientation;
    const std::array<float, 3> world_offset{{
        orientation[0] * local_offset[0] +
            orientation[3] * local_offset[1] +
            orientation[6] * local_offset[2],
        orientation[4] * local_offset[1] +
            orientation[7] * local_offset[2] +
            orientation[1] * local_offset[0],
        orientation[8] * local_offset[2] +
            orientation[2] * local_offset[0] +
            orientation[5] * local_offset[1],
    }};

    for (std::size_t i = 0; i < orientation.size(); ++i) {
        output.transform[i] = orientation[i];
    }
    for (std::size_t i = 0; i < world_offset.size(); ++i) {
        output.transform[orientation.size() + i] =
            input.camera_position_before_mode[i] + world_offset[i];
    }
    output.velocity = input.vehicle_velocity;
    return true;
}

bool prepare_race_camera_mode_3_output(
    const RaceCameraModeState& mode,
    const RaceCameraMode3Input& input,
    RaceCameraMode3Output& output) noexcept
{
    if (mode.raw_mode_0x508758 != 3) {
        return false;
    }

    constexpr float half_turn = 3.1415927F;
    const auto orientation = multiply(
        rotation_y(half_turn), input.vehicle_orientation);
    constexpr float local_y_offset = 0.8F;
    const std::array<float, 3> world_offset{{
        orientation[3] * local_y_offset,
        orientation[4] * local_y_offset,
        orientation[5] * local_y_offset,
    }};

    for (std::size_t i = 0; i < orientation.size(); ++i) {
        output.transform[i] = orientation[i];
    }
    for (std::size_t i = 0; i < world_offset.size(); ++i) {
        output.transform[orientation.size() + i] =
            input.camera_position_before_mode[i] + world_offset[i];
    }
    output.velocity = input.vehicle_velocity;
    return true;
}

bool update_race_camera_look_at_basis(
    RaceCameraLookAtState& state,
    const std::array<float, 3>& target) noexcept
{
    const std::array<float, 3> delta{{
        target[0] - state.position[0],
        target[1] - state.position[1],
        target[2] - state.position[2],
    }};
    const auto distance_squared = delta[0] * delta[0] +
        delta[1] * delta[1] + delta[2] * delta[2];
    if (!(1.0F <= distance_squared &&
          (0.01F <= std::abs(delta[0]) || 0.01F <= std::abs(delta[2])))) {
        return false;
    }
    build_look_at_basis(state.orientation, delta);
    return true;
}

bool prepare_race_camera_modes_4_to_7_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes4To7Input& input,
    RaceCameraModes4To7Output& output) noexcept
{
    const auto camera_mode = mode.raw_mode_0x508758;
    if (camera_mode < 4 || camera_mode > 7) {
        return false;
    }

    RaceCameraLookAtState camera{};
    for (std::size_t i = 0; i < camera.orientation.size(); ++i) {
        camera.orientation[i] = input.cached_transform[i];
    }
    for (std::size_t i = 0; i < camera.position.size(); ++i) {
        camera.position[i] = input.cached_transform[9 + i];
    }

    const auto height = camera_mode == 6 ? 3.7F : 1.5F;
    const std::array<float, 3> elevated_target{{
        input.camera_anchor_position[0],
        input.camera_anchor_position[1] + height,
        input.camera_anchor_position[2],
    }};
    (void)update_race_camera_look_at_basis(camera, elevated_target);

    const auto speed_squared = input.vehicle_velocity[0] * input.vehicle_velocity[0] +
        input.vehicle_velocity[2] * input.vehicle_velocity[2] +
        input.vehicle_velocity[1] * input.vehicle_velocity[1];
    auto velocity_direction = input.vehicle_velocity;
    const bool speed_below_one = !(1.0F <= speed_squared);
    if (!speed_below_one) {
        const auto speed = std::sqrt(static_cast<long double>(speed_squared));
        for (auto& component : velocity_direction) {
            component = static_cast<float>(component / speed);
        }
    }

    if (input.global_0x508760_is_zero) {
        const auto& basis = camera.orientation;
        const auto velocity_dot =
            basis[6] * velocity_direction[0] +
            basis[8] * velocity_direction[2] +
            basis[7] * velocity_direction[1];
        const auto lateral_velocity_dot =
            basis[0] * velocity_direction[0] +
            basis[2] * velocity_direction[2] +
            basis[1] * velocity_direction[1];
        float acceleration_scale{};
        if (speed_below_one || 0.0F <= velocity_dot) {
            acceleration_scale = -0.144F * lateral_velocity_dot;
        } else {
            const auto direction_sign =
                lateral_velocity_dot <= 0.0F ? -1.0F : 1.0F;
            acceleration_scale = direction_sign * -0.144F;
        }
        camera.position[0] += basis[0] * acceleration_scale;
        camera.position[1] += basis[1] * acceleration_scale;
        camera.position[2] += basis[2] * acceleration_scale;
    }

    (void)update_race_camera_look_at_basis(camera, elevated_target);
    const auto distance = camera_mode == 4 ? -6.0F
        : camera_mode == 6 ? -18.0F : -10.0F;
    camera.position = {
        distance * camera.orientation[6] + elevated_target[0],
        distance * camera.orientation[7] + elevated_target[1],
        distance * camera.orientation[8] + elevated_target[2],
    };

    auto write_transform = [&camera](std::array<float, 12>& transform) {
        for (std::size_t i = 0; i < camera.orientation.size(); ++i) {
            transform[i] = camera.orientation[i];
        }
        for (std::size_t i = 0; i < camera.position.size(); ++i) {
            transform[9 + i] = camera.position[i];
        }
    };
    write_transform(output.cached_transform);
    output.mirrored_camera_branch = camera_mode == 7 || input.vehicle_flag_0x4ec;
    if (output.mirrored_camera_branch) {
        const auto cached_position = camera.position;
        camera.position = {
            input.camera_anchor_position[0] * 2.0F - cached_position[0],
            input.camera_anchor_position[1] * 2.0F - cached_position[1] +
                (camera_mode == 6 ? 8.0F : 3.0F),
            input.camera_anchor_position[2] * 2.0F - cached_position[2],
        };
        (void)update_race_camera_look_at_basis(camera,
                                               input.camera_anchor_position);
    }
    write_transform(output.transform);
    output.velocity = input.vehicle_velocity;
    return true;
}

bool prepare_race_camera_modes_10_or_12_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes10And12Input& input,
    RaceCameraModes10And12Output& output) noexcept
{
    const auto camera_mode = mode.raw_mode_0x508758;
    if (camera_mode != 10 && camera_mode != 12) {
        return false;
    }
    const bool has_multiple_selectors = input.selector_count_0x5081dc > 1;
    const auto selector_count = static_cast<std::size_t>(
        input.selector_count_0x5081dc > 0 ? input.selector_count_0x5081dc : 0);
    if (has_multiple_selectors &&
        selector_count > input.camera_records_0x507e10.size()) {
        return false;
    }
    if (has_multiple_selectors && camera_mode == 10 &&
        selector_count > input.target_candidates_0x507e2c.size()) {
        return false;
    }

    RaceCameraLookAtState camera{};
    for (std::size_t i = 0; i < camera.orientation.size(); ++i) {
        camera.orientation[i] = input.transform_before_branch[i];
    }
    for (std::size_t i = 0; i < camera.position.size(); ++i) {
        camera.position[i] = input.transform_before_branch[9 + i];
    }

    auto selected_record_index = input.selected_record_index_0x4d488c;
    auto manual_camera_transform = input.manual_camera_transform_before;
    bool preset_record_selected = false;
    bool manual_camera_transform_written = false;
    if (has_multiple_selectors) {
        std::size_t selected = 0;
        if (camera_mode == 10) {
            selected = 1;
            float nearest_squared = 400000000.0F;
            for (std::size_t i = 1; i < selector_count; ++i) {
                const auto dx =
                    input.target_candidates_0x507e2c[i][0] -
                    input.camera_anchor_position[0];
                const auto dy =
                    input.target_candidates_0x507e2c[i][1] -
                    input.camera_anchor_position[1];
                const auto dz =
                    input.target_candidates_0x507e2c[i][2] -
                    input.camera_anchor_position[2];
                const auto distance_squared =
                    dx * dx + dz * dz + dy * dy;
                if (distance_squared < nearest_squared) {
                    selected = i;
                    nearest_squared = distance_squared;
                }
            }
        }

        const auto& record = input.camera_records_0x507e10[selected];
        camera.position = record.position;
        const std::array<float, 3> elevated_target{{
            input.camera_anchor_position[0],
            input.camera_anchor_position[1] + 1.0F,
            input.camera_anchor_position[2],
        }};
        if (record.raw_word_0x00 == 0) {
            camera.orientation = preset_axis_angle_rotation(record);
        } else {
            std::array<float, 3> direction{{
                elevated_target[0] - camera.position[0],
                elevated_target[1] - camera.position[1],
                elevated_target[2] - camera.position[2],
            }};
            constexpr float vertical_epsilon = 1.1920929e-07F;
            if (std::abs(direction[0]) <= vertical_epsilon &&
                std::abs(direction[2]) <= vertical_epsilon) {
                direction[0] = input.vehicle_transform_0x38[6];
                direction[2] = input.vehicle_transform_0x38[8];
            }
            build_look_at_basis(camera.orientation, direction);
        }

        if (record.raw_word_0x00 == 1) {
            const auto distance = record.raw_scalar_0x10;
            camera.position[0] += camera.orientation[6] * distance;
            camera.position[1] += camera.orientation[7] * distance;
            camera.position[2] += camera.orientation[8] * distance;
        } else if (record.raw_word_0x00 == 2) {
            const auto projected_distance =
                (elevated_target[0] - camera.position[0]) * camera.orientation[6] +
                (elevated_target[1] - camera.position[1]) * camera.orientation[7] +
                (elevated_target[2] - camera.position[2]) * camera.orientation[8];
            const auto distance =
                (projected_distance - record.raw_scalar_0x14) *
                record.raw_scalar_0x10;
            camera.position[0] += camera.orientation[6] * distance;
            camera.position[1] += camera.orientation[7] * distance;
            camera.position[2] += camera.orientation[8] * distance;
        }
        selected_record_index = static_cast<std::int32_t>(selected);
        preset_record_selected = true;
        manual_camera_transform_written = true;
    } else {
        std::array<float, 3> direction{{
            input.camera_anchor_position[0] - camera.position[0],
            input.camera_anchor_position[1] - camera.position[1],
            input.camera_anchor_position[2] - camera.position[2],
        }};
        constexpr float vertical_epsilon = 1.1920929e-07F;
        if (std::abs(direction[0]) <= vertical_epsilon &&
            std::abs(direction[2]) <= vertical_epsilon) {
            direction[0] = input.vehicle_transform_0x38[6];
            direction[2] = input.vehicle_transform_0x38[8];
        }
        build_look_at_basis(camera.orientation, direction);
    }

    std::array<float, 12> transform{};
    for (std::size_t i = 0; i < camera.orientation.size(); ++i) {
        transform[i] = camera.orientation[i];
    }
    for (std::size_t i = 0; i < camera.position.size(); ++i) {
        transform[9 + i] = camera.position[i];
    }
    if (manual_camera_transform_written) {
        manual_camera_transform = transform;
    }
    output.transform = transform;
    output.velocity = {};
    output.manual_camera_transform = manual_camera_transform;
    output.selected_record_index_0x4d488c = selected_record_index;
    output.preset_record_selected = preset_record_selected;
    output.manual_camera_transform_written = manual_camera_transform_written;
    return true;
}

void apply_race_camera_postprocess(
    RaceCameraPublishedState& destination,
    const std::int32_t selected_car_index,
    const std::array<float, 12>& transform,
    const std::array<float, 3>& velocity) noexcept
{
    destination.update_flag_0x50 = -1;
    destination.selected_car_index_0x54 = selected_car_index;
    destination.transform_0x58 = transform;
    destination.velocity_0x88[0] = velocity[0];
    destination.velocity_0x88[1] = velocity[1];
    destination.update_flag_0x50 = 0;
    destination.velocity_0x88[2] = velocity[2];
}

RaceCameraPresetCaptureStatus capture_race_camera_preset(
    const RaceCameraControlState& camera,
    const std::span<RaceCameraPresetRecord> presets,
    const std::int32_t active_index,
    const std::int32_t camera_count) noexcept
{
    if (active_index < 0 || active_index >= camera_count) {
        return RaceCameraPresetCaptureStatus::camera_index_out_of_range;
    }
    const auto index = static_cast<std::size_t>(active_index);
    if (index >= presets.size()) {
        return RaceCameraPresetCaptureStatus::record_unavailable;
    }

    auto& record = presets[index];
    record.position = camera.position;
    if (record.raw_word_0x00 != 0) {
        return RaceCameraPresetCaptureStatus::position_only;
    }

    constexpr long double radians_to_degrees = 57.295780181884765625L;
    record.raw_scalar_0x10 = static_cast<float>(
        std::atan2(static_cast<long double>(camera.orientation[8]),
                   static_cast<long double>(camera.orientation[6])) *
        radians_to_degrees);
    record.raw_scalar_0x14 = static_cast<float>(
        std::atan2(static_cast<long double>(camera.orientation[7]),
                   static_cast<long double>(camera.orientation[4])) *
        radians_to_degrees);
    return RaceCameraPresetCaptureStatus::position_and_angles;
}

RaceCameraControlResult update_race_camera_control(
    RaceCameraControlState& state,
    const RaceCameraKeyState& keys) noexcept
{
    for (auto& velocity : state.translation_velocity) {
        velocity *= 0.7F;
    }
    for (auto& velocity : state.rotation_velocity) {
        velocity *= 0.7F;
    }

    const auto speed_mode = static_cast<std::uint8_t>(
        (key_down(keys, 0xfdU) ? 2U : 0U) |
        (key_down(keys, 0xfeU) ? 1U : 0U));
    const auto move_step = translation_speed[speed_mode];
    const auto turn_step = rotation_speed[speed_mode];

    std::array<float, 3> requested_translation{};
    if (key_down(keys, 0x51U)) {
        requested_translation[0] = move_step;
    } else if (key_down(keys, 0x4fU)) {
        requested_translation[0] = -move_step;
    }
    if (key_down(keys, 0x53U)) {
        requested_translation[1] = -move_step * 0.5F;
    } else if (key_down(keys, 0x52U)) {
        requested_translation[1] = move_step * 0.5F;
    }
    if (key_down(keys, 0x1eU)) {
        requested_translation[2] = move_step;
    } else if (key_down(keys, 0x2cU)) {
        requested_translation[2] = -move_step;
    }
    if (requested_translation[0] != 0.0F ||
        requested_translation[1] != 0.0F ||
        requested_translation[2] != 0.0F) {
        state.translation_velocity = requested_translation;
    }

    std::array<float, 3> requested_rotation{};
    if (key_down(keys, 0x48U)) {
        requested_rotation[0] = turn_step * 0.5F;
    } else if (key_down(keys, 0x50U)) {
        requested_rotation[0] = -turn_step * 0.5F;
    }
    if (key_down(keys, 0x4dU)) {
        requested_rotation[1] = key_down(keys, 0x4fU)
            ? turn_step * 0.5F : turn_step;
    } else if (key_down(keys, 0x4bU)) {
        requested_rotation[1] = key_down(keys, 0x51U)
            ? -turn_step * 0.5F : -turn_step;
    }
    if (key_down(keys, 0x47U)) {
        requested_rotation[2] = turn_step;
    } else if (key_down(keys, 0x49U)) {
        requested_rotation[2] = -turn_step;
    }
    if (requested_rotation[0] != 0.0F ||
        requested_rotation[1] != 0.0F ||
        requested_rotation[2] != 0.0F) {
        state.rotation_velocity = requested_rotation;
    }

    const auto& orientation = state.orientation;
    state.position[0] += state.translation_velocity[0] * orientation[0] +
        state.translation_velocity[1] * orientation[3] +
        state.translation_velocity[2] * orientation[6];
    state.position[1] += state.translation_velocity[0] * orientation[1] +
        state.translation_velocity[1] * orientation[4] +
        state.translation_velocity[2] * orientation[7];
    state.position[2] += state.translation_velocity[0] * orientation[2] +
        state.translation_velocity[1] * orientation[5] +
        state.translation_velocity[2] * orientation[8];

    const auto x_rotation = rotation_x(state.rotation_velocity[0]);
    const auto y_rotation = rotation_y(state.rotation_velocity[1]);
    const auto z_rotation = rotation_z(state.rotation_velocity[2]);
    state.orientation = multiply(
        multiply(z_rotation, multiply(y_rotation, x_rotation)),
        state.orientation);

    RaceCameraControlResult result{speed_mode, false};
    if (key_down(keys, 0x10U)) {
        if (!state.preset_capture_latch) {
            state.preset_capture_latch = true;
            result.preset_capture_requested = true;
        }
    } else {
        state.preset_capture_latch = false;
    }
    return result;
}

} // namespace viper
