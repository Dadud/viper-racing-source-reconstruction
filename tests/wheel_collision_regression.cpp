#include "viper/wheel_body_coupling.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace {
void require(const bool condition)
{
    if (!condition) std::abort();
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
    require(selected_pair->wheel_setter_reached ==
        std::array<bool, 4>{true, true, false, false});

    pair_input.selector_value_bits_0xb4 = 0x3f800000U;
    selected_pair =
        viper::advance_ncar_selected_wheel_pair_target(pair_input);
    require(selected_pair.has_value() && selected_pair->selected_target == 2U);
    require(selected_pair->wheel_setter_reached ==
        std::array<bool, 4>{false, false, true, true});
    require(std::abs(selected_pair->wheel_value_0xe4[2] - 5.829F) < 1e-4F &&
            std::abs(selected_pair->wheel_value_0xe4[3] + 5.029F) < 1e-4F);

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
}
