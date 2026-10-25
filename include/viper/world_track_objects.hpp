#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

class ResourceBundle;
struct TerrainSurfaceResourcePayload;

struct TrackObjectTextTable {
    static constexpr std::uint32_t bats_resource_type_code = 0x53544142U;
    static constexpr std::size_t header_size = 0x4CU;
    static constexpr std::size_t offset_count = 16;

    std::uint32_t line_count{};
    std::uint32_t raw_header_word_0x04{};
    std::array<std::uint32_t, offset_count> line_offsets{};
    std::uint32_t line_stride{};
    std::vector<std::byte> raw_payload;
};

struct IdealLineResourcePayload {
    static constexpr std::uint32_t supported_resource_version = 3U;
    static constexpr std::uint32_t internal_marker = 0xFFFFFFFEU;
    static constexpr std::uint32_t record_stride = 0x44U;
    static constexpr std::size_t header_size = 0x0CU;
    using Record = std::array<std::byte, record_stride>;

    std::uint32_t resource_version{};
    std::uint32_t line_count{};
    std::vector<Record> records;
    std::vector<std::byte> trailing_bytes;
};

// Parses the ILIN/version-3 payload accepted by the ideal-line loader at
// 0x41CE80 while preserving the complete 0x44-byte records.
[[nodiscard]] std::optional<IdealLineResourcePayload> parse_ideal_line_resource(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

struct IdealLineTrackData {
    using Record = IdealLineResourcePayload::Record;

    std::vector<Record> records;
    std::vector<std::size_t> next_indices;
    std::vector<std::byte> trailing_bytes;
    bool orientation_normalized{};
    bool category_labels_normalized{};
    bool category_label_input_incomplete{};
};

// Models the optional orientation normalization and unconditional circular
// list/index setup in 0x41CF60.
[[nodiscard]] std::optional<IdealLineTrackData> normalize_ideal_line_resource(
    const IdealLineResourcePayload& payload,
    std::uint8_t orientation_mode);

// Parses the BATS payload returned for "track.obt" by 0x418F20.
[[nodiscard]] std::optional<TrackObjectTextTable> parse_track_object_text_table(
    std::span<const std::byte> payload);

// Mirrors 0x418FB0's base + 0x4C + stride*line + line_offsets[channel].
[[nodiscard]] std::optional<std::string_view> track_object_text_line(
    const TrackObjectTextTable& table,
    std::size_t line_index,
    std::size_t channel = 0) noexcept;

enum class TrackDirectiveKind {
    empty_or_unscannable,
    comment,
    end,
    map,
    ai,
    object,
    bad_token,
};

struct TrackDirectiveObservation {
    TrackDirectiveKind kind{};
    std::string_view object_line;
};

// Classifies the first token consumed by the 0x45C630 track.obt loop.
// The map and ai handlers have no observed state effect in that loop.
[[nodiscard]] TrackDirectiveObservation classify_track_directive(
    std::string_view line) noexcept;

enum class TrackPhysicsCursorStatus {
    complete,
    track_data_incomplete,
    unsupported_prior_object,
    player_car_not_found,
    stream_plan_failed,
};

struct TrackPhysicsCursorPlan {
    TrackPhysicsCursorStatus status{TrackPhysicsCursorStatus::player_car_not_found};
    std::optional<std::uint32_t> serialized_object_stream_offset;
    std::size_t target_car_index{};
    std::size_t preceding_car_count{};
    struct PrecedingRecord {
        std::size_t object_row{};
        // Ordinal among preceding obj car directives; it is not yet joined
        // to a resolved entrant row or verified subtype.
        std::optional<std::size_t> car_directive_index;
        std::uint32_t record_tag{};
        std::uint32_t cursor_offset{};
        std::uint32_t cursor_advance{};
    };
    // Modeled prefix in source-row order. This is a cursor manifest only;
    // complete constructor record bytes and native task-load order remain
    // separate inputs.
    std::vector<PrecedingRecord> preceding_records;
    std::optional<std::size_t> incomplete_row;
    std::optional<std::size_t> unsupported_object_row;
};

enum class TrackPhysicsTaskManifestStatus {
    complete,
    track_data_incomplete,
    unsupported_directive,
    unsupported_object_type,
    invalid_configured_car_tag,
    cursor_overflow,
};

struct TrackPhysicsTaskManifestEntry {
    // Present for records emitted while 0x45C630 walks track.obt. Configured
    // cars emitted by the later 0x45CAB0 pass have no track row.
    std::optional<std::size_t> track_object_row;
    std::optional<std::size_t> configured_car_index;
    std::uint32_t record_tag{};
    std::uint32_t cursor_offset{};
    std::uint32_t cursor_advance{};
};

struct TrackPhysicsTaskManifest {
    TrackPhysicsTaskManifestStatus status{
        TrackPhysicsTaskManifestStatus::complete};
    std::uint32_t initial_cursor{};
    std::optional<std::uint32_t> final_cursor;
    // This is the ordered tag/cursor manifest, not a claim that record payload
    // bytes or physics task objects have been produced.
    std::vector<TrackPhysicsTaskManifestEntry> entries;
    std::optional<std::size_t> player_entry_index;
    std::optional<std::uint32_t> player_stream_offset;
    std::optional<std::size_t> failed_track_object_row;
};

// Models 0x45C630's source order: object directives first, then any configured
// car rows not consumed by an obj car directive via 0x45CAB0. The supplied car
// tags come from the configured entrant rows (0x45CAE0 subtype markers).
[[nodiscard]] TrackPhysicsTaskManifest plan_track_physics_task_manifest(
    const TrackObjectTextTable& table,
    std::span<const std::uint32_t> configured_car_record_tags,
    std::size_t player_car_index,
    std::uint32_t initial_cursor = 0U);

// Derives the native physics-task stream cursor before a configured player car
// from track.obt serialization order. It supports recovered car, static,
// obstacle, wobble, checkpoint, and ball tags; unknown preceding scene-object
// classes leave the cursor unresolved instead of assigning a guessed advance.
[[nodiscard]] TrackPhysicsCursorPlan plan_track_player_physics_cursor(
    const TrackObjectTextTable& table,
    std::size_t player_car_index);

enum class TrackCarSubtype : std::uint32_t {
    pcar = 0,
    acar = 1,
    ncar = 2,
    gcar = 3,
};

enum class CarTrackPlacementRoute {
    track_basis_and_terrain,
    precomputed_transform,
};

enum class CarTrackLineDisposition {
    no_configured_car,
    bad_car_line,
    unknown_subtype,
    construction_ready,
};

struct TrackBasisTransform {
    // Binary global order: 0x53A438, 0x53A43C, 0x53A440,
    // 0x53A444, 0x53A448, 0x53A44C, 0x53A450, 0x53A454, 0x53A458.
    std::array<float, 9> basis{};
    std::array<float, 3> translation{};
};

struct TrackTransformCheckpointCandidate {
    // Result count from sscanf("obj checkpoint %s %f,%f %f,%f"). The target
    // accepts any count greater than three, even if one coordinate is absent.
    std::uint32_t scanf_conversion_count{};
    float first_x{};
    float first_z{};
    float second_x{};
    std::optional<float> second_z;
};

enum class TrackTransformDisposition {
    checkpoint_not_found_fatal,
    identity_from_checkpoint,
    centered_half_turn,
    centered_half_turn_missing_coordinate,
};

struct TrackTransformPlan {
    TrackTransformDisposition disposition{};
    TrackBasisTransform transform;
    std::optional<std::size_t> checkpoint_candidate_index;
    bool uses_uninitialized_fourth_coordinate{};
    bool x87_rounding_approximated{};
};

// Models the checkpoint scan and transform setup in 0x45C860. Candidate
// indices correspond to the scanned BATS lines, beginning at line index 1.
[[nodiscard]] TrackTransformPlan plan_track_transform_from_checkpoint(
    std::span<const TrackTransformCheckpointCandidate> candidates,
    bool centered_half_turn_enabled) noexcept;

struct CarSplinePoseObservation {
    // Result of the geometry iterator in 0x41E380. A missing segment makes
    // 0x45CAE0 fall back to its track-basis/terrain placement branch.
    bool segment_selected{};
    // 0x45CE10's placement ordinal; its low bit chooses the lateral side.
    std::int32_t placement_ordinal{};
    float segment_fraction{};
    // The four 2D samples supplied to the cubic Hermite interpolation at
    // 0x45D110: endpoint 0, endpoint 1, tangent 0, tangent 1 (X,Z).
    std::array<std::array<float, 2>, 4> curve_samples{};
    // Direction values at the selected track segment's two ends, blended and
    // normalized by 0x41FD80.
    std::array<float, 2> direction_start{};
    std::array<float, 2> direction_end{};
    // Result of the terrain query at the laterally offset position.
    float terrain_height{};
};

struct TrackSplineIndexNode {
    // Index of the next node in the target's circular linked list.
    std::size_t next_index{};
    float position_x{};
    float position_z{};
    float direction_x{};
    float direction_z{};
    float segment_key{};
    float segment_duration{};
};

[[nodiscard]] std::vector<TrackSplineIndexNode> make_track_spline_index_nodes(
    const IdealLineTrackData& track_data);

enum class TrackSplineSelectionDisposition {
    geometry_unavailable,
    selected,
    invalid_head,
    invalid_link,
    invalid_cycle,
};

struct TrackSplineSelectionObservation {
    bool geometry_loaded{};
    std::size_t head_index{};
    std::span<const TrackSplineIndexNode> nodes;
    float target_key{};
};

struct TrackGeometryKeyObservation {
    // Gate at geometry object +0x30 in 0x41E6C0.
    bool projection_key_enabled{};
    float object_x_0x18{};
    float object_z_0x1c{};
    float reference_x_0x3c{};
    float reference_z_0x40{};
    float x_weight_0x4c{};
    float z_weight_0x50{};
};

[[nodiscard]] float compute_track_geometry_key(
    const TrackGeometryKeyObservation& observation) noexcept;

struct CarSplineTargetKeyPlan {
    float placement_base_key{};
    float car_key_offset{};
    float target_key{};
};

// Models 0x45CE10's key query: placement base at geometry +0x5C minus
// (ordinal * 15 + 28).
[[nodiscard]] CarSplineTargetKeyPlan plan_car_spline_target_key(
    float placement_base_key,
    std::int32_t placement_ordinal) noexcept;

// 0x45CAE0 computes the ordinal from the global car index minus the GCAR
// counter, after incrementing that counter for a GCAR placement.
[[nodiscard]] std::int32_t compute_car_placement_ordinal(
    std::int32_t global_car_index,
    std::int32_t gcar_count_before,
    bool current_car_is_gcar) noexcept;

struct TrackSplineSelectionPlan {
    TrackSplineSelectionDisposition disposition{};
    std::optional<std::size_t> selected_node_index;
    float segment_fraction{};
    bool fraction_unclamped{};
};

// Models 0x41E380's circular-list walk and raw interpolation fraction.
[[nodiscard]] TrackSplineSelectionPlan select_track_spline_segment(
    const TrackSplineSelectionObservation& observation) noexcept;

struct TrackSplineInitialSelectionObservation {
    std::span<const TrackSplineIndexNode> nodes;
    std::size_t head_index{};
    std::array<float, 2> target_xz{};
};

struct TrackSplineInitialSelectionPlan {
    std::optional<std::size_t> segment_index;
    float nearest_endpoint_distance_squared{};
    bool returned_empty_head{};
    bool malformed_link{};
};

// Models 0x41D290's oriented-segment and nearest-endpoint selection.
[[nodiscard]] TrackSplineInitialSelectionPlan select_initial_track_segment(
    const TrackSplineInitialSelectionObservation& observation) noexcept;

struct TrackSplineProjectionObservation {
    std::span<const TrackSplineIndexNode> nodes;
    std::size_t head_index{};
    std::array<float, 2> target_xz{};
    std::optional<std::size_t> current_segment;
    float current_fraction{};
    float maximum_track_key{10000.0F};
};

struct TrackSplineProjectionPlan {
    std::optional<std::size_t> segment_index;
    float fraction{};
    float projected_track_key{};
    std::uint32_t iteration_count{};
    bool converged{};
    bool stopped_at_key_limit{};
    bool cleared_when_projection_moved_backward{};
    bool malformed_link_or_traversal_guard{};
    bool x87_and_pentium_fdiv_rounding_approximated{};
};

// Models 0x41D3A0's bounded Newton-like projection over the linked segments.
[[nodiscard]] TrackSplineProjectionPlan project_track_position_to_key(
    const TrackSplineProjectionObservation& observation) noexcept;

struct CarSplinePosePlan {
    bool helper_succeeded{};
    std::array<float, 12> transform{};
    bool x87_and_pentium_fdiv_rounding_approximated{};
};

// Reconstructs the selected-segment interpolation and 3x4 pose construction
// in 0x45CE10; loaded geometry values and the terrain query remain observations.
[[nodiscard]] CarSplinePosePlan plan_car_spline_pose(
    const CarSplinePoseObservation& observation) noexcept;

struct CarSplineTrackPlacementObservation {
    bool geometry_loaded{};
    std::size_t head_index{};
    std::span<const TrackSplineIndexNode> nodes;
    float placement_base_key{};
    std::int32_t placement_ordinal{};
    float terrain_height{};
};

struct CarSplineTrackPlacementPlan {
    CarSplineTargetKeyPlan key_plan;
    TrackSplineSelectionPlan segment_plan;
    CarSplinePosePlan pose_plan;
    std::optional<std::size_t> next_node_index;
};

// Composes 0x45CE10's geometry +0x5C key adjustment, 0x41E380 segment
// selection, and 0x45D110/0x41FD80 pose inputs from adjacent ILIN rows.
[[nodiscard]] CarSplineTrackPlacementPlan plan_car_spline_track_placement(
    const CarSplineTrackPlacementObservation& observation) noexcept;

struct CarTrackLineObservation {
    std::size_t configured_car_index{};
    std::size_t configured_car_count{};
    std::uint32_t raw_subtype{};
    std::string model_name;
    // These values are the results of scanning obj car %f, %f; the target
    // initializes both locals to zero and ignores the conversion count.
    float x{};
    float z{};
    bool force_default_placement_0x4dc1b0{};
    std::optional<std::array<float, 12>> precomputed_transform;
    std::optional<CarSplinePoseObservation> precomputed_pose;
    // GCAR counter before this car's subtype-specific increment.
    std::int32_t gcar_count_before{};
    std::optional<CarSplineTrackPlacementObservation> spline_placement;
    TrackBasisTransform track_basis;
    // Result of 0x45F600 at the default branch's transformed X/Z.
    float terrain_height{};
    bool configured_effect_flag_0xc4{};
    std::uint32_t gcar_timer_value{};
};

struct CarTrackConstructionPlan {
    static constexpr std::size_t runtime_record_size = 0x1e4U;
    static constexpr std::size_t transform_record_offset = 0x18U;

    CarTrackLineDisposition disposition{};
    std::optional<TrackCarSubtype> subtype;
    std::uint32_t record_tag{};
    std::uint32_t record_size{0x1e4U};
    std::uint32_t constructor_address{};
    std::size_t allocation_size{};
    std::uint32_t gcar_timer_value{};
    std::size_t car_index_before{};
    std::size_t car_index_after{};
    std::int32_t placement_ordinal{};
    CarTrackPlacementRoute placement_route{};
    // Row-major 3x3 orientation followed by X/Y/Z position; these 12 floats
    // are stored at runtime-record offset +0x18 by 0x45CAE0.
    std::array<float, 12> transform{};
    std::optional<CarSplineTrackPlacementPlan> spline_placement_plan;
    std::string model_name;
    bool calls_acar_helper_0x41a2a0{};
    bool increments_gcar_counter{};
    bool reads_gcar_timer_value{};
    bool creates_configured_ball_effect{};
    bool precomputed_pose_rounding_approximated{};
};

// Models 0x45CAE0's configured-car gate, subtype selection, constructor route,
// and placement selection. CARF record contents and helper side effects remain
// outside this plan.
[[nodiscard]] CarTrackConstructionPlan plan_car_track_line(
    const CarTrackLineObservation& observation);

// Applies only the placement transform, preserving the rest of the CARF-mapped
// runtime record. Requires a complete 0x1E4-byte record and ready plan.
[[nodiscard]] bool apply_car_track_placement(
    std::span<std::byte> runtime_record,
    const CarTrackConstructionPlan& plan) noexcept;

struct CarFilePayload {
    static constexpr std::uint32_t resource_type_code = 0x43415246U;
    static constexpr std::uint32_t supported_resource_version = 6;
    static constexpr std::size_t payload_size = 0x228;
    static constexpr std::size_t word_count = payload_size / sizeof(std::uint32_t);
    static constexpr std::string_view bundle_extension = ".car";
    static constexpr std::string_view resource_extension = ".cf";

    std::uint32_t resource_version{};
    std::size_t source_payload_size{};
    bool payload_size_differs_from_copy_size{};
    std::array<std::uint32_t, word_count> raw_words{};
};

// 0x45E2F0 calls the empty 0x45E470 helper with a size-check result, then
// unconditionally copies 0x8A little-endian DWORDs for version 6. This safe
// span projection accepts surplus bytes and records the mismatch; a short
// span cannot supply the native fixed-size copy and returns nullopt.
[[nodiscard]] std::optional<CarFilePayload> parse_car_file_payload(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

enum class CarFileReadStep : std::uint8_t {
    open_resource,
    report_missing_resource,
    find_car_chunk,
    report_wrong_version,
    call_size_check_helper,
    copy_fixed_payload,
    enter_upgrade_overlay_branch,
    release_chunk_payload,
};

struct CarFileReadObservation {
    std::optional<bool> resource_open_succeeded;
    std::optional<bool> chunk_pointer_returned;
    std::optional<std::uint32_t> resource_version;
    std::optional<std::uint32_t> payload_size;
    std::optional<bool> upgrade_selector_argument_nonnull;
};

struct CarFileReadPlan {
    bool input_complete{};
    bool missing_resource_error_called{};
    bool wrong_version_error_called{};
    std::optional<bool> null_chunk_pointer_would_be_used;
    bool size_helper_called{};
    bool fixed_payload_copy_reached{};
    bool payload_size_differs_from_copy_size{};
    bool upgrade_overlay_branch_reached{};
    std::optional<bool> release_called;
    std::uint32_t copied_payload_bytes{};
    std::vector<CarFileReadStep> ordered_steps;
};

// Projects 0x45E2F0's resource/chunk/version/copy path. Loader outcomes are
// observations; this does not emulate the resource manager itself.
[[nodiscard]] CarFileReadPlan plan_car_file_read(
    const CarFileReadObservation& observation);

struct CarFileUpgradeEntry {
    std::uint32_t record_byte_offset{};
    std::uint32_t value{};
};

struct CarFileUpgradeSet {
    std::vector<CarFileUpgradeEntry> entries;
};

struct CarFileUpgradePayload {
    static constexpr std::uint32_t resource_type_code = 0x55504753U;
    static constexpr std::string_view resource_extension = ".ugs";
    std::uint32_t resource_version{};
    bool pointer_fixup_enabled{};
    bool version_mismatch{};
    bool input_complete{};
    std::vector<CarFileUpgradeSet> upgrade_sets;
};

struct CarFileResourceNames {
    std::string bundle_name;
    std::string car_file_resource_name;
    std::string upgrade_resource_name;
};

struct CarFileModelNamePlan {
    static constexpr std::size_t local_copy_capacity = 260U;
    static constexpr std::size_t runtime_record_size = 0x1e4U;
    static constexpr std::size_t runtime_name_offset = 0x198U;

    std::string normalized_name;
    std::size_t local_copy_bytes{};
    std::size_t runtime_copy_bytes{};
    bool local_stack_copy_overflow{};
    bool runtime_record_copy_overflow{};
    bool input_complete{};
};

// Projects 0x45EC20's final-dot truncation followed by last-backslash
// basename selection and its copy to the runtime record at +0x198.
[[nodiscard]] CarFileModelNamePlan plan_car_file_model_name(
    std::string_view resource_path);
[[nodiscard]] bool apply_car_file_model_name(
    std::span<std::byte> runtime_record,
    const CarFileModelNamePlan& plan) noexcept;

// 0x41A0C0/0x45E2F0 use one stem with `.car`, `.cf`, and `.ugs` suffixes.
[[nodiscard]] CarFileResourceNames make_car_file_resource_names(
    std::string_view car_stem);

// Decodes the bounded relative-offset form observed in UPGS resources. The
// native absolute-pointer form is intentionally left incomplete for a raw span.
[[nodiscard]] CarFileUpgradePayload parse_car_file_upgrade_payload(
    std::uint32_t resource_version,
    bool pointer_fixup_enabled,
    std::span<const std::byte> payload);

struct CarFileResourcePayloads {
    CarFileResourceNames names;
    std::optional<CarFilePayload> car_file;
    std::optional<CarFileUpgradePayload> upgrades;
};

// Looks up the `.cf` CARF and optional `.ugs` UPGS entries in an already
// opened RST0 bundle. Resource attributes carry each entry's native version.
[[nodiscard]] CarFileResourcePayloads parse_car_file_resources(
    const ResourceBundle& bundle,
    std::string_view car_stem,
    bool upgrade_pointer_fixup_enabled);

struct CarFileUpgradeObservation {
    std::uint32_t car_file_version{};
    std::optional<bool> upgrade_resource_found;
    std::optional<CarFileUpgradePayload> parsed_upgrade_payload;
    std::span<const std::uint8_t> selection_flags;
};

struct CarFileUpgradeWrite {
    std::size_t upgrade_set_index{};
    std::size_t entry_index{};
    std::uint32_t record_byte_offset{};
    std::uint32_t value{};
    bool within_car_file_payload{};
};

struct CarFileUpgradePlan {
    bool input_complete{};
    bool car_file_version_supported{};
    bool upgrade_resource_missing_panic_called{};
    bool upgrade_version_panic_called{};
    bool relative_pointer_fixup_branch{};
    bool write_outside_car_file_payload{};
    std::vector<CarFileUpgradeWrite> ordered_writes;
};

// Projects 0x45ED10's UPGS gate and 0x45E480's ordered offset/value stores.
// Parsed set contents and selection flags are supplied as observations.
[[nodiscard]] CarFileUpgradePlan plan_car_file_upgrades(
    const CarFileUpgradeObservation& observation);

// Applies a complete in-bounds UPGS write plan to a caller-owned payload.
[[nodiscard]] bool apply_car_file_upgrade_plan(
    std::span<std::byte> car_file_payload,
    const CarFileUpgradePlan& plan) noexcept;

struct CarFileRuntimeFieldWrite {
    // Offset from the start of the 0x1E4-byte car record. The observed mapper
    // writes +0x04 but does not write the subtype tag at +0x00.
    std::uint16_t byte_offset{};
    std::uint32_t raw_word{};
};

struct CarFileRuntimeMappingObservation {
    static constexpr std::size_t configured_row_size = 0xc8U;
    static constexpr std::size_t configuration_word_offset = 0x34U;
    static constexpr std::size_t configuration_word_count = 0x23U;

    CarFilePayload car_file;
    // Raw 32-bit words from the configured-car row at param_2.
    std::array<std::uint32_t, configuration_word_count> configuration_words{};
    bool pentium_fdiv_correction_enabled{};
};

// Reads exactly the 35 DWORDs consumed from configured row +0x34.
[[nodiscard]] std::optional<std::array<std::uint32_t,
    CarFileRuntimeMappingObservation::configuration_word_count>>
extract_car_file_configuration_words(
    std::span<const std::byte> configured_car_record);

[[nodiscard]] std::optional<CarFileRuntimeMappingObservation>
make_car_file_runtime_mapping_observation(
    const CarFilePayload& car_file,
    std::span<const std::byte> configured_car_record,
    bool pentium_fdiv_correction_enabled);

struct CarFileRuntimeMapping {
    static constexpr std::size_t runtime_record_size = 0x1e4;

    // Writes are retained in target order, including later overwrites of a
    // field. Unlisted bytes are outside the observed stores in 0x45E4B0.
    std::vector<CarFileRuntimeFieldWrite> writes;
    bool pentium_fdiv_correction_approximated{};
};

struct CarFileRuntimePreparationObservation {
    const CarFileResourcePayloads* resources{};
    std::array<std::uint32_t,
        CarFileRuntimeMappingObservation::configuration_word_count> configuration_words{};
    std::span<const std::uint8_t> upgrade_selection_flags;
    bool upgrade_overlay_requested{};
    bool pentium_fdiv_correction_enabled{};
};

struct CarFileRuntimePreparation {
    bool input_complete{};
    std::optional<CarFilePayload> effective_car_file;
    std::optional<CarFileUpgradePlan> upgrade_plan;
    std::optional<CarFileRuntimeMapping> mapping;
};

// Models the observed field stores from CARF v6 words into the 0x1E4-byte
// runtime record. Arithmetic uses host floating point; the target's x87 and
// Pentium FDIV correction can differ in low bits.
[[nodiscard]] std::optional<CarFileRuntimeMapping>
map_car_file_runtime_fields(
    const CarFileRuntimeMappingObservation& observation);

// Performs the observed CARF -> optional UPGS overlay -> runtime-field-map
// sequence. Caller-provided selection bytes and environment flags remain
// observations; the original storage allocation and constructors are excluded.
[[nodiscard]] CarFileRuntimePreparation prepare_car_file_runtime_mapping(
    const CarFileRuntimePreparationObservation& observation);

// Connects the configured 0xC8-byte race row directly to the resource-based
// preparation path, extracting its 35 configuration words at +0x34 first.
[[nodiscard]] CarFileRuntimePreparation
prepare_car_file_runtime_mapping_from_configured_row(
    const CarFileResourcePayloads& resources,
    std::span<const std::byte> configured_car_record,
    std::span<const std::uint8_t> upgrade_selection_flags,
    bool upgrade_overlay_requested,
    bool pentium_fdiv_correction_enabled);

// The garage path passes the active 0x8C-byte CCS/CSU setup buffer directly
// as the 35-word configuration source to 0x45EC20/0x45E4B0. This adapter
// consumes that buffer without fabricating the surrounding 0xC8 participant
// row. Upgrade selection and FDIV mode remain explicit observations.
[[nodiscard]] CarFileRuntimePreparation
prepare_car_file_runtime_mapping_from_setup_payload(
    const CarFileResourcePayloads& resources,
    std::span<const std::byte> setup_payload,
    std::span<const std::uint8_t> upgrade_selection_flags,
    bool upgrade_overlay_requested,
    bool pentium_fdiv_correction_enabled);

// Composes the CARF mapper, normalized name, subtype header/final size stores,
// and the 0x45CAE0 placement transform onto caller-owned runtime-record
// storage. Unmapped record bytes retain their caller-provided values.
[[nodiscard]] bool apply_car_track_runtime_record(
    std::span<std::byte> runtime_record,
    const CarFileRuntimePreparation& preparation,
    const CarFileModelNamePlan& model_name,
    const CarTrackConstructionPlan& construction) noexcept;

// Applies the ordered writes to an existing runtime record. Existing bytes
// not written by 0x45E4B0 are preserved, matching the observed store coverage.
[[nodiscard]] bool apply_car_file_runtime_mapping(
    std::span<std::byte> runtime_record,
    const CarFileRuntimeMapping& mapping) noexcept;

struct AcarTrackAdjustmentObservation {
    // Raw fields read through the selected entry pointer by 0x41A2A0.
    std::uint32_t returned_word_0x2c{};
    std::uint32_t returned_word_0x30{};
    std::uint32_t car_record_word_0x08{};
    std::uint32_t car_record_word_0x150{};
    std::uint32_t car_record_word_0x154{};
};

struct AcarVirtualLookupObservation {
    // 0x41AFF0 returns a row selector; the manager has eight rows.
    std::uint32_t configured_row{};
    // Manager field at +0x228, added to the remapped track index.
    std::uint32_t manager_cursor{};
    std::uint32_t remapped_track_index{};
    // Manager row count is the 17th DWORD in each 0x44-byte row.
    std::array<std::uint32_t, 8> row_counts{};
    // First 16 DWORDs from each manager row, populated from DAT_00504A78.
    std::array<std::array<std::uint32_t, 16>, 8> row_entries{};
};

enum class AcarVirtualLookupDisposition {
    selected,
    row_out_of_range,
    empty_row_divide_by_zero,
    signed_divide_overflow,
    selected_column_out_of_range,
};

struct AcarVirtualLookupPlan {
    AcarVirtualLookupDisposition disposition{};
    std::uint32_t selected_column{};
    std::uint32_t selected_entry_word{};
};

// Models the vtable method at 0x41BB00. Its selected DWORD is an entry value
// (used as a pointer by 0x41A2A0); the entry's pointed-to payload stays raw.
[[nodiscard]] AcarVirtualLookupPlan plan_acar_virtual_lookup(
    const AcarVirtualLookupObservation& observation) noexcept;

struct AcarTrackAdjustmentPlan {
    std::optional<std::uint32_t> record_word_0x08;
    std::optional<std::uint32_t> record_word_0x150;
    std::optional<std::uint32_t> record_word_0x154;
    bool queries_world_game_options_0x45c2c0{};
    bool queries_world_track_number_0x45c280{};
};

// Models the thresholded ACAR record writes; getter and virtual-call results
// remain observations because their source semantics are unresolved.
[[nodiscard]] AcarTrackAdjustmentPlan plan_acar_track_adjustment(
    const AcarTrackAdjustmentObservation& observation) noexcept;

struct ConfiguredBallEffectRecord {
    std::uint32_t object_tag{0x42414C4CU}; // BALL
    std::uint32_t record_size{0x38U};
    float raw_0x08{3000.0F};
    float raw_0x0c{5000.0F};
    float raw_0x10{5000.0F};
    float raw_0x14{5000.0F};
    float raw_0x18{};
    float terrain_adjusted_y_0x1c{};
    float raw_0x20{};
    float raw_0x24{18.0F};
    float raw_0x28{5000.0F};
    float raw_0x2c{0.6F};
    float raw_0x30{5.0F};
    std::uint32_t car_index_0x34{};
};
static_assert(sizeof(ConfiguredBallEffectRecord) == 0x38U);
static_assert(offsetof(ConfiguredBallEffectRecord, terrain_adjusted_y_0x1c) == 0x1cU);
static_assert(offsetof(ConfiguredBallEffectRecord, car_index_0x34) == 0x34U);

struct ConfiguredBallEffectPlan {
    ConfiguredBallEffectRecord record;
    std::string model_name{"ball.mod"};
    std::uint32_t constructor_address{0x00463590U};
    std::size_t allocation_size{0x40U};
    bool calls_append_helper_0x45c320{true};
};

// Terrain height is the observed result of 0x45F600 at X=0, Z=0.
[[nodiscard]] ConfiguredBallEffectPlan plan_configured_ball_effect(
    std::uint32_t car_index,
    float terrain_height) noexcept;

struct CheckpointEndpoint {
    float x{};
    float terrain_height{};
    float z{};
};

// The record header at 0x45D950 is CHKP/0x20 followed by two XYZ endpoints.
struct CheckpointTrackRecord {
    std::uint32_t object_tag{0x43484B50U}; // CHKP
    std::uint32_t record_size{0x20U};
    CheckpointEndpoint start;
    CheckpointEndpoint end;
};
static_assert(sizeof(CheckpointTrackRecord) == 0x20U);
static_assert(offsetof(CheckpointTrackRecord, start) == 0x08U);
static_assert(offsetof(CheckpointTrackRecord, end) == 0x14U);

enum class CheckpointConstructionRoute {
    visual_scene_object,
    serialized_object_record,
};

struct CheckpointConstructionPlan {
    std::string name;
    CheckpointTrackRecord record;
    CheckpointConstructionRoute route{};
};

// Mirrors 0x45D950's scanf conversion gate and branch on DAT_0053B194.
// Endpoint heights are observations supplied by the terrain query at 0x45F600.
struct CheckpointTrackLineObservation {
    std::string name;
    std::uint32_t scanf_conversion_count{};
    float start_x{};
    float start_z{};
    float end_x{};
    float end_z{};
    float start_terrain_height{};
    float end_terrain_height{};
    // Raw shared track-state DWORD: 0x426000 uses it as a service selector,
    // while 0x45D950 uses zero/nonzero to choose checkpoint construction.
    std::uint32_t track_state_selector_0x53b194{};
};

[[nodiscard]] std::optional<CheckpointConstructionPlan>
plan_checkpoint_track_line(
    const CheckpointTrackLineObservation& observation);

enum class CheckpointTaskRecordProductionStatus {
    not_checkpoint_line,
    malformed_line,
    visual_scene_object,
    serialized_record_ready,
};

struct CheckpointTaskRecordProduction {
    CheckpointTaskRecordProductionStatus status{
        CheckpointTaskRecordProductionStatus::not_checkpoint_line};
    std::optional<CheckpointConstructionPlan> construction;
    std::optional<std::array<std::byte, sizeof(CheckpointTrackRecord)>>
        serialized_record_bytes;
};

// Parses a raw track.obt object line, samples the two endpoint heights using
// the native height-or-zero helper, and emits complete CHKP bytes only when
// the observed selector chooses the serialized-record route.
[[nodiscard]] CheckpointTaskRecordProduction
produce_checkpoint_task_record_from_line(
    std::string_view object_line,
    const TerrainSurfaceResourcePayload& terrain,
    std::uint32_t track_state_selector_0x53b194,
    bool compatibility_division_enabled);

enum class WobbleType : std::uint32_t {
    pole = 0,
    flap = 1,
};

// Raw WOBL record fields written by 0x45D5D0; meanings remain unassigned.
struct WobbleTrackRecord {
    std::uint32_t object_tag{0x574F424CU}; // WOBL
    std::uint32_t record_size{0x20U};
    float raw_0x08{};
    float raw_0x0c{};
    float raw_0x10{};
    float raw_0x14{};
    std::int32_t parsed_parameter_0x18{};
    WobbleType type_0x1c{};
};
static_assert(sizeof(WobbleTrackRecord) == 0x20U);
static_assert(offsetof(WobbleTrackRecord, raw_0x08) == 0x08U);
static_assert(offsetof(WobbleTrackRecord, parsed_parameter_0x18) == 0x18U);
static_assert(offsetof(WobbleTrackRecord, type_0x1c) == 0x1CU);

struct WobbleTrackLineObservation {
    std::string_view type_name;
    std::uint32_t scanf_conversion_count{};
    std::int32_t parsed_parameter{};
};

[[nodiscard]] std::optional<WobbleTrackRecord> make_wobble_track_record(
    const WobbleTrackLineObservation& observation) noexcept;

enum class StaticTrackLineDisposition {
    skipped_by_dispatch_gate,
    malformed_record,
    unknown_type,
    record_ready,
};

// Payload fields following the STAT/0x48 header. Names retain binary offsets;
// only the rotation-vector input's matrix conversion is assigned a meaning.
struct StaticTrackRecordPayload {
    std::array<float, 9> rotation_matrix_0x08{};
    std::array<float, 3> parsed_vector_0x2c{};
    std::uint32_t raw_word_0x38{};
    std::array<float, 3> parsed_vector_0x3c{};
};

struct StaticTrackRecord {
    std::uint32_t object_tag{0x53544154U}; // STAT
    std::uint32_t record_size{0x48U};
    StaticTrackRecordPayload payload;
};
static_assert(sizeof(StaticTrackRecord) == 0x48U);
static_assert(offsetof(StaticTrackRecord, payload) == 0x08U);

struct StaticTrackLineObservation {
    bool parser_enabled_by_dispatch{};
    std::uint32_t scanf_conversion_count{};
    std::string_view type_name;
    std::array<float, 3> parsed_vector_0x2c{};
    std::array<float, 3> rotation_vector{};
    std::array<float, 3> parsed_vector_0x3c{};
};

struct StaticTrackLinePlan {
    StaticTrackLineDisposition disposition{};
    std::optional<StaticTrackRecord> record;
};

// Models the 0x40D2F0 caller gate and parser/record body at 0x45D6E0.
[[nodiscard]] StaticTrackLinePlan plan_static_track_line(
    const StaticTrackLineObservation& observation) noexcept;

enum class ObstacleType : std::uint32_t {
    ball = 0,
    cube = 1,
    prism = 2,
};

enum class ObstacleLineDisposition {
    malformed_record,
    unknown_type,
    record_ready,
};

struct ObstacleModelBounds {
    float min_x{};
    float max_x{};
    float min_y{};
    float max_y{};
    float min_z{};
    float max_z{};
};

struct ObstacleModelVertexRecord {
    std::array<float, 8> words{};
};

struct ObstacleModelBoundsObservation {
    bool vertex_block_present{};
    std::int32_t vertex_count{};
    std::span<const ObstacleModelVertexRecord> vertices;
};

struct ObstacleModelBoundsResult {
    ObstacleModelBounds bounds;
    bool fatal_helper_called{};
    bool observation_complete{true};
};

// Models 0x509B0's eight-float stride and first-three-float min/max scan.
[[nodiscard]] ObstacleModelBoundsResult compute_obstacle_model_bounds(
    const ObstacleModelBoundsObservation& observation) noexcept;

// Complete 0x38-byte OBST record passed to 0x463590 by 0x45D390.
struct ObstacleTrackRecord {
    std::uint32_t object_tag{0x4F425354U}; // OBST
    std::uint32_t record_size{0x38U};
    float scale_0x08{};
    float scaled_extent_term_y_0x0c{};
    float scaled_extent_term_z_0x10{};
    float scaled_extent_term_x_0x14{};
    float x_0x18{};
    float adjusted_y_0x1c{};
    float z_0x20{};
    float parsed_parameter_0x24{};
    float extent_x_0x28{};
    float extent_y_0x2c{};
    float extent_z_0x30{};
    ObstacleType type_0x34{};
};
static_assert(sizeof(ObstacleTrackRecord) == 0x38U);
static_assert(offsetof(ObstacleTrackRecord, scale_0x08) == 0x08U);
static_assert(offsetof(ObstacleTrackRecord, type_0x34) == 0x34U);

struct ObstacleTrackLineObservation {
    std::uint32_t scanf_conversion_count{};
    std::string_view type_name;
    std::string_view asset_name;
    float x{};
    float z{};
    float raw_parameter_0x14{};
    float scale{};
    float terrain_height{};
    ObstacleModelBounds model_bounds;
};

struct ObstacleTrackConstruction {
    ObstacleTrackRecord record;
    std::string asset_name;
};

struct ObstacleTrackLinePlan {
    ObstacleLineDisposition disposition{};
    std::optional<ObstacleTrackConstruction> construction;
};

// Reconstructs the record written by 0x45D390; model bounds and terrain height
// are observations from 0x509B0 and 0x45F600 respectively.
[[nodiscard]] ObstacleTrackLinePlan plan_obstacle_track_line(
    const ObstacleTrackLineObservation& observation);

} // namespace viper
