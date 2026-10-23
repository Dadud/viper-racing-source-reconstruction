#include "viper/race_setup.hpp"

#include <algorithm>
#include <initializer_list>

namespace viper {
namespace {

void append_call(std::vector<RaceSetupTraceOperation>& trace,
                 const std::uint32_t address)
{
    trace.push_back({RaceSetupTraceOperationKind::direct_call, address});
}

} // namespace

std::vector<RaceSetupTraceOperation> trace_race_setup(
    const RaceSetupTraceOptions& options)
{
    std::vector<RaceSetupTraceOperation> trace;
    trace.reserve(38);

    append_call(trace, 0x46ac30);
    if (options.read_race_camera_setting) {
        append_call(trace, 0x46af00);
    }

    for (const std::uint32_t address : {
             0x413800, 0x40aa20, 0x46b4c0, 0x45b8f0, 0x417870,
             0x4031f0, 0x40c2b0, 0x402060, 0x46b900, 0x412d60,
             0x4259b0, 0x40c9d0, 0x425530}) {
        append_call(trace, address);
    }

    append_call(trace, options.use_4020e0_loop ? 0x4020e0 : 0x41ab00);

    for (const std::uint32_t address : {
             0x46b8f0, 0x49beb0, 0x13e40, 0x4256b0, 0x4034b0,
             0x40c2d0, 0x4178b0, 0x45fbd0, 0x423d90, 0x49c2c0,
             0x413810}) {
        append_call(trace, address);
    }

    if (options.post_cleanup_mode_is_zero) {
        if (options.caller_callback_present) {
            trace.push_back({RaceSetupTraceOperationKind::invoke_caller_callback, 0});
        } else {
            append_call(trace, 0x40b3f0);
        }
    }
    if (options.copy_race_results) {
        trace.push_back({RaceSetupTraceOperationKind::copy_race_results, 0});
    }

    for (const std::uint32_t address : {
             0x45ba10, 0x46b500, 0x40aa40, 0x46b910, 0x46af50}) {
        append_call(trace, address);
    }
    return trace;
}

std::optional<RaceSetupEntryRecord> find_race_setup_entry(
    const std::span<const RaceSetupEntryRecord> records,
    const char selector) noexcept
{
    for (const auto& record : records) {
        if (record.selector_0x41 == selector) {
            return record;
        }
    }
    return std::nullopt;
}

std::vector<std::array<std::int32_t, 3>> project_race_setup_results(
    const std::span<const RaceSetupResultSource> sources)
{
    std::vector<std::array<std::int32_t, 3>> result;
    result.reserve(sources.size());
    for (const auto& source : sources) {
        result.push_back({
            source.field_0x0c > 0 ? source.associated_field_0x50 : 0,
            source.field_0x0c,
            source.field_0x04,
        });
    }
    return result;
}

RaceSetupResultsPlan project_race_setup_result_buffer(
    const RaceSetupResultsObservation& observation)
{
    RaceSetupResultsPlan plan;
    plan.result_count_written = observation.result_count;
    plan.output_dword_count = 1;
    if (observation.result_count <= 0) return plan;

    const auto count = static_cast<std::uint32_t>(observation.result_count);
    plan.output_dword_count = 1ULL + static_cast<std::uint64_t>(count) * 3ULL;
    plan.result_buffer_overrun = plan.output_dword_count > plan.zeroed_result_buffer.size();
    const auto observed_count = std::min<std::size_t>(count, observation.indexed_entries.size());
    plan.rows.reserve(observed_count);
    plan.selector_bytes.reserve(observed_count + (observed_count < count ? 1U : 0U));
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto selector = static_cast<std::int8_t>(static_cast<std::uint8_t>(index));
        plan.selector_bytes.push_back(selector);
        if (index >= observation.indexed_entries.size() ||
            !observation.indexed_entries[index]) {
            plan.input_complete = false;
            plan.missing_entry_pointer_crash_possible = true;
            break;
        }
        const auto& source = *observation.indexed_entries[index];
        std::int32_t associated_value = 0;
        if (source.field_0x0c > 0) {
            if (index >= observation.associated_fields_0x50.size() ||
                !observation.associated_fields_0x50[index]) {
                plan.input_complete = false;
                plan.missing_associated_record_crash_possible = true;
                break;
            }
            associated_value = *observation.associated_fields_0x50[index];
        }
        plan.rows.push_back({associated_value, source.field_0x0c, source.field_0x04});
    }
    return plan;
}

RaceSetupPlan plan_race_setup(const RaceSetupObservation& observation)
{
    RaceSetupPlan plan;
    std::int32_t camera_setting = observation.global_0x4cf5b4_before;
    if (observation.global_0x4cf5b0 != 4) {
        plan.race_camera_setting_read = true;
        if (!observation.race_camera_setting_after_read) {
            plan.input_complete = false;
            return plan;
        }
        camera_setting = *observation.race_camera_setting_after_read;
        if (camera_setting == 12) camera_setting = 4;
    }
    if (plan.race_camera_setting_read) {
        plan.camera_setting_written_to_0x4cf5b4 = camera_setting;
        plan.camera_setting_written_to_0x4eb184 = camera_setting;
    }
    plan.camera_setting_passed_to_race_initialization = camera_setting;

    if (!observation.first_mode_byte_is_zero ||
        !observation.post_cleanup_mode_byte_is_zero) {
        plan.input_complete = false;
        return plan;
    }
    if (observation.copy_race_results && !observation.results) {
        plan.input_complete = false;
        return plan;
    }

    RaceSetupTraceOptions options;
    options.read_race_camera_setting = plan.race_camera_setting_read;
    options.use_4020e0_loop = *observation.first_mode_byte_is_zero;
    options.post_cleanup_mode_is_zero = *observation.post_cleanup_mode_byte_is_zero;
    options.caller_callback_present = observation.caller_callback_present;
    options.copy_race_results = observation.copy_race_results;
    plan.call_trace = trace_race_setup(options);
    plan.dashboard_initialization = scene_dashboard_initialization_plan();
    if (observation.copy_race_results) {
        plan.result_buffer = project_race_setup_result_buffer(*observation.results);
        plan.input_complete = plan.input_complete && plan.result_buffer->input_complete;
    }
    plan.camera_setting_persisted = observation.camera_setting_to_persist;
    if (!plan.camera_setting_persisted) plan.input_complete = false;
    return plan;
}

} // namespace viper
