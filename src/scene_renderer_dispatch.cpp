#include "viper/scene_renderer_dispatch.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <utility>

namespace viper {

SceneRendererObjectObservation observe_scene_wheel_resource_manager(
    const bool pointer_present,
    const std::uint8_t renderer_gate_0x4dcae4) noexcept
{
    return {pointer_present, false, renderer_gate_0x4dcae4 == 0U,
        0x004623F0U};
}

SceneRendererObjectObservation observe_scene_fixed_effect_object(
    const bool pointer_present) noexcept
{
    return {pointer_present, true, true, 0x004686A0U};
}

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

std::int32_t wrap_add_i32(const std::int32_t left, const std::int32_t right) noexcept
{
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

std::uint32_t float_bits(const float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value);
}

} // namespace

SceneTextureCacheDeresourcePlan plan_scene_texture_cache_deresource(
    const SceneTextureCacheDeresourceObservation& observation)
{
    SceneTextureCacheDeresourcePlan result{};
    result.next_out_of_texture_space_0x4db0c8 =
        observation.out_of_texture_space_0x4db0c8;
    result.next_deresource_generation_0x4dbccc =
        observation.deresource_generation_0x4dbccc;
    constexpr auto required_table_bytes =
        scene_texture_cache_entry_count * scene_texture_cache_entry_size;
    if (!observation.out_of_texture_space_0x4db0c8) {
        return result;
    }

    result.reached = true;
    if (observation.table_bytes_0x50a0f8.size() < required_table_bytes) {
        result.input_complete = false;
        return result;
    }
    result.next_table_bytes_0x50a0f8.assign(
        observation.table_bytes_0x50a0f8.begin(),
        observation.table_bytes_0x50a0f8.begin() + required_table_bytes);
    result.ordered_operations.push_back({
        SceneTextureCacheDeresourceOperationKind::log_texture_pressure,
        0x00410EA0U,
    });

    const auto read_dword = [&result](const std::size_t offset) {
        const auto& bytes = result.next_table_bytes_0x50a0f8;
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
            (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
            (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
    };
    const auto write_dword = [&result](const std::size_t offset,
                                       const std::uint32_t value) {
        auto& bytes = result.next_table_bytes_0x50a0f8;
        for (std::size_t byte = 0; byte < 4U; ++byte) {
            bytes[offset + byte] = static_cast<std::uint8_t>(
                value >> (static_cast<std::uint32_t>(byte) * 8U));
        }
    };

    for (std::size_t index = 0; index < scene_texture_cache_entry_count;
         ++index) {
        const auto row = index * scene_texture_cache_entry_size;
        if (result.next_table_bytes_0x50a0f8[row] == 0U) {
            continue;
        }
        result.ordered_operations.push_back({
            SceneTextureCacheDeresourceOperationKind::release_cached_surface_slot_0x4540a0,
            0x004540A0U,
            index,
            0U,
            read_dword(row + 0x24U),
        });
        if (result.next_table_bytes_0x50a0f8[row + 0x1cU] != 0U) {
            result.ordered_operations.push_back({
                SceneTextureCacheDeresourceOperationKind::release_cached_surface_reference_0x459c30,
                0x00459C30U,
                index,
                0U,
                read_dword(row + 0x24U),
            });
            result.next_table_bytes_0x50a0f8[row + 0x1cU] = 0U;
            write_dword(row + 0x24U, 0U);
        }
        const auto texture_pointer = read_dword(row + 0x20U);
        if (texture_pointer != 0U) {
            result.ordered_operations.push_back({
                SceneTextureCacheDeresourceOperationKind::release_texture_reference_0x459bc0,
                0x00459BC0U,
                index,
                0U,
                texture_pointer,
            });
        }
    }

    result.ordered_operations.push_back({
        SceneTextureCacheDeresourceOperationKind::increment_deresource_generation_0x4597d0,
        0x004597D0U,
    });
    ++result.next_deresource_generation_0x4dbccc;

    std::size_t reload_outcome_index = 0U;
    for (std::size_t index = 0; index < scene_texture_cache_entry_count;
         ++index) {
        const auto row = index * scene_texture_cache_entry_size;
        if (result.next_table_bytes_0x50a0f8[row] == 0U) {
            continue;
        }
        const auto texture_pointer = read_dword(row + 0x20U);
        if (texture_pointer == 0U) {
            continue;
        }
        const auto name_pointer = static_cast<std::uint32_t>(
            0x0050A100U + index * scene_texture_cache_entry_size);
        result.ordered_operations.push_back({
            SceneTextureCacheDeresourceOperationKind::reload_texture_by_name_0x459830,
            0x00459830U,
            index,
            name_pointer,
            0x0050A118U + static_cast<std::uint32_t>(
                index * scene_texture_cache_entry_size),
        });
        ++result.reload_calls;
        if (reload_outcome_index >= observation.reload_outcomes.size()) {
            result.input_complete = false;
            ++reload_outcome_index;
            continue;
        }
        const auto& outcome = observation.reload_outcomes[reload_outcome_index++];
        if (!outcome.succeeded) {
            continue;
        }
        if (!outcome.replacement_texture_pointer.has_value()) {
            result.input_complete = false;
            continue;
        }
        write_dword(row + 0x20U, *outcome.replacement_texture_pointer);
        ++result.reloads_succeeded;
    }
    if (reload_outcome_index != observation.reload_outcomes.size()) {
        result.input_complete = false;
    }
    result.ordered_operations.push_back({
        SceneTextureCacheDeresourceOperationKind::clear_texture_pressure_flag,
        0U,
    });
    result.next_out_of_texture_space_0x4db0c8 = false;
    return result;
}

SceneDeviceFrameBatchPlan plan_scene_device_frame_batch(
    const SceneDeviceFrameBatchObservation& observation,
    const bool texture_pressure_recovery_reached)
{
    SceneDeviceFrameBatchPlan result{};
    result.reached = true;
    result.input_complete = observation.begin_frame_hresult.has_value() &&
        observation.render_state_hresults[0].has_value() &&
        observation.render_state_hresults[1].has_value() &&
        observation.end_frame_hresult.has_value();
    result.ordered_operations.push_back({
        SceneDeviceFrameBatchOperationKind::begin_frame_virtual_call,
        0x00458B60U, 0x28U, 0U, 0U, 0U, 0U,
        observation.begin_frame_hresult,
    });
    result.final_hresult_0x509fc4 = observation.begin_frame_hresult;
    if (observation.begin_frame_hresult.has_value() &&
        *observation.begin_frame_hresult != 0) {
        result.ordered_operations.push_back({
            SceneDeviceFrameBatchOperationKind::log_begin_frame_failure,
            0x00452770U, 0U, 0U, 0U, 0U, 0U,
            observation.begin_frame_hresult,
        });
    }
    result.ordered_operations.push_back({
        SceneDeviceFrameBatchOperationKind::texture_pressure_check_0x453f70,
        0x00453F70U,
    });
    if (texture_pressure_recovery_reached) {
        result.ordered_operations.push_back({
            SceneDeviceFrameBatchOperationKind::texture_pressure_recovery,
            0x00453F70U,
        });
    }

    result.ordered_operations.push_back({
        SceneDeviceFrameBatchOperationKind::no_op_helper_0x454000,
        0x00454000U,
    });
    constexpr std::array<std::uint32_t, 2> state_indices{1U, 0x1BU};
    for (std::size_t index = 0; index < state_indices.size(); ++index) {
        const auto hresult = observation.render_state_hresults[index];
        result.ordered_operations.push_back({
            SceneDeviceFrameBatchOperationKind::set_render_state,
            0x00458BC0U, 0x5CU, state_indices[index], 0U, 0U, 0U,
            hresult,
        });
        result.final_hresult_0x509fc4 = hresult;
        if (hresult.has_value() && *hresult != 0) {
            result.ordered_operations.push_back({
                SceneDeviceFrameBatchOperationKind::log_render_state_failure,
                0x00452770U, 0U, state_indices[index], 0U, 0U, 0U,
                hresult,
            });
        }
        if (index == 0U) {
            result.next_cached_render_state_0x4da17c = 0xFFFFFFFFU;
            result.ordered_operations.push_back({
                SceneDeviceFrameBatchOperationKind::store_cached_render_state,
                0U, 0U, 0U, 0U, 0x004DA17CU, 0xFFFFFFFFU,
            });
        } else {
            result.next_render_state_flag_0x509b3e = 0U;
            result.ordered_operations.push_back({
                SceneDeviceFrameBatchOperationKind::store_cached_render_state,
                0U, 0U, 0U, 0U, 0x00509B3EU, 0U,
            });
        }
    }
    result.ordered_operations.push_back({
        SceneDeviceFrameBatchOperationKind::end_frame_virtual_call,
        0x00458B90U, 0x2CU, 0U, 0U, 0U, 0U,
        observation.end_frame_hresult,
    });
    result.final_hresult_0x509fc4 = observation.end_frame_hresult;
    if (observation.end_frame_hresult.has_value() &&
        *observation.end_frame_hresult != 0) {
        result.ordered_operations.push_back({
            SceneDeviceFrameBatchOperationKind::log_end_frame_failure,
            0x00452770U, 0U, 0U, 0U, 0U, 0U,
            observation.end_frame_hresult,
        });
    }
    return result;
}

SceneRendererDispatchPlan plan_scene_renderer_dispatch(
    const SceneRendererDispatchInput& input)
{
    SceneRendererDispatchPlan result{};
    result.camera_transform_after = input.camera_transform;
    result.view_transform_cache_reference_counts_after =
        input.view_transform_cache_reference_counts;
    result.view_transform_cache_flags_after = input.view_transform_cache_flags;

    const auto append_viewport = [&](const std::int32_t x, const std::int32_t y,
                                     const std::int32_t width, const std::int32_t height,
                                     const bool caller_flag, const std::size_t index) {
        auto& projection = result.viewport_projections[index];
        projection.x = x;
        projection.y = y;
        projection.width = width;
        projection.height = height;
        projection.center_x = wrap_add_i32(x, width / 2);
        projection.center_y = wrap_add_i32(y, height / 2);

        float aspect_ratio{};
        if (input.use_adjusted_division && input.adjusted_viewport_aspect_ratios[index]) {
            aspect_ratio = *input.adjusted_viewport_aspect_ratios[index];
        } else {
            aspect_ratio = static_cast<float>(static_cast<double>(height)
                / static_cast<double>(width));
            projection.adjusted_division_observation_missing =
                input.use_adjusted_division
                && !input.adjusted_viewport_aspect_ratios[index].has_value();
        }
        projection.device_record = {
            0x2cU,
            static_cast<std::uint32_t>(x),
            static_cast<std::uint32_t>(y),
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            float_bits(-1.0F),
            float_bits(aspect_ratio),
            float_bits(2.0F),
            float_bits(0.0F),
            float_bits(static_cast<float>(static_cast<double>(aspect_ratio) * 2.0)),
            float_bits(1.0F),
        };

        SceneRendererDispatchAction viewport_wrapper{};
        viewport_wrapper.kind = SceneRendererDispatchActionKind::viewport_set;
        viewport_wrapper.address = 0x004487A0U;
        viewport_wrapper.integer_arguments = {
            x, y, width, height, caller_flag ? 1 : 0};
        result.actions.push_back(std::move(viewport_wrapper));

        const auto viewport_result = input.viewport_hresult[index];
        SceneRendererDispatchAction set_viewport{};
        set_viewport.kind = SceneRendererDispatchActionKind::device_viewport_set;
        set_viewport.address = 0x00458740U;
        set_viewport.integer_arguments = {x, y, width, height, 0};
        set_viewport.result_value = viewport_result;
        set_viewport.viewport_record = projection.device_record;
        result.actions.push_back(std::move(set_viewport));
        ++result.viewport_calls;
        if (viewport_result != 0) {
            SceneRendererDispatchAction error{};
            error.kind = SceneRendererDispatchActionKind::device_error_report;
            error.address = 0x00452770U;
            error.integer_arguments[0] = viewport_result;
            error.label = "set_viewport";
            result.actions.push_back(std::move(error));
            ++result.viewport_error_reports;
        }

        const auto clear_result = input.zbuffer_clear_hresult[index];
        const auto right = wrap_add_i32(x, width);
        const auto bottom = wrap_add_i32(y, height);
        SceneRendererDispatchAction clear{};
        clear.kind = SceneRendererDispatchActionKind::device_zbuffer_clear;
        clear.address = 0x00458800U;
        clear.integer_arguments = {
            1, caller_flag ? 3 : 2, x, y, right, bottom};
        clear.result_value = clear_result;
        result.actions.push_back(std::move(clear));
        ++result.zbuffer_clear_calls;
        if (clear_result != 0) {
            SceneRendererDispatchAction error{};
            error.kind = SceneRendererDispatchActionKind::device_error_report;
            error.address = 0x00452770U;
            error.integer_arguments[0] = clear_result;
            error.label = "clearing_zbuffer";
            result.actions.push_back(std::move(error));
            ++result.zbuffer_clear_error_reports;
        }
    };

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
        append_viewport(static_cast<std::int32_t>(input.setup_viewport_origin),
            0x40, 0x80, 0x20, true, 0U);

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
            // This dispatcher calls 0x460210 before the object walk. That
            // helper unconditionally stores 1 to DAT_004DCAE4, and the
            // optional camera update has no reference to that global. The
            // wheel manager's vtable target therefore returns false from
            // +0x0C for this frame, regardless of a separately observed
            // pre-frame gate byte.
            const bool is_wheel_resource_manager =
                object.virtual_0x14_target == 0x004623F0U;
            const bool is_fixed_effect_object =
                object.virtual_0x14_target == 0x004686A0U;
            const bool virtual_0x10_result = is_wheel_resource_manager
                ? false : is_fixed_effect_object
                ? true : object.virtual_0x10_result;
            const bool virtual_0x0c_result = is_wheel_resource_manager
                ? false : is_fixed_effect_object
                ? true : object.virtual_0x0c_result;
            result.actions.push_back({SceneRendererDispatchActionKind::object_virtual_call,
                0U, i, 0x10U});
            const bool mode_permits_draw = !virtual_0x10_result
                || input.renderer_mode > 2;
            result.actions.push_back({SceneRendererDispatchActionKind::object_virtual_call,
                0U, i, 0x0cU});
            if (virtual_0x0c_result && mode_permits_draw) {
                result.actions.push_back({SceneRendererDispatchActionKind::object_virtual_call,
                    object.virtual_0x14_target, i, 0x14U});
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
            append_viewport(0, 0, input.screen_width, input.screen_height, false, 1U);
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
