#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace viper {

struct SceneRendererObjectObservation {
    bool pointer_present{true};
    bool virtual_0x10_result{};
    bool virtual_0x0c_result{};
};

inline constexpr std::size_t scene_texture_cache_entry_count = 250U;
inline constexpr std::size_t scene_texture_cache_entry_size = 0x28U;

struct SceneTextureCacheReloadOutcome {
    bool succeeded{};
    std::optional<std::uint32_t> replacement_texture_pointer;
};

enum class SceneTextureCacheDeresourceOperationKind : std::uint8_t {
    log_texture_pressure,
    release_cached_surface_slot_0x4540a0,
    release_cached_surface_reference_0x459c30,
    release_texture_reference_0x459bc0,
    increment_deresource_generation_0x4597d0,
    reload_texture_by_name_0x459830,
    clear_texture_pressure_flag,
};

struct SceneTextureCacheDeresourceOperation {
    SceneTextureCacheDeresourceOperationKind kind{};
    std::uint32_t helper{};
    std::size_t entry_index{};
    std::uint32_t name_pointer{};
    // Object pointer for release; pointer-to-slot for the reload helper.
    std::uint32_t texture_pointer_or_slot_address{};
};

struct SceneTextureCacheDeresourceObservation {
    bool out_of_texture_space_0x4db0c8{};
    std::span<const std::uint8_t> table_bytes_0x50a0f8;
    std::uint32_t deresource_generation_0x4dbccc{};
    // One outcome per reached 0x459830 call, in table order.
    std::span<const SceneTextureCacheReloadOutcome> reload_outcomes;
};

struct SceneTextureCacheDeresourcePlan {
    bool reached{};
    bool input_complete{true};
    bool next_out_of_texture_space_0x4db0c8{};
    std::uint32_t next_deresource_generation_0x4dbccc{};
    std::vector<std::uint8_t> next_table_bytes_0x50a0f8;
    std::vector<SceneTextureCacheDeresourceOperation> ordered_operations;
    std::size_t reload_calls{};
    std::size_t reloads_succeeded{};
};

// Projects the 0x453F70 texture-pressure recovery across its 250 rows. The
// resource-manager releases and texture reload results remain observations.
[[nodiscard]] SceneTextureCacheDeresourcePlan
plan_scene_texture_cache_deresource(
    const SceneTextureCacheDeresourceObservation& observation);

enum class SceneRendererDispatchActionKind : std::uint8_t {
    profiling_begin,
    graphics_state_begin,
    viewport_set,
    device_viewport_set,
    device_zbuffer_clear,
    view_transform_set,
    transform_cache_invalidate,
    device_transform_state_dispatch,
    device_view_transform_set,
    device_error_report,
    scene_state_configure,
    camera_update,
    object_virtual_call,
    graphics_state_end,
    viewport_restore,
    profiling_end,
};

struct SceneRendererDispatchAction {
    SceneRendererDispatchActionKind kind{};
    std::uint32_t address{};
    std::size_t object_index{};
    std::uint32_t virtual_slot{};
    std::array<std::int32_t, 6> integer_arguments{};
    std::array<float, 12> transform{};
    std::array<float, 16> matrix_4x4{};
    std::string label;
    std::int32_t result_value{};
    std::array<std::uint32_t, 11> viewport_record{};
};

struct SceneRendererViewportProjection {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
    std::int32_t center_x{};
    std::int32_t center_y{};
    std::array<std::uint32_t, 11> device_record{};
    bool adjusted_division_observation_missing{};
};

enum class SceneRendererDispatchStatus : std::uint8_t {
    complete,
    object_list_observation_exhausted,
    null_object_pointer,
};

struct SceneRendererDispatchInput {
    std::int32_t renderer_mode{}; // DAT_00539408
    std::int32_t renderer_state{}; // DAT_0053A4B4
    std::int32_t object_count{}; // DAT_0053A4C0
    std::span<const SceneRendererObjectObservation> objects{}; // pointers at 0x539430
    std::array<float, 12> camera_transform{}; // DAT_0053A480, three 3x4 rows
    std::uint32_t profiling_context{}; // DAT_004DC1A4
    std::uint32_t setup_viewport_origin{}; // DAT_004DC1AC
    std::int32_t screen_width{}; // DAT_00509A4C
    std::int32_t screen_height{}; // DAT_00509A2C
    std::array<std::int32_t, 8> view_transform_cache_reference_counts{};
    std::array<std::uint32_t, 8> view_transform_cache_flags{};
    std::array<std::int32_t, 2> view_transform_hresult{}; // transformed camera, restore
    bool use_adjusted_division{}; // DAT_004E9754
    std::array<std::optional<float>, 2> adjusted_viewport_aspect_ratios{};
    std::array<std::int32_t, 2> viewport_hresult{}; // inset viewport, full-window restore
    std::array<std::int32_t, 2> zbuffer_clear_hresult{};
};

struct SceneRendererDispatchPlan {
    SceneRendererDispatchStatus status{SceneRendererDispatchStatus::complete};
    bool draw_path_enabled{};
    std::array<float, 12> camera_transform_after{};
    std::array<std::int32_t, 8> view_transform_cache_reference_counts_after{};
    std::array<std::uint32_t, 8> view_transform_cache_flags_after{};
    std::array<float, 3> cached_camera_position{};
    std::array<float, 3> cached_camera_right{};
    std::array<std::array<float, 16>, 2> installed_view_matrices{};
    std::size_t view_matrices_installed{};
    std::size_t view_transform_error_reports{};
    std::array<SceneRendererViewportProjection, 2> viewport_projections{};
    std::size_t viewport_calls{};
    std::size_t zbuffer_clear_calls{};
    std::size_t viewport_error_reports{};
    std::size_t zbuffer_clear_error_reports{};
    std::size_t objects_processed{};
    std::size_t object_draw_callbacks{};
    std::vector<SceneRendererDispatchAction> actions;
};

// Projects the bounded CPU-side flow in 0x45BEB0. Graphics API calls and
// object virtual return values are observations, not executed by this plan.
[[nodiscard]] SceneRendererDispatchPlan plan_scene_renderer_dispatch(
    const SceneRendererDispatchInput& input);

} // namespace viper
