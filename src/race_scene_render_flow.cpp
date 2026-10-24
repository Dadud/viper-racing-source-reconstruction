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
        if (call.address == 0x00422A70U && call.receives_render_context) {
            plan.state5_overlay_reached = true;
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

    if (plan.state5_overlay_reached) {
        auto state5_observation = observation.state5_overlay;
        if (plan.render_context_acquisition &&
            plan.render_context_acquisition->disposition ==
                RenderContextAcquisitionDisposition::acquired) {
            const auto& context = plan.render_context_acquisition->context;
            state5_observation.framebuffer_base_value =
                static_cast<std::uint32_t>(context.surface_bits_0x04);
            state5_observation.framebuffer_pitch_bytes =
                context.viewport.row_pitch_0x10;
            state5_observation.clip_left = context.viewport.viewport_left_0x14;
            state5_observation.clip_top = context.viewport.viewport_top_0x18;
            state5_observation.clip_right = context.viewport.viewport_right_0x1c;
            state5_observation.clip_bottom = context.viewport.viewport_bottom_0x20;
        }
        plan.state5_overlay = state5_numeric_overlay_plan(state5_observation);
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

        if (observation.state0_dashboard_raster.has_value()) {
            auto raster_observation = *observation.state0_dashboard_raster;
            std::vector<SceneDashboardRasterObservation> raster_contexts(
                raster_observation.callback_text.per_call_renderers.begin(),
                raster_observation.callback_text.per_call_renderers.end());
            raster_observation.callback_text.per_call_renderers =
                raster_contexts;
            const auto apply_render_context =
                [&](const RenderContextViewport& context,
                    const std::uint32_t framebuffer_base) {
                    for (auto& renderer : raster_contexts) {
                        renderer.renderer_mode = static_cast<std::uint8_t>(
                            observation.state0_dashboard_render
                                .renderer_mode_0x4d7a48);
                        renderer.clip_left = context.viewport_left_0x14;
                        renderer.clip_top = context.viewport_top_0x18;
                        renderer.clip_right = context.viewport_right_0x1c;
                        renderer.clip_bottom = context.viewport_bottom_0x20;
                        renderer.framebuffer_pitch_bytes =
                            context.row_pitch_0x10;
                        renderer.framebuffer_base = framebuffer_base;
                    }
                };
            if (plan.render_context_acquisition.has_value() &&
                plan.render_context_acquisition->disposition ==
                    RenderContextAcquisitionDisposition::acquired) {
                const auto& context =
                    plan.render_context_acquisition->context;
                apply_render_context(
                    context.viewport,
                    static_cast<std::uint32_t>(context.surface_bits_0x04));
            } else if (plan.optional_dashboard_branch_enabled) {
                apply_render_context(
                    observation.state0_dashboard_render.render_context,
                    observation.state0_dashboard_render.framebuffer_base_value);
            }
            raster_observation.callback_text.per_call_renderers =
                raster_contexts;
            plan.state0_dashboard_text_raster =
                raster_scene_dashboard_core_callback(
                    plan.dashboard_core_callback,
                    raster_observation.callback_text,
                    raster_observation.layout_font,
                    raster_observation.atlas_font);
        }
    }
    if (plan.dashboard_callback_geometry_reached) {
        plan.dashboard_callback_geometry = scene_dashboard_callback_geometry_plan(
            observation.dashboard_callback_geometry);
    }
    if (plan.shared_callback_reached) {
        auto paused_banner_raster_observation =
            observation.shared_paused_banner_raster;
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
        if (paused_banner_raster_observation.has_value() &&
            paused_banner_raster_observation->resource_image.has_value()) {
            auto& paused_render =
                shared_overlay_observation.paused_banner.render;
            const auto& resource_image =
                *paused_banner_raster_observation->resource_image;
            paused_render.resource_text_width = resource_image.width;
            paused_render.resource_text_height = resource_image.height;
            if (!paused_render.load_stp_resource_lookup.has_value()) {
                paused_render.load_stp_resource_found = true;
            }
        }
        if (paused_banner_raster_observation.has_value()) {
            const auto& banner_raster =
                *paused_banner_raster_observation;
            auto layout_font = banner_raster.layout_font;
            auto parsed_font_complete = true;
            if (banner_raster.parsed_layout_font.has_value()) {
                const auto projection = scene_dashboard_font_projection(
                    *banner_raster.parsed_layout_font,
                    banner_raster.layout_font_top_metric_0x44dbc0.value_or(0),
                    banner_raster.layout_font_bottom_metric_0x44dc00.value_or(0));
                layout_font = projection.font;
                parsed_font_complete = projection.input_complete;
                paused_banner_raster_observation->layout_font = layout_font;
            }
            const auto& text_observation = banner_raster.text;
            const auto gates = scene_shared_overlay_gates(
                shared_overlay_observation.overlay_globals);
            auto resource_found = shared_overlay_observation.paused_banner
                .render.load_stp_resource_found;
            if (shared_overlay_observation.paused_banner.render
                    .load_stp_resource_lookup.has_value()) {
                const auto lookup = plan_resource_get(
                    *shared_overlay_observation.paused_banner.render
                         .load_stp_resource_lookup);
                resource_found = lookup.exit ==
                    ResourceGetExit::returned_resource;
            }
            if (gates.paused_banner && !resource_found &&
                text_observation.style14.has_value()) {
                auto text_end = text_observation.text.find('\0');
                if (text_end == std::string_view::npos) {
                    text_end = text_observation.text.size();
                }
                const auto text = text_observation.text.substr(0, text_end);
                const auto style = scene_paused_banner_style14_plan(
                    *text_observation.style14);
                const auto text_layout = layout_scene_dashboard_text(
                    style.text_style, text, layout_font);
                plan.shared_paused_banner_fallback_measure =
                    scene_paused_banner_style14_measure_width(
                        style, text_observation.text_pointer_nonnull,
                        text_layout.measured_width);
                if (text_observation.text_pointer_nonnull &&
                    !parsed_font_complete) {
                    plan.shared_paused_banner_fallback_measure
                        ->input_complete = false;
                }
                if (plan.shared_paused_banner_fallback_measure->input_complete) {
                    shared_overlay_observation.paused_banner.render
                        .fallback_text_width = plan
                        .shared_paused_banner_fallback_measure->returned_width;
                }
            }
        }
        plan.shared_overlay = scene_shared_overlay_frame_plan(
            shared_overlay_observation);
        if (plan.shared_paused_banner_fallback_measure.has_value() &&
            !plan.shared_paused_banner_fallback_measure->input_complete) {
            plan.shared_overlay.paused_banner.inner_render.input_complete =
                false;
        }
        if (paused_banner_raster_observation.has_value() &&
            plan.shared_overlay.paused_banner.inner_render.draws_text_handle) {
            auto raster_observation = *paused_banner_raster_observation;
            std::vector<SceneDashboardRasterObservation> line_renderers(
                raster_observation.text.per_line_renderers.begin(),
                raster_observation.text.per_line_renderers.end());
            const auto apply_context = [&](
                const RenderContextViewport& context,
                const std::uint8_t renderer_mode,
                const std::uint32_t framebuffer_base) {
                for (auto& renderer : line_renderers) {
                    renderer.renderer_mode = renderer_mode;
                    renderer.clip_left = context.viewport_left_0x14;
                    renderer.clip_top = context.viewport_top_0x18;
                    renderer.clip_right = context.viewport_right_0x1c;
                    renderer.clip_bottom = context.viewport_bottom_0x20;
                    renderer.framebuffer_pitch_bytes = context.row_pitch_0x10;
                    renderer.framebuffer_base = framebuffer_base;
                }
            };
            if (plan.render_context_acquisition.has_value() &&
                plan.render_context_acquisition->disposition ==
                    RenderContextAcquisitionDisposition::acquired) {
                const auto& context =
                    plan.render_context_acquisition->context;
                const auto renderer_mode =
                    plan.optional_dashboard_branch_enabled
                    ? static_cast<std::uint8_t>(
                        observation.state0_dashboard_render
                            .renderer_mode_0x4d7a48)
                    : shared_overlay_observation.renderer_mode_0x4d7a48;
                apply_context(
                    context.viewport,
                    renderer_mode,
                    static_cast<std::uint32_t>(context.surface_bits_0x04));
            } else if (plan.optional_dashboard_branch_enabled) {
                apply_context(
                    observation.state0_dashboard_render.render_context,
                    static_cast<std::uint8_t>(
                        observation.state0_dashboard_render
                            .renderer_mode_0x4d7a48),
                    observation.state0_dashboard_render
                        .framebuffer_base_value);
            }
            raster_observation.text.per_line_renderers = line_renderers;
            plan.shared_paused_banner_text_raster =
                raster_scene_paused_banner_text(
                    plan.shared_overlay.paused_banner.inner_render,
                    raster_observation.text,
                    raster_observation.layout_font,
                    raster_observation.atlas_font);
            if (raster_observation.parsed_layout_font.has_value() &&
                !raster_observation.parsed_layout_font->glyph_table_complete) {
                plan.shared_paused_banner_text_raster->input_complete = false;
            }
        }
        if (paused_banner_raster_observation.has_value() &&
            plan.shared_overlay.paused_banner.inner_render.draws_resource_text &&
            paused_banner_raster_observation->resource_image.has_value()) {
            auto resource_renderer =
                paused_banner_raster_observation->resource_renderer;
            const auto apply_resource_context = [&] (
                const RenderContextViewport& context,
                const std::uint8_t renderer_mode,
                const std::uint32_t framebuffer_base) {
                resource_renderer.renderer_mode = renderer_mode;
                resource_renderer.clip_left = context.viewport_left_0x14;
                resource_renderer.clip_top = context.viewport_top_0x18;
                resource_renderer.clip_right = context.viewport_right_0x1c;
                resource_renderer.clip_bottom = context.viewport_bottom_0x20;
                resource_renderer.framebuffer_pitch_bytes =
                    context.row_pitch_0x10;
                resource_renderer.framebuffer_base = framebuffer_base;
            };
            if (plan.render_context_acquisition.has_value() &&
                plan.render_context_acquisition->disposition ==
                    RenderContextAcquisitionDisposition::acquired) {
                const auto& context =
                    plan.render_context_acquisition->context;
                const auto renderer_mode =
                    plan.optional_dashboard_branch_enabled
                    ? static_cast<std::uint8_t>(
                        observation.state0_dashboard_render
                            .renderer_mode_0x4d7a48)
                    : shared_overlay_observation.renderer_mode_0x4d7a48;
                apply_resource_context(
                    context.viewport,
                    renderer_mode,
                    static_cast<std::uint32_t>(context.surface_bits_0x04));
            } else if (plan.optional_dashboard_branch_enabled) {
                apply_resource_context(
                    observation.state0_dashboard_render.render_context,
                    static_cast<std::uint8_t>(
                        observation.state0_dashboard_render
                            .renderer_mode_0x4d7a48),
                    observation.state0_dashboard_render
                        .framebuffer_base_value);
            }
            const ScenePausedBannerResourceRasterObservation resource_observation{
                true,
                *paused_banner_raster_observation->resource_image,
                std::move(resource_renderer),
            };
            plan.shared_paused_banner_resource_raster =
                raster_scene_paused_banner_resource(
                    plan.shared_overlay.paused_banner.inner_render,
                    resource_observation);
        }
        const auto& text_effects = plan.shared_overlay.text_effects;
        if (text_effects.predicate_0x4048e0.has_text_draw ||
            text_effects.predicate_0x404900.has_text_draw) {
            plan.shared_text_effects_raster_reached = true;
            const auto& raster = observation.shared_text_effects_raster;
            plan.shared_text_effects_raster =
                raster_scene_shared_text_effects(
                    text_effects, raster.text, raster.layout_font,
                    raster.atlas_font);
        }
        if (plan.shared_overlay.race_results.gates.race_results) {
            plan.shared_results_raster_reached = true;
            const auto& results = plan.shared_overlay.race_results;
            const auto& raster = observation.shared_results_raster;
            plan.shared_results_heading_raster =
                raster_scene_shared_results_heading_text(
                    results.heading, raster.heading, raster.layout_font,
                    raster.atlas_font);
            plan.shared_results_rows_raster = raster_scene_shared_results_rows(
                results.row_draws, raster.row_renderers, raster.layout_font,
                raster.atlas_font);
        }
    }
    return plan;
}

} // namespace viper
