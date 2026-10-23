#pragma once

#include <string>
#include <vector>

namespace viper {

struct StartupCommandState {
    int startup_mode{};
    bool tri_mode{};
    bool grid_mode{};
    bool z_flag{};
};

struct StartupCommandLineObservation {
    // Result supplied by the original __chdir wrapper when a -d option is
    // encountered. It has no effect when the command line contains no -d.
    bool directory_change_succeeded{};
};

struct StartupCommandLineResult {
    StartupCommandState state{};
    std::string command_line_after_parse;
    std::string directory_path;
    std::string location_argument;
    std::vector<char> invalid_programmer_flags;
    bool directory_change_attempted{};
    bool directory_change_succeeded{};
    bool directory_error_reported{};
    bool location_argument_seen{};
    bool alternate_mode_flag_set{};
    bool returns_success{true};
};

// Models the command-line switch scan in 0x401280. The parser owns a mutable
// copy because its -d path handling writes a NUL at the first following space.
[[nodiscard]] StartupCommandLineResult parse_startup_command_line(
    std::string command_line,
    StartupCommandState initial_state = {},
    const StartupCommandLineObservation& observation = {});

} // namespace viper
