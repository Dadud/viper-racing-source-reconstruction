#include "viper/wheel_body_coupling.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <span>

namespace {
void require(const bool condition,
    const std::source_location location = std::source_location::current())
{
    if (!condition) {
        std::fprintf(stderr, "wheel collision regression failed at line %u\n",
            location.line());
        std::abort();
    }
}

viper::WheelCollisionImpulseInput make_input()
{
    viper::WheelCollisionImpulseInput input{};
    input.body_center = {0.0F, 0.0F, 0.0F};
    input.contact_point = {1.0F, 0.0F, 0.0F};
    input.body_velocity = {0.0F, -1.0F, 0.0F};
    input.contact_normal = {0.0F, 1.0F, 0.0F};
    input.body_mass = 1.0F;
    input.angular_response_matrix = {1.0F, 0.0F, 0.0F,
                                     0.0F, 1.0F, 0.0F,
                                     0.0F, 0.0F, 1.0F};
    input.contact_depth = 0.1F;
    input.contact_threshold_response_0x28 = 0.5F;
    return input;
}
}

int main()
{
    require(viper::ncar_body_contact_query_gate_passed(0x3f800000U));
    require(viper::ncar_body_contact_query_gate_passed(0x00000001U));
    require(!viper::ncar_body_contact_query_gate_passed(0x00000000U));
    require(!viper::ncar_body_contact_query_gate_passed(0x80000000U));
    require(!viper::ncar_body_contact_query_gate_passed(0xbf800000U));

    viper::NcarIntegratorSignalState precision_state{};
    precision_state.value_bits_0xc7c = 0x3e947f4cU;
    precision_state.limit_scale_0xc8c = 100.0F;
    precision_state.callback_scale_0xc90 = 1.0F;
    precision_state.smoothed_value_0xc94 = std::bit_cast<float>(0x40a0ec07U);
    viper::NcarIntegratorSignalInput precision_input{};
    precision_input.callback_value_0xc84 = std::bit_cast<float>(0x40850485U);
    const auto precision_signal = viper::advance_ncar_integrator_signal_control(
        precision_state, precision_input, 0U);
    require(std::bit_cast<std::uint32_t>(precision_signal.paired_setter_value) ==
        0x3ed2f414U &&
        std::bit_cast<std::uint32_t>(precision_signal.paired_setter_opposite) ==
        0xbed2f414U);
    std::array<std::byte, 0x6cU> signal_runtime_record{};
    constexpr std::uint32_t signal_source_bits = 0x40000000U; // 2.0f
    for (std::size_t byte = 0U; byte < sizeof(signal_source_bits); ++byte) {
        signal_runtime_record[0x68U + byte] = static_cast<std::byte>(
            (signal_source_bits >> (byte * 8U)) & 0xffU);
    }
    const auto signal_constructor_state =
        viper::make_ncar_integrator_signal_state_from_runtime_record(
            signal_runtime_record);
    require(signal_constructor_state.has_value());
    require(signal_constructor_state->value_bits_0xc7c == 0U);
    require(signal_constructor_state->limit_scale_0xc8c == 8.0F);
    require(signal_constructor_state->callback_scale_0xc90 == 0.02F);
    require(signal_constructor_state->smoothed_value_0xc94 == 0.0F);
    std::array<std::byte, 0x148U> wheel_setup_record{};
    const auto write_setup_float = [&](const std::size_t offset, const float value) {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        for (std::size_t i = 0U; i < 4U; ++i)
            wheel_setup_record[offset + i] = std::byte((bits >> (8U * i)) & 0xffU);
    };
    write_setup_float(0x130U, 200.0F);
    write_setup_float(0x134U, 50.0F);
    write_setup_float(0x138U, 16.0F);
    write_setup_float(0x13cU, 240.0F);
    write_setup_float(0x140U, 40.0F);
    write_setup_float(0x144U, 18.0F);
    for (std::size_t wheel = 0U; wheel < 4U; ++wheel) {
        const auto value = viper::derive_ncar_wheel_scalar_0x5c(wheel_setup_record, wheel);
        require(value.has_value());
        require(std::abs(*value - (wheel < 2U ? 0.3032F : 0.3246F)) < 1e-6F);
    }
    require(!viper::derive_ncar_wheel_scalar_0x5c(wheel_setup_record, 4U));
    require(!viper::derive_ncar_wheel_scalar_0x5c(
        std::span<const std::byte>(wheel_setup_record.data(), 0x147U), 0U));
    const auto front_setup = viper::derive_ncar_wheel_scalar_0x5c(wheel_setup_record, 0U);
    const auto rear_setup = viper::derive_ncar_wheel_scalar_0x5c(wheel_setup_record, 2U);
    require(front_setup && rear_setup);
    const auto source_multiplier = viper::derive_ncar_integrator_signal_multiplier(
        *front_setup, *rear_setup, 0.25F, 2.0F,
        std::bit_cast<float>(0x3dd67750U));
    require(source_multiplier && std::abs(*source_multiplier -
        ((0.3032F * 0.75F + 0.3246F * 0.25F) / 2.0F *
            std::bit_cast<float>(0x3dd67750U))) < 1e-6F);
    const auto signal_multiplier =
        viper::derive_ncar_integrator_signal_multiplier(
            2.0F, 10.0F, 0.25F, 2.0F, 3.0F);
    require(signal_multiplier.has_value());
    require(std::abs(*signal_multiplier - 6.0F) < 1e-6F);
    const auto signal_multiplier_at_first_endpoint =
        viper::derive_ncar_integrator_signal_multiplier(
            2.0F, 10.0F, 0.0F, 2.0F, 3.0F);
    require(signal_multiplier_at_first_endpoint.has_value());
    require(std::abs(*signal_multiplier_at_first_endpoint - 3.0F) < 1e-6F);
    const auto signal_multiplier_at_second_endpoint =
        viper::derive_ncar_integrator_signal_multiplier(
            2.0F, 10.0F, 1.0F, 2.0F, 3.0F);
    require(signal_multiplier_at_second_endpoint.has_value());
    require(std::abs(*signal_multiplier_at_second_endpoint - 15.0F) < 1e-6F);
    require(!viper::derive_ncar_integrator_signal_multiplier(
        2.0F, 10.0F, 0.25F, 0.0F, 3.0F));
    require(!viper::derive_ncar_integrator_signal_multiplier(
        std::numeric_limits<float>::quiet_NaN(), 10.0F, 0.25F, 2.0F, 3.0F));
    require(!viper::derive_ncar_integrator_signal_multiplier(
        2.0F, 10.0F, 0.25F, 2.0F,
        std::numeric_limits<float>::infinity()));
    const auto controller_enabled = viper::project_ncar_controller_setter(
        {true, 10.0F, 2.0F, 3.0F, 7.0F, 4.0F});
    require(controller_enabled && controller_enabled->observed_value_0x28 == 20.0F &&
        controller_enabled->stored_value_0x10 == 20.0F &&
        controller_enabled->selected_pair_argument == -112.0F &&
        !controller_enabled->disabled_integrator_required);
    const auto controller_clamped = viper::project_ncar_controller_setter(
        {true, 1000.0F, 1.0F, 1.0F, 0.0F, 5.0F});
    require(controller_clamped && controller_clamped->selected_pair_argument == -395.0F);
    const auto controller_disabled = viper::project_ncar_controller_setter(
        {false, 0.0F, 0.0F, 0.0F, 7.0F, 4.0F});
    require(controller_disabled && controller_disabled->observed_value_0x28 == 7.0F &&
        controller_disabled->selected_pair_argument == 0.0F &&
        controller_disabled->disabled_integrator_required);
    require(!viper::project_ncar_controller_setter(
        {true, std::numeric_limits<float>::infinity(), 1.0F, 1.0F, 0.0F, 0.0F}));
    const auto disabled_update = viper::project_ncar_disabled_controller_integrator(
        {2.0F, 0.5F, 4.0F, 5.0F, 3.0F, {0.0F, 1.0F, 0.0F}});
    require(disabled_update && std::abs(disabled_update->value_0x10 - 3.22231F) < 1e-4F &&
        std::abs(disabled_update->body_moment_delta[1] - 40.0F) < 1e-3F &&
        disabled_update->body_moment_delta[0] == 0.0F &&
        disabled_update->body_moment_delta[2] == 0.0F);
    const auto disabled_equilibrium = viper::project_ncar_disabled_controller_integrator(
        {2.0F, 0.5F, 4.0F, 5.0F, 1.0F, {1.0F, 2.0F, 3.0F}});
    require(disabled_equilibrium && disabled_equilibrium->value_0x10 == 2.0F &&
        disabled_equilibrium->body_moment_delta == std::array<float, 3>{});
    require(!viper::project_ncar_disabled_controller_integrator(
        {0.0F, 0.0F, 1.0F, 1.0F, 0.0F,
         {0.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F}}));
    viper::NcarCurveSetterInput curve_step{};
    curve_step.prefix.getter_value_0x10 = 500.0F;
    curve_step.prefix.curve.mode_0x40 = 1U;
    curve_step.prefix.curve.mode_0x41 = 1U;
    curve_step.prefix.curve.cutoff_0x3c = 1000.0F;
    curve_step.prefix.setter_argument = 10.0F;
    curve_step.inverse_scale_0x0c = 1.0F;
    curve_step.body_scale_0x08 = 5.0F;
    curve_step.body_axis_0x50 = {0.0F, 1.0F, 0.0F};
    curve_step.value_0x24 = 400.0F;
    curve_step.value_0x68 = 401.0F;
    const auto curve_result = viper::project_ncar_curve_setter(curve_step);
    require(curve_result && curve_result->prefix.raw_curve_output == 80.0F &&
        curve_result->integrator.value_0x10 > 500.0F &&
        curve_result->integrator.body_moment_delta[1] > 0.0F &&
        curve_result->tail.accumulator_0x28 > 160.0F &&
        std::abs(curve_result->tail.accumulator_0x6c + 160.0F) < 1e-4F &&
        curve_result->tail.mode_0x40 == 0U && curve_result->tail.mode_0x41 == 0U);
    curve_step.body_axis_0x50[0] = std::numeric_limits<float>::quiet_NaN();
    require(!viper::project_ncar_curve_setter(curve_step));
    viper::NcarBodyRuntimeObjectState curve_body{};
    const auto empty_curve_body = curve_body;
    require(!viper::advance_ncar_curve_setter_in_body(curve_body, 10.0F) &&
        curve_body.bytes == empty_curve_body.bytes &&
        curve_body.initialized_bytes == empty_curve_body.initialized_bytes);
    constexpr std::array<std::uint32_t, 25> curve_offsets{{
        0xc04U, 0xc08U, 0xc0cU, 0xc10U, 0xc20U, 0xc24U, 0xc28U,
        0xc2cU, 0xc30U, 0xc34U, 0xc38U, 0xc40U, 0xc44U, 0xc48U,
        0xc4cU, 0xc50U, 0xc54U, 0xc64U, 0xc68U, 0x50U, 0x54U,
        0x58U, 0x258U, 0x25cU, 0x260U}};
    std::array<viper::NcarObjectFieldWrite, 25> curve_zero_writes{};
    for (std::size_t i = 0; i < curve_offsets.size(); ++i)
        curve_zero_writes[i] = {curve_offsets[i], 4U, 0U};
    const std::array<viper::NcarObjectFieldWrite, 10> curve_values{{
        {0xc04U, 4U, std::bit_cast<std::uint32_t>(5.0F)},
        {0xc08U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xc0cU, 4U, std::bit_cast<std::uint32_t>(500.0F)},
        {0xc20U, 4U, std::bit_cast<std::uint32_t>(400.0F)},
        {0xc38U, 4U, std::bit_cast<std::uint32_t>(1000.0F)},
        {0xc64U, 4U, std::bit_cast<std::uint32_t>(401.0F)},
        {0x54U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0x25cU, 4U, std::bit_cast<std::uint32_t>(7.0F)},
        {0xc3cU, 1U, 1U}, {0xc3dU, 1U, 1U}}};
    require(viper::apply_ncar_body_object_writes(curve_body, curve_zero_writes) &&
        viper::apply_ncar_body_object_writes(curve_body, curve_values));
    const auto body_result = viper::advance_ncar_curve_setter_in_body(curve_body, 10.0F);
    require(body_result &&
        viper::read_ncar_body_object_u32(curve_body, 0xc0cU) ==
            std::bit_cast<std::uint32_t>(body_result->integrator.value_0x10) &&
        viper::read_ncar_body_object_u32(curve_body, 0xc24U) ==
            std::bit_cast<std::uint32_t>(body_result->tail.accumulator_0x28) &&
        std::abs(std::bit_cast<float>(*viper::read_ncar_body_object_u32(curve_body, 0x25cU)) -
            (7.0F + body_result->integrator.body_moment_delta[1])) < 1e-4F &&
        viper::read_ncar_body_object_u8(curve_body, 0xc3cU) == 0U &&
        viper::read_ncar_body_object_u8(curve_body, 0xc3dU) == 0U);
    const auto saved_curve_body = curve_body;
    require(!viper::advance_ncar_curve_setter_in_body(curve_body,
        std::numeric_limits<float>::infinity()) &&
        curve_body.bytes == saved_curve_body.bytes &&
        curve_body.initialized_bytes == saved_curve_body.initialized_bytes);
    require(!viper::make_ncar_integrator_signal_state_from_runtime_record(
        std::span<const std::byte>(signal_runtime_record.data(), 0x6bU)));
    signal_runtime_record[0x68U] = std::byte{0U};
    signal_runtime_record[0x69U] = std::byte{0U};
    signal_runtime_record[0x6aU] = std::byte{0xc0U};
    signal_runtime_record[0x6bU] = std::byte{0x7fU};
    require(!viper::make_ncar_integrator_signal_state_from_runtime_record(
        signal_runtime_record));

    auto input = make_input();
    auto response = viper::prepare_wheel_collision_impulse(input);
    require(response.applied);
    require(std::abs(response.normal_response - 0.5F) < 1e-6F);
    require(std::abs(response.duration - 100.0F) < 1e-5F);

    viper::NcarBodyRuntimeObjectState collision_body{};
    std::array<viper::NcarObjectFieldWrite, 13> collision_body_fields{};
    for (std::size_t i = 0; i < 3U; ++i)
        collision_body_fields[i] = {0x5cU + static_cast<std::uint32_t>(i * 4U), 4U, 0U};
    collision_body_fields[3] = {0x1f0U, 4U, std::bit_cast<std::uint32_t>(1.0F)};
    for (std::size_t i = 0; i < 9U; ++i)
        collision_body_fields[4U + i] = {0x264U + static_cast<std::uint32_t>(i * 4U), 4U,
            std::bit_cast<std::uint32_t>(i % 4U == 0U ? 1.0F : 0.0F)};
    require(viper::apply_ncar_body_object_writes(collision_body, collision_body_fields));
    viper::NcarWheelRuntimeObjectState collision_wheel{};
    const std::array<viper::NcarObjectFieldWrite, 4> collision_wheel_fields{{
        {0x554U + 0xc8U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0x554U + 0xccU, 4U, 0U}, {0x554U + 0xd0U, 4U, 0U},
        {0x554U + 0x20U, 1U, 0U}}};
    require(viper::apply_ncar_wheel_object_writes(collision_wheel, 0x554U, collision_wheel_fields));
    const auto rotating_collision = viper::make_ncar_wheel_collision_impulse_input_from_objects(
        collision_body, collision_wheel, {0.0F, -2.0F, 0.0F},
        {0.0F, 1.0F, 0.0F}, 0.1F, 0.5F, false);
    require(rotating_collision && rotating_collision->body_velocity.y == -2.0F &&
        rotating_collision->contact_point.x == 1.0F &&
        viper::prepare_wheel_collision_impulse(*rotating_collision).applied);
    require(!viper::make_ncar_wheel_collision_impulse_input_from_objects(
        collision_body, collision_wheel,
        {0.0F, std::numeric_limits<float>::quiet_NaN(), 0.0F},
        {0.0F, 1.0F, 0.0F}, 0.1F, 0.5F, false));

    viper::NcarPointImpulseAccumulator accumulator{};
    std::uint8_t damage_flag = 0U;
    auto application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.point_impulse_accumulated);
    require(!application.damage_gate_input_unresolved);
    require(std::abs(accumulator.impulse_time_0x294[1] - 50.0F) < 1e-4F);
    require(std::abs(accumulator.duration_0x2ac - 100.0F) < 1e-5F);

    // The high-response hook is reached, but an unknown native global cannot
    // authorize a byte write in the portable model.
    input.body_velocity.y = -10000.0F;
    accumulator = {};
    damage_flag = 0U;
    application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.response.damage_hook_reached);
    require(application.damage_gate_input_unresolved);
    require(!application.damage_flag_written);
    require(damage_flag == 0U);
    require(application.point_impulse_accumulated);

    input.query_flag_nonzero_0x50878c = false;
    accumulator = {};
    application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.response.damage_gate_input_known);
    require(!application.damage_gate_input_unresolved);
    require(!application.damage_flag_written);
    require(damage_flag == 0U);

    input.query_flag_nonzero_0x50878c = true;
    accumulator = {};
    application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.response.damage_gate_input_known);
    require(application.damage_flag_written);
    require(damage_flag == 1U);

    viper::NcarSelectedWheelPairTargetInput pair_input{};
    pair_input.value_0x90 = 2.0F;
    pair_input.selector_value_bits_0xb4 = 0x3f000000U;
    pair_input.value_0xb8 = 1.0F;
    pair_input.value_0xbc = 2.0F;
    pair_input.value_0xc0 = 1.5F;
    pair_input.controller_setter_argument = 0.4F;
    pair_input.wheel_spin_0x128 = {2.0F, 4.0F, 6.0F, 8.0F};
    auto selected_pair =
        viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair.has_value());
    require(selected_pair->selected_target == 1U);
    require(std::abs(selected_pair->getter_d24 - 6.0F) < 1e-6F);
    require(std::abs(selected_pair->getter_d44 - 14.0F) < 1e-6F);
    require(std::abs(selected_pair->getter_d04 - 10.0F) < 1e-6F);
    const auto selected_pair_ddc =
        viper::derive_ncar_selected_pair_value_0xddc(*selected_pair, 0.5F);
    require(selected_pair_ddc && std::abs(*selected_pair_ddc - 5.0F) < 1e-6F);
    require(!viper::derive_ncar_selected_pair_value_0xddc(
        *selected_pair, std::numeric_limits<float>::infinity()));
    require(selected_pair->wheel_setter_reached ==
        std::array<bool, 4>{true, true, true, true});
    require(std::abs(selected_pair->wheel_value_0xe4[0] - 19.2015F) < 1e-4F &&
            std::abs(selected_pair->wheel_value_0xe4[1] - 13.7725F) < 1e-4F &&
            std::abs(selected_pair->wheel_value_0xe4[2] + 10.658F) < 1e-4F &&
            std::abs(selected_pair->wheel_value_0xe4[3] + 21.516F) < 1e-4F);

    // A value below the first raw boundary selects D24 and leaves the rear
    // wheel bytes untouched. The fixture uses the source constructor values,
    // including the exact 0x4C7EF4 scale bits.
    pair_input.selector_value_bits_0xb4 = 0x3c000000U;
    selected_pair =
        viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair.has_value() && selected_pair->selected_target == 0U);
    require(viper::derive_ncar_selected_pair_value_0xddc(*selected_pair, 0.5F) == 3.0F);
    require(selected_pair->wheel_setter_reached ==
        std::array<bool, 4>{true, true, false, false});

    pair_input.selector_value_bits_0xb4 = 0x3f800000U;
    selected_pair =
        viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair.has_value() && selected_pair->selected_target == 2U);
    require(viper::derive_ncar_selected_pair_value_0xddc(*selected_pair, 0.5F) == 7.0F);
    require(selected_pair->wheel_setter_reached ==
        std::array<bool, 4>{false, false, true, true});
    require(std::abs(selected_pair->wheel_value_0xe4[2] - 5.829F) < 1e-4F &&
            std::abs(selected_pair->wheel_value_0xe4[3] + 5.029F) < 1e-4F);

    pair_input.selector_value_bits_0xb4 = 0xbf800000U;
    selected_pair = viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair && selected_pair->selected_target == 0U);

    pair_input.selector_value_bits_0xb4 = 0x3c000000U;
    selected_pair =
        viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair.has_value() && selected_pair->selected_target == 0U);

    constexpr std::uint32_t first_wheel_offset = 0x554U;
    constexpr std::uint32_t wheel_stride = 0x1a4U;
    std::array<viper::NcarWheelRuntimeObjectState, 4> wheel_objects{};
    for (std::size_t wheel = 0; wheel < wheel_objects.size(); ++wheel) {
        const auto base = first_wheel_offset +
            wheel_stride * static_cast<std::uint32_t>(wheel);
        const viper::NcarObjectFieldWrite initial{
            base + 0xe4U, 4U, std::bit_cast<std::uint32_t>(99.0F)};
        require(viper::apply_ncar_wheel_object_writes(wheel_objects[wheel],
            base, std::span<const viper::NcarObjectFieldWrite>(&initial, 1U)));
    }
    require(viper::apply_ncar_selected_wheel_pair_target_to_wheels(
        wheel_objects, first_wheel_offset, *selected_pair));
    require(std::abs(std::bit_cast<float>(
        *viper::read_ncar_wheel_object_u32(wheel_objects[0], 0xe4U)) -
        selected_pair->wheel_value_0xe4[0]) < 1e-6F);
    require(std::abs(std::bit_cast<float>(
        *viper::read_ncar_wheel_object_u32(wheel_objects[1], 0xe4U)) -
        selected_pair->wheel_value_0xe4[1]) < 1e-6F);
    require(std::bit_cast<float>(
        *viper::read_ncar_wheel_object_u32(wheel_objects[2], 0xe4U)) == 99.0F);
    require(std::bit_cast<float>(
        *viper::read_ncar_wheel_object_u32(wheel_objects[3], 0xe4U)) == 99.0F);

    pair_input.selector_value_bits_0xb4 = 0x7fc00000U;
    selected_pair =
        viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair.has_value() && selected_pair->selected_target == 2U);

    std::array<std::byte, 0xc4U> runtime_record{};
    const auto write_record_u32 = [&runtime_record](const std::size_t offset,
                                        const std::uint32_t bits) {
        for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
            runtime_record[offset + byte] = static_cast<std::byte>(
                (bits >> (byte * 8U)) & 0xffU);
        }
    };
    write_record_u32(0x90U, std::bit_cast<std::uint32_t>(2.0F));
    write_record_u32(0xb4U, 0x3c000000U);
    write_record_u32(0xb8U, std::bit_cast<std::uint32_t>(1.0F));
    write_record_u32(0xbcU, std::bit_cast<std::uint32_t>(2.0F));
    write_record_u32(0xc0U, std::bit_cast<std::uint32_t>(1.5F));
    const auto record_input =
        viper::make_ncar_selected_wheel_pair_target_input_from_runtime_record(
            runtime_record, 0.4F, pair_input.wheel_spin_0x128);
    require(record_input.has_value() &&
            record_input->selector_value_bits_0xb4 == 0x3c000000U &&
            record_input->value_0x90 == 2.0F &&
            record_input->value_0xb8 == 1.0F &&
            record_input->value_0xbc == 2.0F &&
            record_input->value_0xc0 == 1.5F);
    require(!viper::make_ncar_selected_wheel_pair_target_input_from_runtime_record(
        std::span<const std::byte>(runtime_record.data(), 0xc3U), 0.4F,
        pair_input.wheel_spin_0x128));

    std::array<viper::NcarWheelRuntimeObjectState, 4> integrated_wheels{};
    for (std::size_t wheel = 0; wheel < integrated_wheels.size(); ++wheel) {
        const auto base = first_wheel_offset +
            wheel_stride * static_cast<std::uint32_t>(wheel);
        const std::array<viper::NcarObjectFieldWrite, 2> initial{
            viper::NcarObjectFieldWrite{
                base + 0xe4U, 4U, std::bit_cast<std::uint32_t>(99.0F)},
            viper::NcarObjectFieldWrite{
                base + 0x128U, 4U,
                std::bit_cast<std::uint32_t>(pair_input.wheel_spin_0x128[wheel])}};
        require(viper::apply_ncar_wheel_object_writes(integrated_wheels[wheel],
            base, initial));
    }
    const auto integrated_pair =
        viper::advance_ncar_selected_wheel_pair_target_in_wheels(
            runtime_record, integrated_wheels, first_wheel_offset, 0.4F);
    require(integrated_pair.has_value() &&
            integrated_pair->selected_target == 0U);
    require(std::abs(std::bit_cast<float>(
        *viper::read_ncar_wheel_object_u32(integrated_wheels[0], 0xe4U)) -
        integrated_pair->wheel_value_0xe4[0]) < 1e-6F);
    require(std::bit_cast<float>(
        *viper::read_ncar_wheel_object_u32(integrated_wheels[2], 0xe4U)) == 99.0F);

    viper::NcarBodyRuntimeObjectState controller_body{};
    const std::array<viper::NcarObjectFieldWrite, 11> controller_fields{{
        {0xcf0U, 1U, 1U},
        {0xca8U, 4U, std::bit_cast<std::uint32_t>(7.0F)},
        {0xcacU, 4U, std::bit_cast<std::uint32_t>(3.0F)},
        {0xcb0U, 4U, std::bit_cast<std::uint32_t>(2.0F)},
        {0xca0U, 4U, std::bit_cast<std::uint32_t>(5.0F)},
        {0xca4U, 4U, std::bit_cast<std::uint32_t>(4.0F)},
        {0x50U, 4U, 0U}, {0x54U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0x58U, 4U, 0U}, {0x258U, 4U, 0U}, {0x25cU, 4U, 0U}}};
    require(viper::apply_ncar_body_object_writes(controller_body, controller_fields));
    const viper::NcarObjectFieldWrite moment_z{0x260U, 4U, 0U};
    require(viper::apply_ncar_body_object_writes(controller_body,
        std::span<const viper::NcarObjectFieldWrite>(&moment_z, 1U)));
    auto controller_wheels = integrated_wheels;
    const auto enabled_step = viper::advance_ncar_controller_setter_in_body_and_wheels(
        controller_body, controller_wheels, runtime_record, first_wheel_offset, 4.0F);
    require(enabled_step && !enabled_step->integrator &&
        enabled_step->controller.observed_value_0x28 == 12.0F &&
        enabled_step->controller.selected_pair_argument == -64.0F &&
        viper::read_ncar_body_object_u32(controller_body, 0xca8U) ==
            std::bit_cast<std::uint32_t>(12.0F) &&
        viper::read_ncar_body_object_u32(controller_body, 0xcc0U) ==
            std::bit_cast<std::uint32_t>(12.0F) &&
        viper::read_ncar_wheel_object_u32(controller_wheels[0], 0xe4U) ==
            std::bit_cast<std::uint32_t>(enabled_step->pair.wheel_value_0xe4[0]) &&
        std::bit_cast<float>(*viper::read_ncar_wheel_object_u32(controller_wheels[2], 0xe4U)) == 99.0F);
    const viper::NcarObjectFieldWrite disable_controller{0xcf0U, 1U, 0U};
    require(viper::apply_ncar_body_object_writes(controller_body,
        std::span<const viper::NcarObjectFieldWrite>(&disable_controller, 1U)));
    const auto disabled_step = viper::advance_ncar_controller_setter_in_body_and_wheels(
        controller_body, controller_wheels, runtime_record, first_wheel_offset, 4.0F);
    require(disabled_step && disabled_step->integrator &&
        disabled_step->controller.observed_value_0x28 == 12.0F &&
        viper::read_ncar_body_object_u32(controller_body, 0xca8U) ==
            std::bit_cast<std::uint32_t>(disabled_step->integrator->value_0x10) &&
        viper::read_ncar_body_object_u32(controller_body, 0x25cU) ==
            std::bit_cast<std::uint32_t>(disabled_step->integrator->body_moment_delta[1]));
    const auto saved_controller_body = controller_body;
    const auto saved_controller_wheels = controller_wheels;
    require(!viper::advance_ncar_controller_setter_in_body_and_wheels(
        controller_body, controller_wheels, runtime_record, first_wheel_offset,
        std::numeric_limits<float>::infinity()) &&
        controller_body.bytes == saved_controller_body.bytes &&
        controller_body.initialized_bytes == saved_controller_body.initialized_bytes);
    for (std::size_t i = 0; i < controller_wheels.size(); ++i)
        require(controller_wheels[i].bytes == saved_controller_wheels[i].bytes &&
            controller_wheels[i].initialized_bytes == saved_controller_wheels[i].initialized_bytes);

    auto signal_body = saved_curve_body;
    require(viper::apply_ncar_body_object_writes(signal_body, controller_fields));
    const std::array<viper::NcarObjectFieldWrite, 5> signal_fields{{
        {0xc7cU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xc8cU, 4U, std::bit_cast<std::uint32_t>(1000.0F)},
        {0xc90U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xc94U, 4U, 0U}, {0x260U, 4U, 0U}}};
    require(viper::apply_ncar_body_object_writes(signal_body, signal_fields));
    auto signal_wheels = integrated_wheels;
    auto expected_body = signal_body;
    auto expected_wheels = signal_wheels;
    const float curve_getter = std::bit_cast<float>(
        *viper::read_ncar_body_object_u32(signal_body, 0xc0cU));
    viper::NcarIntegratorSignalState signal_state{};
    signal_state.value_bits_0xc7c = std::bit_cast<std::uint32_t>(1.0F);
    signal_state.limit_scale_0xc8c = 1000.0F;
    signal_state.callback_scale_0xc90 = 1.0F;
    viper::NcarIntegratorSignalInput signal_input{};
    signal_input.callback_value_0xc80 = curve_getter;
    signal_input.callback_value_0xc84 = 12.0F;
    const auto expected_signal = viper::advance_ncar_integrator_signal_control(
        signal_state, signal_input, 0U);
    const viper::NcarObjectFieldWrite expected_smoother{0xc94U, 4U,
        std::bit_cast<std::uint32_t>(signal_state.smoothed_value_0xc94)};
    require(viper::apply_ncar_body_object_writes(expected_body,
        std::span<const viper::NcarObjectFieldWrite>(&expected_smoother, 1U)) &&
        viper::advance_ncar_curve_setter_in_body(expected_body,
            expected_signal.paired_setter_value).has_value() &&
        viper::advance_ncar_controller_setter_in_body_and_wheels(expected_body,
            expected_wheels, runtime_record, first_wheel_offset,
            expected_signal.paired_setter_opposite).has_value());
    const auto signal_step = viper::advance_ncar_signal_pair_in_body_and_wheels(
        signal_body, signal_wheels, runtime_record, first_wheel_offset);
    require(signal_step && signal_body.bytes == expected_body.bytes &&
        signal_body.initialized_bytes == expected_body.initialized_bytes &&
        signal_step->signal.paired_setter_value == expected_signal.paired_setter_value);
    for (std::size_t i = 0; i < signal_wheels.size(); ++i)
        require(signal_wheels[i].bytes == expected_wheels[i].bytes &&
            signal_wheels[i].initialized_bytes == expected_wheels[i].initialized_bytes);
    const auto before_failure = signal_body;
    const auto wheels_before_failure = signal_wheels;
    require(!viper::advance_ncar_signal_pair_in_body_and_wheels(signal_body,
        signal_wheels, std::span<const std::byte>(runtime_record.data(), 0xc3U),
        first_wheel_offset) && signal_body.bytes == before_failure.bytes &&
        signal_body.initialized_bytes == before_failure.initialized_bytes);
    for (std::size_t i = 0; i < signal_wheels.size(); ++i)
        require(signal_wheels[i].bytes == wheels_before_failure[i].bytes &&
            signal_wheels[i].initialized_bytes == wheels_before_failure[i].initialized_bytes);

    auto selector_body = saved_curve_body;
    const std::array<viper::NcarObjectFieldWrite, 11> selector_fields{{
        {0xde0U, 4U, std::bit_cast<std::uint32_t>(0.75F)},
        {0xdecU, 4U, std::bit_cast<std::uint32_t>(0.5F)},
        {0xcb0U, 4U, std::bit_cast<std::uint32_t>(10.0F)},
        {0xcb4U, 4U, 1U}, {0xcbcU, 4U, 2U},
        {0xcc4U, 4U, std::bit_cast<std::uint32_t>(10.0F)},
        {0xcc8U, 4U, std::bit_cast<std::uint32_t>(20.0F)},
        {0xcf8U, 4U, 0U}, {0xd00U, 4U, 0U},
        {0x534U, 1U, 0U}, {0xc3dU, 1U, 0U}}};
    require(viper::apply_ncar_body_object_writes(selector_body, selector_fields));
    const auto selector_step = viper::advance_ncar_integrator_child_control_in_body(
        selector_body, integrated_wheels, runtime_record, first_wheel_offset);
    require(selector_step && selector_step->controller.input_complete &&
        selector_step->child.previous_index_0x1c == 2 &&
        selector_step->child.selected_value_0x18 == 20.0F &&
        viper::read_ncar_body_object_u32(selector_body, 0xcb0U) ==
            std::bit_cast<std::uint32_t>(20.0F) &&
        viper::read_ncar_body_object_u32(selector_body, 0xc28U) ==
            std::bit_cast<std::uint32_t>(0.75F) &&
        viper::read_ncar_body_object_u32(selector_body, 0xc7cU) ==
            std::bit_cast<std::uint32_t>(0.5F) &&
        viper::read_ncar_body_object_u8(selector_body, 0xcf0U) == 1U);

    // The native automatic shift enters state 1 on the gear-change tick,
    // then suppresses Engine throttle on following state-1 updates.
    auto automatic_shift_body = saved_curve_body;
    const std::array<viper::NcarObjectFieldWrite, 12> automatic_shift_fields{{
        {0xde0U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xdecU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xcb0U, 4U, std::bit_cast<std::uint32_t>(10.0F)},
        {0xcb4U, 4U, 1U}, {0xcbcU, 4U, 2U},
        {0xcc4U, 4U, std::bit_cast<std::uint32_t>(10.0F)},
        {0xcc8U, 4U, std::bit_cast<std::uint32_t>(20.0F)},
        {0xcf8U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xd00U, 4U, 0U}, {0x534U, 1U, 1U}, {0xc3dU, 1U, 0U},
        {0xc28U, 4U, std::bit_cast<std::uint32_t>(1.0F)}}};
    require(viper::apply_ncar_body_object_writes(
        automatic_shift_body, automatic_shift_fields));
    auto automatic_shift_wheels = integrated_wheels;
    const auto shift_entry = viper::advance_ncar_integrator_child_control_in_body(
        automatic_shift_body, automatic_shift_wheels, runtime_record,
        first_wheel_offset);
    require(shift_entry && shift_entry->child.animation_state_0x68 == 1 &&
        viper::read_ncar_body_object_u32(automatic_shift_body, 0xc28U) ==
            std::bit_cast<std::uint32_t>(1.0F));
    auto stalled_engine_body = automatic_shift_body;
    auto stalled_engine_wheels = automatic_shift_wheels;
    const viper::NcarObjectFieldWrite engine_stalled{0xc3dU, 1U, 1U};
    require(viper::apply_ncar_body_object_writes(
        stalled_engine_body,
        std::span<const viper::NcarObjectFieldWrite>(&engine_stalled, 1U)));
    const auto stalled_shift = viper::advance_ncar_integrator_child_control_in_body(
        stalled_engine_body, stalled_engine_wheels, runtime_record,
        first_wheel_offset);
    require(stalled_shift && stalled_shift->child.animation_state_0x68 == 1 &&
        viper::read_ncar_body_object_u32(stalled_engine_body, 0xc28U) ==
            std::bit_cast<std::uint32_t>(1.0F));
    const viper::NcarObjectFieldWrite engine_running{0xc3dU, 1U, 0U};
    require(viper::apply_ncar_body_object_writes(
        stalled_engine_body,
        std::span<const viper::NcarObjectFieldWrite>(&engine_running, 1U)));
    const auto resumed_shift = viper::advance_ncar_integrator_child_control_in_body(
        stalled_engine_body, stalled_engine_wheels, runtime_record,
        first_wheel_offset);
    require(resumed_shift && resumed_shift->child.animation_state_0x68 == 1 &&
        viper::read_ncar_body_object_u32(stalled_engine_body, 0xc28U) ==
            std::bit_cast<std::uint32_t>(0.0F));
    const auto shift_hold = viper::advance_ncar_integrator_child_control_in_body(
        automatic_shift_body, automatic_shift_wheels, runtime_record,
        first_wheel_offset);
    require(shift_hold && shift_hold->child.animation_state_0x68 == 1 &&
        viper::read_ncar_body_object_u32(automatic_shift_body, 0xc28U) ==
            std::bit_cast<std::uint32_t>(0.0F));

    const viper::NcarObjectFieldWrite unavailable_index{0xcbcU, 4U, 7U};
    require(viper::apply_ncar_body_object_writes(selector_body,
        std::span<const viper::NcarObjectFieldWrite>(&unavailable_index, 1U)));
    const auto selector_before_failure = selector_body;
    require(!viper::advance_ncar_integrator_child_control_in_body(selector_body,
        integrated_wheels, runtime_record, first_wheel_offset) &&
        selector_body.bytes == selector_before_failure.bytes &&
        selector_body.initialized_bytes == selector_before_failure.initialized_bytes);

    auto drivetrain_body = saved_curve_body;
    require(viper::apply_ncar_body_object_writes(drivetrain_body, controller_fields) &&
        viper::apply_ncar_body_object_writes(drivetrain_body, selector_fields) &&
        viper::apply_ncar_body_object_writes(drivetrain_body, signal_fields));
    const std::array<viper::NcarObjectFieldWrite, 4> drivetrain_fields{{
        {0xdd0U, 4U, std::bit_cast<std::uint32_t>(0.5F)},
        {0xbe4U, 1U, 0U}, {0xd70U, 4U, 0U}, {0xc3cU, 1U, 0U}}};
    require(viper::apply_ncar_body_object_writes(drivetrain_body, drivetrain_fields));
    auto drivetrain_wheels = integrated_wheels;
    const auto drivetrain = viper::advance_ncar_drivetrain_control_in_body_and_wheels(
        drivetrain_body, drivetrain_wheels, runtime_record, first_wheel_offset);
    require(drivetrain && drivetrain->selected_pair_value_0xddc == 3.0F &&
        drivetrain->selector.child.previous_index_0x1c == 2 &&
        drivetrain->selector.child.selected_value_0x18 == 20.0F &&
        viper::read_ncar_body_object_u32(drivetrain_body, 0xddcU) ==
            std::bit_cast<std::uint32_t>(3.0F) &&
        viper::read_ncar_body_object_u8(drivetrain_body, 0xe49U) == 0U &&
        drivetrain->signal_pair.controller.controller.observed_value_0x28 == 120.0F);
    const auto drivetrain_before_failure = drivetrain_body;
    const auto drivetrain_wheels_before_failure = drivetrain_wheels;
    require(!viper::advance_ncar_drivetrain_control_in_body_and_wheels(
        drivetrain_body, drivetrain_wheels,
        std::span<const std::byte>(runtime_record.data(), 0xc3U), first_wheel_offset) &&
        drivetrain_body.bytes == drivetrain_before_failure.bytes &&
        drivetrain_body.initialized_bytes == drivetrain_before_failure.initialized_bytes);
    for (std::size_t i = 0; i < drivetrain_wheels.size(); ++i)
        require(drivetrain_wheels[i].bytes == drivetrain_wheels_before_failure[i].bytes &&
            drivetrain_wheels[i].initialized_bytes ==
                drivetrain_wheels_before_failure[i].initialized_bytes);
    auto gated_body = saved_curve_body;
    require(viper::apply_ncar_body_object_writes(gated_body, controller_fields) &&
        viper::apply_ncar_body_object_writes(gated_body, selector_fields) &&
        viper::apply_ncar_body_object_writes(gated_body, signal_fields) &&
        viper::apply_ncar_body_object_writes(gated_body, drivetrain_fields));
    const std::array<viper::NcarObjectFieldWrite, 4> gate_fields{{
        {0xc3dU, 1U, 1U}, {0xc7cU, 4U, 0U}, {0xbe4U, 1U, 1U},
        {0xd70U, 4U, std::bit_cast<std::uint32_t>(2001.0F)}}};
    require(viper::apply_ncar_body_object_writes(gated_body, gate_fields));
    auto gated_wheels = integrated_wheels;
    const auto gated = viper::advance_ncar_drivetrain_control_in_body_and_wheels(
        gated_body, gated_wheels, runtime_record, first_wheel_offset);
    require(gated && gated->gate.curve_byte_0x40_written &&
        gated->gate.setter_0x4331a0_reached &&
        viper::read_ncar_body_object_u32(gated_body, 0xde0U) ==
            std::bit_cast<std::uint32_t>(1.0F) &&
        viper::read_ncar_body_object_u8(gated_body, 0xe49U) == 1U &&
        viper::read_ncar_body_object_u32(gated_body, 0xc7cU) ==
            std::bit_cast<std::uint32_t>(1.0F));

    // Follow the selected wheel setter through the same spin and exit-rate
    // adapters used by the player tick, using an explicit zero tire vector.
    auto powered_wheel = drivetrain_wheels[0];
    auto neutral_wheel = powered_wheel;
    const std::array<viper::NcarObjectFieldWrite, 17> spin_fields{{
        {first_wheel_offset + 0x124U, 4U, std::bit_cast<std::uint32_t>(2.0F)},
        {first_wheel_offset + 0x05cU, 4U, std::bit_cast<std::uint32_t>(1.25F)},
        {first_wheel_offset + 0x0a8U, 4U, std::bit_cast<std::uint32_t>(2.5F)},
        {first_wheel_offset + 0x050U, 4U, std::bit_cast<std::uint32_t>(0.8F)},
        {first_wheel_offset + 0x054U, 4U, std::bit_cast<std::uint32_t>(0.1F)},
        {first_wheel_offset + 0x058U, 4U, std::bit_cast<std::uint32_t>(0.2F)},
        {first_wheel_offset + 0x0e0U, 4U, std::bit_cast<std::uint32_t>(0.4F)},
        {first_wheel_offset + 0x178U, 4U, std::bit_cast<std::uint32_t>(10.0F)},
        {first_wheel_offset + 0x17cU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {first_wheel_offset + 0x174U, 4U, std::bit_cast<std::uint32_t>(0.2F)},
        {first_wheel_offset + 0x170U, 4U, std::bit_cast<std::uint32_t>(0.1F)},
        {first_wheel_offset + 0x15cU, 4U, 0U}, {first_wheel_offset + 0x164U, 4U, 0U},
        {first_wheel_offset + 0x020U, 1U, 0U}, {first_wheel_offset + 0x160U, 1U, 0U},
        {first_wheel_offset + 0x038U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {first_wheel_offset + 0x120U, 1U, 0U}}};
    require(viper::apply_ncar_wheel_object_writes(powered_wheel, first_wheel_offset, spin_fields) &&
        viper::apply_ncar_wheel_object_writes(neutral_wheel, first_wheel_offset, spin_fields));
    const viper::NcarObjectFieldWrite zero_e4{first_wheel_offset + 0xe4U, 4U, 0U};
    require(viper::apply_ncar_wheel_object_writes(neutral_wheel, first_wheel_offset,
        std::span<const viper::NcarObjectFieldWrite>(&zero_e4, 1U)));
    std::array<viper::NcarObjectFieldWrite, 12> spin_body_fields{};
    for (std::size_t i = 0; i < 9U; ++i)
        spin_body_fields[i] = {0x38U + static_cast<std::uint32_t>(i * 4U), 4U,
            std::bit_cast<std::uint32_t>(i % 4U == 0U ? 1.0F : 0.0F)};
    for (std::size_t i = 0; i < 3U; ++i)
        spin_body_fields[9U + i] = {0x234U + static_cast<std::uint32_t>(i * 4U), 4U, 0U};
    require(viper::apply_ncar_body_object_writes(drivetrain_body, spin_body_fields));
    const auto powered_spin = viper::advance_ncar_wheel_spin_preprojection_in_objects(
        powered_wheel, first_wheel_offset, drivetrain_body);
    const auto neutral_spin = viper::advance_ncar_wheel_spin_preprojection_in_objects(
        neutral_wheel, first_wheel_offset, drivetrain_body);
    require(powered_spin && neutral_spin &&
        powered_spin->response_local_0x90 != neutral_spin->response_local_0x90);
    const auto powered_rate = viper::advance_ncar_wheel_rate_finalization_in_object(
        powered_wheel, first_wheel_offset, *powered_spin, {}, {});
    const auto neutral_rate = viper::advance_ncar_wheel_rate_finalization_in_object(
        neutral_wheel, first_wheel_offset, *neutral_spin, {}, {});
    require(powered_rate && neutral_rate && powered_rate->rate_0x124 != neutral_rate->rate_0x124 &&
        powered_rate->auxiliary_rate_0x128 != neutral_rate->auxiliary_rate_0x128);

    // Start from the same constructor and curve configuration projections
    // used by the session, then advance several fixed control steps.
    viper::NcarBodyRuntimeObjectState startup_body{};
    const auto startup_defaults = viper::plan_ncar_constructor_subobject_defaults();
    const auto startup_initializer = viper::plan_ncar_object_initialization({});
    require(viper::apply_ncar_body_object_writes(startup_body, startup_defaults.writes) &&
        viper::apply_ncar_body_object_writes(startup_body,
            startup_initializer.writes_before_backend_refresh));
    const auto d70_initializer = std::find_if(
        startup_initializer.writes_before_helper_0x4331e0.begin(),
        startup_initializer.writes_before_helper_0x4331e0.end(),
        [](const auto& write) { return write.offset == 0xd70U && write.width_bytes == 4U; });
    require(d70_initializer != startup_initializer.writes_before_helper_0x4331e0.end() &&
        viper::apply_ncar_body_object_writes(startup_body,
            std::span<const viper::NcarObjectFieldWrite>(&*d70_initializer, 1U)));
    const auto startup_curve = viper::configure_update_curve(
        0U, 2.0F, 0.1F, {0.0F, 0.0F, 100.0F, 0.0F, 0.0F, 0.0F},
        6000.0F, 1000.0F);
    for (const auto& source : viper::make_configured_update_curve_object_writes(
             startup_curve, 0xbfcU)) {
        const viper::NcarObjectFieldWrite write{source.offset, source.width, source.value_bits};
        require(viper::apply_ncar_body_object_writes(startup_body,
            std::span<const viper::NcarObjectFieldWrite>(&write, 1U)));
    }
    require(viper::read_ncar_body_object_u32(startup_body, 0xc0cU) ==
        std::bit_cast<std::uint32_t>(1000.0F));
    const viper::NcarObjectFieldWrite startup_curve_reset{
        0xc0cU, 4U, startup_initializer.cleared_field_0xc0c};
    require(viper::apply_ncar_body_object_writes(startup_body,
        std::span<const viper::NcarObjectFieldWrite>(&startup_curve_reset, 1U)) &&
        viper::read_ncar_body_object_u32(startup_body, 0xc0cU) == 0U);
    const std::array<viper::NcarObjectFieldWrite, 15> startup_setup{{
        {0x050U, 4U, 0U}, {0x054U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0x058U, 4U, 0U}, {0x258U, 4U, 0U}, {0x25cU, 4U, 0U}, {0x260U, 4U, 0U},
        {0xc8cU, 4U, std::bit_cast<std::uint32_t>(1000.0F)},
        {0xc90U, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        {0xca0U, 4U, std::bit_cast<std::uint32_t>(2.0F)},
        {0xca4U, 4U, std::bit_cast<std::uint32_t>(0.5F)},
        {0xcacU, 4U, std::bit_cast<std::uint32_t>(0.1F)},
        {0xcc4U, 4U, std::bit_cast<std::uint32_t>(2.0F)},
        {0xdd0U, 4U, std::bit_cast<std::uint32_t>(0.5F)},
        {0xbe4U, 1U, 0U}, {0x534U, 1U, 0U}}};
    require(viper::apply_ncar_body_object_writes(startup_body, startup_setup));
    auto startup_wheels = integrated_wheels;
    for (std::uint32_t tick = 0U; tick < 3U; ++tick) {
        const std::array<viper::NcarObjectFieldWrite, 3> controls{{
            {0xde0U, 4U, std::bit_cast<std::uint32_t>(0.25F * static_cast<float>(tick))},
            {0xdecU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
            {0xcbcU, 4U, tick == 0U ? 0U : 1U}}};
        require(viper::apply_ncar_body_object_writes(startup_body, controls));
        const auto step = viper::advance_ncar_drivetrain_control_in_body_and_wheels(
            startup_body, startup_wheels, runtime_record, first_wheel_offset);
        require(step && std::isfinite(step->signal_pair.curve.integrator.value_0x10));
        const auto getter = viper::read_ncar_body_object_u32(startup_body, 0xc0cU);
        require(getter.has_value());
        const viper::NcarObjectFieldWrite post_wheel_sample{0xd70U, 4U, *getter};
        require(viper::apply_ncar_body_object_writes(startup_body,
            std::span<const viper::NcarObjectFieldWrite>(&post_wheel_sample, 1U)));
    }
    require(viper::read_ncar_body_object_u8(startup_body, 0xcf0U) == 1U &&
        viper::read_ncar_body_object_u32(startup_body, 0xcb0U) ==
            std::bit_cast<std::uint32_t>(2.0F));
}
