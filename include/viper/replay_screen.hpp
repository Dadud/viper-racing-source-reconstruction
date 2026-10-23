#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

struct ReplayScreenMenuIteration {
    // Raw value returned by 0x472B30. Enter is 0x0D.
    std::uint32_t menu_result{};
    // Raw result from the nested Replay Analysis menu at 0x472B30.
    std::optional<std::uint32_t> analysis_menu_result;
    // Results of the eight DAT_004D3868 pointer comparisons in 0x408160.
    std::array<std::optional<bool>, 8> analysis_cache_refresh_needed;
};

struct ReplayAnalysisScreenPlan {
    bool input_complete{true};
    bool screen_built{};
    std::uint32_t speed_scale_bits_0x4eb810{0x40a00000U};
    std::uint32_t position_scale_bits_0x4eb80c{0x3f800000U};
    std::uint32_t metric_mode_0x4ec50c{2U};
    struct ControlConstructorCall {
        std::int32_t descriptor_ebp_offset{};
        std::array<std::optional<std::uint32_t>, 14> arguments{};
        // Nonempty only when an argument is a pointer to an EBP-relative local.
        std::array<std::optional<std::int32_t>, 14> stack_pointer_offsets{};
    };
    struct LabelRegistration {
        std::uint32_t object_address{};
        std::uint32_t resource_key_address{};
        std::uint32_t atexit_callback{};
        std::uint32_t flag_bit{};
    };
    struct CacheRefresh {
        std::uint32_t object_address{};
        std::optional<bool> performed;
    };
    std::optional<std::uint32_t> updated_label_registration_mask;
    std::vector<LabelRegistration> label_registrations;
    std::array<CacheRefresh, 8> cache_refreshes{};
    std::vector<ControlConstructorCall> controls;
    std::optional<std::uint8_t> returned_low_byte;
    std::vector<std::uint32_t> projected_calls;
};

struct ReplayAnalysisScreenObservation {
    bool replay_visible{};
    std::optional<std::uint32_t> menu_result;
    // Runtime DWORDs passed to the Load, Save, Full Screen, Analysis, Exit,
    // Long, Lat, and Speed controls, in that order.
    std::array<std::optional<std::uint32_t>, 8> control_values;
    // Existing one-time localized-label registration flags at DAT_004EC788.
    std::optional<std::uint32_t> label_registration_mask_0x4ec788;
    // Results of the eight cache pointer comparisons, in execution order.
    std::array<std::optional<bool>, 8> cache_refresh_needed;
};

// Models the replay-visible guard and the 0x42 exit result in 0x408160.
[[nodiscard]] ReplayAnalysisScreenPlan plan_replay_analysis_screen(
    const ReplayAnalysisScreenObservation& observation);

struct ReplayScreenObservation {
    bool replay_record_argument_present{};
    // DAT_004EC5F0; with no record argument, nonzero preserves the prompt flag.
    bool replay_already_open_0x4ec5f0{};
    // Raw DWORD globals passed into the Load and Full Screen controls.
    std::optional<std::uint32_t> control_value_0x4ec554;
    std::optional<std::uint32_t> control_value_0x4ec804;
    // Existing one-time button registration bits at DAT_004EC704.
    std::optional<std::uint32_t> button_registration_mask_0x4ec704;
    // Results of the three observed cache pointer comparisons, in order.
    std::array<std::optional<bool>, 3> refresh_control_cache;
    // Values used when the analysis page is entered from this screen.
    std::array<std::optional<std::uint32_t>, 8> analysis_control_values;
    std::optional<std::uint32_t> analysis_label_registration_mask_0x4ec788;
    // One observation per 0x472B30 iteration, in execution order.
    std::span<const ReplayScreenMenuIteration> iterations;
};

struct ReplayControlConstructorCall {
    // Stack descriptor address expressed as an EBP-relative signed offset.
    std::int32_t descriptor_ebp_offset{};
    // The 14 arguments following the descriptor pointer at 0x408F50.
    std::array<std::optional<std::uint32_t>, 14> arguments{};
};

struct ReplayButtonRegistration {
    std::uint32_t object_address{};
    std::uint32_t resource_key_address{};
    std::uint32_t atexit_callback{};
    std::uint32_t flag_bit{};
};

struct ReplayCallbackBinding {
    std::uint32_t assignment_instruction{};
    std::uint32_t callback_address{};
    std::uint32_t stack_esp_displacement{};
};

struct ReplayScreenPlan {
    bool input_complete{true};
    bool missing_replay_prompt_shown{};
    bool replay_record_copied_to_0x4eb828{};
    std::uint32_t replay_record_copy_dword_count{};
    std::optional<std::uint32_t> updated_button_registration_mask;
    std::vector<ReplayButtonRegistration> button_registrations;
    std::vector<ReplayCallbackBinding> callback_bindings;
    std::vector<ReplayControlConstructorCall> controls;
    std::vector<ReplayAnalysisScreenPlan> analysis_pages;
    std::vector<std::uint8_t> replay_action_returns;
    bool exited_menu{};
    bool replay_action_requested{};
    bool replay_action_kept_menu_open{};
    bool existing_replay_post_action_called{};
    bool final_replay_flag_clear_performed{};
    // Ordered projected calls; input_complete marks missing flag/cache inputs.
    std::vector<std::uint32_t> calls;
};

// Projects the replay screen's initialization, Enter/action loop and teardown
// in 0x406950. Menu results and the callback's raw byte are runtime inputs.
[[nodiscard]] ReplayScreenPlan plan_replay_screen(
    const ReplayScreenObservation& observation);

} // namespace viper
