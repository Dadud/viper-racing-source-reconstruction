#pragma once

#include "viper/config_directory.hpp"
#include "viper/startup_command_line.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

inline constexpr std::uint32_t startup_error_already_exists = 183;
inline constexpr std::uint32_t invalid_vxd_handle = 0xffffffffU;

struct SingleInstanceGateObservation {
    bool semaphore_created{};
    std::uint32_t last_error{};
    bool matching_window_found{};
    std::uint32_t close_handle_result{};
};

struct SingleInstanceGateResult {
    bool panic_helper_reached{};
    bool window_lookup_reached{};
    bool show_window_reached{};
    bool close_handle_reached{};
    std::uint32_t show_command{};
    std::uint32_t return_value{};
    bool continue_startup{};
};

struct CanaryGateObservation {
    bool semaphore_created{};
    std::uint32_t last_error{};
    std::uint32_t message_box_result{};
};

struct CanaryGateResult {
    bool get_last_error_reached{};
    bool close_handle_reached{};
    bool warning_message_reached{};
    std::uint32_t message_box_flags{};
    std::uint32_t return_value{};
    bool continue_startup{};
};

struct LoadingWindowObservation {
    int screen_width{};
    int screen_height{};
    bool class_registration_succeeded{};
    bool window_creation_succeeded{};
};

struct LoadingWindowPlan {
    std::uint32_t window_class_style{};
    int class_extra_bytes{};
    int window_extra_bytes{};
    std::uint32_t cursor_resource_id{};
    std::uint32_t background_resource_id{};
    std::uint32_t extended_window_style{};
    std::uint32_t window_style{};
    int x{};
    int y{};
    int width{};
    int height{};
    bool create_window_reached{};
    bool show_window_reached{};
    bool update_window_reached{};
    std::uint32_t show_command{};
};

struct TscCalibrationObservation {
    std::uint32_t performance_counter_overshoot_low{};
    std::uint32_t tsc_start_low{};
    std::uint32_t tsc_end_low{};
};

struct TscCalibrationResult {
    bool valid{};
    std::uint32_t tsc_delta_low{};
    std::int64_t estimated_cycles_per_second{};
    std::int64_t cycles_per_millisecond{};
    std::uint32_t global_0x4d2364{};
    std::uint32_t global_0x4d2368{};
};

struct ClockBackendObservation {
    bool eflags_id_bit_mutable{};
    bool cpuid_leaf_1_available{};
    bool leaf_1_tsc_feature{};
    std::uint32_t logical_processor_count{};
    bool performance_frequency_succeeded{};
    std::uint64_t performance_frequency_value{};
    std::uint64_t performance_counter_value{};
    std::uint64_t prior_fallback_counter_anchor{};
    std::uint64_t initial_tsc_counter{};
    TscCalibrationObservation tsc_calibration{};
};

struct ClockBackendResult {
    bool cpuid_tsc_gate_passed{};
    bool calibrated_tsc_path_selected{};
    bool high_resolution_flag_0x4ef320{};
    bool fallback_clock_callbacks_installed{};
    bool performance_frequency_queried{};
    bool performance_counter_queried{};
    std::uint32_t callback_0x4d235c{};
    std::uint32_t callback_0x4d2350{};
    std::uint32_t callback_0x4d234c{};
    std::uint32_t callback_0x4d2354{};
    std::uint32_t callback_0x4d2360{};
    std::uint32_t callback_0x4d2358{};
    std::uint32_t frequency_per_millisecond_0x4ef310{};
    std::uint64_t counter_anchor_0x4ef318{};
    std::uint64_t tsc_origin_0x4d2370{};
    bool tsc_calibration_reached{};
    TscCalibrationResult tsc_calibration{};
    bool returns_success{};
};

inline constexpr std::size_t critical_section_pool_capacity = 16;
inline constexpr std::uint32_t critical_section_pool_base = 0x004ef9d0U;
inline constexpr std::uint32_t critical_section_pool_stride = 0x1cU;
inline constexpr std::uint32_t critical_section_used_flag_offset = 0x18U;
inline constexpr std::uint32_t startup_file_registry_base = 0x004ed020U;
inline constexpr std::uint32_t startup_file_registry_stride = 0x104U;
inline constexpr std::size_t startup_file_registry_capacity = 32;
inline constexpr std::uint32_t startup_file_active_flag_offset = 0U;
inline constexpr std::uint32_t startup_file_handle_offset = 0x100U;

struct CriticalSectionPoolState {
    std::uint32_t allocated_count{};
    std::array<bool, critical_section_pool_capacity> slot_used{};
};

struct CriticalSectionPoolAllocation {
    bool allocated{};
    bool initialize_call_reached{};
    bool panic_helper_reached{};
    std::size_t slot_index{};
    std::uint32_t storage_address{};
};

inline constexpr std::size_t named_critical_section_capacity = 16;

struct NamedCriticalSectionRecord {
    std::string name;
    std::uint32_t pointer_token{};
};

struct NamedCriticalSectionRegistryState {
    std::size_t registered_count{};
    std::array<NamedCriticalSectionRecord,
               named_critical_section_capacity> records{};
};

struct NamedCriticalSectionRegistrationResult {
    bool manager_lock_entered{};
    bool duplicate_name_panic_reached{};
    bool slot_reused{};
    bool registry_full_panic_reached{};
    bool critical_section_allocation_reached{};
    bool critical_section_allocation_panic_reached{};
    bool invalid_registry_index_store_reached{};
    bool manager_lock_left{};
    std::size_t registry_index{named_critical_section_capacity};
    std::uint32_t returned_one_based_index{};
    CriticalSectionPoolAllocation allocation{};
};

struct ExecutableSectionRawRange {
    std::uint32_t raw_offset{};
    std::uint32_t raw_size{};
};

struct ExecutableOverlayMappingObservation {
    std::string_view module_path;
    bool module_path_query_succeeded{};
    bool executable_open_succeeded{};
    std::uint32_t file_size{};
    std::vector<ExecutableSectionRawRange> sections;
    bool mapping_creation_succeeded{};
    bool view_mapping_succeeded{};
    std::uint32_t mapped_view_token{};
};

struct ExecutableOverlayMappingResult {
    std::array<std::uint32_t, 5> call_addresses{};
    std::size_t call_count{};
    bool module_path_query_reached{};
    bool executable_open_reached{};
    bool file_size_query_reached{};
    bool mapping_creation_reached{};
    bool view_mapping_reached{};
    bool error_log_reached{};
    bool no_overlay_log_reached{};
    bool view_unmap_reached{};
    bool mapping_close_reached{};
    bool file_close_reached{};
    bool mapping_retained{};
    bool view_retained{};
    std::uint32_t file_mapping_access{2U};
    std::uint32_t view_access{4U};
    std::uint32_t maximum_raw_end{};
    std::uint32_t overlay_size{};
    std::uint32_t overlay_pointer_token{};
    bool returns_overlay{};
};

struct StartupExceptionHandlerObservation {
    std::optional<std::uint32_t> prior_control_word;
};

struct StartupExceptionHandlerResult {
    bool filter_install_reached{};
    std::uint32_t filter_address{0x00415680U};
    bool floating_point_control_reached{};
    std::optional<std::uint32_t> prior_control_word;
    std::optional<std::uint32_t> requested_control_word;
    std::uint32_t control_word_mask{0x0008001fU};
    std::uint32_t exception_mask_bits{0x0000007fU};
};

struct StartupInitializationObservation {
    bool already_initialized_0x4d2fbc{};
    ClockBackendObservation clock_backend{};
    bool heap_creation_0x414110_succeeded{};
    bool vxd_allocator_active_0x4161c0{};
    bool timer_callback_signaled{};
    std::uint32_t timer_polls_before_signal{};
    std::optional<bool> timer_worker_thread_creation_succeeded;
    std::optional<bool> background_worker_thread_creation_succeeded;
    std::uint32_t background_task_one_based_index{};
    std::optional<std::uint32_t> exception_log_file_handle;
    ExecutableOverlayMappingObservation executable_overlay{};
    StartupExceptionHandlerObservation exception_handler{};
    std::uint32_t panic_return_value{};
    std::uint32_t cleanup_thunk_return_value{};
    std::uint32_t final_initializer_return_value{};
};

struct StartupTimerProbeResult {
    std::uint32_t timer_period_ms{100U};
    std::uint32_t sleep_interval_ms{200U};
    std::uint32_t maximum_sleep_count{30U};
    std::uint32_t sleep_count{};
    bool timer_thread_create_reached{};
    std::optional<bool> timer_thread_creation_succeeded;
    bool thread_creation_failure_log_reached{};
    bool completion_flag_cleared_before_start{};
    bool panic_message_reached{};
    bool timer_stopped{};
    bool completion_flag_cleared_after_stop{};
};

struct StartupFileRegistryInitializationResult {
    bool reached{};
    bool named_lock_registration_reached{};
    std::uint32_t named_lock_name_address{0x004d1a50U};
    std::uint32_t table_base{startup_file_registry_base};
    std::uint32_t record_stride{startup_file_registry_stride};
    std::size_t records_initialized{};
    std::uint32_t active_flag_offset{startup_file_active_flag_offset};
    std::uint8_t active_flag_value{};
    std::uint32_t handle_offset{startup_file_handle_offset};
    std::uint32_t handle_value{invalid_vxd_handle};
};

struct StartupFileRecord {
    std::string path;
    std::uint32_t handle{invalid_vxd_handle};
};

struct StartupFileRegistryState {
    std::size_t allocated_count{};
    std::array<StartupFileRecord, startup_file_registry_capacity> records{};
};

struct StartupFileHandleRegistrationResult {
    bool panic_helper_reached{};
    bool reused_slot{};
    std::size_t slot_index{startup_file_registry_capacity};
    std::uint32_t one_based_index{};
};

struct StartupExceptionLogResult {
    bool create_file_reached{};
    std::string_view path{"c:\\except.log"};
    std::uint32_t desired_access{0x40000000U};
    std::uint32_t share_mode{1U};
    std::uint32_t creation_disposition{2U};
    std::uint32_t flags_and_attributes{0x80U};
    std::uint32_t template_handle{invalid_vxd_handle};
    std::optional<std::uint32_t> returned_handle;
    std::optional<std::uint32_t> one_based_file_index;
    StartupFileHandleRegistrationResult registration{};
};

struct StartupBackgroundTaskResult {
    bool reached{};
    NamedCriticalSectionRegistrationResult lock_registration{};
    std::uint32_t lock_name_address{0x004d32b0U};
    std::string_view lock_name{"BG"};
    std::string_view task_name{"BGTask"};
    std::uint32_t task_name_address{0x004d32b4U};
    std::uint32_t callback_address{0x004166d0U};
    std::uint32_t task_parameter{0x10U};
    std::optional<bool> thread_creation_succeeded;
    bool thread_creation_failure_log_reached{};
    std::uint32_t task_one_based_index{};
    std::uint32_t callback_pointer_global_value{0x00416720U};
    std::uint32_t callback_mode_global_value{2U};
    std::uint32_t active_global_value{1U};
};

struct StartupInitializationResult {
    std::array<std::uint32_t, 16> call_addresses{};
    std::size_t call_count{};
    bool panic_helper_reached{};
    std::size_t panic_call_count{};
    bool critical_section_gate_reached{};
    bool memory_subsystem_reached{};
    bool memory_subsystem_succeeded{};
    bool heap_create_reached{};
    std::uint32_t heap_create_flags{};
    std::uint32_t heap_initial_size{};
    std::uint32_t heap_maximum_size{};
    bool secondary_allocator_initialized{};
    bool heap_counters_zeroed{};
    bool vxd_allocator_status_checked{};
    bool vxd_allocator_message_reached{};
    StartupTimerProbeResult timer_probe{};
    StartupFileRegistryInitializationResult file_registry_initialization{};
    StartupFileHandleRegistrationResult exception_log_registration{};
    StartupExceptionLogResult exception_log{};
    ExecutableOverlayMappingResult executable_overlay{};
    StartupExceptionHandlerResult exception_handler{};
    NamedCriticalSectionRegistrationResult task_timer_lock{};
    NamedCriticalSectionRegistrationResult file_registry_lock{};
    NamedCriticalSectionRegistrationResult logger_lock{};
    NamedCriticalSectionRegistrationResult logger_output_lock{};
    NamedCriticalSectionRegistrationResult heap_allocator_lock{};
    StartupBackgroundTaskResult background_task{};
    NamedCriticalSectionRegistryState named_lock_registry_after{};
    CriticalSectionPoolAllocation critical_section_allocation{};
    std::uint32_t global_critical_section_pointer_0x4d278c{};
    bool heap_failure_cleanup_reached{};
    bool cleanup_thunk_reached{};
    bool initialized_flag_set_0x4d2fbc{};
    std::uint32_t return_value{};
    bool success_low_byte{};
};

struct StartupShutdownObservation {
    std::uint32_t panic_return_value{};
    std::uint32_t cleanup_thunk_return_value{};
};

struct StartupShutdownResult {
    std::array<std::uint32_t, 16> call_addresses{};
    std::size_t call_count{};
    std::uint32_t return_value{};
    bool panic_helper_reached{};
    bool cleanup_sequence_reached{};
    bool cleanup_thunk_reached{};
    bool initialized_flag_cleared_0x4d2fbc{};
};

struct HandleFinalizationResult {
    std::uint32_t handle_value{};
    std::uint32_t global_value_after{};
    bool close_handle_reached{};
    bool global_reset_to_invalid{};
};

struct VxdOpenResult {
    std::uint32_t handle_value{invalid_vxd_handle};
    std::uint32_t desired_access{0xc0000000U};
    std::uint32_t share_mode{3U};
    std::uint32_t creation_disposition{3U};
    std::uint32_t flags_and_attributes{0x04000000U};
    bool create_file_reached{true};
    bool import_patch_scan_reached{};
    bool returns_success{};
};

struct ApplicationStartupHandoffObservation {
    std::uint32_t module_handle{};
    std::string_view command_line;
    SingleInstanceGateObservation single_instance{};
    CanaryGateObservation canary{};
    LoadingWindowObservation loading_window{};
    StartupInitializationObservation initialization{};
    StartupShutdownObservation shutdown{};
    std::string module_path;
    bool get_module_file_name_succeeded{};
    std::vector<ConfigDirectoryCreateObservation> config_directory_create_observations;
    StartupCommandState initial_command_state{};
    StartupCommandLineObservation command_line_observation{};
    NamedCriticalSectionRegistryState named_critical_sections{};
    StartupFileRegistryState startup_files{};
    bool loading_window_handle_nonzero{};
    bool loading_window_is_window{};
    std::uint32_t vxd_handle{invalid_vxd_handle};
    std::uint32_t semaphore_handle{};
};

struct ApplicationStartupHandoffResult {
    std::array<std::uint32_t, 20> call_addresses{};
    std::size_t call_count{};
    std::uint32_t global_module_handle_0x4d1ca8{};
    std::string_view global_command_line_0x4d1cac;
    bool global_startup_flag_0x4d1cb4_cleared{};
    bool dedicated_mode_0x4d1cbc{};
    std::int16_t dedicated_port{};
    bool initial_gate_reached{};
    bool canary_gate_reached{};
    bool loading_window_setup_reached{};
    bool long_config_directory_warning_reached{};
    bool config_directory_reached{};
    bool process_exit_reached{};
    ConfigDirectoryPlan config_directory{};
    ConfigDirectoryCreationResult config_directory_creation{};
    StartupCommandLineResult command_line_parse{};
    bool command_line_parser_reached{};
    bool frontend_reached{};
    bool loading_window_destroyed{};
    bool shutdown_reached{};
    bool vxd_finalizer_reached{};
    bool semaphore_finalizer_reached{};
    bool dedicated_handler_reached{};
    StartupInitializationResult initialization{};
    StartupShutdownResult shutdown{};
    HandleFinalizationResult vxd_finalization{};
    HandleFinalizationResult semaphore_finalization{};
    VxdOpenResult vxd_open{};
    NamedCriticalSectionRegistryState named_critical_sections{};
    StartupFileRegistryState startup_files{};
    std::uint32_t return_value{};
};

// Reconstructs the named-object and existing-window gate at 0x4120A0.
[[nodiscard]] SingleInstanceGateResult evaluate_single_instance_gate(
    const SingleInstanceGateObservation& observation) noexcept;

// Reconstructs the second named-object check at 0x412050. The canary object
// must already exist for this helper to return the caller's nonzero gate byte.
[[nodiscard]] CanaryGateResult evaluate_canary_gate(
    const CanaryGateObservation& observation) noexcept;

// Reconstructs the centered 300x0x175 loading-window setup at 0x412270.
[[nodiscard]] LoadingWindowPlan plan_loading_window(
    const LoadingWindowObservation& observation) noexcept;

// Models the CPU/timer backend branch inside 0x413610.
[[nodiscard]] ClockBackendResult select_clock_backend(
    const ClockBackendObservation& observation) noexcept;

// Reconstructs the TSC calibration conversion in 0x413610. Long-double
// arithmetic approximates the original x87 extended-precision intermediate.
[[nodiscard]] TscCalibrationResult calibrate_tsc_clock(
    std::uint64_t performance_frequency,
    const TscCalibrationObservation& observation) noexcept;

// Reconstructs the observed branch/call order and low-byte gates in 0x416360.
[[nodiscard]] StartupInitializationResult plan_startup_initialization(
    const StartupInitializationObservation& observation,
    CriticalSectionPoolState& critical_section_pool,
    NamedCriticalSectionRegistryState& named_critical_sections,
    StartupFileRegistryState& startup_files);

// Models 0x416450's initialized-state cleanup order and panic path.
[[nodiscard]] StartupShutdownResult plan_startup_shutdown(
    bool& initialized_flag_0x4d2fbc,
    const StartupShutdownObservation& observation) noexcept;

// Models 0x4161A0's conditional CloseHandle and invalid-sentinel reset for
// global VxD handle 0x4D2F20.
[[nodiscard]] HandleFinalizationResult close_vxd_file_handle(
    std::uint32_t& handle_global_0x4d2f20) noexcept;

// Models 0x412110's unconditional CloseHandle of global 0x4D1CB8. The source
// helper does not reset the global or branch on the CloseHandle result.
[[nodiscard]] HandleFinalizationResult close_startup_semaphore_handle(
    std::uint32_t handle_global_0x4d1cb8) noexcept;

// Projects 0x416080's CreateFileA call and its conditional import-section scan.
[[nodiscard]] VxdOpenResult plan_vxd_open(std::uint32_t create_file_handle) noexcept;

// Composes the observed outer branch/call order in application handoff 0x411F10.
[[nodiscard]] ApplicationStartupHandoffResult plan_application_startup_handoff(
    const ApplicationStartupHandoffObservation& observation,
    CriticalSectionPoolState& critical_section_pool);

// Models 0x4152E0's fixed 16-record allocator and 0x4152C0's initialization
// wrapper. A successful result is the stored critical-section pointer.
[[nodiscard]] CriticalSectionPoolAllocation allocate_critical_section(
    CriticalSectionPoolState& state) noexcept;

// Models 0x4150C0's case-insensitive registry around the 0x4152C0 allocator.
[[nodiscard]] NamedCriticalSectionRegistrationResult register_named_critical_section(
    std::string_view name,
    NamedCriticalSectionRegistryState& registry,
    CriticalSectionPoolState& pool);

// Models 0x4112A0/0x4112E0 after a successful file-handle open.
[[nodiscard]] StartupFileHandleRegistrationResult register_startup_file_handle(
    std::string_view path,
    std::uint32_t handle,
    StartupFileRegistryState& registry);

// Models 0x4154E0's read-only PE mapping and trailing-overlay selection.
[[nodiscard]] ExecutableOverlayMappingResult map_executable_overlay(
    const ExecutableOverlayMappingObservation& observation) noexcept;

} // namespace viper
