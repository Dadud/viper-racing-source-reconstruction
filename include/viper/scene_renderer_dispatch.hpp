#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace viper {

struct SceneRendererObjectObservation {
    bool pointer_present{true};
    bool virtual_0x10_result{};
    bool virtual_0x0c_result{};
};

enum class SceneRendererDispatchActionKind : std::uint8_t {
    profiling_begin,
    graphics_state_begin,
    viewport_set,
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
    std::array<std::int32_t, 5> integer_arguments{};
    std::array<float, 12> transform{};
    std::array<float, 16> matrix_4x4{};
    std::string label;
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
    std::size_t objects_processed{};
    std::size_t object_draw_callbacks{};
    std::vector<SceneRendererDispatchAction> actions;
};

// Projects the bounded CPU-side flow in 0x45BEB0. Graphics API calls and
// object virtual return values are observations, not executed by this plan.
[[nodiscard]] SceneRendererDispatchPlan plan_scene_renderer_dispatch(
    const SceneRendererDispatchInput& input);

} // namespace viper
