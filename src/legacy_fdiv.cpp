#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace viper {

MsvcP5FdivProbeResult evaluate_msvc_p5_fdiv_test(
    const MsvcP5FdivProbeObservation& observation) noexcept
{
    MsvcP5FdivProbeResult result{};
    if (observation.initial_probe_return_nonzero) {
        result.compatibility_enabled = true;
        result.exit = MsvcP5FdivProbeExit::initial_probe_nonzero;
        return result;
    }

    if (!observation.kernel32_loaded) {
        result.exit = MsvcP5FdivProbeExit::kernel32_load_failed;
        return result;
    }
    if (!observation.get_process_affinity_mask_resolved) {
        result.exit = MsvcP5FdivProbeExit::get_process_affinity_mask_missing;
        return result;
    }
    if (!observation.get_current_process_resolved) {
        result.exit = MsvcP5FdivProbeExit::get_current_process_missing;
        return result;
    }
    if (!observation.set_thread_affinity_mask_resolved) {
        result.exit = MsvcP5FdivProbeExit::set_thread_affinity_mask_missing;
        return result;
    }
    if (!observation.get_current_thread_resolved) {
        result.exit = MsvcP5FdivProbeExit::get_current_thread_missing;
        return result;
    }

    result.processor_affinity_query_reached = true;
    if (!observation.process_affinity_query_succeeded) {
        result.exit = MsvcP5FdivProbeExit::affinity_query_failed;
        return result;
    }

    for (std::uint32_t index = 0;
         index < observation.per_processor_probe_nonzero.size(); ++index) {
        const auto processor_bit = std::uint32_t{1} << index;
        if ((observation.process_affinity_mask & processor_bit) == 0U) {
            continue;
        }

        result.processor_bits_tested |= processor_bit;
        ++result.processor_probe_count;
        if (observation.per_processor_probe_nonzero[index]) {
            result.compatibility_enabled = true;
            result.affinity_restore_reached = true;
            result.exit = MsvcP5FdivProbeExit::processor_probe_nonzero;
            return result;
        }
    }

    result.affinity_restore_reached = true;
    result.exit = MsvcP5FdivProbeExit::processor_probes_exhausted;
    return result;
}

bool initialize_msvc_fdiv_compatibility_flag(
    const MsvcP5FdivProbeResult& probe) noexcept
{
    return probe.compatibility_enabled;
}

long double divide_with_vc98_fdiv_compatibility(
    const long double numerator,
    const long double denominator,
    const bool enabled) noexcept
{
    if (!enabled || !std::isfinite(numerator) || !std::isfinite(denominator) ||
        numerator == 0.0L || denominator == 0.0L) {
        return numerator / denominator;
    }

    const auto x87_exponent_field = [](const long double value) noexcept {
        if (value == 0.0L) {
            return 0U;
        }
        if (!std::isfinite(value)) {
            return 0x7fffU;
        }
        int binary_exponent{};
        (void)std::frexp(std::fabs(value), &binary_exponent);
        const int field = binary_exponent - 1 + 16383;
        return static_cast<unsigned>(std::clamp(field, 0, 0x7fff));
    };
    const auto denominator_exponent = x87_exponent_field(denominator);
    const auto numerator_exponent = x87_exponent_field(numerator);
    if (denominator_exponent == 0U || denominator_exponent == 0x7fffU ||
        numerator_exponent == 0U || numerator_exponent == 0x7fffU) {
        return numerator / denominator;
    }

    int denominator_binary_exponent{};
    const auto denominator_fraction =
        std::frexp(std::fabs(denominator), &denominator_binary_exponent) * 2.0L - 1.0L;
    const auto index = static_cast<unsigned>(denominator_fraction * 16.0L);
    constexpr std::array<bool, 16> suspect_significand_groups{
        false, true, false, false, true, false, false, true,
        false, false, true, false, false, true, false, false};
    const auto next_three_bits = static_cast<unsigned>(denominator_fraction * 128.0L) & 7U;
    if (index >= suspect_significand_groups.size() ||
        !suspect_significand_groups[index] || next_three_bits != 7U) {
        return numerator / denominator;
    }

    // The CRT changes the x87 control word, scales both 80-bit operands by
    // 0.9375 (or 1.0625 for numerator exponent 1), restores the word, then
    // divides. Host long double preserves that path only where it is wider.
    const long double scale = numerator_exponent == 1U ? 1.0625L : 0.9375L;
    return (numerator * scale) / (denominator * scale);
}

} // namespace viper
