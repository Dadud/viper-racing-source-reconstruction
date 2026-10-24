#include "viper/race_scene_render_flow.hpp"

#include <cstddef>
#include <utility>

namespace viper {

RaceLoopSharedOverlayPlan plan_race_loop_shared_overlay(
    const RaceLoopSharedOverlayObservation& observation)
{
    RaceLoopSharedOverlayPlan plan{};
    auto frame_observation = observation.following_frame;
    if (observation.render_context_acquisition) {
        // First project through the acquisition call site. The success value
        // is filled from the acquisition plan below before the final frame
        // plan is emitted.
        frame_observation.render_context_acquired = true;
        const auto frame_to_acquisition = plan_race_overlay_toggle_following_frame(
            observation.current_callback_state, frame_observation);
        if (frame_to_acquisition.following_frame.render_context_requested) {
            plan.render_context_acquisition = plan_render_context_acquisition(
                *observation.render_context_acquisition);
            frame_observation.render_context_acquired =
                plan.render_context_acquisition->disposition ==
                RenderContextAcquisitionDisposition::acquired;
        }
    }
    plan.state_transition_and_frame = plan_race_overlay_toggle_following_frame(
        observation.current_callback_state, frame_observation);

    for (const auto& call : plan.state_transition_and_frame.following_frame.calls) {
        if (!call.state_indexed_callback) {
            continue;
        }
        if (call.address == 0x004035E0U) {
            plan.dashboard_core_callback_reached = true;
        }
        if (call.address == 0x004052E0U) {
            plan.dashboard_callback_geometry_reached = true;
        }
        if (call.address == 0x00404960U) {
            plan.shared_callback_reached = true;
            plan.shared_callback_wrapper = call.address;
            plan.shared_callback_second_argument = 0U;
        } else if (call.address == 0x004052D0U) {
            plan.shared_callback_reached = true;
            plan.shared_callback_wrapper = call.address;
            plan.shared_callback_second_argument = 1U;
            plan.optional_dashboard_branch_enabled = true;
        }
    }

    if (plan.dashboard_core_callback_reached) {
        plan.dashboard_core_callback = scene_dashboard_core_callback_plan(
            observation.dashboard_core_callback);
        plan.dashboard_core_text_calls = scene_dashboard_text_plan(
            plan.dashboard_core_callback.reached_text_call_site_mask);
        plan.dashboard_supplemental_time_reached =
            plan.dashboard_core_callback.supplemental_time_branch_reached;
        plan.dashboard_supplemental_time =
            plan.dashboard_core_callback.supplemental_time;
        if (observation.dashboard_core_callback.selected_object_type_0x18 != 0) {
            plan.dashboard_test_track_reached = true;
            auto test_track_observation = observation.dashboard_test_track;
            test_track_observation.selected_record_type_0x18 =
                observation.dashboard_core_callback.selected_object_type_0x18;
            test_track_observation.redraw_all =
                plan.dashboard_core_callback.position.redraw_all;
            test_track_observation.redraw_from_prior_state_change =
                observation.dashboard_core_callback.state
                    .dashboard_kind_cache_0x4cf9ec != 2;
            plan.dashboard_test_track = update_scene_dashboard_test_track(
                plan.dashboard_core_callback.next_state,
                test_track_observation);
        }

        for (std::size_t index = 0; index < plan.dashboard_metrics.size(); ++index) {
            auto metric_observation = observation.dashboard_metrics[index];
            metric_observation.slot = index == 0
                ? SceneDashboardMetricSlot::value_0x10
                : SceneDashboardMetricSlot::value_0x14;
            metric_observation.selected_object_type_0x18 =
                observation.dashboard_core_callback.selected_object_type_0x18;
            plan.dashboard_metrics[index] = update_scene_dashboard_metric(
                plan.dashboard_core_callback.next_state,
                metric_observation,
                plan.dashboard_core_callback.position.redraw_all);
        }

        plan.dashboard_auxiliary_readout_reached = true;
        auto auxiliary_observation = observation.dashboard_auxiliary_readout;
        auxiliary_observation.redraw_all =
            plan.dashboard_core_callback.position.redraw_all;
        plan.dashboard_auxiliary_readout = scene_dashboard_auxiliary_readout_plan(
            auxiliary_observation);
    }
    if (plan.dashboard_callback_geometry_reached) {
        plan.dashboard_callback_geometry = scene_dashboard_callback_geometry_plan(
            observation.dashboard_callback_geometry);
    }
    if (plan.shared_callback_reached) {
        plan.dashboard_gear_display_reached = true;
        auto gear_observation = observation.dashboard_gear_display;
        gear_observation.dashboard_path_enabled =
            plan.optional_dashboard_branch_enabled;
        plan.dashboard_gear_display = scene_dashboard_gear_display_plan(
            gear_observation);
        if (plan.optional_dashboard_branch_enabled) {
            auto render_observation = observation.state0_dashboard_render;
            if (plan.dashboard_core_callback_reached) {
                render_observation.dashboard_mask =
                    plan.dashboard_core_callback.next_state
                        .redraw_mask_0x4eb40c;
            }
            plan.state0_dashboard_render_operations = scene_dashboard_render_plan(
                render_observation);
            plan.state0_dashboard_fill_projection =
                scene_dashboard_full_surface_fill_projection(
                    render_observation);
            for (std::size_t index = 0;
                 index < plan.state0_dashboard_render_operations.size();
                 ++index) {
                const auto& operation =
                    plan.state0_dashboard_render_operations[index];
                if (operation.kind !=
                    SceneDashboardRenderOperationKind::bitmap_draw_0x44afd0) {
                    continue;
                }
                auto blit_observation = observation.state0_bitmap_blit;
                blit_observation.destination_x = operation.left;
                blit_observation.destination_y = operation.top;
                blit_observation.bitmap_width = operation.bitmap_width;
                blit_observation.bitmap_height = operation.bitmap_height;
                blit_observation.destination_renderer_mode =
                    static_cast<std::uint8_t>(
                        operation.renderer_mode_0x4d7a48);
                blit_observation.destination_context =
                    render_observation.render_context;
                blit_observation.destination_base_address =
                    render_observation.framebuffer_base_value;
                RaceLoopSharedOverlayPlan::State0BitmapProjection projection{};
                projection.render_operation_index = index;
                projection.blit = scene_dashboard_background_bitmap_blit_plan(
                    blit_observation);
                projection.pixel_writes =
                    scene_dashboard_background_bitmap_pixel_writes(
                        blit_observation,
                        observation.state0_bitmap_source_pixels);
                plan.state0_dashboard_bitmap_projections.push_back(
                    std::move(projection));
            }
        }
        auto shared_overlay_observation = observation.shared_overlay;
        if (plan.optional_dashboard_branch_enabled) {
            shared_overlay_observation.renderer_mode_0x4d7a48 =
                static_cast<std::uint8_t>(
                    observation.state0_dashboard_render.renderer_mode_0x4d7a48);
        }
        if (plan.render_context_acquisition &&
            plan.render_context_acquisition->disposition ==
                RenderContextAcquisitionDisposition::acquired) {
            shared_overlay_observation.render_context =
                plan.render_context_acquisition->context.viewport;
            shared_overlay_observation.framebuffer_base_value =
                static_cast<std::uint32_t>(
                    plan.render_context_acquisition->context
                        .surface_bits_0x04);
        } else if (plan.optional_dashboard_branch_enabled) {
            shared_overlay_observation.render_context =
                observation.state0_dashboard_render.render_context;
            shared_overlay_observation.framebuffer_base_value =
                observation.state0_dashboard_render.framebuffer_base_value;
        }
        plan.shared_overlay = scene_shared_overlay_frame_plan(
            shared_overlay_observation);
    }
    return plan;
}

} // namespace viper
