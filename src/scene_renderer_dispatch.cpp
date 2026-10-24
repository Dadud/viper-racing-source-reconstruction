#include "viper/scene_renderer_dispatch.hpp"

#include <algorithm>
#include <cstddef>

namespace viper {
namespace {

std::array<float, 12> make_scene_view_transform(
    const std::array<float, 12>& source) noexcept
{
    auto result = source;
    result[6] = -source[6];
    result[7] = -source[7];
    result[8] = -source[8];
    result[0] = static_cast<float>(
        static_cast<double>(result[8]) * source[4]
        - static_cast<double>(source[5]) * result[7]);
    result[1] = static_cast<float>(
        static_cast<double>(source[5]) * result[6]
        - static_cast<double>(source[3]) * result[8]);
    result[2] = static_cast<float>(
        static_cast<double>(result[7]) * source[3]
        - static_cast<double>(result[6]) * source[4]);
    result[9] = static_cast<float>(
        static_cast<double>(result[6]) * -4.0 + source[9]);
    result[10] = static_cast<float>(
        static_cast<double>(result[7]) * -4.0 + source[10]);
    result[11] = static_cast<float>(
        static_cast<double>(result[8]) * -4.0 + source[11]);
    return result;
}

std::array<float, 16> make_device_view_matrix(
    const std::array<float, 12>& transform) noexcept
{
    return {
        transform[0], transform[3], transform[6], 0.0F,
        transform[1], transform[4], transform[7], 0.0F,
        transform[2], transform[5], transform[8], 0.0F,
        static_cast<float>(-(static_cast<double>(transform[0]) * transform[9]
            + static_cast<double>(transform[1]) * transform[10]
            + static_cast<double>(transform[2]) * transform[11])),
        static_cast<float>(-(static_cast<double>(transform[3]) * transform[9]
            + static_cast<double>(transform[4]) * transform[10]
            + static_cast<double>(transform[5]) * transform[11])),
        static_cast<float>(-(static_cast<double>(transform[6]) * transform[9]
            + static_cast<double>(transform[7]) * transform[10]
            + static_cast<double>(transform[8]) * transform[11])),
        1.0F,
    };
}

} // namespace

SceneRendererDispatchPlan plan_scene_renderer_dispatch(
    const SceneRendererDispatchInput& input)
{
    SceneRendererDispatchPlan result{};
    result.camera_transform_after = input.camera_transform;
    result.view_transform_cache_reference_counts_after =
        input.view_transform_cache_reference_counts;
    result.view_transform_cache_flags_after = input.view_transform_cache_flags;
    result.actions.push_back({SceneRendererDispatchActionKind::profiling_begin,
        0x00415090U, 0U, 0U,
        {static_cast<std::int32_t>(input.profiling_context), 0, 0, 0, 0}, {}});

    result.draw_path_enabled = input.renderer_mode > 0
        && input.renderer_state >= 0 && input.renderer_state <= 2;
    if (result.draw_path_enabled) {
        result.actions.push_back({SceneRendererDispatchActionKind::graphics_state_begin,
            0x00448450U});
        result.actions.push_back({SceneRendererDispatchActionKind::graphics_state_begin,
            0x00448440U});
        result.actions.push_back({SceneRendererDispatchActionKind::viewport_set,
            0x004487A0U, 0U, 0U,
            {static_cast<std::int32_t>(input.setup_viewport_origin), 0x40, 0x80, 0x20, 1}, {}});

        const auto view_transform = make_scene_view_transform(input.camera_transform);
        result.camera_transform_after = view_transform;
        result.actions.push_back({SceneRendererDispatchActionKind::view_transform_set,
            0x00448940U, 0U, 0U, {}, view_transform});
        const auto append_transform_install = [&](const std::array<float, 12>& transform,
                                                  const std::int32_t hresult) {
            result.cached_camera_position = {transform[9], transform[10], transform[11]};
            result.cached_camera_right = {transform[6], transform[7], transform[8]};
            result.actions.push_back({SceneRendererDispatchActionKind::transform_cache_invalidate,
                0x00454CA0U, 0U, 0U, {}, transform});
            for (std::size_t i = 0; i < result.view_transform_cache_reference_counts_after.size(); ++i) {
                if (result.view_transform_cache_reference_counts_after[i] > 0) {
                    --result.view_transform_cache_reference_counts_after[i];
                }
                result.view_transform_cache_flags_after[i] = 0;
            }
            const auto matrix = make_device_view_matrix(transform);
            const auto matrix_index = result.view_matrices_installed;
            if (matrix_index < result.installed_view_matrices.size()) {
                result.installed_view_matrices[matrix_index] = matrix;
            }
            ++result.view_matrices_installed;
            result.actions.push_back({SceneRendererDispatchActionKind::device_transform_state_dispatch,
                0x004576E0U, 0U, 0U, {2, 0, 0, 0, 0}, transform, matrix});
            result.actions.push_back({SceneRendererDispatchActionKind::device_view_transform_set,
                0x00458C20U, 0U, 0U, {2, hresult, 0, 0, 0}, transform, matrix});
            if (hresult != 0) {
                result.actions.push_back({SceneRendererDispatchActionKind::device_error_report,
                    0x00452770U, 0U, 0U, {hresult, 0, 0, 0, 0}, {}, {}, "SetTransform"});
                ++result.view_transform_error_reports;
            }
        };
        append_transform_install(view_transform, input.view_transform_hresult[0]);
        result.actions.push_back({SceneRendererDispatchActionKind::scene_state_configure,
            0x00460210U});
        if (input.renderer_mode > 1) {
            result.actions.push_back({SceneRendererDispatchActionKind::camera_update,
                0x00460360U});
        }

        const std::size_t requested = input.object_count > 0
            ? static_cast<std::size_t>(input.object_count)
            : 0U;
        const auto available = std::min(requested, input.objects.size());
        for (std::size_t i = 0; i < available; ++i) {
            const auto& object = input.objects[i];
            if (!object.pointer_present) {
                result.status = SceneRendererDispatchStatus::null_object_pointer;
                break;
            }
            result.actions.push_back({SceneRendererDispatchActionKind::object_virtual_call,
                0U, i, 0x10U});
            const bool mode_permits_draw = !object.virtual_0x10_result
                || input.renderer_mode > 2;
            result.actions.push_back({SceneRendererDispatchActionKind::object_virtual_call,
                0U, i, 0x0cU});
            if (object.virtual_0x0c_result && mode_permits_draw) {
                result.actions.push_back({SceneRendererDispatchActionKind::object_virtual_call,
                    0U, i, 0x14U});
                ++result.object_draw_callbacks;
            }
            ++result.objects_processed;
        }
        if (result.status == SceneRendererDispatchStatus::complete
            && requested > input.objects.size()) {
            result.status = SceneRendererDispatchStatus::object_list_observation_exhausted;
        }

        if (result.status != SceneRendererDispatchStatus::null_object_pointer) {
            result.actions.push_back({SceneRendererDispatchActionKind::graphics_state_end,
                0x004602A0U});
            result.actions.push_back({SceneRendererDispatchActionKind::view_transform_set,
                0x00448940U, 0U, 0U, {}, input.camera_transform});
            append_transform_install(input.camera_transform, input.view_transform_hresult[1]);
            result.camera_transform_after = input.camera_transform;
            result.actions.push_back({SceneRendererDispatchActionKind::viewport_restore,
                0x004487A0U, 0U, 0U,
                {0, 0, input.screen_width, input.screen_height, 0}, {}});
        }
    }

    if (result.status != SceneRendererDispatchStatus::null_object_pointer) {
        result.actions.push_back({SceneRendererDispatchActionKind::profiling_end,
            0x004150A0U, 0U, 0U,
            {static_cast<std::int32_t>(input.profiling_context), 0, 0, 0, 0}, {}});
    }
    return result;
}

} // namespace viper
