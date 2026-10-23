#pragma once

#include <array>
#include <cstdint>

namespace viper {

enum class MsvcP5FdivProbeExit : std::uint8_t {
    initial_probe_nonzero,
    kernel32_load_failed,
    get_process_affinity_mask_missing,
    get_current_process_missing,
    set_thread_affinity_mask_missing,
    get_current_thread_missing,
    affinity_query_failed,
    processor_probe_nonzero,
    processor_probes_exhausted,
};

struct MsvcP5FdivProbeObservation {
    bool initial_probe_return_nonzero{};
    bool kernel32_loaded{};
    bool get_process_affinity_mask_resolved{};
    bool get_current_process_resolved{};
    bool set_thread_affinity_mask_resolved{};
    bool get_current_thread_resolved{};
    bool process_affinity_query_succeeded{};
    std::uint32_t process_affinity_mask{};
    std::array<bool, 32> per_processor_probe_nonzero{};
};

struct MsvcP5FdivProbeResult {
    bool compatibility_enabled{};
    bool processor_affinity_query_reached{};
    bool affinity_restore_reached{};
    std::uint32_t processor_bits_tested{};
    std::uint8_t processor_probe_count{};
    MsvcP5FdivProbeExit exit{MsvcP5FdivProbeExit::kernel32_load_failed};
};

// Reconstructs __ms_p5_test_fdiv at 0x4BE370 from observations of its fixed
// x87 probe, dynamically resolved kernel32 APIs, affinity mask, and per-CPU
// probe results. This function performs no OS calls or processor-affinity changes.
[[nodiscard]] MsvcP5FdivProbeResult evaluate_msvc_p5_fdiv_test(
    const MsvcP5FdivProbeObservation& observation) noexcept;

// Models __fpmath at 0x4BC260 storing the detector's return in DAT_004E9754.
[[nodiscard]] bool initialize_msvc_fdiv_compatibility_flag(
    const MsvcP5FdivProbeResult& probe) noexcept;

// Approximate the common finite-normal correction used by the VC98 FDIV
// helper. Exceptional operands and exact x87 control-word rounding are not
// reproduced; use the result only with the observed compatibility flag.
[[nodiscard]] long double divide_with_vc98_fdiv_compatibility(
    long double numerator,
    long double denominator,
    bool enabled) noexcept;

} // namespace viper
