#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "viper/race_loop_callbacks.hpp"
#include "viper/graphics_startup.hpp"
#include "viper/scene_overlay.hpp"

namespace viper {

struct RaceLoopSharedResultsRasterObservation {
    SceneSharedResultsHeadingTextRasterObservation heading{};
    std::vector<SceneDashboardRasterObservation> row_renderers;
    SceneDashboardFontObservation layout_font{};
    SceneDashboardAtlasFontObservation atlas_font{};
};

struct RaceLoopSharedTextEffectsRasterObservation {
    SceneSharedTextEffectsRasterObservation text{};
    SceneDashboardFontObservation layout_font{};
    SceneDashboardAtlasFontObservation atlas_font{};
};

struct RaceLoopSharedStampRasterObservation {
    std::optional<StampImage> image;
    SceneDashboardRasterObservation renderer{};
};

struct RaceLoopState0DashboardRasterObservation {
    SceneDashboardCoreCallbackRasterObservation callback_text{};
    SceneDashboardFontObservation layout_font{};
    SceneDashboardAtlasFontObservation atlas_font{};
};

struct RaceLoopSharedPausedBannerRasterObservation {
    ScenePausedBannerTextRasterObservation text{};
    SceneDashboardFontObservation layout_font{};
    // When present, this shipped STMP character map supplies the glyph index,
    // advance, and x-offset tables instead of a manually projected font.
    std::optional<StampFontIndex> parsed_layout_font;
    SceneDashboardAtlasFontObservation atlas_font{};
    std::optional<StampImage> resource_image;
    SceneDashboardRasterObservation resource_renderer{};
};

struct RaceLoopSharedLoadingBannerRasterObservation {
    ScenePausedBannerRenderObservation render{};
    ScenePausedBannerTextRasterObservation text{};
    std::optional<std::string_view> localized_text;
    SceneDashboardFontObservation layout_font{};
    std::optional<StampFontIndex> parsed_layout_font;
    SceneDashboardAtlasFontObservation atlas_font{};
    std::optional<StampImage> resource_image;
    SceneDashboardRasterObservation resource_renderer{};
};

struct RaceLoopSharedOverlayObservation {
    std::int32_t current_callback_state{};
    RaceLoopActiveFrameObservation following_frame{};
    // When supplied, acquisition runs only if the following frame reaches
    // 0x447C90; its result then controls submission and post-submit callbacks.
    std::optional<RenderContextAcquisitionObservation> render_context_acquisition;
    SceneDashboardRenderObservation state0_dashboard_render{};
    SceneDashboardBackgroundBitmapBlitObservation state0_bitmap_blit{};
    // Optional raw 0x4EB3D0 bitmap object bytes; when supplied, its mode,
    // pixel base, dimensions and pitch override the projected source fields.
    std::optional<std::vector<std::uint8_t>>
        state0_bitmap_descriptor_bytes;
    // Optional decoded race.res::status.stp, drawn during 0x4031F0 over the
    // observed zero-filled 0x4EB2C0 staging bitmap.
    std::optional<StampImage> state0_dashboard_background_image;
    std::vector<std::uint8_t> state0_bitmap_source_pixels;
    SceneDashboardCoreCallbackObservation dashboard_core_callback{};
    SceneDashboardTestTrackObservation dashboard_test_track{};
    std::array<SceneDashboardMetricObservation, 2> dashboard_metrics{};
    SceneDashboardAuxiliaryReadoutObservation dashboard_auxiliary_readout{};
    SceneDashboardCallbackGeometryObservation dashboard_callback_geometry{};
    SceneDashboardGearDisplayObservation dashboard_gear_display{};
    // Decoded image for the registered 0x4EB420 resource. Populate with
    // parse_state0_dashboard_gear_image(race_res) when that bundle is loaded.
    std::optional<StampImage> state0_dashboard_gear_resource_image;
    SceneState5OverlayObservation state5_overlay{};
    SceneSharedOverlayFrameObservation shared_overlay{};
    // race.res registrations: award.stp at 0x4EB46C and start.stp at
    // 0x4EB350. Use the matching parse_shared_*_image helper when available.
    RaceLoopSharedStampRasterObservation centered_resource_raster{};
    RaceLoopSharedStampRasterObservation cheater_banner_raster{};
    RaceLoopSharedResultsRasterObservation shared_results_raster{};
    RaceLoopSharedTextEffectsRasterObservation shared_text_effects_raster{};
    std::optional<RaceLoopSharedPausedBannerRasterObservation>
        shared_paused_banner_raster;
    std::optional<RaceLoopSharedLoadingBannerRasterObservation>
        shared_loading_banner_raster;
    std::optional<RaceLoopState0DashboardRasterObservation>
        state0_dashboard_raster;
};

struct RaceLoopSharedOverlayPlan {
    RaceOverlayToggleFramePlan state_transition_and_frame{};
    std::optional<RenderContextAcquisitionPlan> render_context_acquisition;
    bool shared_callback_reached{};
    std::uint32_t shared_callback_wrapper{};
    std::uint32_t shared_callback_second_argument{};
    bool optional_dashboard_branch_enabled{};
    bool state5_overlay_reached{};
    SceneState5OverlayPlan state5_overlay{};
    bool dashboard_core_callback_reached{};
    SceneDashboardCoreCallbackPlan dashboard_core_callback{};
    std::vector<SceneDashboardTextCall> dashboard_core_text_calls;
    std::optional<SceneDashboardCoreCallbackRasterPlan>
        state0_dashboard_text_raster;
    bool dashboard_test_track_reached{};
    SceneDashboardTestTrackResult dashboard_test_track{};
    bool dashboard_supplemental_time_reached{};
    SceneDashboardSupplementalTimeResult dashboard_supplemental_time{};
    std::array<SceneDashboardMetricResult, 2> dashboard_metrics{};
    bool dashboard_auxiliary_readout_reached{};
    SceneDashboardAuxiliaryReadoutResult dashboard_auxiliary_readout{};
    bool dashboard_callback_geometry_reached{};
    SceneDashboardCallbackGeometryPlan dashboard_callback_geometry{};
    bool dashboard_gear_display_reached{};
    SceneDashboardGearDisplayResult dashboard_gear_display{};
    std::optional<ScenePausedBannerResourceRasterPlan>
        state0_dashboard_gear_resource_raster;
    std::optional<SceneDashboardBitmapDescriptorResult>
        state0_bitmap_descriptor;
    std::vector<SceneDashboardRenderOperation> state0_dashboard_render_operations;
    std::optional<SceneDashboardRectangleFillProjection>
        state0_dashboard_fill_projection;
    struct State0BitmapProjection {
        std::size_t render_operation_index{};
        bool input_complete{};
        SceneDashboardBackgroundBitmapBlitResult blit{};
        std::vector<SceneDashboardBitmapPixelWrite> pixel_writes;
    };
    std::vector<State0BitmapProjection> state0_dashboard_bitmap_projections;
    std::vector<SceneRectangleFillPixelWrite>
        state0_dashboard_fill_pixel_writes;
    bool state0_dashboard_mode_zero_gear_fill_complete{};
    std::vector<SceneRectangleFillPixelWrite>
        state0_dashboard_mode_zero_gear_fill_writes;
    bool shared_results_raster_reached{};
    SceneSharedResultsHeadingTextRasterPlan shared_results_heading_raster{};
    SceneSharedResultsRasterPlan shared_results_rows_raster{};
    bool shared_text_effects_raster_reached{};
    SceneSharedTextEffectsRasterPlan shared_text_effects_raster{};
    std::optional<SceneStampImageRasterPlan> shared_centered_resource_raster;
    std::optional<SceneStampImageRasterPlan> shared_cheater_banner_raster;
    SceneSharedOverlayFramePlan shared_overlay{};
    std::optional<ScenePausedBannerStyle14MeasurePlan>
        shared_paused_banner_fallback_measure;
    std::optional<ScenePausedBannerTextRasterPlan>
        shared_paused_banner_text_raster;
    std::optional<ScenePausedBannerResourceRasterPlan>
        shared_paused_banner_resource_raster;
    std::optional<ScenePausedBannerFrameRasterPlan>
        shared_paused_banner_frame_raster;
    std::optional<ScenePausedBannerRenderPlan> shared_loading_banner_render;
    std::optional<ScenePausedBannerTextRasterPlan>
        shared_loading_banner_text_raster;
    std::optional<ScenePausedBannerResourceRasterPlan>
        shared_loading_banner_resource_raster;
    std::optional<ScenePausedBannerFrameRasterPlan>
        shared_loading_banner_frame_raster;
};

// Resolves the statically registered race.res image used by the state-0 gear
// resource handle DAT_004EB420 (resource key at 0x004CFBDC: "rpm.stp").
[[nodiscard]] std::optional<StampImage>
parse_state0_dashboard_gear_image(const ResourceBundle& race_bundle);
// 0x4031F0 registers status.stp and draws it at (0, 0) into the dashboard's
// 0x4EB2C0 staging bitmap during initialization.
[[nodiscard]] std::optional<StampImage>
parse_state0_dashboard_background_image(const ResourceBundle& race_bundle);

[[nodiscard]] std::optional<StampImage> parse_shared_centered_resource_image(
    const ResourceBundle& race_bundle);
[[nodiscard]] std::optional<StampImage> parse_shared_cheater_banner_image(
    const ResourceBundle& race_bundle);
[[nodiscard]] std::optional<StampImage> parse_shared_loading_resource_image(
    const ResourceBundle& common_bundle);

struct RaceLoopPausedBannerSurfaceWriteResult {
    bool frame_submitted{};
    bool banner_callback_before_surface_unlock{};
    bool banner_write_plan_available{};
    ScenePausedBannerFrameApplyResult application{};
};

// Carries the event-0x31 state change through callback dispatch and, when a
// wrapper is reached, through known state-0 dashboard geometry and shared tail.
[[nodiscard]] RaceLoopSharedOverlayPlan plan_race_loop_shared_overlay(
    const RaceLoopSharedOverlayObservation& observation);

// Applies the merged paused-banner writes only when the composed callback is
// inside a submitted frame and precedes the observed 0x447CE0 unlock wrapper.
[[nodiscard]] RaceLoopPausedBannerSurfaceWriteResult
apply_race_loop_paused_banner_to_locked_surface(
    const RaceLoopSharedOverlayPlan& plan,
    std::span<std::uint8_t> locked_surface);

struct RaceLoopState0DashboardSurfaceWriteResult {
    bool frame_submitted{};
    bool dashboard_callback_before_surface_unlock{};
    bool dashboard_write_plan_available{};
    bool applied{};
    std::size_t pixel_writes_applied{};
};

// Applies the modeled state-0 background, gear, and supported shared-tail
// writes in callback order only when the callback is inside a submitted frame
// and precedes surface unlock. Reached shared branches without a pixel model
// leave the complete batch unavailable.
[[nodiscard]] RaceLoopState0DashboardSurfaceWriteResult
apply_race_loop_state0_dashboard_pixel_writes_to_locked_surface(
    const RaceLoopSharedOverlayPlan& plan,
    std::span<std::uint8_t> locked_surface);

} // namespace viper
