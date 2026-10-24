#include "viper/race_participants.hpp"
#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <utility>

namespace viper {
namespace {

std::string c_string_prefix(const std::string_view value)
{
    return std::string(value.substr(0, value.find('\0')));
}

} // namespace

std::array<std::int32_t, single_race_driver_order_capacity>
initialize_single_race_driver_order() noexcept
{
    std::array<std::int32_t, single_race_driver_order_capacity> values{};
    for (std::size_t index = 0; index < values.size(); ++index)
        values[index] = static_cast<std::int32_t>(index);
    return values;
}

SingleRaceEntrantPlan build_single_race_entrants(
    const SingleRaceEntrantObservation& observation)
{
    SingleRaceEntrantPlan result{};
    result.opponent_strength_global_value = observation.opponent_strength_value;
    result.participant_count = observation.configured_car_count;

    // x86 SUB of one is modulo 2^32. 0x41C9B0 searches this exact value in
    // the driver-order table; the match's ordinal becomes the human row.
    const auto car_count_word =
        static_cast<std::uint32_t>(observation.configured_car_count);
    const auto selector = std::bit_cast<std::int32_t>(car_count_word - 1U);
    const auto selected = std::find(
        observation.driver_order_values.begin(),
        observation.driver_order_values.end(),
        selector);

    std::optional<std::size_t> player_index;
    if (selected != observation.driver_order_values.end()) {
        player_index = static_cast<std::size_t>(
            selected - observation.driver_order_values.begin());
    } else {
        result.driver_order_lookup_panicked = true;
        if (!observation.panic_helper_returned.has_value()) {
            result.input_complete = false;
            result.panic_continuation_unknown = true;
            return result;
        }
        if (!*observation.panic_helper_returned) {
            result.panic_prevented_record_build = true;
            return result;
        }
        player_index = 0;
    }

    result.opponent_strength_global_written = true;
    result.player_record_index = player_index;
    if (result.participant_count <= 0) {
        return result;
    }

    result.records.reserve(static_cast<std::size_t>(result.participant_count));
    for (std::int32_t index = 0; index < result.participant_count; ++index) {
        SingleRaceEntrantRecordPlan record{};
        record.record_index = static_cast<std::size_t>(index);
        record.record_offset = 0x28ULL +
            static_cast<std::uint64_t>(index) * 0xc8ULL;
        const bool ai_marker_nonzero =
            !player_index.has_value() ||
            static_cast<std::size_t>(index) != *player_index;
        record.marker_value = ai_marker_nonzero ? 1U : 0U;

        if (!ai_marker_nonzero) {
            if (observation.resolved_player_name.has_value()) {
                auto name = c_string_prefix(*observation.resolved_player_name);
                if (observation.player_name_ini_key_found && name.size() > 12U) {
                    name.resize(12U);
                } else if (!observation.player_name_ini_key_found &&
                           name.size() > 63U) {
                    // The missing-key helper copies and zero-fills a 0x40-byte
                    // destination.
                    name.resize(63U);
                }
                record.display_name = std::move(name);
            } else {
                result.input_complete = false;
            }
            if (observation.player_car_resource_name.has_value()) {
                record.car_resource_name =
                    c_string_prefix(*observation.player_car_resource_name);
            } else {
                result.input_complete = false;
            }
            if (observation.player_paintjob_value.has_value()) {
                record.word_at_0xc0 = ~static_cast<std::uint32_t>(
                    *observation.player_paintjob_value);
            } else {
                result.input_complete = false;
            }
            record.byte_at_0xc4 =
                observation.global_0x4d10fc_nonzero &&
                observation.global_0x4eccc0_nonzero
                    ? 1U
                    : 0U;
        } else {
            record.car_resource_name = "viper";
            if (static_cast<std::size_t>(index) >=
                observation.driver_order_values.size()) {
                result.input_complete = false;
            } else {
                const auto driver_slot =
                    observation.driver_order_values[
                        static_cast<std::size_t>(index)] % 7;
                if (driver_slot < 0 ||
                    static_cast<std::size_t>(driver_slot) >=
                        observation.ai_names_by_slot.size()) {
                    result.input_complete = false;
                } else {
                    record.display_name = c_string_prefix(
                        observation.ai_names_by_slot[
                            static_cast<std::size_t>(driver_slot)]);
                }
                record.word_at_0xc0 = static_cast<std::uint32_t>(driver_slot);
            }
            record.byte_at_0xc4 = 0;
        }
        result.records.push_back(std::move(record));
    }

    return result;
}

std::optional<CarTrackLineObservation>
make_single_race_car_track_line_observation(
    const SingleRaceEntrantPlan& entrants,
    CarTrackLineObservation placement_observation)
{
    if (entrants.participant_count <= 0 ||
        placement_observation.configured_car_index >=
            static_cast<std::size_t>(entrants.participant_count)) {
        return std::nullopt;
    }

    const auto configured_car_index = placement_observation.configured_car_index;
    const auto record = std::find_if(
        entrants.records.begin(), entrants.records.end(),
        [configured_car_index](const auto& candidate) {
            return candidate.record_index == configured_car_index;
        });
    if (record == entrants.records.end() || !record->car_resource_name.has_value()) {
        return std::nullopt;
    }

    placement_observation.configured_car_count =
        static_cast<std::size_t>(entrants.participant_count);
    placement_observation.raw_subtype = record->marker_value;
    placement_observation.model_name = c_string_prefix(*record->car_resource_name);
    return placement_observation;
}

SingleRaceSetupPostReadPlan plan_single_race_setup_post_read(
    const SingleRaceSetupPostReadObservation& observation)
{
    constexpr std::array<std::string_view, 8> difficulty_resources{{
        "easy.res", "medium.res", "hard.res", "career1.res",
        "career2.res", "career3.res", "career4.res", "career5.res",
    }};

    SingleRaceSetupPostReadPlan result{};
    const auto c_string_prefix = [](const std::string_view value) {
        return value.substr(0, value.find('\0'));
    };
    result.resource_actions.reserve(observation.entrants.size() + 4U);
    result.resource_actions.push_back({
        SingleRaceResourceActionKind::unload_resource_set,
        "ui.res",
        0x004178c0U,
        std::nullopt,
    });
    result.resource_actions.push_back({
        SingleRaceResourceActionKind::load_resource_set,
        "race.res",
        0x00417740U,
        std::nullopt,
    });
    result.resource_actions.push_back({
        SingleRaceResourceActionKind::load_resource_set,
        std::string(c_string_prefix(observation.selected_track_resource_name)) +
            ".trk",
        0x00417740U,
        std::nullopt,
    });

    if (observation.difficulty_resource_index >= 0 &&
        static_cast<std::size_t>(observation.difficulty_resource_index) <
            difficulty_resources.size()) {
        result.resource_actions.push_back({
            SingleRaceResourceActionKind::load_resource_set,
            std::string(difficulty_resources[static_cast<std::size_t>(
                observation.difficulty_resource_index)]),
            0x00417740U,
            std::nullopt,
        });
    } else {
        result.difficulty_index_out_of_range = true;
        result.input_complete = false;
    }

    const auto expected_count = observation.participant_count > 0
        ? static_cast<std::size_t>(observation.participant_count)
        : 0U;
    if (observation.entrants.size() != expected_count) {
        result.input_complete = false;
    }

    const auto entrant_for_row = [&observation](const std::size_t row)
        -> const SingleRaceEntrantRecordPlan* {
        const auto found = std::find_if(
            observation.entrants.begin(), observation.entrants.end(),
            [row](const auto& entrant) { return entrant.record_index == row; });
        if (found == observation.entrants.end()) return nullptr;
        if (std::find_if(std::next(found), observation.entrants.end(),
                [row](const auto& entrant) { return entrant.record_index == row; }) !=
            observation.entrants.end()) return nullptr;
        return &*found;
    };
    const auto available_count = std::min(
        expected_count, observation.entrants.size());
    for (std::size_t index = 0; index < available_count; ++index) {
        const auto* entrant = entrant_for_row(index);
        if (entrant == nullptr) {
            result.input_complete = false;
            continue;
        }
        if (entrant->car_resource_name.has_value()) {
            result.resource_actions.push_back({
                SingleRaceResourceActionKind::load_resource_set,
                std::string(c_string_prefix(*entrant->car_resource_name)) +
                    ".car",
                0x00417740U,
                index,
            });
        } else {
            result.input_complete = false;
        }
    }

    result.entrant_initializers.reserve(available_count);
    for (std::size_t index = 0; index < available_count; ++index) {
        const auto* entrant = entrant_for_row(index);
        if (entrant == nullptr) {
            result.input_complete = false;
            continue;
        }
        const auto marker = entrant->marker_value;
        SingleRaceEntrantInitializerPlan init{};
        init.entrant_index = index;
        init.marker_value = marker;
        switch (marker) {
        case 0:
            if (!observation.global_0x4e322c_nonzero) {
                init.kind = SingleRaceEntrantInitializerKind::player_vehicle;
                init.initializer_address = 0x0045efd0U;
                init.global_0x4e322c_gate_checked = true;
                init.vehicle_record_offset_0x34_used = true;
                init.entrant_car_name_offset_0x39_used = true;
            } else {
                init.kind =
                    SingleRaceEntrantInitializerKind::player_vehicle_skipped_by_global;
                init.global_0x4e322c_gate_checked = true;
            }
            break;
        case 1:
            init.kind = SingleRaceEntrantInitializerKind::ai_vehicle;
            init.initializer_address = 0x0041a000U;
            init.vehicle_record_offset_0x34_used = true;
            init.entrant_car_name_offset_0x39_used = true;
            break;
        case 2:
            init.kind = SingleRaceEntrantInitializerKind::marker_2_noop;
            break;
        default:
            init.kind = SingleRaceEntrantInitializerKind::panic_path;
            result.panic_helper_called = true;
            result.entrant_initializers.push_back(init);
            if (!observation.panic_helper_returned.has_value()) {
                result.input_complete = false;
                result.panic_continuation_unknown = true;
                return result;
            }
            if (!*observation.panic_helper_returned) {
                result.panic_prevented_remaining_initializers = true;
                return result;
            }
            continue;
        }
        result.entrant_initializers.push_back(init);
    }

    return result;
}

VehicleInitializerWrapperPlan plan_vehicle_initializer_wrapper(
    const std::uint32_t initializer_address)
{
    VehicleInitializerWrapperPlan result{};
    result.entry_address = initializer_address;
    switch (initializer_address) {
    case 0x0041a000U:
        result.record_copy_bytes = 0x8cU;
        result.ai_fallback_branch_conditional = true;
        result.ordered_steps = {
            VehicleInitializerStep::test_ai_record_for_4d3e88,
            VehicleInitializerStep::build_ai_fallback_record,
            VehicleInitializerStep::copy_ai_fallback_record_0x8c,
            VehicleInitializerStep::resolve_ai_car_configuration,
            VehicleInitializerStep::resolve_ai_wheel_table,
            VehicleInitializerStep::initialize_ai_vehicle,
            VehicleInitializerStep::write_vehicle_scalar_0x04,
            VehicleInitializerStep::scale_vehicle_scalar_0x4c,
            VehicleInitializerStep::scale_vehicle_scalar_0x48,
        };
        break;
    case 0x0045efd0U:
        result.human_car_setup_loader_address = 0x0045ee40U;
        result.record_copy_bytes = 0x8cU;
        result.ai_fallback_branch_conditional = false;
        result.ordered_steps = {
            VehicleInitializerStep::load_human_car_record_resource,
            VehicleInitializerStep::copy_human_record_0x8c,
        };
        break;
    default:
        result.entry_address = 0;
        result.record_copy_bytes = 0;
        result.ordered_steps.clear();
        break;
    }
    return result;
}

AiVehicleConfigurationPlan plan_ai_vehicle_configuration(
    const AiVehicleConfigurationObservation& observation)
{
    AiVehicleConfigurationPlan result{};
    result.ordered_steps = {
        AiVehicleConfigurationStep::build_configuration_resource_name,
        AiVehicleConfigurationStep::load_configuration_resource,
        AiVehicleConfigurationStep::query_configuration_record,
    };
    if (!observation.configuration_data_available.has_value()) {
        result.input_complete = false;
    } else if (!*observation.configuration_data_available) {
        result.input_complete = true;
        result.fallback_coefficients_written = true;
        result.ordered_steps.push_back(
            AiVehicleConfigurationStep::write_fallback_coefficients);
    } else {
        result.input_complete = true;
        result.ordered_steps.push_back(
            AiVehicleConfigurationStep::calculate_configuration_coefficients);
        const bool loaded_record_valid = observation.loaded_car_file.has_value() &&
            observation.loaded_car_file->resource_version ==
                CarFilePayload::supported_resource_version;
        if (!loaded_record_valid) {
            result.input_complete = false;
        }
        const auto record_word = [&observation, loaded_record_valid](
                                     const std::size_t byte_offset) {
            return loaded_record_valid
                ? observation.loaded_car_file->raw_words[byte_offset / 4U]
                : 0U;
        };
        const auto count = std::bit_cast<std::int32_t>(record_word(0x108U));
        result.coefficient_loop_reached = count > 0;
        result.coefficient_writes = count > 0 ? count : 0;
        result.exact_five_entry_table_selected = count == 5;
        if (!observation.compatibility_division_helper_enabled.has_value()) {
            result.input_complete = false;
        } else {
            result.compatibility_helper_branch =
                *observation.compatibility_division_helper_enabled;
            result.base_division_helper_path =
                *observation.compatibility_division_helper_enabled;
        }

        const std::span<const std::uint32_t> table_bits = count == 5
            ? std::span<const std::uint32_t>(result.exact_five_entry_table_bits)
            : std::span<const std::uint32_t>(result.other_count_table_bits);
        const bool inputs_present =
            observation.vehicle_scalar_0x00.has_value() &&
            observation.call_scale.has_value() &&
            loaded_record_valid;
        const bool table_present = count <= 0 ||
            static_cast<std::size_t>(count) <= table_bits.size();
        if (!inputs_present || !table_present ||
            !observation.compatibility_division_helper_enabled.has_value()) {
            result.input_complete = false;
        } else {
            const auto record_float = [&record_word](const std::size_t byte_offset) {
                return std::bit_cast<float>(record_word(byte_offset));
            };
            const long double numerator =
                static_cast<long double>(record_float(0x1b4U)) * 0.0254F +
                static_cast<long double>(record_float(0x1b0U)) *
                    record_float(0x1acU) * 0.000010000001F;
            const long double divisor =
                (static_cast<long double>(record_float(0xccU)) -
                 record_float(0xc8U)) * *observation.vehicle_scalar_0x00 +
                record_float(0xc8U);
            const long double base_scale = divide_with_vc98_fdiv_compatibility(
                numerator, divisor,
                observation.compatibility_division_helper_enabled.value_or(false));
            const float stored_scale = static_cast<float>(
                base_scale * record_float(0x4cU) * 0.1295748F);
            result.coefficient_values.clear();
            result.coefficient_values.reserve(
                count > 0 ? static_cast<std::size_t>(count) : 0U);
            for (std::int32_t index = 0; index < count; ++index) {
                float denominator = static_cast<float>(
                    static_cast<long double>(std::bit_cast<float>(
                        table_bits[static_cast<std::size_t>(index)])) *
                    *observation.call_scale);
                if (index == 0) {
                    denominator = 39.0F;
                }
                const long double value = divide_with_vc98_fdiv_compatibility(
                    stored_scale, denominator,
                    observation.compatibility_division_helper_enabled.value_or(false));
                result.coefficient_values.push_back(static_cast<float>(value));
            }
            result.coefficient_values_calculated = true;
        }
    }
    result.ordered_steps.push_back(
        AiVehicleConfigurationStep::unload_configuration_resource);
    return result;
}

AiConfiguredSetupPlan build_ai_configured_setup(
    const AiConfiguredSetupObservation& observation)
{
    AiConfiguredSetupPlan result{};
    constexpr auto setup_size = result.setup_words.size() * sizeof(std::uint32_t);
    if (observation.base_setup_payload.size() != setup_size) {
        return result;
    }
    for (std::size_t index = 0; index < result.setup_words.size(); ++index) {
        const auto offset = index * sizeof(std::uint32_t);
        result.setup_words[index] =
            std::to_integer<std::uint32_t>(observation.base_setup_payload[offset]) |
            (std::to_integer<std::uint32_t>(observation.base_setup_payload[offset + 1U]) << 8U) |
            (std::to_integer<std::uint32_t>(observation.base_setup_payload[offset + 2U]) << 16U) |
            (std::to_integer<std::uint32_t>(observation.base_setup_payload[offset + 3U]) << 24U);
    }

    result.setup_words[1U] = std::bit_cast<std::uint32_t>(
        observation.driver_tuning.track_value_0x90 *
        observation.driver_tuning.record_scale_0x20);
    const float setup_scale = observation.driver_tuning.track_value_0x9c *
        observation.driver_tuning.record_scale_0x6c;
    for (const std::size_t index : {0x12U, 0x13U}) {
        result.setup_words[index] = std::bit_cast<std::uint32_t>(
            std::bit_cast<float>(result.setup_words[index]) * setup_scale);
    }
    result.track_driver_scalars_written = true;

    AiVehicleConfigurationObservation coefficient_observation{};
    coefficient_observation.configuration_data_available =
        observation.configuration_data_available;
    coefficient_observation.loaded_car_file = observation.loaded_car_file;
    coefficient_observation.vehicle_scalar_0x00 =
        std::bit_cast<float>(result.setup_words[0]);
    coefficient_observation.call_scale =
        observation.driver_tuning.track_value_0x94;
    coefficient_observation.compatibility_division_helper_enabled =
        observation.compatibility_division_helper_enabled;
    result.coefficient_plan =
        plan_ai_vehicle_configuration(coefficient_observation);
    if (!result.coefficient_plan.input_complete) {
        return result;
    }

    constexpr std::size_t coefficient_start = 0x14U;
    if (result.coefficient_plan.fallback_coefficients_written) {
        for (std::size_t index = 0;
             index < result.coefficient_plan.fallback_coefficient_bits.size();
             ++index) {
            result.setup_words[coefficient_start + index] =
                result.coefficient_plan.fallback_coefficient_bits[index];
        }
    } else {
        if (result.coefficient_plan.coefficient_values.size() !=
                static_cast<std::size_t>(
                    result.coefficient_plan.coefficient_writes) ||
            result.coefficient_plan.coefficient_values.size() >
                result.setup_words.size() - coefficient_start) {
            return result;
        }
        for (std::size_t index = 0;
             index < result.coefficient_plan.coefficient_values.size(); ++index) {
            result.setup_words[coefficient_start + index] = std::bit_cast<std::uint32_t>(
                result.coefficient_plan.coefficient_values[index]);
        }
    }
    result.input_complete = true;
    return result;
}

SingleRaceAISetupPlan prepare_single_race_ai_setups(
    const SingleRaceAISetupObservation& observation)
{
    SingleRaceAISetupPlan result{};
    if (observation.entrants == nullptr || observation.drivers == nullptr ||
        observation.tuning_mode > 1U ||
        static_cast<std::size_t>(observation.driver_group) >= ai_driver_group_count) {
        return result;
    }

    result.input_complete = true;
    for (const auto& entrant : observation.entrants->records) {
        if (entrant.marker_value != 1U) continue;
        if (!entrant.word_at_0xc0.has_value()) {
            result.input_complete = false;
            continue;
        }
        const auto driver_slot = static_cast<std::size_t>(*entrant.word_at_0xc0);
        const auto* driver = find_ai_driver_record(
            *observation.drivers, observation.driver_group, driver_slot);
        if (driver == nullptr) {
            result.input_complete = false;
            continue;
        }
        const auto tuning = select_ai_driver_track_tuning(
            *driver, observation.track_index, observation.tuning_mode);
        if (!tuning.has_value()) {
            result.input_complete = false;
            continue;
        }

        AiConfiguredSetupObservation configured{};
        configured.base_setup_payload = observation.base_setup_payload;
        configured.driver_tuning = *tuning;
        configured.configuration_data_available =
            observation.configuration_data_available;
        configured.loaded_car_file = observation.loaded_car_file;
        configured.compatibility_division_helper_enabled =
            observation.compatibility_division_helper_enabled;
        auto setup = build_ai_configured_setup(configured);
        result.input_complete = result.input_complete && setup.input_complete;
        result.ai_setups.push_back({entrant.record_index,
            static_cast<std::uint32_t>(driver_slot), setup.input_complete,
            std::move(setup)});
    }
    return result;
}

} // namespace viper
