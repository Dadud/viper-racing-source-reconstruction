#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <vector>

namespace viper {

enum class HumanCarSetupExit {
    incomplete,
    opened_and_read,
    open_failed_with_fallback,
    wrong_size_with_fallback,
};

enum class HumanCarSetupResourceOutcome : std::uint8_t {
    not_found,
    wrong_version,
    wrong_size,
    loaded,
};

enum class HumanCarSetupFallbackCandidate : std::uint8_t {
    caller_param3_ccs,
    caller_param2_ccs,
    default_ccs,
};

struct HumanCarSetupNumericDefaultWrite {
    std::uint8_t dword_index{};
    std::uint32_t value_bits{};
};

struct HumanCarSetupFallbackResourceObservation {
    std::optional<bool> resource_found;
    std::optional<std::uint32_t> resource_version;
    std::optional<std::uint32_t> payload_bytes;
};

struct HumanCarSetupObservation {
    std::optional<bool> file_opened;
    std::optional<std::uint32_t> file_size_bytes;
    std::optional<bool> record_read_succeeded;
    std::optional<std::uint32_t> record_version_at_0x8c;
    // Inputs read by 0x45EDA0 for param_3.ccs, param_2.ccs, and default.ccs.
    // Needed only when 0x45F150 fallback is reached.
    std::array<HumanCarSetupFallbackResourceObservation, 3>
        fallback_resources{};
};

struct HumanCarSetupPlan {
    bool input_complete{true};
    HumanCarSetupExit exit{HumanCarSetupExit::incomplete};
    std::optional<bool> loader_returned_success;
    std::uint32_t file_size_queries{};
    std::uint32_t record_read_bytes{};
    std::uint32_t copied_into_vehicle_record_bytes{0x8cU};
    bool record_read_result_ignored{};
    bool read_failure_panic_called{};
    std::optional<bool> bad_version_warning_reported;
    bool fallback_loader_called{};
    bool fallback_header_initialized{};
    bool fallback_default_setup_label_written{};
    std::optional<HumanCarSetupFallbackCandidate> selected_fallback_candidate;
    std::vector<HumanCarSetupFallbackCandidate> attempted_fallback_candidates;
    std::uint32_t fallback_resource_version{2U};
    std::uint32_t fallback_resource_bytes{0x8cU};
    std::uint32_t fallback_resource_copy_bytes{0x8cU};
    std::uint32_t fallback_resource_type_code{0x43435330U};
    std::uint32_t numeric_default_initializer_calls{};
    std::vector<HumanCarSetupNumericDefaultWrite> numeric_default_writes;
    std::vector<HumanCarSetupResourceOutcome> fallback_resource_outcomes;
    std::uint32_t fallback_version{1U};
    std::uint32_t fallback_record_size{0xd4U};
    std::uint8_t fallback_flags{};
    bool file_handle_closed{};
    std::vector<std::uint32_t> error_string_addresses;
    std::vector<std::uint32_t> calls;
};

// Models 0x45EE40, called by the human vehicle initializer 0x45EFD0.
// File API results and the loaded record's version word remain observations.
[[nodiscard]] HumanCarSetupPlan plan_human_car_setup_load(
    const HumanCarSetupObservation& observation);

} // namespace viper
