#include "viper/startup_command_line.hpp"

#include <utility>

namespace viper {
namespace {

char ascii_lower(const char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

bool starts_with_at(const std::string& text,
                    const std::size_t offset,
                    const char* literal,
                    const std::size_t length) noexcept
{
    return offset <= text.size() && text.size() - offset >= length &&
        text.compare(offset, length, literal, length) == 0;
}

} // namespace

StartupCommandLineResult parse_startup_command_line(
    std::string command_line,
    StartupCommandState initial_state,
    const StartupCommandLineObservation& observation)
{
    StartupCommandLineResult result{};
    result.state = initial_state;
    result.state.z_flag = false;

    for (std::size_t index = 0; index < command_line.size(); ++index) {
        if (command_line[index] != '-' && command_line[index] != '/') {
            continue;
        }

        const std::size_t option = index + 1;
        if (starts_with_at(command_line, option, "location", 8)) {
            result.state.startup_mode = 4;
            result.location_argument_seen = true;
            result.location_argument = command_line.substr(option + 8);
            continue;
        }
        if (starts_with_at(command_line, option, "tri", 3)) {
            result.state.tri_mode = true;
            continue;
        }
        if (starts_with_at(command_line, option, "grid", 4)) {
            result.state.grid_mode = true;
            continue;
        }

        if (option >= command_line.size()) {
            continue;
        }

        switch (ascii_lower(command_line[option])) {
        case '1':
            result.alternate_mode_flag_set = true;
            break;
        case 'd': {
            const std::size_t path_start = option + 1;
            const std::size_t path_end = command_line.find(' ', path_start);
            result.directory_path = command_line.substr(
                path_start,
                path_end == std::string::npos ? std::string::npos
                                               : path_end - path_start);
            result.directory_change_attempted = true;
            result.directory_change_succeeded =
                observation.directory_change_succeeded;
            result.directory_error_reported =
                !observation.directory_change_succeeded;
            if (path_end != std::string::npos) {
                command_line.resize(path_end);
            }
            break;
        }
        case 'u': {
            const char mode = option + 1 < command_line.size()
                ? ascii_lower(command_line[option + 1])
                : '\0';
            switch (mode) {
            case 'b': result.state.startup_mode = 1; break;
            case 'd': result.state.startup_mode = 2; break;
            case 'r': result.state.startup_mode = 3; break;
            default: result.invalid_programmer_flags.push_back(mode); break;
            }
            break;
        }
        case 'z':
            result.state.z_flag = true;
            break;
        default:
            break;
        }
    }

    result.command_line_after_parse = std::move(command_line);
    return result;
}

} // namespace viper
