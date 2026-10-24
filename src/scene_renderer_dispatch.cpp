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

} // namespace

SceneRendererDispatchPlan plan_scene_renderer_dispatch(
    const SceneRendererDispatchInput& input)
{
    SceneRendererDispatchPlan result{};
    result.camera_transform_after = input.camera_transform;
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
