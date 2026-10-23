#include "viper/config_directory.hpp"

#include <cstddef>
#include <string_view>

namespace viper {
namespace {

char ascii_lower(const char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

bool ascii_case_equal(const std::string_view left,
                      const std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (ascii_lower(left[index]) != ascii_lower(right[index])) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> directory_creation_candidates(
    const std::string& config_path)
{
    std::vector<std::string> paths;
    std::size_t root_end{};
    if (config_path.size() >= 3 &&
        ((config_path[0] >= 'A' && config_path[0] <= 'Z') ||
         (config_path[0] >= 'a' && config_path[0] <= 'z')) &&
        config_path[1] == ':' && config_path[2] == '\\') {
        root_end = 3;
    } else if (config_path.size() >= 2 &&
               config_path[0] == '\\' && config_path[1] == '\\') {
        const auto server_end = config_path.find('\\', 2);
        if (server_end == std::string::npos) {
            return paths;
        }
        const auto share_end = config_path.find('\\', server_end + 1);
        if (share_end == std::string::npos) {
            return paths;
        }
        root_end = share_end + 1;
    }

    std::size_t component_start = root_end;
    while (component_start < config_path.size()) {
        const auto separator = config_path.find('\\', component_start);
        if (separator == std::string::npos) {
            if (component_start < config_path.size()) {
                paths.push_back(config_path);
            }
            break;
        }
        if (separator > component_start) {
            paths.push_back(config_path.substr(0, separator));
        }
        component_start = separator + 1;
    }
    return paths;
}

void mark_installation_failure(ConfigDirectoryPlan& plan) noexcept
{
    plan.installation_error_reported = true;
    plan.exit_process_reached = true;
    plan.exit_process_code = 6;
}

} // namespace

ConfigDirectoryPlan plan_config_directory(
    const std::string& module_path,
    const bool get_module_file_name_succeeded)
{
    ConfigDirectoryPlan plan{};
    plan.module_path_may_exceed_260_byte_buffer = module_path.size() >= 260;

    if (!get_module_file_name_succeeded) {
        plan.module_filename_error_reported = true;
        mark_installation_failure(plan);
        return plan;
    }

    const auto nul = module_path.find('\0');
    const std::string visible_path = module_path.substr(0, nul);
    const auto filename_separator = visible_path.find_last_of('\\');
    if (filename_separator == std::string::npos) {
        plan.diagnostic_path = visible_path;
        plan.derive_path_error_reported = true;
        mark_installation_failure(plan);
        return plan;
    }

    const std::string executable_directory =
        visible_path.substr(0, filename_separator);
    plan.diagnostic_path = executable_directory;
    const auto data_separator = executable_directory.find_last_of('\\');
    if (data_separator == std::string::npos ||
        !ascii_case_equal(
            std::string_view(executable_directory).substr(data_separator),
            "\\Data")) {
        plan.derive_path_error_reported = true;
        mark_installation_failure(plan);
        return plan;
    }

    plan.application_root = executable_directory.substr(0, data_separator);
    plan.config_path_0x4ef0c0 = plan.application_root + "\\Config\\";
    plan.directory_creation_paths =
        directory_creation_candidates(plan.config_path_0x4ef0c0);
    plan.directory_creation_helper_reached = true;
    plan.returns_success = true;
    return plan;
}

ConfigDirectoryCreationResult evaluate_config_directory_creation(
    const ConfigDirectoryPlan& plan,
    const std::span<const ConfigDirectoryCreateObservation> observations) noexcept
{
    ConfigDirectoryCreationResult result{};
    result.helper_reached = plan.directory_creation_helper_reached;
    if (!result.helper_reached) {
        return result;
    }

    for (std::size_t index = 0;
         index < plan.directory_creation_paths.size();
         ++index) {
        if (index >= observations.size()) {
            result.observations_incomplete = true;
            return result;
        }

        ++result.call_count;
        const auto& observation = observations[index];
        if (observation.create_directory_succeeded ||
            observation.last_error == 183U) {
            continue;
        }

        result.failed_path_index = index;
        result.returns_success = false;
        return result;
    }

    result.returns_success = true;
    return result;
}

} // namespace viper
