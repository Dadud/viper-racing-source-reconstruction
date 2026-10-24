#include "viper/pcar_control_update.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main()
{
    const auto defaults = viper::initialize_pcar_constructor_controls(
        std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, 7U);
    if (!defaults.traction_control || !defaults.abs_braking ||
        !defaults.auto_shifting || !defaults.auto_clutch || !defaults.auto_clutch_0x534 ||
        defaults.yaw_control != 0 || defaults.game_mode_0x4f4 != 7U ||
        defaults.value_0xd74.has_value() ||
        defaults.wheel_traction_0x6b0 != std::array<std::uint32_t, 4>{1U, 1U, 1U, 1U} ||
        defaults.wheel_abs_0x6b8 != std::array<std::uint32_t, 4>{1U, 1U, 1U, 1U}) {
        std::cerr << "PCAR constructor defaults or four-wheel settings were not retained.\n";
        return EXIT_FAILURE;
    }
    const auto observed_helper = viper::initialize_pcar_constructor_controls(
        std::nullopt, std::nullopt, std::nullopt, std::nullopt,
        std::nullopt, std::nullopt, 0xA5U);
    if (observed_helper.value_0xd74 != 0xA5U) {
        std::cerr << "PCAR constructor did not retain the observed 0x43BA90 result byte.\n";
        return EXIT_FAILURE;
    }
    const auto manual = viper::initialize_pcar_constructor_controls(
        false, false, false, false, -1, std::nullopt);
    if (manual.traction_control || manual.abs_braking || manual.auto_shifting ||
        manual.auto_clutch || manual.auto_clutch_0x534 || manual.yaw_control != -1 ||
        manual.auto_clutch_forced_by_auto_shift ||
        manual.wheel_traction_0x6b0 != std::array<std::uint32_t, 4>{0U, 0U, 0U, 0U} ||
        manual.wheel_abs_0x6b8 != std::array<std::uint32_t, 4>{0U, 0U, 0U, 0U}) {
        std::cerr << "PCAR constructor did not preserve explicit disabled controls.\n";
        return EXIT_FAILURE;
    }
    const auto forced_clutch = viper::initialize_pcar_constructor_controls(
        true, true, true, false, 0, std::nullopt);
    if (!forced_clutch.auto_clutch || !forced_clutch.auto_clutch_0x534 ||
        !forced_clutch.auto_clutch_forced_by_auto_shift) {
        std::cerr << "PCAR auto-shifting must force auto-clutch enabled.\n";
        return EXIT_FAILURE;
    }

    viper::PcarCallbackGlobals globals{};
    globals.value_0x5093f4 = 0.5F;
    globals.value_0x509320 = 0.4F;
    globals.value_0x5093b8 = 0.2F;
    globals.value_0x5093c8 = 0.5F;
    viper::PcarScalarFields fields{};

    viper::update_pcar_callback_scalar_prefix(fields, globals, 2.0F);
    if (std::abs(fields.value_0xde0 - 1.0F) > 1.0e-6F ||
        std::abs(fields.value_0xde4 - 0.4F) > 1.0e-6F ||
        std::abs(fields.value_0xde8 - 0.2F) > 1.0e-6F ||
        std::abs(fields.value_0xdec - 0.5F) > 1.0e-6F) {
        std::cerr << "PCAR scalar prefix did not preserve the observed field mapping.\n";
        return EXIT_FAILURE;
    }

    viper::PcarCallbackControlState callback_state{};
    callback_state.smoother.multiplier_0xdcc = 2.0F;
    callback_state.smoother.output_0x978 = 9.0F;
    callback_state.smoother.output_0xb1c = 8.0F;
    viper::PcarCallbackControlObservation callback_observation{};
    callback_observation.body_velocity_0x234 = {12.0F, 0.0F, 16.0F};
    callback_observation.body_basis_0x50 = {0.6F, 0.0F, 0.8F};
    callback_observation.globals = globals;
    callback_observation.globals.value_0x509358 = 0.8F;
    callback_observation.globals.value_0x5093f4 = 0.5F;
    callback_observation.value_from_0x40d270 = 2.0F;
    const auto callback_result = viper::advance_pcar_callback_control_prefix(
        callback_state, callback_observation);
    const float expected_scaled_request =
        ((20.0F * -0.01730769F) + 1.269231F) * 0.8F;
    if (std::abs(callback_result.absolute_velocity_projection - 20.0F) > 1.0e-6F ||
        std::abs(callback_result.scaled_smoother_request - expected_scaled_request) >
            1.0e-5F ||
        std::abs(callback_state.smoother.value_0x500 - 0.24F) > 1.0e-6F ||
        callback_state.smoother.output_0x630 != 0.48F ||
        callback_state.smoother.output_0x7d4 != 0.48F ||
        callback_state.smoother.output_0x978 != 9.0F ||
        callback_state.smoother.output_0xb1c != 8.0F ||
        std::abs(callback_state.scalar_prefix.value_0xde0 - 1.0F) > 1.0e-6F ||
        std::abs(callback_state.scalar_prefix.value_0xde4 - globals.value_0x509320) >
            1.0e-6F ||
        std::abs(callback_state.scalar_prefix.value_0xde8 - globals.value_0x5093b8) >
            1.0e-6F) {
        std::cerr << "PCAR callback control prefix did not preserve native call order/dataflow.\n";
        return EXIT_FAILURE;
    }
    callback_state.smoother.mode_0xbe4 = true;
    callback_observation.body_velocity_0x234 = {};
    callback_observation.globals.value_0x509358 = -0.5F;
    callback_observation.globals.gate_0x5093fc = true;
    const auto reverse_callback_result =
        viper::advance_pcar_callback_control_prefix(
            callback_state, callback_observation);
    if (reverse_callback_result.absolute_velocity_projection != 0.0F ||
        reverse_callback_result.scaled_smoother_request != -0.25F ||
        std::abs(callback_state.smoother.value_0x500) > 1.0e-6F ||
        callback_state.smoother.output_0x978 != 0.0F ||
        callback_state.smoother.output_0xb1c != 0.0F ||
        callback_state.smoother.output_0x630 != 0.48F ||
        callback_state.smoother.output_0x7d4 != 0.48F) {
        std::cerr << "PCAR callback control prefix lost its alternate mode/global branch.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
