#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "viper/render_context.hpp"

namespace viper {

struct DisplayModeObservation {
    std::uint32_t width{};
    std::uint32_t height{};
};

enum class FallbackDrawCreationResult : std::uint8_t {
    unavailable,
    created,
    directdraw_create_failed,
    query_interface_failed,
};

struct DirectDrawStartupObservation {
    bool display_enumeration_already_done{}; // DAT_004D9CF0 before 0x44E560
    std::optional<bool> device_selected_after_enumeration;
    std::optional<FallbackDrawCreationResult> fallback_draw_creation_result;
    std::optional<bool> fallback_capability_bit_zero_set;
    std::optional<bool> mode_enumeration_call_succeeded;
    std::vector<DisplayModeObservation> enumerated_display_modes;
    std::optional<bool> exclusive_mode_call_succeeded;
};

struct DisplayModeCallbackResult {
    DisplayModeObservation mode;
    std::optional<std::uint32_t> matched_resolution_flag_index;
    bool callback_returned_continue{};
};

enum class DirectDrawStartupExit : std::uint8_t {
    incomplete,
    initialized,
    no_device_exit_4,
    required_mode_exit_5,
    directdraw_create_exit_2,
    directdraw_query_exit_2,
    exclusive_mode_exit_2,
};

struct DirectDrawStartupPlan {
    bool input_complete{true};
    DirectDrawStartupExit exit{DirectDrawStartupExit::incomplete};
    bool initial_state_cleared{};
    bool display_enumeration_called{};
    bool fallback_device_selection_called{};
    bool fallback_draw_created{};
    bool base_directdraw_interface_released{};
    bool directdraw2_interface_acquired{};
    bool wrapper_allocation_attempted{};
    bool wrapper_object_allocated{};
    bool directdraw2_interface_release_missing_on_allocation_failure{};
    bool directdraw_create_error_logged{};
    bool directdraw2_query_error_logged{};
    bool fallback_capability_checked{};
    bool rejected_fallback_object_released{};
    bool selected_fallback_object_stored{};
    bool no_device_message_logged{};
    bool required_mode_enumerated{};
    bool enum_modes_called{};
    bool mode_enumeration_error_logged{};
    std::uint32_t mode_description_size{0x6C};
    std::uint32_t mode_description_flags{0x1001};
    std::uint32_t mode_surface_caps{0x2000};
    std::vector<DisplayModeCallbackResult> display_mode_callbacks;
    std::vector<bool> recognized_resolution_flags;
    bool mode_640x480_supported{};
    bool required_mode_message_logged{};
    bool exclusive_mode_attempted{};
    bool exclusive_mode_message_logged{};
    bool success_state_cleared{};
    bool initialized_flag_set{};
    std::optional<std::uint32_t> process_exit_code;
    std::vector<std::uint32_t> calls;
};

struct DirectDrawVideoMemoryObservation {
    std::uint32_t reported_video_memory_total{};
    std::uint32_t reported_texture_memory_total{};
    std::uint32_t reported_agp_texture_memory_total{};
};

struct DirectDrawVideoMemoryPlan {
    // 0x44EC00 performs this adjustment in 32-bit DWORD arithmetic.
    std::uint32_t adjusted_video_memory_total{};
    std::uint32_t initial_memory_class{};
    std::uint32_t final_memory_class{};
    bool agp_texture_memory_present{};
    bool low_texture_memory_warning{};
    bool supported_memory_class{};
    std::array<bool, 5> resolution_flags_cleared{};
    std::optional<std::uint32_t> process_exit_code;
};

// Models the available-video-memory classification and optional AGP upgrade
// in 0x44EC00. The three DWORD totals are outputs of DirectDraw observations.
[[nodiscard]] DirectDrawVideoMemoryPlan plan_directdraw_video_memory_class(
    const DirectDrawVideoMemoryObservation& observation) noexcept;

// Models the ordered startup path rooted at 0x44E080. Device enumeration,
// display-mode support, and DirectDraw results are supplied as observations.
[[nodiscard]] DirectDrawStartupPlan plan_directdraw_startup(
    const DirectDrawStartupObservation& observation);

enum class SurfaceLockDisposition : std::uint8_t {
    waiting_for_result,
    locked,
    surface_lost,
    lock_error,
};

struct SurfaceLockObservation {
    // HRESULT returned by the acquired surface's vtable +0x64 method.
    std::vector<std::int32_t> lock_results;
};

struct SurfaceLockPlan {
    SurfaceLockDisposition disposition{SurfaceLockDisposition::waiting_for_result};
    std::uint32_t lock_attempts{};
    bool retry_still_drawing{};
    bool lost_surface_error_reported{};
    bool generic_lock_error_reported{};
    std::optional<std::int32_t> error_hresult;
    std::uint32_t error_helper{0x452770};
};

// Projects 0x458FB0. DDERR_WASSTILLDRAWING repeats the lock call without
// yielding; supplied outcomes end at waiting_for_result if they only contain
// retry values. Surface loss and all other failures use distinct paths.
[[nodiscard]] SurfaceLockPlan plan_surface_lock(
    const SurfaceLockObservation& observation) noexcept;

struct SurfaceRestoreObservation {
    std::int32_t primary_surface_hresult{};
    std::int32_t backbuffer_hresult{};
};

struct SurfaceRestorePlan {
    bool recovery_flag_set{};
    std::uint32_t restore_call_count{};
    std::uint32_t restore_error_count{};
    std::uint32_t error_helper{0x452770};
    std::vector<std::int32_t> failed_hresults;
};

// Projects the failed-acquisition recovery path 0x44EBD0 -> 0x459190.
[[nodiscard]] SurfaceRestorePlan plan_surface_restore(
    const SurfaceRestoreObservation& observation);

struct SurfaceUnlockPlan {
    bool reached{};
    std::uint32_t surface_global{0x004D9CE0U};
    std::uint32_t unlock_method_offset{0x80U};
    std::array<std::uint32_t, 3> ordered_helpers{
        0x00415090U, 0x00459030U, 0x004150A0U};
};

[[nodiscard]] SurfaceUnlockPlan plan_surface_unlock() noexcept;

enum class SurfaceFlipDisposition : std::uint8_t {
    renderer_not_ready,
    waiting_for_result,
    flipped,
    non_lost_error_returned,
    lost_surface_recovered,
};

struct SurfaceFlipObservation {
    bool renderer_ready{};
    // Results from the vtable +0x2C call made by 0x459040.
    std::vector<std::int32_t> flip_results;
    SurfaceRestoreObservation restore{};
    std::uint32_t lost_surface_error_return_word{};
    std::uint32_t restore_return_word{};
};

struct SurfaceFlipOperation {
    std::uint32_t helper{};
    std::uint32_t surface_global{};
    std::int32_t hresult{};
};

struct SurfaceFlipPlan {
    SurfaceFlipDisposition disposition{SurfaceFlipDisposition::renderer_not_ready};
    std::uint32_t surface_global{0x004D9CDCU};
    std::uint32_t flip_method_offset{0x2CU};
    std::uint32_t flip_attempts{};
    bool recovery_flag_set{};
    std::uint32_t return_word{};
    std::optional<SurfaceRestorePlan> restore;
    std::vector<SurfaceFlipOperation> ordered_operations;
};

// Projects 0x44E350 -> 0x459040, including still-drawing retries and the
// lost-surface recovery route. HRESULT and restore outcomes remain observed.
[[nodiscard]] SurfaceFlipPlan plan_surface_flip(
    const SurfaceFlipObservation& observation);

enum class RenderContextAcquisitionDisposition : std::uint8_t {
    renderer_not_ready,
    waiting_for_lock_result,
    lock_failed,
    null_page_pointer,
    acquired,
};

enum class RenderContextAcquisitionOperationKind : std::uint8_t {
    clear_context,
    pump_messages,
    lock_surface,
    report_lost_surface,
    report_lock_error,
    begin_surface_restore,
    restore_primary_surface,
    restore_backbuffer_surface,
    report_restore_error,
    initialize_context,
    report_null_page,
};

struct RenderContextAcquisitionObservation {
    bool renderer_ready{};
    SurfaceLockObservation lock{};
    SurfaceRestoreObservation restore{};
    std::uint8_t format_tag{};
    // Optional raw 0x6C-byte output descriptor supplied to 0x458FB0. The
    // acquired pixel pointer is at +0x28 and pitch is at +0x10.
    std::optional<std::array<std::uint32_t, 0x6cU / sizeof(std::uint32_t)>>
        locked_surface_description;
    // Compatibility observations used when the raw descriptor is absent.
    std::uintptr_t surface_bits{};
    int width{};
    int height{};
    int row_pitch{};
    // Raw helper return words are retained because their high-byte status is
    // propagated by 0x44E210 on non-success exits.
    std::uint32_t message_pump_return_word{};
    std::uint32_t restore_return_word{};
    std::uint32_t null_page_error_return_word{};
    std::uint32_t context_setup_return_word{};
};

struct RenderContextAcquisitionOperation {
    RenderContextAcquisitionOperationKind kind{};
    std::uint32_t helper{};
    std::uint32_t item_index{};
    std::int32_t hresult{};
};

struct RenderContextAcquisitionPlan {
    RenderContextAcquisitionDisposition disposition{
        RenderContextAcquisitionDisposition::renderer_not_ready};
    RenderContextSurface context{};
    SurfaceLockPlan lock{};
    std::optional<SurfaceRestorePlan> restore;
    bool locked_surface_description_used{};
    std::uintptr_t acquired_surface_bits{};
    int acquired_row_pitch{};
    std::uint32_t return_word{};
    std::vector<RenderContextAcquisitionOperation> ordered_operations;
};

// Composes 0x44E210 around the observed lock/restore helpers and the
// 0x449350 surface-to-context field writes. OS, DirectDraw, and helper return
// values remain observations.
[[nodiscard]] RenderContextAcquisitionPlan plan_render_context_acquisition(
    const RenderContextAcquisitionObservation& observation);

} // namespace viper
