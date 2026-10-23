#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace viper {

struct FrontendInitializationObservation {
    // The result of the 0x4015E0 CDDirectory registry lookup.
    bool cd_directory_lookup_succeeded{};
    std::string cd_directory;
};

struct FrontendInitializationPlan {
    std::array<std::uint32_t, 20> call_addresses{};
    std::size_t call_count{};
    std::array<float, 12> matrix_global_0x5081a8{};
    std::array<std::string_view, 3> resource_names{
        "common.res", "ui.res", "drivers.res"};
    int helper_argument_0x4479f0{2};
    int helper_argument_0x40d110{1};
    std::string intro_video_path;
    bool registry_lookup_reached{};
    bool intro_video_helper_reached{};
    bool intro_path_exceeds_260_byte_local_buffer{};
    bool identity_matrix_written{};
};

// Reconstructs the call order, optional intro path, and matrix initialization
// performed by the non-dedicated frontend initializer at 0x4014D0.
[[nodiscard]] FrontendInitializationPlan plan_frontend_initialization(
    const FrontendInitializationObservation& observation);

} // namespace viper
