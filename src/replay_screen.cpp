#include "viper/replay_screen.hpp"

#include <utility>

namespace viper {

ReplayAnalysisScreenPlan plan_replay_analysis_screen(
    const ReplayAnalysisScreenObservation& observation)
{
    ReplayAnalysisScreenPlan result{};
    if (!observation.replay_visible) {
        result.returned_low_byte.emplace(std::uint8_t{0U});
        return result;
    }

    result.screen_built = true;
    constexpr std::array<ReplayAnalysisScreenPlan::LabelRegistration, 8>
        label_definitions{{
            {0x004ec7c8U, 0x004d09f8U, 0x00408a00U, 0x01U},
            {0x004ec970U, 0x004d0a0cU, 0x004089f0U, 0x02U},
            {0x004eb800U, 0x004d0a20U, 0x004089e0U, 0x04U},
            {0x004ec590U, 0x004d0a38U, 0x004089d0U, 0x08U},
            {0x004eb7f0U, 0x004d0a4cU, 0x004089c0U, 0x10U},
            {0x004ec958U, 0x004d0a60U, 0x004089b0U, 0x20U},
            {0x004ec6f8U, 0x004d0a74U, 0x004089a0U, 0x40U},
            {0x004ec7a8U, 0x004d0a8cU, 0x00408990U, 0x80U},
        }};
    std::vector<std::uint32_t> projected_calls{0x44cfd0U, 0x44cfd0U};
    if (!observation.label_registration_mask_0x4ec788.has_value()) {
        result.input_complete = false;
    } else {
        auto registration_mask = *observation.label_registration_mask_0x4ec788;
        for (const auto& label : label_definitions) {
            if ((registration_mask & label.flag_bit) == 0U) {
                result.label_registrations.push_back(label);
                projected_calls.push_back(0x418c90U);
                projected_calls.push_back(0x4bd130U);
                registration_mask |= label.flag_bit;
            }
        }
        result.updated_label_registration_mask.emplace(registration_mask);
    }

    for (const auto& value : observation.control_values) {
        if (!value.has_value()) {
            result.input_complete = false;
        }
    }

    using Control = ReplayAnalysisScreenPlan::ControlConstructorCall;
    using Arguments = std::array<std::optional<std::uint32_t>, 14>;
    const auto add_control = [&](const std::int32_t offset,
                                 const Arguments& arguments,
                                 const std::optional<std::int32_t> stack_pointer =
                                     std::nullopt,
                                 const std::size_t stack_pointer_index = 8U) {
        Control call{};
        call.descriptor_ebp_offset = offset;
        call.arguments = arguments;
        if (stack_pointer.has_value()) {
            call.stack_pointer_offsets[stack_pointer_index] = stack_pointer;
        }
        result.controls.push_back(std::move(call));
    };

    const auto& value = observation.control_values;
    add_control(-0x5b0, {0x17U, 0U, 0x7aU, 0x88U, 0x114U, 0x66U,
                         0x004d0bd0U, 0U, std::nullopt, 0U, 0U, 0U, 0U, 0U},
                -0x608);
    add_control(-0x578, {1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                         0x004ec99cU, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x540, {1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U,
                         0x004ec99cU, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x508, {1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                         0x004ec99cU, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x4d0, {2U, 0U, 0x181U, 6U, 0U, 0U, value[0], 0U,
                         0x00407760U, 4U, 0U, 0U, 0U, 0U});
    add_control(-0x498, {1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U,
                         0x004ec99cU, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x460, {1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                         0x004ec520U, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x428, {2U, 0U, 0x181U, 6U, 0U, 0U, value[1], 0U,
                         0x00407c60U, 4U, 0U, 0U, 0U, 0U});
    add_control(-0x3f0, {1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U,
                         0x004ec520U, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x3b8, {2U, 0U, 0x181U, 0x1cU, 0U, 0U, value[2], 0U,
                         0x00407390U, 4U, 0U, 0U, 0U, 0U});
    add_control(-0x380, {2U, 0xdU, 0x181U, 0x32U, 0U, 0U, value[3], 0U,
                         0U, 4U, 0U, 0U, 0U, 0U});
    add_control(-0x348, {2U, 0x42U, 0x181U, 0x48U, 0U, 0U, value[4], 0U,
                         0U, 4U, 0U, 0U, 0U, 0U});
    add_control(-0x310, {4U, 0xfffffffeU, 0x1bU, 0U, 0U, 0U, 0U, 0U,
                         0U, 0U, 0U, 0U, 0U, 0U});
    add_control(-0x2d8, {0xcU, 0U, 300U, 0x73U, 0U, 0U, value[5], 2U,
                         0x004ec50cU, 8U, 0U, 0U, 0U, 0U});
    add_control(-0x2a0, {0xcU, 0U, 300U, 0x82U, 0U, 0U, value[6], 1U,
                         0x004ec50cU, 8U, 0U, 0U, 0U, 0U});
    add_control(-0x268, {0xcU, 0U, 300U, 0x91U, 0U, 0U, value[7], 3U,
                         0x004ec50cU, 8U, 0U, 0U, 0U, 0U});
    add_control(-0x230, {6U, 0U, 0x1a8U, 0x73U, 0U, 0U, 0x004d0aa4U, 0U,
                         0x004eb810U, 0x15U, 0x3f800000U, 0U, 0U, 0U});
    add_control(-0x1f8, {0x10U, 0U, 0x196U, 0x73U, 0U, 0U, 0x004d0bd0U,
                         0x14U, 0x004eb810U, 0U, 0x3f000000U,
                         0x41200000U, 0U, 0U});
    add_control(-0x1c0, {0x10U, 0U, 0x1d5U, 0x73U, 0U, 0U, 0x004d0bd0U,
                         0x14U, 0x004eb810U, 1U, 0x3f000000U,
                         0x41200000U, 0U, 0U});
    add_control(-0x188, {6U, 0U, 0x1a8U, 0x8cU, 0U, 0U, 0x004d0aacU, 0U,
                         0x004eb80cU, 0x15U, 0x42c80000U, 0U, 0U, 0U});
    add_control(-0x150, {0x10U, 0U, 0x196U, 0x8cU, 0U, 0U, 0x004d0bd0U,
                         5U, 0x004eb80cU, 0U, 0x3f800000U,
                         0x40400000U, 0U, 0U});
    add_control(-0x118, {0x10U, 0U, 0x1d5U, 0x8cU, 0U, 0U, 0x004d0bd0U,
                         5U, 0x004eb80cU, 1U, 0x3f800000U,
                         0x40400000U, 0U, 0U});
    add_control(-0xe0, {5U, 0U, 300U, 0xaaU, 0U, 0U, 0x004ec738U, 0U,
                        0U, 0x15U, 0U, 0U, 0U, 0U});
    add_control(-0xa8, {6U, 0U, 0x17cU, 0xaaU, 0U, 0U, 0x004d0ab4U, 0U,
                        0x004d066cU, 0x15U, 0x3f800000U, 0U, 0U, 0U});
    add_control(-0x70, {0x17U, 0U, 0x10U, 0x6eU, 0x104U, 0x52U,
                       0x004d0bd0U, 0U, std::nullopt, 0U, 0U, 0U, 0U, 0U},
                -0x5c8);

    constexpr std::array<std::uint32_t, 8> cache_objects{{
        0x004eb7f0U, 0x004ec958U, 0x004ec6f8U, 0x004ec7a8U,
        0x004ec590U, 0x004ec970U, 0x004ec7c8U, 0x004eb800U,
    }};
    constexpr std::array<std::size_t, 8> refresh_before_control{{
        4U, 7U, 9U, 10U, 11U, 13U, 14U, 15U,
    }};
    for (std::size_t refresh_index = 0;
         refresh_index < cache_objects.size();
         ++refresh_index) {
        result.cache_refreshes[refresh_index].object_address =
            cache_objects[refresh_index];
        result.cache_refreshes[refresh_index].performed =
            observation.cache_refresh_needed[refresh_index];
    }
    for (std::size_t control_index = 0;
         control_index < result.controls.size();
         ++control_index) {
        for (std::size_t refresh_index = 0;
             refresh_index < refresh_before_control.size();
             ++refresh_index) {
            if (refresh_before_control[refresh_index] != control_index) {
                continue;
            }
            const auto& refresh =
                observation.cache_refresh_needed[refresh_index];
            if (!refresh.has_value()) {
                result.input_complete = false;
            } else if (*refresh) {
                projected_calls.push_back(0x418cc0U);
            }
        }
        projected_calls.push_back(0x408f50U);
    }
    projected_calls.insert(
        projected_calls.end(),
        {0x46b900U, 0x472b30U, 0x46b8f0U, 0x44d0a0U, 0x44d0a0U});
    result.projected_calls = std::move(projected_calls);
    if (!observation.menu_result.has_value()) {
        result.input_complete = false;
        return result;
    }
    const auto return_low_byte = static_cast<std::uint8_t>(
        *observation.menu_result == 0x42U ? 0U : 1U);
    result.returned_low_byte.emplace(return_low_byte);
    return result;
}

ReplayScreenPlan plan_replay_screen(
    const ReplayScreenObservation& observation)
{
    ReplayScreenPlan result{};
    result.calls = {0x45ba50U, 0x412d50U, 0x4bd190U};
    result.callback_bindings.push_back({0x00406d40U, 0x00407c60U, 0x19cU});

    result.missing_replay_prompt_shown =
        !observation.replay_record_argument_present &&
        observation.replay_already_open_0x4ec5f0;
    if (result.missing_replay_prompt_shown) {
        result.calls.push_back(0x410ea0U);
    }

    result.calls.push_back(0x45c390U);
    result.calls.push_back(0x46b8f0U);
    result.replay_record_copied_to_0x4eb828 =
        observation.replay_record_argument_present;
    if (result.replay_record_copied_to_0x4eb828) {
        result.replay_record_copy_dword_count = 0x335U;
        result.calls.push_back(0x426520U);
    }

    if (!observation.replay_record_argument_present) {
        result.calls.push_back(0x4178c0U); // ui.res
    }
    result.calls.push_back(0x417740U); // race.res
    result.calls.push_back(0x417740U); // postrace.res
    result.calls.push_back(0x46b8f0U); // install replay screen

    result.calls.push_back(0x409100U);
    if (!observation.control_value_0x4ec554.has_value() ||
        !observation.control_value_0x4ec804.has_value()) {
        result.input_complete = false;
    }

    constexpr std::array<ReplayButtonRegistration, 5> button_definitions{{
        {0x004ec550U, 0x004d0700U, 0x004070b0U, 0x01U},
        {0x004ec990U, 0x004d0714U, 0x004070a0U, 0x02U},
        {0x004ec800U, 0x004d0728U, 0x00407090U, 0x04U},
        {0x004ec530U, 0x004d0740U, 0x00407080U, 0x08U},
        {0x004ec5c0U, 0x004d0758U, 0x00407070U, 0x10U},
    }};
    if (!observation.button_registration_mask_0x4ec704.has_value()) {
        result.input_complete = false;
    } else {
        auto registration_mask =
            *observation.button_registration_mask_0x4ec704;
        for (const auto& button : button_definitions) {
            if ((registration_mask & button.flag_bit) == 0U) {
                result.button_registrations.push_back(button);
                result.calls.push_back(0x418c90U);
                result.calls.push_back(0x4bd130U);
                registration_mask |= button.flag_bit;
            }
        }
        result.updated_button_registration_mask.emplace(registration_mask);
    }

    const auto append_control = [&](ReplayControlConstructorCall call,
                                    const std::size_t refresh_after) {
        result.controls.push_back(std::move(call));
        result.calls.push_back(0x408f50U);
        if (refresh_after < observation.refresh_control_cache.size()) {
            const auto& refresh = observation.refresh_control_cache[refresh_after];
            if (!refresh.has_value()) {
                result.input_complete = false;
            } else if (*refresh) {
                result.calls.push_back(0x418cc0U);
            }
        }
    };

    append_control({-0x2a8, {1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                             0x004ec99cU, 0U, 0U, 0U, 0U, 0U}}, 0U);
    append_control({-0x270, {2U, 0U, 0x181U, 6U, 0U, 0U,
                             observation.control_value_0x4ec554, 0U,
                             0x00407760U, 4U, 0U, 0U, 0U, 0U}},
                    observation.refresh_control_cache.size());
    append_control({-0x238, {1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U,
                             0x004ec99cU, 0U, 0U, 0U, 0U, 0U}},
                    observation.refresh_control_cache.size());
    append_control({-0x200, {1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                             0x004ec520U, 0U, 0U, 0U, 0U, 0U}}, 1U);
    append_control({-0x158, {2U, 0U, 0x181U, 0x1cU, 0U, 0U,
                             observation.control_value_0x4ec804, 0U,
                             0x00407390U, 4U, 0U, 0U, 0U, 0U}}, 2U);
    append_control({-0x0b0, {4U, 0xfffffffeU, 0x1bU, 0U, 0U, 0U, 0U,
                             0U, 0U, 0U, 0U, 0U, 0U, 0U}},
                    observation.refresh_control_cache.size());

    auto analysis_label_registration_mask =
        observation.analysis_label_registration_mask_0x4ec788;
    bool menu_finished = false;
    for (const auto& iteration : observation.iterations) {
        result.calls.push_back(0x472b30U);
        if (iteration.menu_result != 0x0dU) {
            menu_finished = true;
            break;
        }

        result.replay_action_requested = true;
        if (!observation.replay_record_argument_present) {
            // Enter has no action callback in the empty-replay screen, so the
            // loop repeats until the menu returns another value.
            result.replay_action_kept_menu_open = true;
            continue;
        }
        result.calls.push_back(0x408160U);
        const ReplayAnalysisScreenObservation analysis_observation{
            observation.replay_record_argument_present,
            iteration.analysis_menu_result,
            observation.analysis_control_values,
            analysis_label_registration_mask,
            iteration.analysis_cache_refresh_needed,
        };
        const auto analysis = plan_replay_analysis_screen(analysis_observation);
        result.calls.insert(result.calls.end(),
                            analysis.projected_calls.begin(),
                            analysis.projected_calls.end());
        result.input_complete = result.input_complete &&
            analysis.input_complete;
        result.analysis_pages.push_back(analysis);
        analysis_label_registration_mask =
            analysis.updated_label_registration_mask;
        if (!analysis.returned_low_byte.has_value()) {
            result.input_complete = false;
            break;
        }
        result.replay_action_returns.push_back(*analysis.returned_low_byte);
        if (*analysis.returned_low_byte == 0U) {
            menu_finished = true;
            break;
        }
        result.replay_action_kept_menu_open = true;
    }

    result.exited_menu = menu_finished;
    if (!menu_finished) {
        result.input_complete = false;
        return result;
    }

    result.calls.push_back(0x46b8f0U);
    result.calls.push_back(0x40c950U);
    if (observation.replay_record_argument_present) {
        result.existing_replay_post_action_called = true;
        result.calls.push_back(0x426560U);
    }
    result.calls.push_back(0x44d0a0U);
    result.calls.push_back(0x44d0a0U);
    result.calls.push_back(0x4178c0U); // postrace.res
    result.calls.push_back(0x4178c0U); // race.res
    if (!observation.replay_record_argument_present) {
        result.calls.push_back(0x417740U); // ui.res
    }
    result.calls.push_back(0x45ba60U);
    result.final_replay_flag_clear_performed = true;
    return result;
}

} // namespace viper
