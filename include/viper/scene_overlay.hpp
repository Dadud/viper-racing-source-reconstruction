#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "viper/matrix_quaternion.hpp"
#include "viper/resource_bundle.hpp"
#include "viper/render_context.hpp"
#include "viper/scene_car_update.hpp"

namespace viper {

struct LanguageResource;

struct SceneOverlayTextCall {
    std::int32_t x{};
    std::int32_t y{};
    // Either an observed string address or the address of a row record.
    std::uint32_t text_source{};
    std::uint32_t color_source{};
    std::int32_t row_index{-1};
};

// Models text calls selected by the numeric page at 0x4D3DD0 in 0x41A790.
// It omits the page-2 geometric overlay and renderer/font side effects.
[[nodiscard]] std::vector<SceneOverlayTextCall> scene_overlay_text_plan(
    std::int32_t page,
    std::int32_t selected_car_ordinal);

struct ScenePhysicsOverlayFlags {
    std::uint8_t collisions_0x4d4b48{};
    std::uint8_t walls_0x4d4b4c{};
};

struct SceneRectangleFillPixelWrite {
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t destination_offset{};
    std::uint32_t destination_address{};
    std::uint32_t value{};
    std::uint32_t write_mask{};
    std::uint8_t bytes_per_pixel{};
    bool preserves_existing_bits{};
};

struct SceneRectangleFillApplyResult;
struct SceneTextPixelApplyResult;

// Models key dispatch in 0x4236B0: 0x63 updates the collision flag and 0x77
// updates the walls flag. Other values leave both fields alone.
[[nodiscard]] bool handle_scene_physics_overlay_key(
    ScenePhysicsOverlayFlags& flags,
    std::uint16_t key) noexcept;

// Text portion of 0x423380; the four wheel-bar draw calls are not represented.
[[nodiscard]] std::vector<SceneOverlayTextCall>
scene_physics_overlay_text_plan(const ScenePhysicsOverlayFlags& flags);

struct ScenePhysicsWheelBarInput {
    float vector_component_0x9c{};
    float vector_component_0xa0{};
    float fill_fraction_0xa4{};
    // The numeric input read from the caller's stack for format 0x4D4C54.
    float format_input{};
    // Stack vector used by 0x4236F0's 1.2-length normalization gate.
    float normalization_reference_x{};
    float normalization_reference_y{};
    std::uint32_t frame_color_0x508650{};
    std::uint32_t fill_color_0x508654{};
    std::uint32_t indicator_color_0x508660{};
    bool fdiv_compatibility_enabled{};
};

enum class ScenePhysicsWheelBarOperationKind : std::uint8_t {
    fill_rectangle_0x44a1b0,
    outline_circle_0x44c950,
    indicator_line_0x44c260,
    formatted_text_0x479580,
};

struct ScenePhysicsWheelBarOperation {
    ScenePhysicsWheelBarOperationKind kind{};
    std::int32_t x1{};
    std::int32_t y1{};
    std::int32_t x2{};
    std::int32_t y2{};
    std::uint32_t color{};
    std::uint32_t argument{};
    double numeric_argument{};
    // The 0x479580 label call receives the result of formatting numeric_argument.
    std::string text_argument;
};

struct ScenePhysicsWheelBarPlan {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t fill_width{};
    bool fill_rectangle_emitted{};
    std::int32_t fill_left{};
    std::int32_t fill_top{};
    std::int32_t fill_right{};
    std::int32_t fill_bottom{};
    std::int32_t outline_radius{20};
    std::uint32_t frame_color{};
    std::uint32_t fill_color{};
    std::int32_t indicator_end_x{};
    std::int32_t indicator_end_y{};
    std::uint32_t indicator_color{};
    float normalized_component_0x9c{};
    float normalized_component_0xa0{};
    double format_argument{};
    bool vector_normalized{};
    bool fdiv_compatibility_approximated{};
    bool formatted_label_complete{true};
    // Observed order inside 0x4236F0: optional fill, circle, indicator, text.
    std::vector<ScenePhysicsWheelBarOperation> ordered_operations;
};

// Models one of the four 0x4236F0 wheel bars, including its fixed position,
// proportional fill, radius-20 outline circle, normalized indicator, format
// argument, and ordered renderer wrapper calls.
[[nodiscard]] std::array<ScenePhysicsWheelBarPlan, 4>
scene_physics_wheel_bar_plans(
    const std::array<ScenePhysicsWheelBarInput, 4>& inputs);

struct ScenePhysicsWheelBarPixelWrites {
    std::vector<SceneRectangleFillPixelWrite> fill_writes;
    std::vector<SceneRectangleFillPixelWrite> outline_writes;
    std::vector<SceneRectangleFillPixelWrite> indicator_writes;
};

// Projects the fill and 0x44C260 indicator-line writes for one wheel bar.
// Also rasterizes the 0x44C950 eight-way midpoint-circle outline.
[[nodiscard]] ScenePhysicsWheelBarPixelWrites
scene_physics_wheel_bar_pixel_writes(
    const ScenePhysicsWheelBarPlan& plan,
    const RenderContextViewport& context,
    std::uint8_t renderer_mode,
    std::uint32_t framebuffer_base);

[[nodiscard]] SceneRectangleFillApplyResult
apply_scene_physics_wheel_bar_pixel_writes(
    const ScenePhysicsWheelBarPixelWrites& writes,
    std::span<std::uint8_t> framebuffer);

struct SceneDashboardViewportCall {
    std::int32_t left{};
    std::int32_t top{};
    std::int32_t right{};
    std::int32_t bottom{};
    // Source row in the binary's rectangle table.
    std::uint32_t row_source{};
    // Zero for an unconditional table row; otherwise the selected mask bit.
    std::uint32_t required_mask_bit{};
};

// Models the 0x4498A0 viewport arguments produced by 0x405100 for three base
// rows and nine mask-selected rows. scene_dashboard_render_plan composes these
// with the surrounding full-surface and bitmap calls.
[[nodiscard]] std::vector<SceneDashboardViewportCall>
scene_dashboard_viewport_plan(std::int32_t surface_width,
                              std::uint32_t dashboard_mask);

enum class SceneDashboardRenderOperationKind : std::uint8_t {
    full_surface_fill_0x44a1b0,
    bitmap_draw_0x44afd0,
    viewport_0x4498a0,
    restore_viewport_0x4498e0,
};

struct SceneDashboardRenderObservation {
    std::int32_t surface_width{};
    std::uint32_t dashboard_mask{};
    std::int32_t full_surface_bottom_0x4eb3dc{};
    std::int32_t full_surface_fill_argument_0x4eb27c{};
    std::int32_t background_bitmap_width{};
    std::int32_t background_bitmap_height{};
    std::int32_t renderer_mode_0x4d7a48{};
    RenderContextViewport render_context;
    std::uint32_t framebuffer_base_value{};
};

struct SceneDashboardRenderOperation {
    SceneDashboardRenderOperationKind kind{};
    std::int32_t left{};
    std::int32_t top{};
    std::int32_t right{};
    std::int32_t bottom{};
    std::uint32_t row_source{};
    std::uint32_t required_mask_bit{};
    std::int32_t raw_auxiliary_argument{};
    std::uint32_t bitmap_object{0x004EB3D0};
    std::int32_t bitmap_width{};
    std::int32_t bitmap_height{};
    std::int32_t renderer_mode_0x4d7a48{};
};

// Projects 0x405100's fill/bitmap/viewport ordering. The bitmap operation is
// represented by its observed 0x44AFD0 wrapper arguments; 0x44A730's renderer
// mode dispatch is exposed by scene_dashboard_background_bitmap_blit_plan;
// DirectDraw device effects remain outside these portable plans.
[[nodiscard]] std::vector<SceneDashboardRenderOperation>
scene_dashboard_render_plan(const SceneDashboardRenderObservation& observation);

enum class SceneDashboardBitmapBlitPath : std::uint8_t {
    copy_16bit,
    copy_32bit,
    convert_mode4_to_mode3,
    convert_mode3_to_mode5,
    convert_mode4_to_mode5,
    convert_mode5_to_mode3,
    convert_mode5_to_mode4,
    unsupported_format_pair,
};

struct SceneDashboardBackgroundBitmapBlitObservation {
    std::uint8_t source_bitmap_mode{};
    std::uint8_t destination_renderer_mode{};
    std::int32_t bitmap_width{};
    std::int32_t bitmap_height{};
    std::int32_t bitmap_pitch_bytes{};
    std::uint32_t bitmap_base_address{};
    std::int32_t destination_x{};
    std::int32_t destination_y{};
    RenderContextViewport destination_context;
    std::uint32_t destination_base_address{};
};

struct SceneDashboardBitmapDescriptorResult {
    bool input_complete{};
    std::uint8_t source_mode{};
    std::uint32_t pixel_base_address{};
    std::int32_t width{};
    std::int32_t height{};
    std::int32_t pitch_bytes{};
};

// Reads the observed 0x14-byte bitmap object fields used by 0x44AFD0/0x44A940:
// mode at +0, pixels at +4, dimensions at +8/+0xC, and pitch at +0x10.
[[nodiscard]] SceneDashboardBitmapDescriptorResult
parse_scene_dashboard_bitmap_descriptor(
    std::span<const std::uint8_t> descriptor_bytes) noexcept;

struct SceneDashboardBackgroundBitmapBlitResult {
    SceneDashboardBitmapBlitPath path{
        SceneDashboardBitmapBlitPath::unsupported_format_pair};
    bool transfer_happened{};
    std::int32_t source_x{};
    std::int32_t source_y{};
    std::int32_t destination_x{};
    std::int32_t destination_y{};
    std::int32_t width{};
    std::int32_t height{};
    std::uint32_t source_offset{};
    std::uint32_t source_address{};
    std::uint32_t destination_offset{};
    std::uint32_t destination_address{};
};

// Projects 0x44AFD0 -> 0x44A730 mode dispatch and 0x4499E0 destination
// clipping for the background bitmap. Pixel conversion routines remain named
// as a path; scene_dashboard_bitmap_pixel_conversion projects one pixel's
// value, but this function does not traverse or rewrite the bitmap.
[[nodiscard]] SceneDashboardBackgroundBitmapBlitResult
scene_dashboard_background_bitmap_blit_plan(
    const SceneDashboardBackgroundBitmapBlitObservation& observation) noexcept;

struct SceneDashboardBitmapPixelConversion {
    bool supported{};
    std::uint32_t value{};
    std::uint8_t destination_bytes_per_pixel{};
};

// Applies the recovered per-pixel conversion for one value from the bitmap
// blit helpers. Unsupported source/destination mode pairs remain unsupported.
[[nodiscard]] SceneDashboardBitmapPixelConversion
scene_dashboard_bitmap_pixel_conversion(std::uint8_t source_mode,
                                        std::uint8_t destination_mode,
                                        std::uint32_t source_pixel) noexcept;

struct SceneDashboardBitmapPixelWrite {
    std::uint32_t source_offset{};
    std::uint32_t source_address{};
    std::uint32_t destination_offset{};
    std::uint32_t destination_address{};
    std::uint32_t value{};
    std::uint8_t bytes_per_pixel{};
};

// Projects the clipped 0x44A730 bitmap loop from a packed little-endian source
// buffer. It reports writes but never reads or mutates destination memory.
[[nodiscard]] std::vector<SceneDashboardBitmapPixelWrite>
scene_dashboard_background_bitmap_pixel_writes(
    const SceneDashboardBackgroundBitmapBlitObservation& observation,
    std::span<const std::uint8_t> source_pixels);

struct SceneDashboardBitmapBlitApplyResult {
    bool applied{};
    std::size_t pixels_written{};
};

// Applies a fully validated projected blit to caller-supplied linear buffers.
// No destination bytes change unless the complete clipped transfer fits.
[[nodiscard]] SceneDashboardBitmapBlitApplyResult
apply_scene_dashboard_background_bitmap_blit(
    const SceneDashboardBackgroundBitmapBlitObservation& observation,
    std::span<const std::uint8_t> source_pixels,
    std::span<std::uint8_t> destination_pixels);

struct SceneDashboardRectangleFillProjection {
    std::int32_t clipped_left{};
    std::int32_t clipped_top{};
    std::int32_t clipped_right{};
    std::int32_t clipped_bottom{};
    std::uint32_t source_color{};
    std::uint32_t encoded_value{};
    std::uint32_t write_mask{};
    std::uint32_t first_pixel_offset{};
    std::uint32_t first_pixel_address{};
    std::uint8_t bytes_per_pixel{};
    bool preserves_existing_bits{};
};

// Projects the optional redraw-all background clear through the 0x44A1B0
// clip and renderer-mode backend. No framebuffer memory is modified.
[[nodiscard]] std::optional<SceneDashboardRectangleFillProjection>
scene_dashboard_full_surface_fill_projection(
    const SceneDashboardRenderObservation& observation);

// Projects all clipped writes of the 0x44A1B0 rectangle helper. This is a
// write plan only; no device or framebuffer is accessed.
[[nodiscard]] std::vector<SceneRectangleFillPixelWrite>
scene_rectangle_fill_pixel_writes(
    std::int32_t left, std::int32_t top,
    std::int32_t right, std::int32_t bottom,
    std::uint32_t source_color,
    const RenderContextViewport& context,
    std::uint8_t renderer_mode,
    std::uint32_t framebuffer_base);

struct SceneRectangleFillApplyResult {
    bool applied{};
    std::size_t pixels_written{};
};

// Validates every projected destination before changing any bytes. Mode-1
// writes preserve the destination high nibble as observed in 0x44A640.
[[nodiscard]] SceneRectangleFillApplyResult
apply_scene_rectangle_fill_pixel_writes(
    std::span<const SceneRectangleFillPixelWrite> writes,
    std::span<std::uint8_t> framebuffer);

enum class SceneDashboardCallbackGeometryKind : std::uint8_t {
    begin_batch_0x448460,
    refresh_resource_0x418cc0,
    measure_text_0x44db10,
    measure_font_0x44dc50,
    rectangle_0x4484c0,
    check_overlay_suppression_0x404890,
    finish_batch_0x448490,
};

struct SceneDashboardTextWidthInput {
    // When false, 0x44DB10 uses its fixed eight-pixel-per-byte fallback.
    bool font_object_present{};
    // Signed SHORT advances at font +0x204 + (unsigned text byte * 2).
    std::array<std::int16_t, 256> glyph_advance_0x204{};
    // Bounded bytes from the text pointer, including its terminating NUL.
    std::vector<std::uint8_t> text_bytes;
};

struct SceneDashboardTextWidthResult {
    std::int32_t width{};
    bool complete{};
    bool used_fixed_width_fallback{};
};

[[nodiscard]] SceneDashboardTextWidthResult scene_dashboard_measure_text_width(
    const SceneDashboardTextWidthInput& input) noexcept;

struct SceneDashboardFontExtentInput {
    // First pointer-sized field of the object passed to 0x44DC50.
    std::optional<std::uint32_t> payload_pointer_0x00;
    // DWORD at payload +0x04, reached through 0x44D460.
    std::optional<std::int32_t> payload_metric_0x04;
};

struct SceneDashboardFontExtentResult {
    std::int32_t extent{};
    bool complete{};
    bool used_default_extent{};
};

[[nodiscard]] SceneDashboardFontExtentResult scene_dashboard_measure_font_extent(
    std::uint32_t font_object_pointer,
    const SceneDashboardFontExtentInput& input) noexcept;

struct SceneDashboardCallbackGeometryObservation {
    std::array<std::uint8_t, 6> source_bytes_0x4eb318{};
    std::int32_t surface_metric_0x509a2c{};
    std::int32_t surface_metric_0x509a4c{};
    // Values returned by 0x44D450 / 0x44D460 for resource 0x4EB46C.
    std::int32_t resource_0x4eb46c_metric_00{};
    std::int32_t resource_0x4eb46c_metric_04{};
    // Corresponding values for resource 0x4EB448.
    std::int32_t resource_0x4eb448_metric_00{};
    std::int32_t resource_0x4eb448_metric_04{};
    bool splash_loading_flag_0x4eb1e8{};
    bool resource_0x4eb290_generation_matches{};
    bool resource_0x4eb428_generation_matches{};
    std::int32_t flag_0x4eb294_text_width{};
    std::int32_t flag_0x4eb42c_text_width{};
    std::int32_t font_metric_0x44dc50{};
    std::optional<SceneDashboardFontExtentInput> font_extent_input;
    std::optional<SceneDashboardTextWidthInput>
        flag_0x4eb294_text_input;
    std::optional<SceneDashboardTextWidthInput>
        flag_0x4eb42c_text_input;
    std::uint32_t font_object_value_0x4eb304{};
    std::uint32_t text_handle_value_0x4eb294{};
    std::uint32_t text_handle_value_0x4eb42c{};
};

struct SceneDashboardCallbackGeometryOperation {
    SceneDashboardCallbackGeometryKind kind{};
    std::uint32_t callee{};
    // 0x4484C0 helper parameter order: four raw coordinates, then color.
    std::array<std::int32_t, 4> rectangle_arguments{};
    std::uint32_t color{0x7F000000};
    std::uint32_t resource_object{};
    std::array<std::uint32_t, 2> helper_arguments{};
    std::uint8_t helper_argument_count{};
    struct Vertex {
        std::array<float, 4> position{};
        std::uint32_t diffuse_color{};
        std::uint32_t specular_color{};
        std::array<float, 2> texture_coordinates{};
    };
    struct IndexedSubmission {
        // Renderer wrapper +0x04 points to IDirect3DDevice2. The call at
        // 0x452730 reaches DrawIndexedPrimitive at vtable slot +0x78.
        std::uint32_t device_pointer_offset{4U};
        std::uint32_t device_method_vtable_offset{0x78U};
        std::uint32_t primitive_type_triangle_list{4U};
        std::uint32_t vertex_type_tlvertex{3U};
        std::array<Vertex, 4> vertices{};
        std::uint32_t vertex_count{4U};
        std::array<std::uint16_t, 6> indices{0U, 1U, 3U, 0U, 3U, 2U};
        std::uint32_t index_count{6U};
        std::uint32_t flags{8U};
        bool hresult_ignored{true};
    };
    std::optional<IndexedSubmission> indexed_submission;
};

struct SceneDashboardCallbackGeometryPlan {
    std::array<std::uint8_t, 6> copied_bytes_0x4eb2f8{};
    // 0x404890 returns true when the results byte or splash-loading byte is
    // nonzero. The callback invokes this predicate at two ordered call sites.
    bool overlay_content_suppressed{};
    std::optional<SceneDashboardTextWidthResult>
        flag_0x4eb294_measured_width;
    std::optional<SceneDashboardTextWidthResult>
        flag_0x4eb42c_measured_width;
    std::optional<SceneDashboardFontExtentResult> measured_font_extent;
    std::vector<SceneDashboardCallbackGeometryOperation> operations;
};

// Models the resource-gated rectangle and line geometry in the raw 0x4052E0
// callback. Text drawing itself remains handled by separate plans.
[[nodiscard]] SceneDashboardCallbackGeometryPlan
scene_dashboard_callback_geometry_plan(
    const SceneDashboardCallbackGeometryObservation& observation);

enum class SceneDashboardGearOperationKind : std::uint8_t {
    register_final_time_banner,
    register_reverse_character,
    register_neutral_character,
    initialize_gear_geometry_constants,
    fill_mode_zero_rectangle,
    draw_gear_resource,
    draw_selected_gear_label,
    draw_rotating_gear_geometry,
};

struct SceneDashboardGearDisplayObservation {
    bool dashboard_path_enabled{};
    std::int32_t surface_width{};
    std::int32_t surface_height{};
    std::int32_t mode_0x45c200{};
    std::int32_t mode_zero_fill_argument_0x4eb27c{};
    std::uint32_t gear_resource_value_0x4eb420{};
    // Base record returned by 0x45DFB0. 0x404970 reads the target object's
    // pointer at record+0x134, then the gear value at target+0x34.
    std::span<const std::uint8_t> selected_record_base_bytes;
    std::uint32_t selected_record_value_object_address{};
    std::span<const std::uint8_t> selected_record_value_object_bytes;
    // Runtime string pointers from [0x4CFAB0 + (selector+1)*4], where slot 0
    // is selector -1 and slot 8 is selector 7.
    std::span<const std::uint32_t> gear_label_text_addresses_from_minus_one;
    // Decoded C strings for the same signed-selector slots, when available.
    std::span<const std::string_view> gear_label_texts_from_minus_one;
    const LanguageResource* language_resource{};
    std::uint32_t gear_label_font_context_value_0x4eb28c{};
    std::uint32_t gear_label_font_resource_value_0x4eb314{};
    // Scalar compatibility input used only when the bounded byte views do not
    // resolve the pointer chain.
    float selected_record_value_0x34{};
    std::uint32_t resource_initialization_mask_0x4eb274{};
    std::uint32_t triangle_color_value_0x4eb29c{};
    std::uint32_t first_edge_color_value_0x4eb27c{};
    std::uint32_t second_edge_color_value_0x4eb33c{};
    RenderContextViewport render_context;
    std::uint8_t renderer_mode_0x4d7a48{};
    std::uint32_t framebuffer_base_value{};
};

struct SceneDashboardGearPoint {
    std::int32_t x{};
    std::int32_t y{};
};

struct SceneDashboardGearGeometry {
    float angle_radians{};
    SceneDashboardGearPoint tip;
    SceneDashboardGearPoint first_base_corner;
    SceneDashboardGearPoint second_base_corner;
    std::uint32_t triangle_color_value{};
    std::uint32_t first_edge_color_value{};
    std::uint32_t second_edge_color_value{};
};

// One 0x44A1B0 horizontal rectangle emitted by the 0x44A230 triangle fill.
struct SceneDashboardTriangleSpan {
    std::int32_t x_begin{};
    std::int32_t y{};
    std::int32_t x_end{};
    std::uint32_t color_value{};
};

// One 0x449E90 pixel call produced by a 0x44C260 gear-outline edge.
struct SceneDashboardGearEdgePixelCall {
    std::uint8_t edge_index{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t color_value{};
};

struct SceneDashboardGearPixelWrite {
    std::size_t pixel_call_index{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t offset_from_framebuffer_base{};
    std::uint32_t destination_address{};
    std::uint32_t value{};
    std::uint8_t bytes_per_pixel{};
};

struct SceneDashboardGearTriangleFillWrite {
    std::size_t span_index{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t offset_from_framebuffer_base{};
    std::uint32_t destination_address{};
    std::uint32_t value{};
    std::uint32_t write_mask{};
    std::uint8_t bytes_per_pixel{};
    bool preserves_existing_bits{};
};

struct SceneDashboardGearOperation {
    SceneDashboardGearOperationKind kind{};
    std::int32_t left_or_x{};
    std::int32_t top_or_y{};
    std::int32_t right{};
    std::int32_t bottom{};
    // Raw value loaded from the associated global, not that global's address.
    std::uint32_t resource_or_fill_argument{};
    std::uint32_t flags{};
    float value{};
    float phase{};
    float rate{};
};

struct SceneDashboardGearDisplayResult {
    std::uint32_t newly_set_resource_bits{};
    bool initialized_geometry_constants{};
    std::optional<std::uint32_t> selected_record_value_bits;
    bool selected_record_value_resolved_from_bytes{};
    std::optional<std::int32_t> selected_gear_label_selector;
    std::optional<std::uint32_t> selected_gear_label_text_address;
    std::optional<std::string> selected_gear_label_text;
    bool selected_gear_label_call_reached{};
    struct GearLabelCall {
        std::int32_t selector{};
        std::optional<std::uint32_t> selected_text_address;
        std::optional<std::string> formatted_text;
        std::uint32_t text_source{};
        std::uint32_t font_context_value{};
        std::uint32_t font_resource_value{};
        std::int32_t x{};
        std::int32_t y{};
        std::uint32_t format_flags{0x0cU};
    };
    std::optional<GearLabelCall> selected_gear_label_call;
    std::optional<SceneDashboardGearGeometry> rotating_gear_geometry;
    // Half-open scanline spans, bounded by the target's 1024-row edge arrays.
    std::vector<SceneDashboardTriangleSpan> rotating_gear_triangle_spans;
    // Ordered raw pixel calls; 4499B0 clipping is applied in the write plan.
    std::vector<SceneDashboardGearEdgePixelCall> rotating_gear_edge_pixels;
    // Clipped, renderer-mode-converted write projections; no surface is mutated.
    std::vector<SceneDashboardGearPixelWrite> rotating_gear_pixel_writes;
    // Per-pixel projection of the triangle's 0x44A1B0 horizontal fill spans.
    std::vector<SceneDashboardGearTriangleFillWrite>
        rotating_gear_triangle_fill_writes;
    std::vector<SceneDashboardGearOperation> operations;
};

// Reconstructs the 256-entry split-plane color resource written by 0x44D530.
// Each returned plane contains 256 16-bit words, matching the 0x44D4F0 store.
[[nodiscard]] std::vector<std::uint16_t> scene_dashboard_color_resource_palette(
    std::uint32_t first_color,
    std::uint32_t second_color);

// Models the width- and mode-gated dashboard branch in 0x404970. Renderer
// helpers and their pixel/device effects remain represented as call operations.
[[nodiscard]] SceneDashboardGearDisplayResult scene_dashboard_gear_display_plan(
    const SceneDashboardGearDisplayObservation& observation);

struct SceneDashboardState {
    std::int32_t selected_ordinal_cache_0x4cf9c8{-1};
    std::int32_t position_cache_0x4cf9cc{-1};
    std::int32_t lap_value_cache_0x4cf9d0{-1};
    std::int32_t dashboard_kind_cache_0x4cf9ec{-1};
    std::uint32_t metric_cache_0x4cf9d8_bits{0xBF800000};
    std::uint32_t metric_cache_0x4cf9dc_bits{0xBF800000};
    std::uint32_t redraw_mask_0x4eb40c{};
    std::uint32_t resource_init_mask_0x4eb484{};
    std::uint32_t resource_init_mask_0x4eb2bc{};
    bool reset_byte_0x4eb300{};
};

enum class SceneDashboardInitializationOperationKind : std::uint8_t {
    store_global,
    initialize_text_resource,
    register_resource,
    create_colored_resource,
    set_resource_color,
    measure_text,
    initialize_text_context,
    set_text_context,
    reset_text_context,
    draw_text,
};

struct SceneDashboardInitializationOperation {
    SceneDashboardInitializationOperationKind kind{};
    std::uint32_t helper{};
    std::uint32_t destination_global{};
    std::uint32_t resource_key_address{};
    std::uint32_t first_argument{};
    std::uint32_t second_argument{};
    std::uint8_t value_size_bytes{};
};

struct SceneDashboardInitializationPlan {
    SceneDashboardState reset_state{};
    std::vector<SceneDashboardInitializationOperation> ordered_operations;
};

// Projects the ordered one-time dashboard/resource initialization in
// 0x4031F0. Resource handles and platform-backed helper results are not run.
[[nodiscard]] SceneDashboardInitializationPlan
scene_dashboard_initialization_plan();

struct SceneDashboardPositionObservation {
    std::int32_t selected_ordinal{};
    std::int32_t record_value_0x140{};
    std::uint32_t entity_count{};
    bool context_changed_0x44e3d0{};
    bool decrement_entity_count_0x40e6d0{};
    // Compared at 0x4036C1 when a full redraw reaches the Position resource.
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> position_cached_generation_value;
};

// The state-0 callback's ESI points at the selected record's +0x138 block.
// Each optional DWORD below is independently populated when its bytes exist.
struct SceneDashboardSelectedRecordProjection {
    std::optional<std::uint32_t> value_0x00_bits;
    std::optional<std::uint32_t> value_0x08_bits;
    std::optional<std::uint32_t> value_0x0c_bits;
    std::optional<std::uint32_t> value_0x10_bits;
    std::optional<std::uint32_t> value_0x14_bits;
    std::optional<std::uint32_t> value_0x18_bits;
    std::optional<std::uint32_t> value_0x1c_bits;
    std::optional<std::uint32_t> value_0x20_bits;
    std::optional<std::uint32_t> value_0x24_bits;
    std::optional<std::uint32_t> value_0x28_bits;
    std::optional<std::uint32_t> value_0x4c_bits;
};

// Reads observed selected-record fields relative to ESI at 0x4035E0. Short
// spans leave only out-of-range fields unresolved.
[[nodiscard]] SceneDashboardSelectedRecordProjection
project_scene_dashboard_selected_record(
    std::span<const std::uint8_t> esi_record_bytes) noexcept;

struct SceneDashboardPositionResult {
    bool redraw_all{};
    // Whether 0x4036CE calls 0x418CC0; unresolved if raw words were absent.
    std::optional<bool> refresh_position_resource;
    std::int32_t displayed_position{};
    std::int32_t displayed_count{};
    std::uint32_t newly_registered_resource_bits{};
    std::uint8_t reached_text_call_sites{};
};

// Models the Position-specific branch of 0x4035E0. The two low result bits
// select call sites 0x4036EA and 0x40372E in scene_dashboard_text_plan.
[[nodiscard]] SceneDashboardPositionResult update_scene_dashboard_position(
    SceneDashboardState& state,
    const SceneDashboardPositionObservation& observation) noexcept;

struct SceneDashboardLapObservation {
    std::int32_t selected_object_type_0x18{};
    bool global_method_0x40_result{};
    std::int32_t record_value_0x138{};
    std::uint32_t global_method_0x20_result{};
};

struct SceneDashboardLapResult {
    bool normal_lap_branch{};
    bool resource_registered{};
    std::int32_t displayed_lap{};
    std::uint32_t displayed_lap_count{};
    std::uint32_t requested_dirty_rectangles{};
    std::uint8_t reached_text_call_sites{};
};

// Models the ordinary Lap branch of 0x4035E0. The composed callback can derive
// its global method outcomes from SceneDashboardGlobalObjectObservation;
// unresolved methods fall back to these explicit inputs.
[[nodiscard]] SceneDashboardLapResult update_scene_dashboard_lap(
    SceneDashboardState& state,
    const SceneDashboardLapObservation& observation,
    bool redraw_all) noexcept;

struct SceneDashboardFinalTimeObservation {
    std::int32_t selected_object_type_0x18{};
    bool global_method_0x40_result{};
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> race_time_cached_generation_value;
};

struct SceneDashboardFinalTimeResult {
    bool final_time_branch{};
    // Whether 0x403804 calls 0x418CC0 before the RaceTime resource draw.
    std::optional<bool> refresh_race_time_resource;
    std::uint32_t requested_dirty_rectangles{};
    std::uint8_t reached_text_call_sites{};
};

// `records` contains 0x48-byte rows from DAT_0050869C; record_count mirrors
// DAT_005086A0. The lookup reads a float at +0x0C, flags at +0x40, and ordinal
// at +0x41. Matching rows update the result; scanning stops at the first row
// with flag bit 0x20 clear. -1.0F is the no-ordinal-match result.
[[nodiscard]] float scene_dashboard_time_for_ordinal(
    std::span<const std::uint8_t> records,
    std::size_t record_count,
    std::uint8_t ordinal) noexcept;

// Models 0x425A40's returned string. time_seconds comes from the result row;
// marker is the byte at 0x4F04F4 and hundredths selects its second argument.
[[nodiscard]] std::string format_scene_dashboard_time(float time_seconds,
                                                       char marker,
                                                       bool hundredths);

// Models the selected-type-0/global-method-true branch. The formatted time
// value returned by the binary's helpers is not reproduced.
[[nodiscard]] SceneDashboardFinalTimeResult update_scene_dashboard_final_time(
    SceneDashboardState& state,
    const SceneDashboardFinalTimeObservation& observation,
    bool redraw_all) noexcept;

struct SceneDashboardTextCall {
    std::uint32_t call_site{};
    // Semantic x/y/flags; callers push them right-to-left as y, x, flags.
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t text_or_format_source{};
    // Zero means the binary supplies a computed register value at this site.
    std::uint32_t color_source{};
    std::uint32_t font_source{};
    std::uint32_t format_flags{};
    // When the callback has recovered every argument, its exact preformatted
    // byte string can be rasterized without a parallel caller observation.
    std::optional<std::string> resolved_text_argument;
};

struct SceneDashboardFontObservation {
    // The binary indexes these signed 16-bit tables with each unsigned text byte.
    std::array<std::int16_t, 256> glyph_advance{}; // font + 0x204
    std::array<std::int16_t, 256> glyph_index{};   // font + 0x004
    std::array<std::int16_t, 256> glyph_x_offset{}; // font + 0x404
    // Results of the font metric helpers used by 0x44DC70.
    std::int32_t top_metric{};    // 0x44DBC0
    std::int32_t bottom_metric{}; // 0x44DC00
};

struct SceneDashboardFontProjection {
    bool input_complete{true};
    SceneDashboardFontObservation font{};
};

// Converts the decoded 0x44DA00 STMP character map and 44DBC0/44DC00 header
// metrics into the runtime layout tables.
[[nodiscard]] SceneDashboardFontProjection scene_dashboard_font_projection(
    const StampFontIndex& index) noexcept;

struct SceneDashboardGlyphCall {
    std::uint32_t call_site{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint8_t character{};
    std::int16_t glyph_index{};
};

struct SceneDashboardTextLayout {
    std::int32_t measured_width{};
    std::int32_t origin_x{};
    std::int32_t origin_y{};
    std::vector<SceneDashboardGlyphCall> glyph_calls;
};

struct SceneDashboardLapTimeObservation {
    std::uint32_t record_value_0x0c_bits{};
    std::uint32_t record_value_0x14_bits{};
    bool redraw_all{};
    bool redraw_from_prior_state_change{};
    bool resource_generation_matches{};
    // Raw words compared at 0x403A1C/0x403A88 before refreshing 0x4EB2B0.
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> lap_time_cached_generation_value;
    char time_marker{};
    std::string resource_text_0x4eb2b4;
};

struct SceneDashboardLapTimeResult {
    bool resource_registered{};
    bool positive_delta_branch{};
    bool fallback_label_drawn{};
    bool refresh_resource{};
    std::uint32_t requested_dirty_rectangles{};
    std::string formatted_time;
    std::string first_line_text;
    std::string second_line_text;
    std::vector<SceneDashboardTextCall> calls;
};

// Models the remaining selected-type-0 LapTime branch around 0x40399A-0x403AF3.
[[nodiscard]] SceneDashboardLapTimeResult update_scene_dashboard_lap_time(
    SceneDashboardState& state,
    const SceneDashboardLapTimeObservation& observation);

// Projects reached 0x44DC70 call sites from 0x4035E0. Bit positions 0..7
// correspond to call sites listed in metadata/simulation_loop.md. Position,
// ordinary Lap, and final-time helpers compute some gates; other gates and
// formatted values remain caller-supplied or unmodeled.
[[nodiscard]] std::vector<SceneDashboardTextCall>
scene_dashboard_text_plan(std::uint8_t reached_call_site_mask);

// Models the font-backed layout in 0x44DC70 after its variadic string has
// been formatted. Bit 1 right-aligns, bit 2 centers horizontally; bits 3,
// 4, and 6 select the observed top, centered, and bottom vertical adjustments.
[[nodiscard]] SceneDashboardTextLayout layout_scene_dashboard_text(
    const SceneDashboardTextCall& call,
    std::string_view formatted_text,
    const SceneDashboardFontObservation& font);

struct SceneDashboardTestTrackObservation {
    std::int32_t displayed_position{};
    std::int32_t selected_record_type_0x18{};
    std::uint32_t record_value_0x0c_bits{};
    std::uint32_t distance_scale_0x0c_bits{};
    bool redraw_all{};
    bool redraw_from_prior_state_change{};
    bool try_resource_generation_matches{};
    bool time_resource_generation_matches{};
    bool distance_resource_generation_matches{};
    // Raw values compared by 0x403D74, 0x403E1F, and 0x403EDE. When the
    // current and cached words are present, they override the bool fallbacks.
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> try_resource_cached_generation_value;
    std::optional<std::uint32_t> time_resource_cached_generation_value;
    std::optional<std::uint32_t> distance_resource_cached_generation_value;
    std::string try_resource_text;
    std::string time_resource_text;
    std::string distance_resource_text;
    std::string distance_suffix_0x08;
    char time_marker{};
};

struct SceneDashboardTestTrackResult {
    bool dashboard_kind_changed{};
    bool try_label_drawn{};
    bool position_row_drawn{};
    bool refresh_try_resource{};
    bool time_resource_label_drawn{};
    bool refresh_time_resource{};
    bool distance_resource_label_drawn{};
    bool refresh_distance_resource{};
    std::uint32_t newly_registered_resource_bits{};
    std::uint32_t requested_dirty_rectangles{};
    std::string try_label_text;
    std::string time_resource_label_text;
    std::string distance_resource_label_text;
    std::string time_value_text;
    std::string distance_value_text;
    std::vector<SceneDashboardTextCall> calls;
};

struct SceneDashboardTestTrackLocalizationPlan {
    bool try_label_resolved{};
    bool time_label_resolved{};
    bool distance_label_resolved{};
};

[[nodiscard]] SceneDashboardTestTrackLocalizationPlan
apply_scene_dashboard_test_track_localization(
    SceneDashboardTestTrackObservation& observation,
    const LanguageResource& language);

enum class SceneDashboardMetricSlot : std::uint8_t {
    value_0x10,
    value_0x14,
};

struct SceneDashboardMetricObservation {
    SceneDashboardMetricSlot slot{SceneDashboardMetricSlot::value_0x10};
    std::int32_t selected_object_type_0x18{};
    std::uint32_t value_bits{};
    std::uint32_t type_3_record_value_0x18_bits{};
    std::uint32_t time_global_value_0x0c_bits{};
    std::uint32_t type_3_global_value_0x1c_bits{};
    bool resource_generation_matches{};
    // Raw resource generation words for Best (+0x10) and Last (+0x14).
    // When both are present, their equality overrides the boolean fallback.
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> cached_resource_generation_value;
    char time_marker{};
    std::string prefix_text_from_0x4eb344_or_0x4eb334;
    std::string global_text_0x08;
    std::string type_3_global_text_0x18;
};

struct SceneDashboardMetricResult {
    bool cache_updated{};
    bool resource_registered{};
    bool refresh_resource{};
    bool value_above_raw_threshold{};
    std::uint32_t requested_dirty_rectangles{};
    std::uint32_t intermediate_format_source{};
    std::string formatted_value;
    std::string displayed_text;
    std::vector<SceneDashboardTextCall> calls;
};

struct SceneDashboardStringTable;

struct SceneDashboardSupplementalTimeObservation {
    // Standalone helper inputs. The composed 0x4035E0 callback replaces these
    // with the shared Position row's displayed value/count.
    std::int32_t displayed_position{};
    std::int32_t displayed_count{};
    std::uint32_t record_value_0x1c_bits{};
    std::uint32_t record_value_0x24_bits{};
    std::string record_text_0x20_plus_5;
    std::string record_text_0x28_plus_5;
    const SceneDashboardStringTable* string_table{};
    std::int32_t record_index_0x20{-1};
    std::int32_t record_index_0x28{-1};
    char decimal_marker{'.'};
};

struct SceneDashboardSupplementalTimeResult {
    bool show_0x24_branch{};
    bool show_0x1c_branch{};
    std::string formatted_negative_value_0x24;
    std::string formatted_positive_value_0x1c;
    std::string record_text_0x28_plus_5;
    std::string record_text_0x20_plus_5;
    std::vector<SceneDashboardTextCall> calls;
};

struct SceneDashboardAuxiliaryReadoutObservation {
    std::uint32_t record_value_0x4c_bits{};
    std::uint32_t global_value_0x1c_bits{};
    bool redraw_all{};
    // When present, resolves the signed advance for ASCII '0' from the
    // observed font table at +0x204 + 0x30*2.
    const SceneDashboardFontObservation* font_observation{};
    // Compatibility fallback used when the font table was not captured.
    std::int16_t zero_glyph_advance{};
    std::string global_text_0x18;
    std::string stack_text_after_esp_0x18_plus_5;
};

struct SceneDashboardAuxiliaryReadoutResult {
    float scaled_value{};
    std::int32_t truncated_value{};
    std::string numeric_text;
    std::int32_t numeric_text_x{};
    std::string heading_text;
    std::string secondary_text;
    std::vector<SceneDashboardTextCall> calls;
};

struct SceneDashboardGlobalObjectObservation {
    // Raw selector copied from factory input +0x18 (param_1[6]) at 0x426000.
    std::uint32_t factory_selector{};
    std::int32_t selected_ordinal{};
    // Bounded bytes beginning at the object pointer stored in DAT_00508A04.
    std::span<const std::uint8_t> object_bytes;
};

struct SceneDashboardGlobalObjectProjection {
    bool selector_supported{};
    bool method_0x40_resolved{};
    bool method_0x40_result{};
    bool method_0x20_resolved{};
    std::uint32_t method_0x20_result{};
};

// Projects the observed factory vtables' virtual +0x40 and +0x20 methods.
// Selector 0's +0x40 reads a byte from its 0x74-stride entry table; selectors
// 1/3/5 share a false-return stub. +0x20 reads the selector's raw object DWORD.
[[nodiscard]] SceneDashboardGlobalObjectProjection
scene_dashboard_global_object_projection(
    const SceneDashboardGlobalObjectObservation& observation) noexcept;

[[nodiscard]] SceneDashboardAuxiliaryReadoutResult
scene_dashboard_auxiliary_readout_plan(
    const SceneDashboardAuxiliaryReadoutObservation& observation);

[[nodiscard]] SceneDashboardSupplementalTimeResult
scene_dashboard_supplemental_time_plan(
    const SceneDashboardSupplementalTimeObservation& observation);

// Models the TestTrack branch from 0x403CA8 through its type-selected
// time/distance rows. Resource contents and generation comparisons are inputs.
[[nodiscard]] SceneDashboardTestTrackResult update_scene_dashboard_test_track(
    SceneDashboardState& state,
    const SceneDashboardTestTrackObservation& observation);

struct SceneDashboardLocaleState;

struct SceneDashboardCoreCallbackObservation {
    SceneDashboardState state{};
    SceneDashboardPositionObservation position{};
    SceneDashboardFinalTimeObservation final_time{};
    // Optional DAT_0050869C rows used by the final-time callback branch.
    std::span<const std::uint8_t> result_time_records;
    std::size_t result_time_record_count{};
    char final_time_marker{'.'};
    std::int32_t selected_object_type_0x18{};
    // Optional bounded view beginning at the selected record's ESI pointer
    // (the record's +0x138 block in the callback).
    std::span<const std::uint8_t> selected_record_esi_bytes;
    // Fallback values used when global_object_dispatch cannot resolve a
    // method result from its supported factory selector and bounded bytes.
    bool global_method_0x40_result{};
    std::int32_t record_value_0x138{};
    std::uint32_t global_method_0x20_result{};
    std::optional<SceneDashboardGlobalObjectObservation>
        global_object_dispatch;
    SceneDashboardLapTimeObservation lap_time{};
    SceneDashboardTestTrackObservation test_track{};
    const LanguageResource* language_resource{};
    const SceneDashboardLocaleState* locale_state{};
    SceneDashboardSupplementalTimeObservation supplemental_time{};
    std::array<SceneDashboardMetricObservation, 2> metrics{};
    SceneDashboardAuxiliaryReadoutObservation auxiliary_readout{};
};

struct SceneDashboardCoreCallbackPlan {
    SceneDashboardState next_state{};
    std::optional<SceneDashboardSelectedRecordProjection>
        selected_record_projection;
    std::optional<SceneDashboardGlobalObjectProjection>
        global_object_projection;
    SceneDashboardPositionResult position{};
    SceneDashboardLapResult lap{};
    SceneDashboardFinalTimeResult final_time{};
    std::optional<float> final_time_seconds;
    std::optional<std::string> final_time_text;
    SceneDashboardLapTimeResult lap_time{};
    SceneDashboardTestTrackResult test_track{};
    std::optional<SceneDashboardTestTrackLocalizationPlan>
        test_track_localization;
    bool test_track_locale_bound{};
    bool test_track_branch_reached{};
    SceneDashboardSupplementalTimeResult supplemental_time{};
    bool supplemental_time_branch_reached{};
    std::array<SceneDashboardMetricResult, 2> metrics{};
    SceneDashboardAuxiliaryReadoutResult auxiliary_readout{};
    bool auxiliary_readout_reached{};
    std::uint8_t reached_text_call_site_mask{};
    // Reached text calls ordered by their native call-site address after the
    // callback's component plans have been composed.
    std::vector<SceneDashboardTextCall> ordered_text_calls;
};

// Composes the common update and the observed +0x18 selected-object branch,
// including TestTrack's separate type-5 distance selector.
[[nodiscard]] SceneDashboardCoreCallbackPlan scene_dashboard_core_callback_plan(
    const SceneDashboardCoreCallbackObservation& observation);

struct SceneDashboardStringTableEntry {
    bool initialized{};
    std::uint32_t raw_mode_0x00{};
    bool enabled_0x04{};
    std::string text_0x05;
    std::string text_0x12;
    std::uint32_t raw_value_0x134{};
    // Native strcpy byte counts include the terminating NUL. These flags
    // expose adjacent-field/table corruption without performing unsafe writes.
    std::size_t text_0x05_copy_bytes{};
    std::size_t text_0x12_copy_bytes{};
    bool text_0x05_overwrites_text_0x12{};
    bool text_0x05_overwrites_raw_value{};
    bool text_0x05_passes_entry_end{};
    bool text_0x12_overwrites_raw_value{};
    bool text_0x12_passes_entry_end{};
};

struct SceneDashboardStringTable {
    std::array<SceneDashboardStringTableEntry, 16> entries{};
    // Mirrors DAT_004DC718: one past the greatest registered entry index.
    std::uint32_t entry_count{};
};

struct SceneDashboardMode2PredicateObservation {
    bool global_interface_0x4e322c_present{};
    std::int32_t raw_vtable_field_0x04{};
    // When supplied, 0x4A1E80 maps -1 to state 3 and other results to state 6.
    std::optional<std::int32_t> chatline_lookup_result_0x49fa00;
    std::uint8_t virtual_method_0x84_return_low_byte{};
    // Optional bounded ChatLine object memory for the observed 0x4A1B70 target.
    std::span<const std::byte> chatline_object_bytes;
    std::optional<std::uint32_t> virtual_method_index;
};

// Models the 0x49C5C0 gate and virtual call. Its return byte is false unless
// the global interface exists and the signed field at its +8 object's +4 is
// greater than 4. If bounded ChatLine memory is supplied, the virtual method
// result is read from +0x1C8 + index*0xD9; otherwise its observed byte is used.
[[nodiscard]] std::optional<std::uint8_t> scene_chatline_virtual_row_flag(
    std::span<const std::byte> object_bytes,
    std::uint32_t index) noexcept;
[[nodiscard]] bool scene_dashboard_string_table_mode_2_predicate(
    const SceneDashboardMode2PredicateObservation& observation) noexcept;

struct SceneDashboardStringTableSetObservation {
    std::int32_t index{};
    std::uint32_t raw_mode_0x00{};
    std::uint32_t raw_value_0x134{};
    SceneDashboardMode2PredicateObservation mode_2_predicate;
    std::string text_0x05;
    std::string text_0x12;
};

struct SceneDashboardStringTableConstructorObservation {
    std::span<const std::byte> runtime_car_record;
    std::span<const std::byte> configured_car_record;
    // The constructor passes `this + 0x3C` to the setter's raw +0x134 slot.
    std::uint32_t object_field_0x3c_address{};
    SceneDashboardMode2PredicateObservation mode_2_predicate;
};

// Decodes the constructor's exact source offsets: runtime +0x194 for the
// string-table index, and configured row +0/+0x04/+0x11 for mode and strings.
// Unterminated strings or short source spans are left unparsed safely.
[[nodiscard]] std::optional<SceneDashboardStringTableSetObservation>
make_scene_dashboard_string_table_set_observation(
    const SceneDashboardStringTableConstructorObservation& observation);

// Models the 16-entry, 0x170-byte table initialized at 0x45DE40 and written
// by 0x45DEB0. Invalid indices are rejected to keep the portable model within
// the observed table; the binary's setter itself has no bounds guard.
void initialize_scene_dashboard_string_table(
    SceneDashboardStringTable& table) noexcept;
[[nodiscard]] bool set_scene_dashboard_string_table_entry(
    SceneDashboardStringTable& table,
    const SceneDashboardStringTableSetObservation& observation);
[[nodiscard]] std::optional<std::string_view>
scene_dashboard_string_table_text_0x05(
    const SceneDashboardStringTable& table,
    std::int32_t index) noexcept;

struct SceneSharedOverlayObservation {
    bool results_flag_0x4eb2f8{};
    bool splash_loading_flag_0x4eb1e8{};
    bool paused_flag_0x508760{};
    bool pause_suppression_flag_0x4ecb40{};
    std::uint8_t cheater_flag_0x4eb2fc{};
    std::uint8_t cheater_flag_0x4eb2fd{};
    std::uint32_t world_index_0x53a4b8{};
    bool state_flag_0x4eb2fa{};
    bool state_flag_0x4eb2f9{};
    bool state_flag_0x4eb2fb{};
};

struct SceneSharedOverlayGates {
    bool results_or_splash_suppresses_shared_overlays{};
    bool paused_banner{};
    bool cheater_banner{};
    bool wrong_way_banner{};
    bool state_flag_0x4eb2f9{};
    bool state_flag_0x4eb2fb{};
    bool race_results{};
    bool splash_loading{};
};

// Models the shared condition helpers used after the optional dashboard
// branch in 0x404970, including state-4's 0x404960 wrapper path.
[[nodiscard]] SceneSharedOverlayGates scene_shared_overlay_gates(
    const SceneSharedOverlayObservation& observation) noexcept;

struct SceneSharedCheaterBannerCall {
    bool reached{};
    std::uint32_t resource_object{0x004EB350};
    std::uint32_t draw_helper{0x0044D0C0};
    std::int32_t x{};
    std::int32_t y{};
    // Third argument passed to 0x44D0C0. Its renderer meaning is unknown.
    std::int32_t raw_draw_mode{};
};

// Projects the cheater-banner call in 0x404970 after the 0x4048B0 gate.
// Text dimensions are supplied from the observed 0x44D460/0x44D450 calls.
[[nodiscard]] SceneSharedCheaterBannerCall
scene_shared_cheater_banner_call(bool gate_reached,
                                 std::int32_t surface_width,
                                 std::int32_t surface_height,
                                 std::int32_t text_width,
                                 std::int32_t text_height,
                                 std::uint32_t world_index) noexcept;

struct SceneSharedCheaterBannerObservation {
    SceneSharedOverlayObservation overlay_globals{};
    std::int32_t surface_width{};
    std::int32_t surface_height{};
    std::int32_t text_width{};
    std::int32_t text_height{};
};

struct SceneSharedCheaterBannerPlan {
    SceneSharedOverlayGates gates{};
    SceneSharedCheaterBannerCall draw{};
};

// Composes the raw shared flags with the cheater-banner placement call.
[[nodiscard]] SceneSharedCheaterBannerPlan scene_shared_cheater_banner_plan(
    const SceneSharedCheaterBannerObservation& observation) noexcept;

struct SceneSharedCenteredResourceCall {
    bool reached{};
    std::uint32_t resource_handle{0x004EB46C};
    std::uint32_t draw_helper{0x0044D0C0};
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t raw_draw_mode{};
    std::int32_t raw_auxiliary_argument{};
};

// Projects the resource draw after raw predicate 0x404920. The resource width
// and surface dimensions are inputs; the flag's meaning remains unknown.
[[nodiscard]] SceneSharedCenteredResourceCall
scene_shared_centered_resource_call(bool gate_reached,
                                    std::int32_t surface_width,
                                    std::int32_t surface_height,
                                    std::int32_t resource_width) noexcept;

struct SceneSharedCenteredResourceObservation {
    SceneSharedOverlayObservation overlay_globals{};
    std::int32_t surface_width{};
    std::int32_t surface_height{};
    std::int32_t resource_width{};
};

struct SceneSharedCenteredResourcePlan {
    SceneSharedOverlayGates gates{};
    SceneSharedCenteredResourceCall draw{};
};

// Composes the common suppression and 0x4EB2FB predicate with the 0x404920
// centered-resource draw.
[[nodiscard]] SceneSharedCenteredResourcePlan
scene_shared_centered_resource_plan(
    const SceneSharedCenteredResourceObservation& observation) noexcept;

struct SceneSharedPausedBannerCall {
    bool reached{};
    bool initialize_resource{};
    std::uint32_t resource_object{0x004EB460};
    std::uint32_t resource_handle{0x004EB464};
    std::uint32_t cached_resource_generation{0x004EB468};
    std::uint32_t current_resource_generation{0x004D3868};
    std::uint32_t initialize_helper{0x00418CC0};
    std::uint32_t draw_helper{0x0040C930};
};

// Projects the paused-banner resource refresh and draw in 0x404970.
[[nodiscard]] SceneSharedPausedBannerCall
scene_shared_paused_banner_call(bool gate_reached,
                                bool resource_generation_matches) noexcept;

struct ScenePausedBannerRenderObservation {
    bool gate_reached{};
    bool load_stp_resource_found{};
    // When present, the native ResourceGet result takes precedence over the
    // compatibility found flag above. Name/type are fixed to load.stp/STMP.
    std::optional<ResourceGetObservation> load_stp_resource_lookup;
    bool text_handle_nonnull{true};
    std::uint32_t text_handle{0x004EB464};
    std::uint32_t resource_object{0x004EB460};
    std::int32_t surface_width{};
    std::int32_t surface_height{};
    // 0x44D450 returns the resource text width; 0x44D460 returns its height.
    std::int32_t resource_text_width{};
    std::int32_t resource_text_height{};
    // Return from 0x479450(0x0E, text_handle) on the fallback path.
    std::int32_t fallback_text_width{};
    std::uint32_t outer_box_color_0x4ecbc8{};
    std::uint32_t inner_box_color_0x4ecb90{};
};

enum class ScenePausedBannerOperationKind : std::uint8_t {
    resource_lookup_0x44cfd0,
    resource_measure_height_0x44d460,
    resource_measure_width_0x44d450,
    fallback_measure_width_0x479450,
    outer_box_fill_0x44a1b0,
    inner_box_fill_0x44a1b0,
    resource_draw_0x44d0c0,
    text_draw_0x479580,
    resource_release_0x44d0a0,
};

struct ScenePausedBannerOperation {
    ScenePausedBannerOperationKind kind{};
    std::uint32_t helper{};
    std::array<std::int32_t, 4> rectangle{};
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t raw_mode{};
    std::uint32_t color{};
    std::uint32_t resource_or_text_source{};
    std::uint32_t text_size{};
};

struct ScenePausedBannerRenderPlan {
    bool reached{};
    bool input_complete{true};
    std::optional<ResourceGetPlan> resource_lookup;
    bool draws_fallback_box{};
    bool draws_resource_text{};
    bool draws_text_handle{};
    bool releases_resource{};
    std::array<std::int32_t, 4> outer_box{};
    std::array<std::int32_t, 4> inner_box{};
    std::uint32_t outer_box_color{};
    std::uint32_t inner_box_color{};
    std::int32_t resource_x{};
    std::int32_t resource_y{};
    std::int32_t text_x{};
    std::int32_t text_y{};
    std::uint32_t resource_object{0x004EB460};
    std::uint32_t resource_load_helper{0x0044CFD0};
    std::uint32_t resource_name_source{0x004D1058};
    std::uint32_t text_source{};
    std::uint32_t resource_draw_helper{0x0044D0C0};
    std::uint32_t box_draw_helper{0x0044A1B0};
    std::uint32_t text_draw_helper{0x00479580};
    std::uint32_t resource_release_helper{0x0044D0A0};
    std::uint32_t raw_text_size_0xe{0x0E};
    std::vector<ScenePausedBannerOperation> ordered_operations;
};

// Models the inner 0x40C930 -> 0x40C790 paused-banner layout. Resource lookup,
// measured text dimensions, and raw colors remain observation inputs.
[[nodiscard]] ScenePausedBannerRenderPlan
scene_paused_banner_render_plan(
    const ScenePausedBannerRenderObservation& observation);

[[nodiscard]] std::vector<SceneRectangleFillPixelWrite>
scene_paused_banner_box_pixel_writes(
    const ScenePausedBannerRenderPlan& plan,
    const RenderContextViewport& context,
    std::uint8_t renderer_mode,
    std::uint32_t framebuffer_base);

struct SceneSharedPausedBannerObservation {
    SceneSharedOverlayObservation overlay_globals{};
    bool resource_generation_matches{};
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> paused_cached_generation_value;
    ScenePausedBannerRenderObservation render{};
};

struct SceneSharedPausedBannerPlan {
    SceneSharedOverlayGates gates{};
    SceneSharedPausedBannerCall outer_call{};
    ScenePausedBannerRenderPlan inner_render{};
};

// Composes the 0x404970 pause predicate, resource refresh gate, and the
// 0x40C930/0x40C790 banner layout from one raw observation bundle.
[[nodiscard]] SceneSharedPausedBannerPlan scene_shared_paused_banner_plan(
    const SceneSharedPausedBannerObservation& observation) noexcept;

struct SceneSharedLoadingBannerObservation {
    SceneSharedOverlayObservation overlay_globals{};
    std::uint32_t resource_init_mask_0x4eb37c{};
    bool resource_generation_matches{};
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> loading_cached_generation_value;
};

enum class SceneSharedLoadingBannerOperationKind : std::uint8_t {
    register_resource_0x418c90,
    refresh_resource_0x418cc0,
    draw_resource_0x40c930,
};

struct SceneSharedLoadingBannerOperation {
    SceneSharedLoadingBannerOperationKind kind{};
    std::uint32_t helper{};
    std::uint32_t object{};
    std::uint32_t cleanup_callback{};
};

struct SceneSharedLoadingBannerPlan {
    SceneSharedOverlayGates gates{};
    bool reached{};
    bool initialize_resource{};
    bool refresh_resource{};
    std::uint32_t next_resource_init_mask_0x4eb37c{};
    std::uint32_t resource_key{0x004CFF44};
    std::uint32_t resource_object{0x004EB258};
    std::uint32_t resource_handle{0x004EB25C};
    std::uint32_t cached_resource_generation{0x004EB260};
    std::uint32_t current_resource_generation{0x004D3868};
    std::uint32_t register_helper{0x00418C90};
    std::uint32_t cleanup_callback{0x00405240};
    std::uint32_t refresh_helper{0x00418CC0};
    std::uint32_t draw_helper{0x0040C930};
    std::vector<SceneSharedLoadingBannerOperation> ordered_operations;
};

// Projects the final Splash:Loading registration/refresh/draw branch in
// 0x404970 after the direct 0x401DF0 loading flag.
[[nodiscard]] SceneSharedLoadingBannerPlan scene_shared_loading_banner_plan(
    const SceneSharedLoadingBannerObservation& observation);

enum class SceneSharedTextEffectBranch : std::uint8_t {
    predicate_0x4048e0,
    predicate_0x404900,
};

struct SceneSharedTextEffectObservation {
    SceneSharedTextEffectBranch branch{
        SceneSharedTextEffectBranch::predicate_0x4048e0};
    bool gate_reached{};
    std::uint32_t resource_object{};
    std::uint32_t cached_resource_generation{};
    std::uint32_t text_handle{};
    std::uint32_t font_object{0x004EB304};
    std::int32_t surface_width{};
    // Width returned by 0x44DB10(font_object, text_handle) for 0x4048E0.
    std::int32_t text_width{};
};

struct SceneSharedRendererCall {
    std::uint32_t callee{};
    std::uint32_t argument_0{};
    std::uint32_t argument_1{};
    std::uint8_t argument_count{};
};

struct SceneSharedTextEffectPlan {
    bool reached{};
    bool initialize_resource{};
    bool has_text_draw{};
    std::uint32_t draw_call_site{};
    std::int32_t draw_x{};
    std::int32_t draw_y{0x43};
    std::uint32_t draw_format_source{};
    std::uint32_t draw_color_source{0x004EB314};
    std::uint32_t draw_font_source{0x004EB304};
    std::uint32_t draw_flags{0x09};
    std::vector<SceneSharedRendererCall> calls;
};

// Models the 0x404970 resource/text setup and direct draw placement for the
// 0x4048E0 and 0x404900 predicates. Gate, resource, and measured text width
// remain observation inputs.
[[nodiscard]] SceneSharedTextEffectPlan scene_shared_text_effect_plan(
    const SceneSharedTextEffectObservation& observation,
    bool resource_generation_matches);

struct SceneSharedTextEffectsObservation {
    SceneSharedOverlayObservation overlay_globals{};
    SceneSharedTextEffectObservation predicate_0x4048e0{};
    bool resource_0x4eb428_generation_matches{};
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> resource_0x4eb430_cached_generation_value;
    SceneSharedTextEffectObservation predicate_0x404900{};
    bool resource_0x4eb290_generation_matches{};
    std::optional<std::uint32_t> resource_0x4eb298_cached_generation_value;
};

struct SceneSharedTextEffectsPlan {
    SceneSharedOverlayGates gates{};
    SceneSharedTextEffectPlan predicate_0x4048e0{};
    SceneSharedTextEffectPlan predicate_0x404900{};
};

// Composes the 0x4EB2FA/0x4EB2F9 shared predicates with their two text draws.
[[nodiscard]] SceneSharedTextEffectsPlan scene_shared_text_effects_plan(
    const SceneSharedTextEffectsObservation& observation);

struct SceneSharedResultsHeadingObservation {
    bool gate_reached{};
    std::uint32_t resource_init_mask_0x4eb274{};
    std::uint32_t prompt_init_mask_0x4eb37c{};
    bool results_resource_generation_matches{};
    bool prompt_resource_generation_matches{};
    std::int32_t surface_width{};
    std::int32_t surface_height{};
    std::int32_t prompt_text_width{};
    std::int32_t prompt_text_height{};
};

enum class SceneSharedResultsHeadingOperationKind : std::uint8_t {
    register_results_resource_0x418c90,
    register_prompt_resource_0x418c90,
    draw_prompt_resource_0x44d0c0,
    refresh_results_resource_0x418cc0,
    draw_results_text_0x44dc70,
    refresh_prompt_resource_0x418cc0,
    draw_prompt_text_0x44dc70,
};

struct SceneSharedResultsHeadingOperation {
    SceneSharedResultsHeadingOperationKind kind{};
    std::uint32_t helper{};
    std::uint32_t object{};
    std::uint32_t cleanup_helper{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t flags{};
    std::uint32_t format_source{};
    std::uint32_t color_source{};
    std::uint32_t font_source{};
};

struct SceneSharedResultsHeadingPlan {
    bool reached{};
    bool initialize_results_resource{};
    bool initialize_prompt_resource{};
    bool refresh_results_resource{};
    bool refresh_prompt_resource{};
    std::uint32_t next_resource_init_mask_0x4eb274{};
    std::uint32_t next_prompt_init_mask_0x4eb37c{};
    std::int32_t prompt_x{};
    std::int32_t prompt_y{};
    std::uint32_t prompt_layout_object{0x004EB448};
    std::uint32_t text_height_helper{0x0044D460};
    std::uint32_t text_width_helper{0x0044D450};
    std::uint32_t layout_helper{0x0044D0C0};
    std::uint32_t resource_register_helper{0x00418C90};
    std::uint32_t initialize_helper{0x00418CC0};
    std::uint32_t results_resource_key{0x004CFF0C};
    std::uint32_t prompt_resource_key{0x004CFF24};
    std::uint32_t results_cleanup{0x00405260};
    std::uint32_t prompt_cleanup{0x00405250};
    std::uint32_t results_resource_object{0x004EB248};
    std::uint32_t prompt_resource_object{0x004EB410};
    std::uint32_t current_resource_generation{0x004D3868};
    std::uint32_t cached_results_generation{0x004EB250};
    std::uint32_t cached_prompt_generation{0x004EB418};
    std::uint32_t draw_helper{0x0044DC70};
    std::vector<SceneSharedResultsHeadingOperation> ordered_operations;
};

// Models the registration, centered prompt layout, resource refresh, and two
// draw calls in the 0x404940 shared-results branch. The decoded text draws
// place the results title ([4EB24C]) at (W/2, (H-0xB4)/2-8) and the ESC
// prompt text ([4EB414]) at (W/2, (H-0xB4)/2+0xBF), both with flags 0x24,
// color [4EB374], and font [4EB304]; all derive from the shared surface/2
// stack slot written at 0x404EA0.
[[nodiscard]] SceneSharedResultsHeadingPlan scene_shared_results_heading_plan(
    const SceneSharedResultsHeadingObservation& observation);

struct SceneSharedResultsRecordObservation {
    std::int32_t type_at_0x00{};
    std::int32_t ordinal_at_0x140{};
    bool virtual_method_0x40_result{};
    // Raw return from 0x45C230, which loads global 0x4DC19C.
    std::uint32_t row_helper_global_0x4dc19c{};
    std::string text_at_0x05;
};

struct SceneSharedResultRowSelection {
    std::size_t ordinal_pass{};
    std::size_t record_index{};
    std::int32_t displayed_ordinal{};
    bool row_helper_result_matches_index{};
    std::string text_at_0x05;
    bool emit_time_value{};
    std::uint8_t time_lookup_ordinal{};
};

// Projects the nested record-selection loops in the 0x404940 results path.
[[nodiscard]] std::vector<SceneSharedResultRowSelection>
scene_shared_result_rows(
    std::span<const SceneSharedResultsRecordObservation> records);

struct SceneSharedResultRowDisplay {
    SceneSharedResultRowSelection selection;
    float time_seconds{-1.0F};
    std::string formatted_time;
};

// Extends row selection through the observed ordinal lookup and 0x425A40
// formatter for rows whose virtual +0x40 call returns true.
[[nodiscard]] std::vector<SceneSharedResultRowDisplay>
scene_shared_result_row_displays(
    std::span<const SceneSharedResultsRecordObservation> records,
    std::span<const std::uint8_t> result_time_records,
    std::size_t result_time_record_count,
    char marker);

struct SceneSharedResultRowDraw {
    std::size_t ordinal_pass{};
    std::size_t record_index{};
    std::uint32_t call_site{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t format_source{};
    std::uint32_t color_source{};
    std::uint32_t font_source{0x004EB304};
    std::uint32_t flags{0x21};
    std::int32_t integer_argument{};
    std::string text_argument;
};

// Projects the three per-record 0x44DC70 calls in 0x404970. The binary derives
// every column from surface_width/2 (stored at 0x404EA0 and re-read at
// 0x404FF0/0x405009/0x40505C): ordinal x = W/2-0x91, record text x = W/2-0x6B,
// and time x = W/2+0x41. Row y starts at (H-0xB4)/2+0x11 because 0x404F71
// adds 0x14 to the row baseline before the loop, then advances 0x14 per
// selected row.
[[nodiscard]] std::vector<SceneSharedResultRowDraw>
scene_shared_result_row_draws(
    std::span<const SceneSharedResultsRecordObservation> records,
    std::span<const SceneSharedResultRowDisplay> displays,
    std::int32_t surface_height,
    std::int32_t surface_width);

struct SceneSharedResultsBranchObservation {
    SceneSharedOverlayObservation overlay_globals{};
    SceneSharedResultsHeadingObservation heading{};
    std::span<const SceneSharedResultsRecordObservation> records{};
    std::span<const std::uint8_t> result_time_records{};
    std::size_t result_time_record_count{};
    char time_marker{};
};

struct SceneSharedResultsBranchPlan {
    SceneSharedOverlayGates gates{};
    SceneSharedResultsHeadingPlan heading{};
    std::vector<SceneSharedResultRowDisplay> row_displays;
    std::vector<SceneSharedResultRowDraw> row_draws;
};

// Composes the raw race-results gate, heading/resource setup, nested row/time
// selection and row draw arguments for the 0x404940 branch.
[[nodiscard]] SceneSharedResultsBranchPlan scene_shared_results_branch_plan(
    const SceneSharedResultsBranchObservation& observation);

enum class SceneSharedOverlayBranch : std::uint8_t {
    paused_banner,
    cheater_banner,
    text_0x4048e0,
    text_0x404900,
    centered_resource_0x404920,
    race_results,
    splash_loading,
};

struct SceneSharedOverlayFrameObservation {
    RenderContextViewport render_context;
    std::uint8_t renderer_mode_0x4d7a48{};
    std::uint32_t framebuffer_base_value{};
    SceneSharedOverlayObservation overlay_globals{};
    // Shared resource-manager state read by the paused and splash branches.
    std::optional<std::uint32_t> current_resource_generation_value;
    std::optional<std::uint32_t> resource_init_mask_0x4eb37c;
    SceneSharedPausedBannerObservation paused_banner{};
    SceneSharedCheaterBannerObservation cheater_banner{};
    SceneSharedTextEffectsObservation text_effects{};
    SceneSharedCenteredResourceObservation centered_resource{};
    SceneSharedResultsBranchObservation race_results{};
    SceneSharedLoadingBannerObservation splash_loading{};
};

struct SceneSharedOverlayFramePlan {
    SceneSharedOverlayGates gates{};
    std::array<SceneSharedOverlayBranch, 7> predicate_evaluation_order{{
        SceneSharedOverlayBranch::paused_banner,
        SceneSharedOverlayBranch::cheater_banner,
        SceneSharedOverlayBranch::text_0x4048e0,
        SceneSharedOverlayBranch::text_0x404900,
        SceneSharedOverlayBranch::centered_resource_0x404920,
        SceneSharedOverlayBranch::race_results,
        SceneSharedOverlayBranch::splash_loading,
    }};
    SceneSharedPausedBannerPlan paused_banner{};
    std::vector<SceneRectangleFillPixelWrite>
        paused_banner_box_pixel_writes;
    SceneSharedCheaterBannerPlan cheater_banner{};
    SceneSharedTextEffectsPlan text_effects{};
    SceneSharedCenteredResourcePlan centered_resource{};
    SceneSharedResultsBranchPlan race_results{};
    SceneSharedLoadingBannerPlan splash_loading{};
};

// Composes the shared post-dashboard portion of 0x404970 in disassembly
// order. Per-branch dimensions, resources, and data buffers remain inputs.
[[nodiscard]] SceneSharedOverlayFramePlan scene_shared_overlay_frame_plan(
    const SceneSharedOverlayFrameObservation& observation);

struct SceneState5OverlayObservation {
    std::array<float, 9> matrix_0x507dc8{};
    bool fdiv_compatibility_enabled{};
    std::uint8_t background_renderer_mode_0x4d7a48{};
    std::uint32_t framebuffer_base_value{};
    std::int32_t clip_left{};
    std::int32_t clip_top{};
    std::int32_t clip_right{};
    std::int32_t clip_bottom{};
    std::int32_t framebuffer_pitch_bytes{};
    std::int32_t camera_index_0x4d488c{};
    std::int32_t camera_count_0x5081dc{};
    std::array<float, 3> position_0x507dec_df0_df4{};
    std::uint32_t background_value_0x507d48{};
    std::uint32_t first_text_color_0x5081e0{};
    std::uint32_t second_text_color_0x507df8{};
};

struct SceneState5TextCall {
    std::int32_t x{};
    std::int32_t y{};
    std::string text;
    std::uint32_t color{};
    std::uint32_t color_source{};
    std::uint32_t format_source{};
    std::uint32_t draw_helper{0x0044D6A0};
};

struct SceneTextGlyphCall {
    std::size_t text_call_index{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint8_t character{};
    std::uint32_t color{};
    std::uint32_t draw_helper{0x0044D780};
};

struct SceneTextPixelCall {
    std::size_t glyph_call_index{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t color{};
    std::uint32_t draw_helper{0x00449E90};
};

struct SceneTextPixelWrite {
    std::size_t glyph_call_index{};
    std::int32_t x{};
    std::int32_t y{};
    std::int64_t offset_from_framebuffer_base{};
    std::uint32_t destination_address{};
    std::uint32_t value{};
    std::uint8_t bytes_per_pixel{};
};

struct SceneDashboardAtlasFontObservation {
    std::int32_t glyph_width{};  // font[0]
    std::int32_t glyph_height{}; // font[1]
    std::int32_t origin_x{};     // font[2]
    std::int32_t origin_y{};     // font[3]
    std::size_t glyph_count{};   // font[4]
    // glyph -> row -> encoded runs. High-bit runs contain palette indices;
    // low-bit runs are transparent skips. Counts are the low seven bits.
    std::vector<std::vector<std::vector<std::uint8_t>>> encoded_rows;
    std::vector<std::uint16_t> embedded_palette;
};

struct SceneDashboardRasterObservation {
    std::uint8_t renderer_mode{}; // byte at the global renderer context
    std::int32_t clip_left{};
    std::int32_t clip_top{};
    std::int32_t clip_right{};
    std::int32_t clip_bottom{};
    std::int32_t framebuffer_pitch_bytes{};
    std::uint32_t framebuffer_base{};
    // Optional caller palette overrides the font's embedded palette.
    std::vector<std::uint16_t> explicit_palette;
};

struct ScenePausedBannerResourceRasterObservation {
    bool resource_pointer_nonnull{true};
    StampImage image;
    SceneDashboardRasterObservation renderer;
};

// Style index 0x0E is descriptor 14 in the runtime table built by 0x4714E0.
// Resource constructors still supply the two resulting handles as inputs.
struct ScenePausedBannerStyle14Observation {
    std::uint32_t font_name_source_0x55fe48{};
    std::uint32_t resolved_font_source_0x44da00{};
    std::uint32_t resolved_color_source_0x44d530{};
    std::int32_t global_x_offset_0x560000{};
    // Width returned by 0x44D450 for the optional style-row resource at +0x2C.
    std::optional<std::int32_t> optional_resource_width_0x44d450;
};

struct ScenePausedBannerStyle14Plan {
    std::array<std::uint32_t, 13> descriptor_words{};
    SceneDashboardTextCall text_style{};
    std::int32_t x_offset{};
    std::int32_t y_offset{};
    bool measure_inputs_complete{true};
    std::int32_t optional_resource_width{};
};

struct ScenePausedBannerStyle14MeasurePlan {
    bool input_complete{true};
    bool text_pointer_nonnull{};
    std::int32_t text_width{};
    std::int32_t style_minimum_width{};
    std::int32_t optional_resource_width{};
    std::int32_t returned_width{};
};

struct ScenePausedBannerTextRasterObservation {
    // The 0x479580 helper skips line draws when its text pointer is null.
    bool text_pointer_nonnull{true};
    std::string_view text;
    // Used when style14 is absent; style14 derives flags and offsets from the
    // initialized 0x4DE7A8 descriptor while retaining resource handles as inputs.
    SceneDashboardTextCall style{};
    std::int32_t style_x_offset_0x20{};
    std::int32_t style_y_offset_0x1c{};
    std::int32_t font_extent_0x44dc50{};
    std::span<const SceneDashboardRasterObservation> per_line_renderers;
    std::optional<ScenePausedBannerStyle14Observation> style14;
};

struct ScenePausedBannerTextRasterPlan {
    bool input_complete{true};
    std::size_t expected_line_count{};
    std::size_t text_calls_rasterized{};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
};

struct SceneDashboardRasterPlan {
    bool input_complete{true};
    std::size_t glyphs_considered{};
    std::size_t glyph_rows_missing{};
    std::size_t palette_indices_missing{};
    std::vector<SceneTextPixelWrite> pixel_writes;
};

struct ScenePausedBannerResourceRasterPlan {
    bool draw_reached{};
    bool input_complete{true};
    std::size_t frame_index{};
    SceneDashboardRasterPlan raster;
};

struct SceneStampImageRasterCall {
    bool reached{};
    std::uint32_t draw_helper{0x0044D0C0};
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t frame_index{};
};

struct SceneStampImageRasterObservation {
    bool resource_pointer_nonnull{true};
    StampImage image;
    SceneDashboardRasterObservation renderer;
};

struct SceneStampImageRasterPlan {
    bool draw_reached{};
    bool input_complete{true};
    std::size_t frame_index{};
    SceneDashboardRasterPlan raster;
};

// Projects one reached 0x44D0C0 STMP draw through the shared RLE/palette path.
[[nodiscard]] SceneStampImageRasterPlan raster_scene_stamp_image(
    const SceneStampImageRasterCall& draw,
    const SceneStampImageRasterObservation& observation);

struct ScenePausedBannerFramePixelWrite {
    std::int32_t x{};
    std::int32_t y{};
    std::int64_t offset_from_framebuffer_base{};
    std::uint32_t value{};
    std::uint32_t write_mask{};
    std::uint8_t bytes_per_pixel{};
    bool preserves_existing_bits{};
};

struct ScenePausedBannerFrameRasterPlan {
    bool reached{};
    bool input_complete{true};
    std::vector<ScenePausedBannerFramePixelWrite> ordered_pixel_writes;
};

struct ScenePausedBannerFrameApplyResult {
    bool applied{};
    std::size_t pixels_written{};
};

struct SceneSharedTextEffectsRasterObservation {
    // Slots correspond to 0x4048E0 then 0x404900; inactive branches need no text.
    std::array<std::optional<std::string_view>, 2> text;
    std::array<SceneDashboardRasterObservation, 2> per_call_renderers{};
};

struct SceneSharedTextEffectsRasterPlan {
    bool input_complete{true};
    std::size_t text_calls_rasterized{};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
};

// Rasterizes reached 0x4048E0/0x404900 text effects in callback order.
[[nodiscard]] SceneSharedTextEffectsRasterPlan
raster_scene_shared_text_effects(
    const SceneSharedTextEffectsPlan& plan,
    const SceneSharedTextEffectsRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font);

struct SceneDashboardBuiltinFontObservation {
    std::string_view text;
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t color{};
    SceneDashboardRasterObservation renderer;
};

struct SceneDashboardBuiltinFontPlan {
    bool input_complete{true};
    std::size_t characters_drawn{};
    std::size_t control_bytes_replaced{};
    std::vector<SceneTextPixelWrite> pixel_writes;
};

// Projects one 0x44D6A0/0x44D780 built-in-font pass through 0x449E90 and the
// mode-specific 16/32-bit writer. Use sequential calls for shadow then text.
[[nodiscard]] SceneDashboardBuiltinFontPlan
raster_scene_dashboard_builtin_font(
    const SceneDashboardBuiltinFontObservation& observation);

struct SceneCarCalloutRasterObservation {
    std::uint32_t color_0x53f194{};
    SceneDashboardRasterObservation renderer;
    std::optional<bool> secondary_resource_pointer_nonnull;
    const StampImage* secondary_resource_image{};
};

struct SceneCarCalloutRasterSegment {
    SceneCarCalloutOperationKind source_kind{};
    std::size_t first_pixel_write{};
    std::size_t pixel_write_count{};
};

struct SceneCarCalloutRasterPlan {
    SceneCarCalloutStatus callout_status{SceneCarCalloutStatus::complete};
    bool input_complete{true};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
    // Includes the secondary resource pixels between the two text segments.
    std::vector<SceneCarCalloutRasterSegment> ordered_segments;
};

// Rasterizes callout strings and the `thought.stp` 0x44D0C0 image through the
// shared built-in-font and STMP image paths, preserving callback draw order.
[[nodiscard]] SceneCarCalloutRasterPlan raster_scene_car_callout(
    const SceneCarCalloutPlan& callout,
    const SceneCarCalloutRasterObservation& observation);

// Models atlas clipping, transparent/literal RLE runs, palette selection,
// and 16-bit destination addressing in 0x44D0C0/0x449D70.
[[nodiscard]] SceneDashboardRasterPlan raster_scene_dashboard_text(
    const SceneDashboardTextLayout& layout,
    const SceneDashboardAtlasFontObservation& font,
    const SceneDashboardRasterObservation& renderer);

// Projects a paused-banner resource draw through 0x44D0C0's frame-row RLE,
// clipping, palette-plane choice, and 16-bit writes.
[[nodiscard]] ScenePausedBannerResourceRasterPlan
raster_scene_paused_banner_resource(
    const ScenePausedBannerRenderPlan& banner,
    const ScenePausedBannerResourceRasterObservation& observation);

// Rasterizes the line calls performed by the paused banner's 0x479580 helper.
// Style-table offsets, font height, localized text, and line renderer state
// remain explicit observations.
[[nodiscard]] ScenePausedBannerTextRasterPlan
raster_scene_paused_banner_text(
    const ScenePausedBannerRenderPlan& banner,
    const ScenePausedBannerTextRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font);

// Projects the statically initialized descriptor 0x4DE7A8 and the style fields
// copied by 0x478EE0. Resource-constructor return values remain observations.
[[nodiscard]] ScenePausedBannerStyle14Plan
scene_paused_banner_style14_plan(
    const ScenePausedBannerStyle14Observation& observation) noexcept;

// Projects 0x479450 for style 0x0E. The text width comes from 0x44DB10; a
// null text pointer returns the optional resource width alone.
[[nodiscard]] ScenePausedBannerStyle14MeasurePlan
scene_paused_banner_style14_measure_width(
    const ScenePausedBannerStyle14Plan& style,
    bool text_pointer_nonnull,
    std::int32_t text_width) noexcept;

struct SceneDashboardCoreCallbackRasterObservation {
    // Optional fallback preformatted arguments aligned with ordered callback
    // calls. A call's resolved_text_argument takes precedence when available.
    std::span<const std::string_view> formatted_text_arguments;
    std::span<const SceneDashboardRasterObservation> per_call_renderers;
};

struct SceneDashboardCoreCallbackRasterPlan {
    bool input_complete{true};
    std::size_t expected_call_count{};
    std::size_t text_arguments_used{};
    std::size_t renderer_contexts_used{};
    std::size_t text_calls_rasterized{};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
};

// Rasterizes the ordered text calls emitted by the state-0 dashboard callback.
// Pixel writes are projected; surface mutation and device ownership stay external.
[[nodiscard]] SceneDashboardCoreCallbackRasterPlan
raster_scene_dashboard_core_callback(
    const SceneDashboardCoreCallbackPlan& callback,
    const SceneDashboardCoreCallbackRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font);

struct ScenePhysicsWheelBarLabelRasterObservation {
    // Text color/font/flags selected by the 0x479580 style object (style 0x11).
    SceneDashboardTextCall text_style{};
    std::array<SceneDashboardRasterObservation, 4> per_bar_renderers{};
};

struct ScenePhysicsWheelBarLabelRasterPlan {
    bool input_complete{true};
    std::size_t labels_rasterized{};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
};

// Rasterizes the formatted `%1.0f` labels submitted by the four 0x479580
// calls, using supplied style, font-table, atlas, and renderer observations.
[[nodiscard]] ScenePhysicsWheelBarLabelRasterPlan
raster_scene_physics_wheel_bar_labels(
    const std::array<ScenePhysicsWheelBarPlan, 4>& bars,
    const ScenePhysicsWheelBarLabelRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font);

struct SceneSharedResultsRasterPlan {
    bool input_complete{true};
    std::size_t row_calls_rasterized{};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
};

// Connects selected results-row calls through 0x44DC70 font layout and the
// 0x44D0C0 atlas rasterizer. Each call has a corresponding runtime renderer
// observation so color palettes, clipping, and surface addresses stay explicit.
// The observed ordinal format handle 0x4CFF38 formats integer_argument as
// signed decimal; the record-text and time handles consume text_argument.
[[nodiscard]] SceneSharedResultsRasterPlan
raster_scene_shared_results_rows(
    std::span<const SceneSharedResultRowDraw> row_draws,
    std::span<const SceneDashboardRasterObservation> per_call_renderers,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font);

struct SceneSharedResultsHeadingTextRasterObservation {
    std::string_view results_title_text;
    std::string_view prompt_text;
    std::array<SceneDashboardRasterObservation, 2> per_call_renderers{};
};

struct SceneSharedResultsHeadingTextRasterPlan {
    bool input_complete{true};
    std::size_t heading_text_calls_rasterized{};
    std::vector<SceneTextPixelWrite> ordered_pixel_writes;
};

// Rasterizes the two 0x44DC70 heading operations from the results plan. The
// localized strings and renderer/font observations remain supplied inputs.
[[nodiscard]] SceneSharedResultsHeadingTextRasterPlan
raster_scene_shared_results_heading_text(
    const SceneSharedResultsHeadingPlan& heading,
    const SceneSharedResultsHeadingTextRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font);

[[nodiscard]] SceneTextPixelApplyResult
apply_scene_shared_results_raster(
    const SceneSharedResultsRasterPlan& plan,
    std::span<std::uint8_t> framebuffer);

struct SceneTextPixelApplyResult {
    bool applied{};
    std::size_t pixels_written{};
};

// Applies a fully bounds-checked set of projected little-endian pixel writes.
// The framebuffer is unchanged when any write falls outside the supplied span.
[[nodiscard]] SceneTextPixelApplyResult apply_scene_text_pixel_writes(
    std::span<const SceneTextPixelWrite> writes,
    std::span<std::uint8_t> framebuffer);

// Merges fallback boxes, resource pixels, and banner text in the native
// paused-banner operation order. Missing raster inputs for reached draw calls
// are reported as incomplete rather than silently omitted.
[[nodiscard]] ScenePausedBannerFrameRasterPlan
compose_scene_banner_frame_raster(
    const ScenePausedBannerRenderPlan& banner,
    std::span<const SceneRectangleFillPixelWrite> box_pixel_writes,
    const ScenePausedBannerResourceRasterPlan* resource_raster,
    const ScenePausedBannerTextRasterPlan* text_raster);

[[nodiscard]] ScenePausedBannerFrameRasterPlan
compose_scene_paused_banner_frame_raster(
    const SceneSharedOverlayFramePlan& frame,
    const ScenePausedBannerResourceRasterPlan* resource_raster,
    const ScenePausedBannerTextRasterPlan* text_raster);

// Validates the full ordered write stream before mutating the supplied buffer.
[[nodiscard]] ScenePausedBannerFrameApplyResult
apply_scene_paused_banner_frame_raster(
    const ScenePausedBannerFrameRasterPlan& plan,
    std::span<std::uint8_t> framebuffer);

struct SceneState5OverlayPlan {
    MatrixQuaternionResult quaternion;
    bool zero_rotation_vector{};
    bool fdiv_compatibility_branch{};
    std::array<float, 3> rotation_vector_radians{};
    std::array<long double, 3> rotation_vector_degrees{};
    // Arguments in callee order: left, top, right, bottom, raw color value.
    std::array<std::uint32_t, 5> background_call_arguments{};
    std::uint32_t background_color_source{0x00507D48};
    std::uint32_t background_draw_helper{0x0044A1B0};
    std::uint32_t background_backend_helper{0x0044A490};
    std::array<SceneState5TextCall, 2> text_calls;
    std::vector<SceneTextGlyphCall> glyph_calls;
    std::vector<SceneTextPixelCall> pixel_calls;
    std::vector<SceneTextPixelWrite> pixel_writes;
};

// Models the state-5 callback's numeric transform and its two text draws.
// Text colors and global camera/position values are supplied as raw inputs.
[[nodiscard]] SceneState5OverlayPlan state5_numeric_overlay_plan(
    const SceneState5OverlayObservation& observation);

struct SceneDashboardLocaleObservation {
    // First byte returned by GetLocaleInfoA(0x400, 0x0D, ...).
    char short_date_first_character{};
    // First byte returned by GetLocaleInfoA(0x400, 0x0E, ...).
    char decimal_marker{};
    // Two raw bytes written by GetLocaleInfoA(0x400, 0x16, ..., 2).
    std::array<std::uint8_t, 2> locale_0x16_bytes{};
};

struct SceneDashboardLocaleState {
    bool short_date_first_character_is_zero{};
    char decimal_marker{};
    std::array<std::uint8_t, 2> locale_0x16_bytes{};
    // The 13 DWORDs copied to 0x4F0500. Preserve their bit patterns: several
    // are floats and the binary also reads the first and fifth as pointers.
    std::array<std::uint32_t, 13> raw_table_dwords{};
};

// Models the locale-dependent data initialization in 0x418500. The choice
// uses only the first short-date byte; raw table words retain their original
// bits and are not interpreted as host strings or physical units.
[[nodiscard]] SceneDashboardLocaleState initialize_scene_dashboard_locale(
    const SceneDashboardLocaleObservation& observation) noexcept;

// Models the target VS 1998 formatter's `%s` behavior: a null pointer is
// replaced with the string at 0x4E9A28 (`"(null)"`).
[[nodiscard]] std::string_view scene_dashboard_crt_string_argument(
    std::uint32_t pointer_bits,
    std::string_view pointed_to_text) noexcept;

// Models the two cached type-0/type-3/type-5 metrics at 0x403F7E and
// 0x404147. Observed printf format strings and decimal-marker substitution are
// reconstructed; resource and global-string values remain caller inputs.
[[nodiscard]] SceneDashboardMetricResult update_scene_dashboard_metric(
    SceneDashboardState& state,
    const SceneDashboardMetricObservation& observation,
    bool redraw_all);

// Models the record-derived 0x404313 readout without assigning its field a
// gameplay meaning. Glyph measurement and external strings remain inputs.
// Copies locale-derived markers and raw scalar words into matching inputs.
// Table words 1/5 map to object +0x0C/+0x1C; the affected printf-backed
// observation strings receive the target's `(null)` substitution as needed.
void bind_scene_dashboard_locale(
    SceneDashboardLapTimeObservation& observation,
    const SceneDashboardLocaleState& locale) noexcept;
void bind_scene_dashboard_locale(
    SceneDashboardTestTrackObservation& observation,
    const SceneDashboardLocaleState& locale);
void bind_scene_dashboard_locale(
    SceneDashboardSupplementalTimeObservation& observation,
    const SceneDashboardLocaleState& locale) noexcept;
void bind_scene_dashboard_locale(
    SceneDashboardMetricObservation& observation,
    const SceneDashboardLocaleState& locale);
void bind_scene_dashboard_locale(
    SceneDashboardAuxiliaryReadoutObservation& observation,
    const SceneDashboardLocaleState& locale);

struct SceneGraphicsOverlayState {
    float lod_factor_0x4d11d4{1.0F};
    bool action_0x40_enabled{};
    bool action_0x01_enabled{};
    std::uint32_t active_action_mask_0x509f50{};
};

enum class SceneGraphicsOverlayOperationKind : std::uint8_t {
    background_fill_0x44a1b0,
    formatted_lod_text_0x44d6a0,
    static_text_0x44d6a0,
};

struct SceneGraphicsOverlayOperation {
    SceneGraphicsOverlayOperationKind kind{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t text_source{};
    std::uint32_t color_source{};
    std::int32_t row_index{-1};
    float lod_value{};
};

// Models 0x40D650's key effects and numeric clamp. It always returns zero in
// the binary, so this helper reports no consumed key event.
void update_scene_graphics_overlay(SceneGraphicsOverlayState& state,
                                   std::uint16_t key) noexcept;

// Text calls emitted by 0x40D570. The LOD value is supplied separately in
// SceneGraphicsOverlayState for the format string at 0x4D11D8.
[[nodiscard]] std::vector<SceneOverlayTextCall>
scene_graphics_overlay_text_plan(const SceneGraphicsOverlayState& state);

// Ordered drawing operations from the 0x40D570 instruction listing.
[[nodiscard]] std::vector<SceneGraphicsOverlayOperation>
scene_graphics_overlay_render_plan(const SceneGraphicsOverlayState& state);

// Projects the state-3 background rectangle (1,1)-(0xE6,0x22) through the
// same 0x44A1B0 renderer-mode, clipping, and pixel-write path.
[[nodiscard]] std::vector<SceneRectangleFillPixelWrite>
scene_graphics_overlay_background_pixel_writes(
    const RenderContextViewport& context,
    std::uint8_t renderer_mode,
    std::uint32_t framebuffer_base,
    std::uint32_t background_color);

struct SceneGraphicsOverlayRasterObservation {
    SceneGraphicsOverlayState state{};
    // Already formatted through the target's 0x4D11D8 format at 0xBD190.
    std::string_view lod_text;
    RenderContextViewport render_context{};
    std::uint8_t renderer_mode{};
    std::uint32_t framebuffer_base{};
    std::uint32_t background_color{};
    std::uint32_t lod_text_color{};
    std::uint32_t instruction_text_color{};
    std::uint32_t active_action_color{};
    std::uint32_t inactive_action_color{};
};

struct SceneGraphicsOverlayRasterPlan {
    bool input_complete{true};
    std::vector<SceneRectangleFillPixelWrite> background_writes;
    std::vector<SceneTextPixelWrite> ordered_text_writes;
};

// Projects state-3 fill and built-in glyph writes in observed draw order.
// Runtime colors, formatted LOD text, and surface context remain inputs.
[[nodiscard]] SceneGraphicsOverlayRasterPlan
raster_scene_graphics_overlay(
    const SceneGraphicsOverlayRasterObservation& observation);

[[nodiscard]] SceneRectangleFillApplyResult
apply_scene_graphics_overlay_raster(
    const SceneGraphicsOverlayRasterPlan& plan,
    std::span<std::uint8_t> framebuffer);

} // namespace viper
