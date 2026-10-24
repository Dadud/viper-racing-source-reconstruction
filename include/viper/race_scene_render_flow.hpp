#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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

struct RaceLoopState0DashboardRasterObservation {
    SceneDashboardCoreCallbackRasterObservation callback_text{};
    SceneDashboardFontObservation layout_font{};
    SceneDashboardAtlasFontObservation atlas_font{};
};

struct RaceLoopSharedPausedBannerRasterObservation {
    ScenePausedBannerTextRasterObservation text{};
    SceneDashboardFontObservation layout_font{};
    SceneDashboardAtlasFontObservation atlas_font{};
};

struct RaceLoopSharedOverlayObservation {
    std::int32_t current_callback_state{};
    RaceLoopActiveFrameObservation following_frame{};
    // When supplied, acquisition runs only if the following frame reaches
    // 0x447C90; its result then controls submission and post-submit callbacks.
    std::optional<RenderContextAcquisitionObservation> render_context_acquisition;
    SceneDashboardRenderObservation state0_dashboard_render{};
    SceneDashboardBackgroundBitmapBlitObservation state0_bitmap_blit{};
    std::vector<std::uint8_t> state0_bitmap_source_pixels;
    SceneDashboardCoreCallbackObservation dashboard_core_callback{};
    SceneDashboardTestTrackObservation dashboard_test_track{};
    std::array<SceneDashboardMetricObservation, 2> dashboard_metrics{};
    SceneDashboardAuxiliaryReadoutObservation dashboard_auxiliary_readout{};
    SceneDashboardCallbackGeometryObservation dashboard_callback_geometry{};
    SceneDashboardGearDisplayObservation dashboard_gear_display{};
    SceneState5OverlayObservation state5_overlay{};
    SceneSharedOverlayFrameObservation shared_overlay{};
    RaceLoopSharedResultsRasterObservation shared_results_raster{};
    RaceLoopSharedTextEffectsRasterObservation shared_text_effects_raster{};
    std::optional<RaceLoopSharedPausedBannerRasterObservation>
        shared_paused_banner_raster;
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
    std::vector<SceneDashboardRenderOperation> state0_dashboard_render_operations;
    std::optional<SceneDashboardRectangleFillProjection>
        state0_dashboard_fill_projection;
    struct State0BitmapProjection {
        std::size_t render_operation_index{};
        SceneDashboardBackgroundBitmapBlitResult blit{};
        std::vector<SceneDashboardBitmapPixelWrite> pixel_writes;
    };
    std::vector<State0BitmapProjection> state0_dashboard_bitmap_projections;
    bool shared_results_raster_reached{};
    SceneSharedResultsHeadingTextRasterPlan shared_results_heading_raster{};
    SceneSharedResultsRasterPlan shared_results_rows_raster{};
    bool shared_text_effects_raster_reached{};
    SceneSharedTextEffectsRasterPlan shared_text_effects_raster{};
    SceneSharedOverlayFramePlan shared_overlay{};
    std::optional<ScenePausedBannerTextRasterPlan>
        shared_paused_banner_text_raster;
};

// Carries the event-0x31 state change through callback dispatch and, when a
// wrapper is reached, through known state-0 dashboard geometry and shared tail.
[[nodiscard]] RaceLoopSharedOverlayPlan plan_race_loop_shared_overlay(
    const RaceLoopSharedOverlayObservation& observation);

} // namespace viper
