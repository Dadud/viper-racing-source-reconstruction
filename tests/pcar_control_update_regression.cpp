#include "viper/pcar_control_update.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

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

    const auto timestamp_clear = viper::update_pcar_timestamp(
        0x3f800000U, false, 0.064F);
    const auto timestamp_first = viper::update_pcar_timestamp(
        0U, true, 0.064F);
    const auto timestamp_recent = viper::update_pcar_timestamp(
        std::bit_cast<std::uint32_t>(0.05F), true, 0.14F);
    const auto timestamp_expired = viper::update_pcar_timestamp(
        std::bit_cast<std::uint32_t>(0.05F), true, 0.2F);
    if (timestamp_clear.timestamp_bits_0xecc != 0U ||
        timestamp_clear.request_backend_callback ||
        timestamp_first.timestamp_bits_0xecc !=
            std::bit_cast<std::uint32_t>(0.064F) ||
        !timestamp_first.request_backend_callback ||
        timestamp_recent.timestamp_bits_0xecc !=
            std::bit_cast<std::uint32_t>(0.05F) ||
        timestamp_recent.request_backend_callback ||
        timestamp_expired.timestamp_bits_0xecc != 0x4CEB79A3U ||
        timestamp_expired.request_backend_callback) {
        std::cerr << "PCAR +0xECC timestamp latch did not preserve its gate, callback request, and expiry transitions.\n";
        return EXIT_FAILURE;
    }

    viper::PcarCallbackGlobals globals{};
    viper::set_pcar_callback_globals_0x43b880(
        globals, 0x11223344U, 0x55667788U, 0x99aabbccU);
    if (globals.value_0x509384 != 0x11223344U ||
        globals.value_0x509388 != 0x55667788U ||
        globals.value_0x5093a4 != 0x99aabbccU) {
        std::cerr << "0x43B880 did not preserve its three raw global stores.\n";
        return EXIT_FAILURE;
    }
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
        callback_result.callback_multiplier.value != 1.0F ||
        callback_result.callback_multiplier.selector_helper_called ||
        std::abs(callback_state.scalar_prefix.value_0xde0 - 0.5F) > 1.0e-6F ||
        std::abs(callback_state.scalar_prefix.value_0xde4 - globals.value_0x509320) >
            1.0e-6F ||
        std::abs(callback_state.scalar_prefix.value_0xde8 - globals.value_0x5093b8) >
            1.0e-6F) {
        std::cerr << "PCAR callback control prefix did not preserve native call order/dataflow.\n";
        return EXIT_FAILURE;
    }
    auto multiplier_observation = callback_observation;
    viper::PcarCallbackControlState multiplier_state{};
    multiplier_observation.multiplier = {true, 0U};
    const auto disabled_multiplier_result =
        viper::advance_pcar_callback_control_prefix(multiplier_state,
            multiplier_observation);
    if (!disabled_multiplier_result.callback_multiplier.selector_helper_called ||
        disabled_multiplier_result.callback_multiplier.selector_is_one ||
        disabled_multiplier_result.callback_multiplier.value != 0.0F ||
        multiplier_state.scalar_prefix.value_0xde0 != 0.0F) {
        std::cerr << "PCAR callback multiplier did not apply the disabled selector branch.\n";
        return EXIT_FAILURE;
    }
    multiplier_observation.multiplier.selector_0x4e322c = 1U;
    const auto selected_multiplier_result =
        viper::advance_pcar_callback_control_prefix(multiplier_state,
            multiplier_observation);
    if (!selected_multiplier_result.callback_multiplier.selector_helper_called ||
        !selected_multiplier_result.callback_multiplier.selector_is_one ||
        selected_multiplier_result.callback_multiplier.value != 1.0F ||
        multiplier_state.scalar_prefix.value_0xde0 != globals.value_0x5093f4) {
        std::cerr << "PCAR callback multiplier did not match selector value one.\n";
        return EXIT_FAILURE;
    }
    auto configured_multiplier_observation = callback_observation;
    configured_multiplier_observation.multiplier = {true, 0U, 1.25F, false};
    viper::PcarCallbackControlState configured_multiplier_state{};
    const auto configured_multiplier_result =
        viper::advance_pcar_callback_control_prefix(
            configured_multiplier_state, configured_multiplier_observation);
    if (!configured_multiplier_result.callback_multiplier.selector_helper_called ||
        configured_multiplier_result.callback_multiplier.selector_is_one ||
        configured_multiplier_result.callback_multiplier.value != 1.25F ||
        std::abs(configured_multiplier_state.scalar_prefix.value_0xde0 -
            globals.value_0x5093f4 * 1.25F) > 1.0e-6F) {
        std::cerr << "PCAR callback multiplier did not apply configured throttle boost.\n";
        return EXIT_FAILURE;
    }

    viper::PcarCallbackBranchFields branch_fields{};
    branch_fields.value_0xd64 = 7U;
    branch_fields.scalars.value_0xde0 = 0.1F;
    branch_fields.scalars.value_0xde4 = 0.3F;
    branch_fields.value_0xc34 = 0.9F;
    viper::PcarCallbackBranchObservation branch_observation{};
    branch_observation.globals.gate_0x5093f0 = true;
    branch_observation.globals.value_0x5093f4 = 0.2F;
    branch_observation.globals.value_0x5093c8 = 0.5F;
    const auto gate_branch = viper::advance_pcar_callback_branch(
        branch_fields, branch_observation);
    if (!gate_branch.complete || !gate_branch.called_0x4331e0 ||
        gate_branch.called_0x433210 || branch_fields.value_0xd64 != 0xffffffffU ||
        std::abs(branch_fields.value_0x54 - 0.3F) > 1.0e-6F ||
        branch_fields.value_0x4 != 3U || gate_branch.argument_0x4331e0 != -1) {
        std::cerr << "PCAR branch gate did not preserve precedence, helper result, max, and flags.\n";
        return EXIT_FAILURE;
    }
    branch_fields = {};
    branch_fields.value_0x54 = 0.6F;
    branch_observation.globals.value_0x5093f4 = 0.7F;
    const auto max_branch = viper::advance_pcar_callback_branch(
        branch_fields, branch_observation);
    if (!max_branch.complete || branch_fields.value_0x54 != 0.7F) {
        std::cerr << "PCAR +0x54 max did not preserve the greater observed value.\n";
        return EXIT_FAILURE;
    }
    branch_fields = {};
    branch_fields.value_0xd64 = 4U;
    branch_fields.value_0xec8 = 1U;
    branch_observation = {};
    const auto missing_helper = viper::advance_pcar_callback_branch(
        branch_fields, branch_observation);
    if (missing_helper.complete || branch_fields.value_0xd64 != 4U ||
        branch_fields.value_0x4 != 0U) {
        std::cerr << "PCAR branch partially mutated state without a helper observation.\n";
        return EXIT_FAILURE;
    }
    branch_fields = {};
    branch_fields.value_0xd64 = 11U;
    branch_fields.value_0xec8 = 1U;
    branch_observation = {};
    branch_observation.selector_0xc98 = viper::EmbeddedSelectionFields{};
    branch_observation.curve_0xbfc = viper::UpdateCurveFields{};
    const auto ec8_branch = viper::advance_pcar_callback_branch(
        branch_fields, branch_observation);
    if (!ec8_branch.complete || !ec8_branch.called_0x433210 ||
        ec8_branch.called_0x4331e0 || branch_fields.value_0xd64 != 1U ||
        branch_fields.value_0xcbc != 1U) {
        std::cerr << "PCAR +0xEC8 branch did not route through 0x433210.\n";
        return EXIT_FAILURE;
    }
    branch_fields = {};
    branch_fields.value_0xd64 = 5U;
    branch_fields.value_0xec9 = 1U;
    branch_fields.value_0xc7c = 0.9F;
    branch_observation = {};
    branch_observation.globals.value_0x5093bc = 0x80000000U;
    const auto ec9_branch = viper::advance_pcar_callback_branch(
        branch_fields, branch_observation);
    if (!ec9_branch.complete || !ec9_branch.called_0x4331e0 ||
        branch_fields.value_0xd64 != 0x80000000U || !ec9_branch.output_0x5093bc ||
        *ec9_branch.output_0x5093bc != 0x80000000U ||
        ec9_branch.argument_0x4331e0 != std::numeric_limits<std::int32_t>::min()) {
        std::cerr << "PCAR +0xEC9 branch or global +0x5093BC copy was incorrect.\n";
        return EXIT_FAILURE;
    }
    branch_fields = {};
    branch_fields.value_0xd64 = 14U;
    branch_fields.value_0xc7c = std::bit_cast<float>(std::uint32_t{0x3f666666U});
    branch_observation = {};
    branch_observation.globals.value_0x5093bc = 14U;
    const auto threshold_branch = viper::advance_pcar_callback_branch(
        branch_fields, branch_observation);
    if (!threshold_branch.complete || threshold_branch.called_0x4331e0 ||
        threshold_branch.output_0x5093bc != 14U) {
        std::cerr << "PCAR +0xC7C threshold must be strictly less than 0x3F666666.\n";
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
