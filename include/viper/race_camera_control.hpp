#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace viper {

inline constexpr std::size_t race_camera_key_count = 0x100;

struct RaceCameraKeyState {
    std::array<bool, race_camera_key_count> down{};
};

struct RaceCameraControlState {
    // Row-major orientation at 0x5081A8 and position at 0x5081CC.
    std::array<float, 9> orientation{
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 1.0F,
    };
    std::array<float, 3> position{};
    // Persistent values corresponding to 0x4EB198..0x4EB1A0 and
    // 0x4EB1F0..0x4EB1F8.
    std::array<float, 3> translation_velocity{};
    std::array<float, 3> rotation_velocity{};
    bool preset_capture_latch{};
};

struct RaceCameraControlResult {
    std::uint8_t speed_mode{};
    bool preset_capture_requested{}; // rising-edge call to 0x4203E0
};

struct RaceCameraModeState {
    // Camera mode selector stored at 0x508758 by 0x4259B0.
    std::int32_t raw_mode_0x508758{};
};

void store_race_camera_mode(RaceCameraModeState& state,
                            std::int32_t mode) noexcept;

struct RaceCameraMode0State {
    std::uint32_t initialization_flags_0x507e04{};
    std::array<float, 3> smoothed_target_0x507d00{};
    float cosine_30_degrees_0x507d44{};
};

struct RaceCameraMode0Input {
    // Twelve floats copied from the selected scene car at +0x38.
    std::array<float, 12> vehicle_transform_0x38{};
    // Three-float local camera point read from the selected car at +0x47C.
    std::array<float, 3> local_camera_point_0x47c{};
    // Result of the terrain-plane query reached through 0x45F760.
    bool ground_sample_found{};
    std::array<float, 3> ground_sample{};
    float vehicle_offset_factor_0xea8{};
};

struct RaceCameraMode0Output {
    std::array<float, 12> transform{};
    bool ground_sample_used{};
};

// Models mode 0's transform, terrain-sample blend, persistent target smoothing,
// basis rebuild, and trailing vehicle-axis offset in 0x421120. The caller's
// terrain sample is produced by 0x45F760; terrain_surface exposes that query.
// One-time atexit registration remains external.
[[nodiscard]] RaceCameraMode0Output update_race_camera_mode_0(
    RaceCameraMode0State& state,
    const RaceCameraMode0Input& input) noexcept;

struct RaceCameraMode11Output {
    // Twelve consecutive floats copied from 0x5081A8 through 0x5081D4.
    std::array<float, 12> transform{};
    // 0x421120 clears this three-float vector before its common postprocess.
    std::array<float, 3> velocity{};
};

// Models the mode-11 branch of 0x421120 before the shared 0x46B920
// postprocess. Other camera modes have separate branches.
[[nodiscard]] bool prepare_race_camera_mode_11_output(
    const RaceCameraModeState& mode,
    const RaceCameraControlState& camera,
    RaceCameraMode11Output& output) noexcept;

struct RaceCameraModes8And9Input {
    // Position values produced by 0x421120's common camera preamble.
    std::array<float, 3> camera_anchor_position{};
    std::array<float, 3> car_velocity{};
};

struct RaceCameraModes8And9Output {
    std::array<float, 12> transform{};
    std::array<float, 3> velocity{};
};

// Models the shared mode-8/mode-9 branch in 0x421120 before 0x46B920.
[[nodiscard]] bool prepare_race_camera_mode_8_or_9_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes8And9Input& input,
    RaceCameraModes8And9Output& output) noexcept;

struct RaceCameraModes1And2Input {
    std::array<float, 9> vehicle_orientation{};
    std::array<float, 3> camera_position_before_mode{};
    std::array<float, 3> vehicle_velocity{};
};

struct RaceCameraModes1And2Output {
    std::array<float, 12> transform{};
    std::array<float, 3> velocity{};
};

// Models the mode-1/mode-2 orientation copy and transformed position offset
// in 0x421120 before the shared 0x46B920 postprocess.
[[nodiscard]] bool prepare_race_camera_mode_1_or_2_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes1And2Input& input,
    RaceCameraModes1And2Output& output) noexcept;

struct RaceCameraMode3Input {
    std::array<float, 9> vehicle_orientation{};
    std::array<float, 3> camera_position_before_mode{};
    std::array<float, 3> vehicle_velocity{};
};

struct RaceCameraMode3Output {
    std::array<float, 12> transform{};
    std::array<float, 3> velocity{};
};

// Models mode 3's Y half-turn and transformed local offset before 0x46B920.
[[nodiscard]] bool prepare_race_camera_mode_3_output(
    const RaceCameraModeState& mode,
    const RaceCameraMode3Input& input,
    RaceCameraMode3Output& output) noexcept;

struct RaceCameraLookAtState {
    std::array<float, 9> orientation{};
    std::array<float, 3> position{};
};

// Models the conditional matrix rebuild in 0x4222A0/0x422C90. It leaves the
// camera position unchanged; its 0x426E50/0x420960 refresh calls are external.
[[nodiscard]] bool update_race_camera_look_at_basis(
    RaceCameraLookAtState& state,
    const std::array<float, 3>& target) noexcept;

struct RaceCameraModes4To7Input {
    // Persistent 3x3 orientation plus position copied from 0x507D10 at entry.
    std::array<float, 12> cached_transform{};
    std::array<float, 3> camera_anchor_position{};
    std::array<float, 3> vehicle_velocity{};
    bool global_0x508760_is_zero{};
    bool vehicle_flag_0x4ec{};
};

struct RaceCameraModes4To7Output {
    std::array<float, 12> transform{};
    // Cache written to 0x507D10 before the optional mirrored-camera branch.
    std::array<float, 12> cached_transform{};
    std::array<float, 3> velocity{};
    bool mirrored_camera_branch{};
};

// Models the numeric camera transform and state writes for modes 4 through 7
// before the shared 0x46B920 postprocess. Refresh helper effects are omitted.
[[nodiscard]] bool prepare_race_camera_modes_4_to_7_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes4To7Input& input,
    RaceCameraModes4To7Output& output) noexcept;

struct RaceCameraPublishedState {
    std::int32_t update_flag_0x50{};
    std::int32_t selected_car_index_0x54{};
    std::array<float, 12> transform_0x58{};
    std::array<float, 3> velocity_0x88{};
};

// Models 0x46B920 -> 0x46DD20's writes into the shared camera object.
void apply_race_camera_postprocess(
    RaceCameraPublishedState& destination,
    std::int32_t selected_car_index,
    const std::array<float, 12>& transform,
    const std::array<float, 3>& velocity) noexcept;

struct RaceCameraPresetRecord {
    std::uint32_t raw_word_0x00{};
    std::array<float, 3> position{};      // record +0x04 through +0x0C
    float raw_scalar_0x10{}; // 0x4203E0 writes degrees here for type zero.
    float raw_scalar_0x14{}; // 0x4203E0 writes degrees here for type zero.
    float raw_scalar_0x18{}; // Read with the other two by type-zero camera setup.
};

struct RaceCameraTableRowObservation {
    std::string_view type_name;
    // Results of the six `_atof` conversions at camera.tab columns 1..6.
    std::array<float, 6> converted_values{};
};

struct RaceCameraTableLoadResult {
    bool input_complete{true};
    std::size_t selector_count{};
    std::size_t unknown_type_panic_count{};
};

// Models the camera.tab record load in 0x4228C0. The caller supplies the
// section rows and `_atof` outcomes; existing records preserve type words for
// unknown names because the binary calls its panic helper without writing one.
[[nodiscard]] RaceCameraTableLoadResult load_race_camera_table(
    std::span<RaceCameraPresetRecord> records,
    bool camera_tab_opened,
    std::span<const RaceCameraTableRowObservation> rows) noexcept;

struct RaceCameraModes10And12Input {
    std::int32_t selector_count_0x5081dc{};
    std::span<const RaceCameraPresetRecord> camera_records_0x507e10{};
    std::span<const std::array<float, 3>> target_candidates_0x507e2c{};
    std::array<float, 3> camera_anchor_position{};
    std::array<float, 12> transform_before_branch{};
    std::array<float, 12> manual_camera_transform_before{};
    // Common preamble copy from the selected scene car at +0x38.
    std::array<float, 12> vehicle_transform_0x38{};
    std::array<float, 3> vehicle_velocity{};
    std::int32_t selected_record_index_0x4d488c{};
};

struct RaceCameraModes10And12Output {
    std::array<float, 12> transform{};
    std::array<float, 3> velocity{};
    std::array<float, 12> manual_camera_transform{};
    std::int32_t selected_record_index_0x4d488c{};
    bool preset_record_selected{};
    bool manual_camera_transform_written{};
};

// Models modes 10 and 12 in 0x421120. Spans are portable bounds for the
// binary's global record arrays; refresh and common-postprocess effects remain
// separate operations. False means the mode is unsupported here or an input
// span cannot cover the binary's observed selector count.
[[nodiscard]] bool prepare_race_camera_modes_10_or_12_output(
    const RaceCameraModeState& mode,
    const RaceCameraModes10And12Input& input,
    RaceCameraModes10And12Output& output) noexcept;

enum class RaceCameraPresetCaptureStatus {
    position_only,
    position_and_angles,
    camera_index_out_of_range,
    record_unavailable,
};

// Models 0x4203E0. The span is the portable boundary for the global seven-DWORD
// preset array; active index and count are observations from their globals.
[[nodiscard]] RaceCameraPresetCaptureStatus capture_race_camera_preset(
    const RaceCameraControlState& camera,
    std::span<RaceCameraPresetRecord> presets,
    std::int32_t active_index,
    std::int32_t camera_count) noexcept;

// Models the page-11 callback at 0x401930. Input key numbers are kept raw;
// its keyboard polling and renderer/preset storage helpers remain external.
[[nodiscard]] RaceCameraControlResult update_race_camera_control(
    RaceCameraControlState& state,
    const RaceCameraKeyState& keys) noexcept;

} // namespace viper
