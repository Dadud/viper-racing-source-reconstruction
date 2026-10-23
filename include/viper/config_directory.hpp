#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace viper {

struct ConfigDirectoryPlan {
    std::string diagnostic_path;
    std::string application_root;
    std::string config_path_0x4ef0c0;
    std::vector<std::string> directory_creation_paths;
    std::uint32_t exit_process_code{};
    bool module_filename_error_reported{};
    bool derive_path_error_reported{};
    bool installation_error_reported{};
    bool directory_creation_helper_reached{};
    bool module_path_may_exceed_260_byte_buffer{};
    bool exit_process_reached{};
    bool returns_success{};
};

struct ConfigDirectoryCreateObservation {
    bool create_directory_succeeded{};
    std::uint32_t last_error{};
};

struct ConfigDirectoryCreationResult {
    std::size_t call_count{};
    std::optional<std::size_t> failed_path_index;
    std::optional<bool> returns_success;
    bool helper_reached{};
    bool observations_incomplete{};
};

// Models 0x412120's executable-path validation and Config-directory setup.
// The module path is the output observation from GetModuleFileNameA.
[[nodiscard]] ConfigDirectoryPlan plan_config_directory(
    const std::string& module_path,
    bool get_module_file_name_succeeded);

// Models 0x4119E0/0x4119B0 from observed CreateDirectoryA outcomes. Error 183
// is accepted by the original helper as success; missing observations leave
// the helper result unknown instead of guessing.
[[nodiscard]] ConfigDirectoryCreationResult evaluate_config_directory_creation(
    const ConfigDirectoryPlan& plan,
    std::span<const ConfigDirectoryCreateObservation> observations) noexcept;

} // namespace viper
