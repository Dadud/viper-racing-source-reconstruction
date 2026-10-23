#include "viper/human_car_setup.hpp"

#include <array>
#include <cstddef>

namespace viper {
namespace {

constexpr std::array<HumanCarSetupNumericDefaultWrite, 31> kNumericDefaults{{
    {0x00U, 0x3f333333U}, {0x14U, 0x402a3d71U}, {0x17U, 0x3f800000U},
    {0x19U, 0x3f000000U}, {0x1aU, 0x00000000U}, {0x1bU, 0x402a3d71U},
    {0x03U, 0x3f000000U}, {0x15U, 0x3fe3d70aU}, {0x16U, 0x3fa66666U},
    {0x13U, 0x3f000000U}, {0x01U, 0x3f800000U}, {0x18U, 0x3f3d70a4U},
    {0x1dU, 0x3f800000U}, {0x06U, 0x3f000000U}, {0x12U, 0x3e4ccccdU},
    {0x02U, 0x3ecccccdU}, {0x04U, 0x3f333333U}, {0x08U, 0x3f666666U},
    {0x0aU, 0x3f333333U}, {0x0fU, 0x3f000000U}, {0x1eU, 0x3f000000U},
    {0x1fU, 0x3f000000U}, {0x20U, 0x3f000000U}, {0x21U, 0x3f000000U},
    {0x1cU, 0x00000000U}, {0x10U, 0x00000000U}, {0x11U, 0x00000000U},
    {0x22U, 0x3f800000U}, {0x0eU, 0x3f333333U}, {0x0cU, 0x3f4ccccdU},
    {0x0dU, 0x3dcccccdU},
}};

void append_numeric_defaults(HumanCarSetupPlan& plan)
{
    ++plan.numeric_default_initializer_calls;
    if (plan.numeric_default_writes.empty()) {
        plan.numeric_default_writes.assign(kNumericDefaults.begin(),
                                           kNumericDefaults.end());
    }
}

void append_error(HumanCarSetupPlan& plan, const std::uint32_t address)
{
    plan.calls.push_back(0x410ea0U);
    plan.error_string_addresses.push_back(address);
}

bool run_fallback(HumanCarSetupPlan& plan,
                  const HumanCarSetupObservation& observation)
{
    constexpr std::array<HumanCarSetupFallbackCandidate, 3> candidates{{
        HumanCarSetupFallbackCandidate::caller_param3_ccs,
        HumanCarSetupFallbackCandidate::caller_param2_ccs,
        HumanCarSetupFallbackCandidate::default_ccs,
    }};

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        plan.calls.push_back(0x45eda0U);
        plan.attempted_fallback_candidates.push_back(candidates[index]);
        plan.calls.push_back(0x4179f0U);
        const auto& resource = observation.fallback_resources[index];
        if (!resource.resource_found.has_value()) {
            plan.input_complete = false;
            return false;
        }

        HumanCarSetupResourceOutcome outcome =
            HumanCarSetupResourceOutcome::not_found;
        if (*resource.resource_found) {
            if (!resource.resource_version.has_value()) {
                plan.input_complete = false;
                return false;
            }
            if (*resource.resource_version != plan.fallback_resource_version) {
                outcome = HumanCarSetupResourceOutcome::wrong_version;
                append_error(plan, 0x004dc940U);
                plan.calls.push_back(0x418160U);
            } else {
                if (!resource.payload_bytes.has_value()) {
                    plan.input_complete = false;
                    return false;
                }
                if (*resource.payload_bytes != plan.fallback_resource_bytes) {
                    outcome = HumanCarSetupResourceOutcome::wrong_size;
                    append_error(plan, 0x004dc914U);
                    plan.calls.push_back(0x418160U);
                } else {
                    outcome = HumanCarSetupResourceOutcome::loaded;
                    plan.calls.push_back(0x418160U);
                }
            }
        }
        plan.fallback_resource_outcomes.push_back(outcome);

        if (outcome == HumanCarSetupResourceOutcome::loaded) {
            plan.selected_fallback_candidate = candidates[index];
            if (index == 1U) {
                append_error(plan, 0x004dcaa4U); // "Using %s"
            } else if (index == 2U) {
                append_error(plan, 0x004dca58U); // caller setup missing
            }
            return true;
        }

        plan.calls.push_back(0x45f2f0U);
        append_numeric_defaults(plan);
    }

    append_error(plan, 0x004dca7cU); // no resource defaults available
    plan.calls.insert(plan.calls.end(), {0x45f2a0U, 0x45f2f0U});
    append_numeric_defaults(plan);
    plan.fallback_default_setup_label_written = true;
    return true;
}

} // namespace

HumanCarSetupPlan plan_human_car_setup_load(
    const HumanCarSetupObservation& observation)
{
    HumanCarSetupPlan result{};
    result.calls.push_back(0x411430U);
    if (!observation.file_opened.has_value()) {
        result.input_complete = false;
        return result;
    }

    if (!*observation.file_opened) {
        append_error(result, 0x004dc9e8U);
        result.calls.push_back(0x45f150U);
        result.fallback_loader_called = true;
        result.fallback_header_initialized = true;
        run_fallback(result, observation);
        result.loader_returned_success = false;
        result.exit = HumanCarSetupExit::open_failed_with_fallback;
        return result;
    }

    result.calls.push_back(0x411540U);
    result.file_size_queries = 1U;
    if (!observation.file_size_bytes.has_value()) {
        result.input_complete = false;
        return result;
    }

    if (*observation.file_size_bytes != 0xd4U) {
        result.calls.push_back(0x411540U);
        result.file_size_queries = 2U;
        append_error(result, 0x004dc9c0U);
        result.calls.push_back(0x45f150U);
        result.fallback_loader_called = true;
        result.fallback_header_initialized = true;
        run_fallback(result, observation);
        result.calls.push_back(0x411500U);
        result.file_handle_closed = true;
        result.loader_returned_success = false;
        result.exit = HumanCarSetupExit::wrong_size_with_fallback;
        return result;
    }

    result.calls.push_back(0x411560U);
    result.record_read_bytes = 0xd4U;
    result.record_read_result_ignored = true;
    if (!observation.record_read_succeeded.has_value()) {
        result.input_complete = false;
    } else if (!*observation.record_read_succeeded) {
        result.calls.push_back(0x411000U);
        result.read_failure_panic_called = true;
    }

    if (!observation.record_version_at_0x8c.has_value()) {
        result.input_complete = false;
    } else {
        const bool bad_version =
            *observation.record_version_at_0x8c != 1U;
        result.bad_version_warning_reported = bad_version;
        if (bad_version) {
            append_error(result, 0x004dc97cU);
        }
    }

    result.calls.push_back(0x411500U);
    result.file_handle_closed = true;
    result.loader_returned_success = true;
    result.exit = HumanCarSetupExit::opened_and_read;
    return result;
}

} // namespace viper
