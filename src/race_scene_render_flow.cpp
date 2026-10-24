#include "viper/race_scene_render_flow.hpp"

#include "viper/language_resource.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace viper {
namespace {

std::optional<std::vector<std::uint8_t>> raster_status_image_source_pixels(
    const StampImage& image, const std::uint8_t renderer_mode,
    const std::int32_t pitch_bytes)
{
    if (image.width <= 0 || image.height <= 0 || pitch_bytes <= 0 ||
        (renderer_mode != 3U && renderer_mode != 4U && renderer_mode != 5U)) {
        return std::nullopt;
    }
    // 0x44D0C0 writes 16-bit palette words through 0x449D70 even when tag 5
    // selects the second palette plane; keep the descriptor's row pitch.
    constexpr auto bytes_per_pixel = 2U;
    const auto minimum_pitch = static_cast<std::uint64_t>(image.width) *
        bytes_per_pixel;
    const auto buffer_size = static_cast<std::uint64_t>(pitch_bytes) *
        static_cast<std::uint64_t>(image.height);
    const auto pixel_count = static_cast<std::uint64_t>(image.width) *
        static_cast<std::uint64_t>(image.height);
    if (minimum_pitch > static_cast<std::uint64_t>(pitch_bytes) ||
        buffer_size > std::numeric_limits<std::size_t>::max() ||
        buffer_size > static_cast<std::uint64_t>(
            std::numeric_limits<std::ptrdiff_t>::max()) ||
        pixel_count > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }

    SceneDashboardRasterObservation renderer{};
    renderer.renderer_mode = renderer_mode;
    renderer.clip_right = image.width;
    renderer.clip_bottom = image.height;
    renderer.framebuffer_pitch_bytes = pitch_bytes;
    const auto raster = raster_scene_stamp_image(
        {true, 0x0044D0C0U, 0, 0, 0}, {true, image, renderer});
    if (!raster.draw_reached || !raster.input_complete) {
        return std::nullopt;
    }

    // 0x4031F0 clears the complete 0x4EB2C0 viewport with color zero before
    // drawing status.stp. Transparent STMP pixels therefore retain zero.
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(buffer_size), 0U);
    for (const auto& write : raster.raster.pixel_writes) {
        if (write.offset_from_framebuffer_base < 0 ||
            write.bytes_per_pixel != bytes_per_pixel) {
            return std::nullopt;
        }
        const auto offset = static_cast<std::uint64_t>(
            write.offset_from_framebuffer_base);
        if (offset > buffer_size || bytes_per_pixel > buffer_size - offset) {
            return std::nullopt;
        }
        for (std::uint32_t byte = 0; byte < bytes_per_pixel; ++byte) {
            pixels[static_cast<std::size_t>(offset + byte)] =
                static_cast<std::uint8_t>(write.value >> (byte * 8U));
        }
    }
    return pixels;
}

} // namespace

std::optional<StampImage> parse_state0_dashboard_gear_image(
    const ResourceBundle& race_bundle)
{
    const auto payload = race_bundle.find_payload(
        "rpm.stp", resource_type_code('S', 'T', 'M', 'P'));
    if (!payload.has_value()) {
        return std::nullopt;
    }
    return parse_stamp_image(*payload, true);
}

std::optional<StampImage> parse_state0_dashboard_background_image(
    const ResourceBundle& race_bundle)
{
    const auto payload = race_bundle.find_payload(
        "status.stp", resource_type_code('S', 'T', 'M', 'P'));
    return payload.has_value() ? parse_stamp_image(*payload, true)
                               : std::nullopt;
}

std::optional<std::vector<std::uint8_t>>
raster_state0_dashboard_background_source(
    const StampImage& image, const std::uint8_t renderer_mode,
    const std::int32_t pitch_bytes)
{
    return raster_status_image_source_pixels(image, renderer_mode, pitch_bytes);
}

bool populate_state0_dashboard_background_source_from_bundle(
    RaceLoopSharedOverlayObservation& observation,
    const ResourceBundle& race_bundle)
{
    const auto image = parse_state0_dashboard_background_image(race_bundle);
    if (!image.has_value()) {
        return false;
    }

    auto renderer_mode =
        observation.state0_bitmap_blit.source_bitmap_mode;
    auto pitch_bytes = observation.state0_bitmap_blit.bitmap_pitch_bytes;
    if (observation.state0_bitmap_descriptor_bytes.has_value()) {
        const auto descriptor = parse_scene_dashboard_bitmap_descriptor(
            *observation.state0_bitmap_descriptor_bytes);
        if (!descriptor.input_complete ||
            descriptor.width != image->width ||
            descriptor.height != image->height) {
            return false;
        }
        renderer_mode = descriptor.source_mode;
        pitch_bytes = descriptor.pitch_bytes;
    } else if ((observation.state0_bitmap_blit.bitmap_width > 0 &&
                observation.state0_bitmap_blit.bitmap_width != image->width) ||
               (observation.state0_bitmap_blit.bitmap_height > 0 &&
                observation.state0_bitmap_blit.bitmap_height != image->height)) {
        return false;
    }

    std::optional<std::vector<std::uint8_t>> derived_pixels;
    if (observation.state0_bitmap_source_pixels.empty()) {
        derived_pixels = raster_state0_dashboard_background_source(
            *image, renderer_mode, pitch_bytes);
        if (!derived_pixels.has_value()) {
            return false;
        }
    }

    observation.state0_dashboard_background_image = *image;
    observation.state0_dashboard_render.background_bitmap_width = image->width;
    observation.state0_dashboard_render.background_bitmap_height = image->height;
    observation.state0_bitmap_blit.source_bitmap_mode = renderer_mode;
    observation.state0_bitmap_blit.bitmap_width = image->width;
    observation.state0_bitmap_blit.bitmap_height = image->height;
    observation.state0_bitmap_blit.bitmap_pitch_bytes = pitch_bytes;
    if (derived_pixels.has_value()) {
        observation.state0_bitmap_source_pixels =
            std::move(*derived_pixels);
    }
    return true;
}

std::optional<StampImage> parse_shared_centered_resource_image(
    const ResourceBundle& race_bundle)
{
    const auto payload = race_bundle.find_payload(
        "award.stp", resource_type_code('S', 'T', 'M', 'P'));
    return payload.has_value() ? parse_stamp_image(*payload, true)
                               : std::nullopt;
}

std::optional<StampImage> parse_shared_cheater_banner_image(
    const ResourceBundle& race_bundle)
{
    const auto payload = race_bundle.find_payload(
        "start.stp", resource_type_code('S', 'T', 'M', 'P'));
    return payload.has_value() ? parse_stamp_image(*payload, true)
                               : std::nullopt;
}

std::optional<StampImage> parse_shared_loading_resource_image(
    const ResourceBundle& common_bundle)
{
    const auto payload = common_bundle.find_payload(
        "Load.stp", resource_type_code('S', 'T', 'M', 'P'));
    return payload.has_value() ? parse_stamp_image(*payload, true)
                               : std::nullopt;
}

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
        auto dashboard_observation = observation.dashboard_core_callback;
        if (dashboard_observation.result_time_records.empty()) {
            dashboard_observation.result_time_records =
                observation.shared_overlay.race_results.result_time_records;
            dashboard_observation.result_time_record_count =
                observation.shared_overlay.race_results.result_time_record_count;
            if (dashboard_observation.final_time_marker == '.' &&
                observation.shared_overlay.race_results.time_marker != '\0') {
                dashboard_observation.final_time_marker =
                    observation.shared_overlay.race_results.time_marker;
            }
        }
        if (dashboard_observation.language_resource == nullptr) {
            dashboard_observation.language_resource =
                observation.language_resource;
        }
        plan.dashboard_core_callback = scene_dashboard_core_callback_plan(
            dashboard_observation);
        plan.dashboard_core_text_calls =
            plan.dashboard_core_callback.ordered_text_calls;
        plan.dashboard_supplemental_time_reached =
            plan.dashboard_core_callback.supplemental_time_branch_reached;
        plan.dashboard_supplemental_time =
            plan.dashboard_core_callback.supplemental_time;
        plan.dashboard_test_track_reached =
            plan.dashboard_core_callback.test_track_branch_reached;
        plan.dashboard_test_track = plan.dashboard_core_callback.test_track;
        plan.dashboard_test_track_localization =
            plan.dashboard_core_callback.test_track_localization;
        plan.dashboard_test_track_locale_bound =
            plan.dashboard_core_callback.test_track_locale_bound;
        plan.dashboard_metrics = plan.dashboard_core_callback.metrics;
        plan.dashboard_auxiliary_readout_reached =
            plan.dashboard_core_callback.auxiliary_readout_reached;
        plan.dashboard_auxiliary_readout =
            plan.dashboard_core_callback.auxiliary_readout;

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
        if (plan.render_context_acquisition.has_value() &&
            plan.render_context_acquisition->disposition ==
                RenderContextAcquisitionDisposition::acquired) {
            const auto& context = plan.render_context_acquisition->context;
            gear_observation.render_context = context.viewport;
            gear_observation.renderer_mode_0x4d7a48 =
                static_cast<std::uint8_t>(
                    observation.state0_dashboard_render
                        .renderer_mode_0x4d7a48);
            gear_observation.framebuffer_base_value =
                static_cast<std::uint32_t>(context.surface_bits_0x04);
        } else if (plan.optional_dashboard_branch_enabled) {
            gear_observation.render_context =
                observation.state0_dashboard_render.render_context;
            gear_observation.renderer_mode_0x4d7a48 =
                static_cast<std::uint8_t>(
                    observation.state0_dashboard_render
                        .renderer_mode_0x4d7a48);
            gear_observation.framebuffer_base_value =
                observation.state0_dashboard_render.framebuffer_base_value;
        }
        plan.dashboard_gear_display = scene_dashboard_gear_display_plan(
            gear_observation);
        if (observation.state0_dashboard_raster.has_value() &&
            plan.dashboard_gear_display.selected_gear_label_call.has_value() &&
            plan.dashboard_gear_display.selected_gear_label_call->formatted_text
                .has_value()) {
            const auto& gear_label =
                *plan.dashboard_gear_display.selected_gear_label_call;
            SceneDashboardTextCall text_call{};
            text_call.call_site = gear_observation.mode_0x45c200 == 0
                ? 0x00404B37U : 0x00404B9AU;
            text_call.x = gear_label.x;
            text_call.y = gear_label.y;
            text_call.text_or_format_source = gear_label.text_source;
            text_call.color_source = 0x004EB314U;
            text_call.font_source = 0x004EB28CU;
            text_call.format_flags = gear_label.format_flags;
            text_call.resolved_text_argument = gear_label.formatted_text;
            const auto layout = layout_scene_dashboard_text(
                text_call, *gear_label.formatted_text,
                observation.state0_dashboard_raster->layout_font);

            SceneDashboardRasterObservation renderer{};
            renderer.renderer_mode = gear_observation.renderer_mode_0x4d7a48;
            renderer.clip_left = gear_observation.render_context.viewport_left_0x14;
            renderer.clip_top = gear_observation.render_context.viewport_top_0x18;
            renderer.clip_right = gear_observation.render_context.viewport_right_0x1c;
            renderer.clip_bottom = gear_observation.render_context.viewport_bottom_0x20;
            renderer.framebuffer_pitch_bytes =
                gear_observation.render_context.row_pitch_0x10;
            renderer.framebuffer_base = gear_observation.framebuffer_base_value;
            renderer.explicit_palette = gear_observation.gear_label_palette_0x4eb314;
            auto label_raster = raster_scene_dashboard_text(
                layout, observation.state0_dashboard_raster->atlas_font,
                renderer);
            if (gear_observation.gear_label_font_resource_value_0x4eb314 != 0U &&
                renderer.explicit_palette.empty()) {
                label_raster.input_complete = false;
            }
            plan.state0_dashboard_gear_label_raster = std::move(label_raster);
        }
        if (plan.optional_dashboard_branch_enabled &&
            gear_observation.mode_0x45c200 == 0) {
            const auto fill = std::find_if(
                plan.dashboard_gear_display.operations.begin(),
                plan.dashboard_gear_display.operations.end(),
                [](const auto& operation) {
                    return operation.kind ==
                        SceneDashboardGearOperationKind::fill_mode_zero_rectangle;
                });
            if (fill != plan.dashboard_gear_display.operations.end()) {
                plan.state0_dashboard_mode_zero_gear_fill_writes =
                    scene_rectangle_fill_pixel_writes(
                        fill->left_or_x, fill->top_or_y, fill->right,
                        fill->bottom, fill->resource_or_fill_argument,
                        gear_observation.render_context,
                        gear_observation.renderer_mode_0x4d7a48,
                        gear_observation.framebuffer_base_value);
                plan.state0_dashboard_mode_zero_gear_fill_complete = true;
            }
        }
        const auto gear_resource_draw = std::find_if(
            plan.dashboard_gear_display.operations.begin(),
            plan.dashboard_gear_display.operations.end(),
            [](const auto& operation) {
                return operation.kind ==
                    SceneDashboardGearOperationKind::draw_gear_resource;
            });
        if (gear_resource_draw !=
                plan.dashboard_gear_display.operations.end() &&
            observation.state0_dashboard_gear_resource_image.has_value()) {
            ScenePausedBannerRenderPlan image_draw_plan{};
            image_draw_plan.reached = true;
            ScenePausedBannerOperation image_draw{};
            image_draw.kind =
                ScenePausedBannerOperationKind::resource_draw_0x44d0c0;
            image_draw.helper = 0x0044D0C0U;
            image_draw.x = gear_resource_draw->left_or_x;
            image_draw.y = gear_resource_draw->top_or_y;
            image_draw.raw_mode = 0;
            image_draw_plan.ordered_operations.push_back(image_draw);

            const auto& context = gear_observation.render_context;
            const ScenePausedBannerResourceRasterObservation image_observation{
                true,
                *observation.state0_dashboard_gear_resource_image,
                {
                    gear_observation.renderer_mode_0x4d7a48,
                    context.viewport_left_0x14,
                    context.viewport_top_0x18,
                    context.viewport_right_0x1c,
                    context.viewport_bottom_0x20,
                    context.row_pitch_0x10,
                    gear_observation.framebuffer_base_value,
                    {},
                },
            };
            plan.state0_dashboard_gear_resource_raster =
                raster_scene_paused_banner_resource(
                    image_draw_plan, image_observation);
        }
        if (plan.optional_dashboard_branch_enabled) {
            auto render_observation = observation.state0_dashboard_render;
            auto bitmap_blit_observation = observation.state0_bitmap_blit;
            auto bitmap_descriptor_complete = true;
            if (observation.state0_bitmap_descriptor_bytes.has_value()) {
                plan.state0_bitmap_descriptor =
                    parse_scene_dashboard_bitmap_descriptor(
                        *observation.state0_bitmap_descriptor_bytes);
                bitmap_descriptor_complete =
                    plan.state0_bitmap_descriptor->input_complete;
                if (bitmap_descriptor_complete) {
                    const auto& descriptor = *plan.state0_bitmap_descriptor;
                    render_observation.background_bitmap_width =
                        descriptor.width;
                    render_observation.background_bitmap_height =
                        descriptor.height;
                    bitmap_blit_observation.source_bitmap_mode =
                        descriptor.source_mode;
                    bitmap_blit_observation.bitmap_base_address =
                        descriptor.pixel_base_address;
                    bitmap_blit_observation.bitmap_width = descriptor.width;
                    bitmap_blit_observation.bitmap_height = descriptor.height;
                    bitmap_blit_observation.bitmap_pitch_bytes =
                        descriptor.pitch_bytes;
                }
            }
            auto bitmap_source_pixels =
                observation.state0_bitmap_source_pixels;
            auto bitmap_source_image_complete = true;
            if (bitmap_source_pixels.empty() &&
                observation.state0_dashboard_background_image.has_value()) {
                const auto& image =
                    *observation.state0_dashboard_background_image;
                if (image.width != bitmap_blit_observation.bitmap_width ||
                    image.height != bitmap_blit_observation.bitmap_height) {
                    bitmap_source_image_complete = false;
                } else {
                    const auto rasterized_source =
                        raster_state0_dashboard_background_source(
                            image,
                            bitmap_blit_observation.source_bitmap_mode,
                            bitmap_blit_observation.bitmap_pitch_bytes);
                    if (rasterized_source.has_value()) {
                        bitmap_source_pixels = *rasterized_source;
                    } else {
                        bitmap_source_image_complete = false;
                    }
                }
            }
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
            auto active_destination_context =
                render_observation.render_context;
            for (std::size_t index = 0;
                 index < plan.state0_dashboard_render_operations.size();
                 ++index) {
                const auto& operation =
                    plan.state0_dashboard_render_operations[index];
                if (operation.kind ==
                    SceneDashboardRenderOperationKind::viewport_0x4498a0) {
                    set_render_context_viewport(
                        active_destination_context, operation.left,
                        operation.top, operation.right, operation.bottom);
                    continue;
                }
                if (operation.kind ==
                    SceneDashboardRenderOperationKind::restore_viewport_0x4498e0) {
                    reset_render_context_viewport(
                        active_destination_context);
                    continue;
                }
                if (operation.kind ==
                    SceneDashboardRenderOperationKind::full_surface_fill_0x44a1b0) {
                    if (plan.state0_dashboard_fill_projection.has_value()) {
                        auto fill_writes = scene_rectangle_fill_pixel_writes(
                            operation.left, operation.top, operation.right,
                            operation.bottom,
                            static_cast<std::uint32_t>(
                                operation.raw_auxiliary_argument),
                            render_observation.render_context,
                            static_cast<std::uint8_t>(
                                render_observation.renderer_mode_0x4d7a48),
                            render_observation.framebuffer_base_value);
                        plan.state0_dashboard_fill_pixel_writes.insert(
                            plan.state0_dashboard_fill_pixel_writes.end(),
                            fill_writes.begin(), fill_writes.end());
                    }
                    continue;
                }
                if (operation.kind !=
                    SceneDashboardRenderOperationKind::bitmap_draw_0x44afd0) {
                    continue;
                }
                auto blit_observation = bitmap_blit_observation;
                blit_observation.destination_x = operation.left;
                blit_observation.destination_y = operation.top;
                blit_observation.bitmap_width = operation.bitmap_width;
                blit_observation.bitmap_height = operation.bitmap_height;
                blit_observation.destination_renderer_mode =
                    static_cast<std::uint8_t>(
                        operation.renderer_mode_0x4d7a48);
                blit_observation.destination_context =
                    active_destination_context;
                blit_observation.destination_base_address =
                    render_observation.framebuffer_base_value;
                RaceLoopSharedOverlayPlan::State0BitmapProjection projection{};
                projection.render_operation_index = index;
                projection.blit = scene_dashboard_background_bitmap_blit_plan(
                    blit_observation);
                projection.pixel_writes =
                    scene_dashboard_background_bitmap_pixel_writes(
                        blit_observation,
                        bitmap_source_pixels);
                const auto expected_pixel_count =
                    static_cast<std::uint64_t>(std::max(0, projection.blit.width)) *
                    static_cast<std::uint64_t>(std::max(0, projection.blit.height));
                projection.input_complete = bitmap_descriptor_complete &&
                    bitmap_source_image_complete &&
                    (!projection.blit.transfer_happened ||
                     projection.pixel_writes.size() == expected_pixel_count);
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
                    *banner_raster.parsed_layout_font);
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
        const auto shared_stamp_renderer = [&](
            SceneDashboardRasterObservation renderer) {
            const auto apply_context = [&](const RenderContextViewport& context,
                                           const std::uint8_t renderer_mode,
                                           const std::uint32_t framebuffer_base) {
                renderer.renderer_mode = renderer_mode;
                renderer.clip_left = context.viewport_left_0x14;
                renderer.clip_top = context.viewport_top_0x18;
                renderer.clip_right = context.viewport_right_0x1c;
                renderer.clip_bottom = context.viewport_bottom_0x20;
                renderer.framebuffer_pitch_bytes = context.row_pitch_0x10;
                renderer.framebuffer_base = framebuffer_base;
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
                    context.viewport, renderer_mode,
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
            return renderer;
        };
        const auto& centered_call = plan.shared_overlay.centered_resource.draw;
        if (centered_call.reached) {
            const auto& input = observation.centered_resource_raster;
            const SceneStampImageRasterCall draw{
                true, centered_call.draw_helper, centered_call.x,
                centered_call.y, centered_call.raw_draw_mode};
            const SceneStampImageRasterObservation raster_observation{
                input.image.has_value(), input.image.value_or(StampImage{}),
                shared_stamp_renderer(input.renderer)};
            plan.shared_centered_resource_raster = raster_scene_stamp_image(
                draw, raster_observation);
        }
        const auto& cheater_call = plan.shared_overlay.cheater_banner.draw;
        if (cheater_call.reached) {
            const auto& input = observation.cheater_banner_raster;
            const SceneStampImageRasterCall draw{
                true, cheater_call.draw_helper, cheater_call.x,
                cheater_call.y, cheater_call.raw_draw_mode};
            const SceneStampImageRasterObservation raster_observation{
                input.image.has_value(), input.image.value_or(StampImage{}),
                shared_stamp_renderer(input.renderer)};
            plan.shared_cheater_banner_raster = raster_scene_stamp_image(
                draw, raster_observation);
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
            auto localized_text = raster_observation.localized_text;
            if (!localized_text.has_value() &&
                observation.language_resource != nullptr) {
                localized_text = observation.language_resource->find(
                    "Dashboard:PausedBanner");
                plan.shared_paused_banner_localization_resolved =
                    localized_text.has_value();
            }
            if (localized_text.has_value()) {
                raster_observation.text.text = *localized_text;
            }
            raster_observation.text.per_line_renderers = line_renderers;
            plan.shared_paused_banner_text_raster =
                raster_scene_paused_banner_text(
                    plan.shared_overlay.paused_banner.inner_render,
                    raster_observation.text,
                    raster_observation.layout_font,
                    raster_observation.atlas_font);
            if (raster_observation.parsed_layout_font.has_value() &&
                (!raster_observation.parsed_layout_font->glyph_table_complete ||
                 !raster_observation.parsed_layout_font->header_metrics_complete)) {
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
        if (plan.shared_overlay.paused_banner.inner_render.reached) {
            const auto* resource_raster = plan
                .shared_paused_banner_resource_raster.has_value()
                ? &*plan.shared_paused_banner_resource_raster
                : nullptr;
            const auto* text_raster = plan
                .shared_paused_banner_text_raster.has_value()
                ? &*plan.shared_paused_banner_text_raster
                : nullptr;
            plan.shared_paused_banner_frame_raster =
                compose_scene_paused_banner_frame_raster(
                    plan.shared_overlay, resource_raster, text_raster);
        }
        if (plan.shared_overlay.splash_loading.reached &&
            observation.shared_loading_banner_raster.has_value()) {
            const auto& input = *observation.shared_loading_banner_raster;
            auto render = input.render;
            render.gate_reached = true;
            render.resource_object =
                plan.shared_overlay.splash_loading.resource_object;
            render.text_handle =
                plan.shared_overlay.splash_loading.resource_handle;
            if (input.resource_image.has_value()) {
                render.resource_text_width = input.resource_image->width;
                render.resource_text_height = input.resource_image->height;
                if (!render.load_stp_resource_lookup.has_value()) {
                    render.load_stp_resource_found = true;
                }
            }
            plan.shared_loading_banner_render =
                scene_paused_banner_render_plan(render);

            auto layout_font = input.layout_font;
            auto parsed_font_complete = true;
            if (input.parsed_layout_font.has_value()) {
                const auto projection = scene_dashboard_font_projection(
                    *input.parsed_layout_font);
                layout_font = projection.font;
                parsed_font_complete = projection.input_complete;
            }

            if (plan.shared_loading_banner_render->draws_text_handle) {
                std::vector<SceneDashboardRasterObservation> line_renderers(
                    input.text.per_line_renderers.begin(),
                    input.text.per_line_renderers.end());
                for (auto& renderer : line_renderers) {
                    renderer = shared_stamp_renderer(std::move(renderer));
                }
                auto text = input.text;
                auto localized_text = input.localized_text;
                if (!localized_text.has_value() &&
                    observation.language_resource != nullptr) {
                    localized_text = observation.language_resource->find(
                        "Splash:Loading");
                    plan.shared_loading_banner_localization_resolved =
                        localized_text.has_value();
                }
                if (localized_text.has_value()) {
                    text.text = *localized_text;
                }
                text.per_line_renderers = line_renderers;
                plan.shared_loading_banner_text_raster =
                    raster_scene_paused_banner_text(
                        *plan.shared_loading_banner_render, text, layout_font,
                        input.atlas_font);
                if (!parsed_font_complete && text.text_pointer_nonnull) {
                    plan.shared_loading_banner_text_raster->input_complete =
                        false;
                }
                if (text.text_pointer_nonnull &&
                    !localized_text.has_value()) {
                    plan.shared_loading_banner_text_raster->input_complete =
                        false;
                }
            }
            if (plan.shared_loading_banner_render->draws_resource_text &&
                input.resource_image.has_value()) {
                auto resource_renderer = shared_stamp_renderer(
                    input.resource_renderer);
                const ScenePausedBannerResourceRasterObservation resource{
                    true, *input.resource_image, std::move(resource_renderer)};
                plan.shared_loading_banner_resource_raster =
                    raster_scene_paused_banner_resource(
                        *plan.shared_loading_banner_render, resource);
            }
            auto box_writes = scene_paused_banner_box_pixel_writes(
                *plan.shared_loading_banner_render,
                shared_overlay_observation.render_context,
                shared_overlay_observation.renderer_mode_0x4d7a48,
                shared_overlay_observation.framebuffer_base_value);
            const auto* resource_raster = plan
                .shared_loading_banner_resource_raster.has_value()
                ? &*plan.shared_loading_banner_resource_raster : nullptr;
            const auto* text_raster = plan
                .shared_loading_banner_text_raster.has_value()
                ? &*plan.shared_loading_banner_text_raster : nullptr;
            plan.shared_loading_banner_frame_raster =
                compose_scene_banner_frame_raster(
                    *plan.shared_loading_banner_render, box_writes,
                    resource_raster, text_raster);
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

RaceLoopPausedBannerSurfaceWriteResult
apply_race_loop_paused_banner_to_locked_surface(
    const RaceLoopSharedOverlayPlan& plan,
    const std::span<std::uint8_t> locked_surface)
{
    RaceLoopPausedBannerSurfaceWriteResult result{};
    const auto& frame = plan.state_transition_and_frame.following_frame;
    result.frame_submitted = frame.frame_submitted;
    result.banner_write_plan_available =
        plan.shared_paused_banner_frame_raster.has_value() &&
        plan.shared_paused_banner_frame_raster->reached &&
        plan.shared_paused_banner_frame_raster->input_complete;

    if (!result.frame_submitted || !plan.shared_callback_reached ||
        (plan.shared_callback_wrapper != 0x00404960U &&
         plan.shared_callback_wrapper != 0x004052D0U) ||
        !frame.surface_unlock.has_value() ||
        !frame.surface_unlock->reached ||
        frame.surface_unlock->ordered_helpers !=
            std::array<std::uint32_t, 3>{
                0x00415090U, 0x00459030U, 0x004150A0U}) {
        return result;
    }

    const auto callback = std::find_if(
        frame.calls.begin(), frame.calls.end(), [&](const auto& call) {
            return call.address == plan.shared_callback_wrapper;
        });
    const auto submit = std::find_if(
        frame.calls.begin(), frame.calls.end(), [](const auto& call) {
            return call.address == 0x0045C090U;
        });
    const auto unlock = std::find_if(
        frame.calls.begin(), frame.calls.end(), [](const auto& call) {
            return call.address == 0x00447CE0U;
        });
    if (callback == frame.calls.end() || submit == frame.calls.end() ||
        unlock == frame.calls.end() || !(submit < callback && callback < unlock)) {
        return result;
    }

    result.banner_callback_before_surface_unlock = true;
    if (result.banner_write_plan_available) {
        result.application = apply_scene_paused_banner_frame_raster(
            *plan.shared_paused_banner_frame_raster, locked_surface);
    }
    return result;
}

RaceLoopState0DashboardSurfaceWriteResult
apply_race_loop_state0_dashboard_pixel_writes_to_locked_surface(
    const RaceLoopSharedOverlayPlan& plan,
    const std::span<std::uint8_t> locked_surface)
{
    RaceLoopState0DashboardSurfaceWriteResult result{};
    const auto& frame = plan.state_transition_and_frame.following_frame;
    result.frame_submitted = frame.frame_submitted;
    if (!result.frame_submitted || !plan.shared_callback_reached ||
        plan.shared_callback_wrapper != 0x004052D0U ||
        !frame.surface_unlock.has_value() ||
        !frame.surface_unlock->reached ||
        frame.surface_unlock->ordered_helpers !=
            std::array<std::uint32_t, 3>{
                0x00415090U, 0x00459030U, 0x004150A0U}) {
        return result;
    }

    const auto callback = std::find_if(
        frame.calls.begin(), frame.calls.end(), [](const auto& call) {
            return call.address == 0x004052D0U && call.state_indexed_callback;
        });
    const auto submit = std::find_if(
        frame.calls.begin(), frame.calls.end(), [](const auto& call) {
            return call.address == 0x0045C090U;
        });
    const auto unlock = std::find_if(
        frame.calls.begin(), frame.calls.end(), [](const auto& call) {
            return call.address == 0x00447CE0U;
        });
    if (callback == frame.calls.end() || submit == frame.calls.end() ||
        unlock == frame.calls.end() || !(submit < callback && callback < unlock)) {
        return result;
    }
    result.dashboard_callback_before_surface_unlock = true;

    struct OrderedWrite {
        std::uint64_t offset{};
        std::uint32_t value{};
        std::uint32_t write_mask{};
        std::uint8_t bytes_per_pixel{};
        bool preserves_existing_bits{};
    };
    std::vector<OrderedWrite> writes;
    auto complete = !plan.state0_dashboard_render_operations.empty();
    for (std::size_t operation_index = 0;
         operation_index < plan.state0_dashboard_render_operations.size();
         ++operation_index) {
        const auto& operation =
            plan.state0_dashboard_render_operations[operation_index];
        if (operation.kind ==
            SceneDashboardRenderOperationKind::full_surface_fill_0x44a1b0) {
            if (!plan.state0_dashboard_fill_projection.has_value()) {
                complete = false;
                break;
            }
            for (const auto& write : plan.state0_dashboard_fill_pixel_writes) {
                writes.push_back({write.destination_offset, write.value,
                                  write.write_mask, write.bytes_per_pixel,
                                  write.preserves_existing_bits});
            }
            continue;
        }
        if (operation.kind !=
            SceneDashboardRenderOperationKind::bitmap_draw_0x44afd0) {
            continue;
        }
        const auto projection = std::find_if(
            plan.state0_dashboard_bitmap_projections.begin(),
            plan.state0_dashboard_bitmap_projections.end(),
            [operation_index](const auto& item) {
                return item.render_operation_index == operation_index;
            });
        if (projection == plan.state0_dashboard_bitmap_projections.end() ||
            !projection->input_complete ||
            projection->blit.path ==
                SceneDashboardBitmapBlitPath::unsupported_format_pair) {
            complete = false;
            break;
        }
        for (const auto& write : projection->pixel_writes) {
            writes.push_back({write.destination_offset, write.value, 0U,
                              write.bytes_per_pixel, false});
        }
    }
    for (const auto& operation : plan.dashboard_gear_display.operations) {
        if (!complete) {
            break;
        }
        switch (operation.kind) {
        case SceneDashboardGearOperationKind::draw_gear_resource: {
            if (!plan.state0_dashboard_gear_resource_raster.has_value() ||
                !plan.state0_dashboard_gear_resource_raster->draw_reached ||
                !plan.state0_dashboard_gear_resource_raster->input_complete ||
                !plan.state0_dashboard_gear_resource_raster->raster.input_complete) {
                complete = false;
                break;
            }
            for (const auto& write : plan.state0_dashboard_gear_resource_raster
                                         ->raster.pixel_writes) {
                writes.push_back({
                    static_cast<std::uint64_t>(
                        write.offset_from_framebuffer_base),
                    write.value, 0xffffffffU, write.bytes_per_pixel, false});
            }
            break;
        }
        case SceneDashboardGearOperationKind::fill_mode_zero_rectangle:
            if (!plan.state0_dashboard_mode_zero_gear_fill_complete) {
                complete = false;
                break;
            }
            for (const auto& write :
                 plan.state0_dashboard_mode_zero_gear_fill_writes) {
                writes.push_back({write.destination_offset, write.value,
                                  write.write_mask, write.bytes_per_pixel,
                                  write.preserves_existing_bits});
            }
            break;
        case SceneDashboardGearOperationKind::draw_selected_gear_label:
            if (!plan.state0_dashboard_gear_label_raster.has_value() ||
                !plan.state0_dashboard_gear_label_raster->input_complete) {
                complete = false;
                break;
            }
            for (const auto& write :
                 plan.state0_dashboard_gear_label_raster->pixel_writes) {
                writes.push_back({
                    static_cast<std::uint64_t>(write.offset_from_framebuffer_base),
                    write.value, 0xffffffffU, write.bytes_per_pixel, false});
            }
            break;
        case SceneDashboardGearOperationKind::draw_rotating_gear_geometry:
            if (!plan.dashboard_gear_display.rotating_gear_geometry.has_value()) {
                complete = false;
                break;
            }
            for (const auto& write : plan.dashboard_gear_display
                                         .rotating_gear_triangle_fill_writes) {
                writes.push_back({write.offset_from_framebuffer_base,
                                  write.value, write.write_mask,
                                  write.bytes_per_pixel,
                                  write.preserves_existing_bits});
            }
            for (const auto& write : plan.dashboard_gear_display
                                         .rotating_gear_pixel_writes) {
                writes.push_back({write.offset_from_framebuffer_base,
                                  write.value, 0xffffffffU,
                                  write.bytes_per_pixel, false});
            }
            break;
        default:
            break;
        }
    }
    if (plan.dashboard_gear_display.selected_gear_label_call_reached &&
        !plan.dashboard_gear_display.selected_gear_label_call.has_value()) {
        complete = false;
    }

    // 0x4052D0 continues into 0x404970 after the dashboard and gear. Preserve
    // the shared wrapper's branch order in the same mutation batch so that
    // overlaps retain the later draw's value. Branches without a pixel
    // projection remain explicit blockers instead of being silently skipped.
    const auto& overlay = plan.shared_overlay;
    const auto& paused = overlay.paused_banner.inner_render;
    if (paused.reached) {
        const auto& raster = plan.shared_paused_banner_frame_raster;
        if (!raster.has_value() || !raster->reached || !raster->input_complete) {
            complete = false;
        } else {
            for (const auto& write : raster->ordered_pixel_writes) {
                if (write.offset_from_framebuffer_base < 0) {
                    complete = false;
                    break;
                }
                writes.push_back({
                    static_cast<std::uint64_t>(
                        write.offset_from_framebuffer_base),
                    write.value, write.write_mask, write.bytes_per_pixel,
                    write.preserves_existing_bits});
            }
        }
    }
    if (overlay.cheater_banner.draw.reached) {
        const auto& raster = plan.shared_cheater_banner_raster;
        if (!raster.has_value() || !raster->draw_reached ||
            !raster->input_complete || !raster->raster.input_complete) {
            complete = false;
        } else {
            for (const auto& write : raster->raster.pixel_writes) {
                if (write.offset_from_framebuffer_base < 0) {
                    complete = false;
                    break;
                }
                writes.push_back({
                    static_cast<std::uint64_t>(
                        write.offset_from_framebuffer_base),
                    write.value, 0xffffffffU, write.bytes_per_pixel, false});
            }
        }
    }
    if (overlay.text_effects.predicate_0x4048e0.has_text_draw ||
        overlay.text_effects.predicate_0x404900.has_text_draw) {
        const auto& raster = plan.shared_text_effects_raster;
        if (!plan.shared_text_effects_raster_reached ||
            !raster.input_complete) {
            complete = false;
        } else {
            for (const auto& write : raster.ordered_pixel_writes) {
                if (write.offset_from_framebuffer_base < 0) {
                    complete = false;
                    break;
                }
                writes.push_back({
                    static_cast<std::uint64_t>(
                        write.offset_from_framebuffer_base),
                    write.value, 0xffffffffU, write.bytes_per_pixel, false});
            }
        }
    }
    if (overlay.centered_resource.draw.reached) {
        const auto& raster = plan.shared_centered_resource_raster;
        if (!raster.has_value() || !raster->draw_reached ||
            !raster->input_complete || !raster->raster.input_complete) {
            complete = false;
        } else {
            for (const auto& write : raster->raster.pixel_writes) {
                if (write.offset_from_framebuffer_base < 0) {
                    complete = false;
                    break;
                }
                writes.push_back({
                    static_cast<std::uint64_t>(
                        write.offset_from_framebuffer_base),
                    write.value, 0xffffffffU, write.bytes_per_pixel, false});
            }
        }
    }
    if (overlay.race_results.gates.race_results) {
        if (!plan.shared_results_raster_reached ||
            !plan.shared_results_heading_raster.input_complete ||
            !plan.shared_results_rows_raster.input_complete) {
            complete = false;
        } else {
            const auto append_text_writes = [&writes, &complete](
                const auto& text_writes) {
                for (const auto& write : text_writes) {
                    if (write.offset_from_framebuffer_base < 0) {
                        complete = false;
                        break;
                    }
                    writes.push_back({
                        static_cast<std::uint64_t>(
                            write.offset_from_framebuffer_base),
                        write.value, 0xffffffffU, write.bytes_per_pixel, false});
                }
            };
            append_text_writes(
                plan.shared_results_heading_raster.ordered_pixel_writes);
            if (complete) {
                append_text_writes(
                    plan.shared_results_rows_raster.ordered_pixel_writes);
            }
        }
    }
    if (overlay.splash_loading.reached) {
        const auto& raster = plan.shared_loading_banner_frame_raster;
        if (!raster.has_value() || !raster->reached ||
            !raster->input_complete) {
            complete = false;
        } else {
            for (const auto& write : raster->ordered_pixel_writes) {
                if (write.offset_from_framebuffer_base < 0) {
                    complete = false;
                    break;
                }
                writes.push_back({
                    static_cast<std::uint64_t>(
                        write.offset_from_framebuffer_base),
                    write.value, write.write_mask, write.bytes_per_pixel,
                    write.preserves_existing_bits});
            }
        }
    }
    result.dashboard_write_plan_available = complete;
    if (!complete) {
        return result;
    }

    for (const auto& write : writes) {
        if (write.bytes_per_pixel == 0U || write.bytes_per_pixel > 4U ||
            (write.preserves_existing_bits && write.bytes_per_pixel != 2U) ||
            write.offset > locked_surface.size() ||
            write.bytes_per_pixel > locked_surface.size() - write.offset) {
            return result;
        }
    }
    for (const auto& write : writes) {
        const auto offset = static_cast<std::size_t>(write.offset);
        if (write.preserves_existing_bits) {
            std::uint32_t existing = 0U;
            for (std::uint8_t byte = 0U; byte < write.bytes_per_pixel; ++byte) {
                existing |= static_cast<std::uint32_t>(locked_surface[offset + byte])
                    << (static_cast<std::uint32_t>(byte) * 8U);
            }
            const auto updated = (existing & ~write.write_mask) |
                (write.value & write.write_mask);
            for (std::uint8_t byte = 0U; byte < write.bytes_per_pixel; ++byte) {
                locked_surface[offset + byte] = static_cast<std::uint8_t>(
                    updated >> (static_cast<std::uint32_t>(byte) * 8U));
            }
        } else {
            for (std::uint8_t byte = 0U; byte < write.bytes_per_pixel; ++byte) {
                locked_surface[offset + byte] = static_cast<std::uint8_t>(
                    write.value >> (static_cast<std::uint32_t>(byte) * 8U));
            }
        }
    }
    result.applied = true;
    result.pixel_writes_applied = writes.size();
    return result;
}

} // namespace viper
