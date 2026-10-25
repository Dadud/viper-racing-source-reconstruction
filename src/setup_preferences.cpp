#include "viper/setup_preferences.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace viper {
namespace {

constexpr std::string_view file_name = "viper-racing-setup.cfg";
constexpr std::string_view header = "VIPER-RACING-SETUP 1\n";

char ascii_lower(const char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool equal_ascii_case_insensitive(const std::string_view left,
                                  const std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        if (ascii_lower(left[index]) != ascii_lower(right[index])) return false;
    return true;
}

std::optional<std::string> normalized_stem(const std::string_view value)
{
    if (value.empty() || value.size() > 64U) return std::nullopt;
    std::string result;
    result.reserve(value.size());
    for (const auto ch : value) {
        const auto byte = static_cast<unsigned char>(ch);
        const bool ascii_alphanumeric = (byte >= 'A' && byte <= 'Z') ||
            (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
        if (!ascii_alphanumeric && ch != '-' && ch != '_') return std::nullopt;
        result.push_back(ascii_lower(ch));
    }
    return result;
}

bool replace_file(const std::filesystem::path& from,
                  const std::filesystem::path& to,
                  std::error_code& error)
{
#ifdef _WIN32
    if (MoveFileExW(from.c_str(), to.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error.clear();
        return true;
    }
    error = std::error_code(static_cast<int>(GetLastError()),
        std::system_category());
    return false;
#else
    std::filesystem::rename(from, to, error);
    return !error;
#endif
}

std::size_t find_index(const std::span<const std::string> values,
                       const std::string_view name,
                       const std::size_t fallback) noexcept
{
    const auto found = std::find_if(values.begin(), values.end(),
        [name](const std::string& value) {
            return equal_ascii_case_insensitive(value, name);
        });
    return found == values.end() ? fallback
        : static_cast<std::size_t>(found - values.begin());
}

} // namespace

SetupPreferenceLoadResult load_setup_selection_preference(
    const std::filesystem::path& config_directory)
{
    SetupPreferenceLoadResult result{};
    if (config_directory.empty()) {
        result.error = "Config directory is unavailable";
        return result;
    }
    result.path = config_directory / file_name;
    std::error_code error;
    const bool exists = std::filesystem::exists(result.path, error);
    if (error) {
        result.error = "could not inspect setup preference: " + error.message();
        return result;
    }
    if (!exists) {
        result.file_missing = true;
        return result;
    }

    std::ifstream input(result.path, std::ios::binary);
    if (!input) {
        result.error = "could not open setup preference";
        return result;
    }
    std::string contents((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (input.bad()) {
        result.error = "failed while reading setup preference";
        return result;
    }
    if (contents.size() > 256U || !contents.starts_with(header)) {
        result.error = "setup preference has an invalid header or size";
        return result;
    }
    const auto first_end = contents.find('\n', header.size());
    const auto second_end = first_end == std::string::npos
        ? std::string::npos : contents.find('\n', first_end + 1U);
    if (first_end == std::string::npos || second_end == std::string::npos ||
        second_end + 1U != contents.size()) {
        result.error = "setup preference has an invalid field layout";
        return result;
    }
    constexpr std::string_view track_prefix = "track=";
    constexpr std::string_view car_prefix = "car=";
    const std::string_view track_line(contents.data() + header.size(),
        first_end - header.size());
    const std::string_view car_line(contents.data() + first_end + 1U,
        second_end - first_end - 1U);
    if (!track_line.starts_with(track_prefix) || !car_line.starts_with(car_prefix)) {
        result.error = "setup preference fields are missing or reordered";
        return result;
    }
    auto track = normalized_stem(track_line.substr(track_prefix.size()));
    auto car = normalized_stem(car_line.substr(car_prefix.size()));
    if (!track || !car) {
        result.error = "setup preference contains an invalid resource stem";
        return result;
    }
    result.selection = SetupSelectionPreference{std::move(*track), std::move(*car)};
    return result;
}

SetupPreferenceSaveResult save_setup_selection_preference(
    const std::filesystem::path& config_directory,
    const SetupSelectionPreference& selection)
{
    SetupPreferenceSaveResult result{};
    if (config_directory.empty()) {
        result.error = "Config directory is unavailable";
        return result;
    }
    const auto track = normalized_stem(selection.track);
    const auto car = normalized_stem(selection.car);
    if (!track || !car) {
        result.error = "track and car must be valid resource stems";
        return result;
    }
    result.path = config_directory / file_name;
    std::error_code error;
    std::filesystem::create_directories(config_directory, error);
    if (error) {
        result.error = "could not create Config directory: " + error.message();
        return result;
    }

    static std::atomic<std::uint64_t> sequence{};
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary = result.path;
    temporary += ".tmp-" + std::to_string(nonce) + "-" +
        std::to_string(sequence.fetch_add(1U));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            result.error = "could not create temporary setup preference";
            return result;
        }
        output << header << "track=" << *track << "\ncar=" << *car << '\n';
        output.flush();
        if (!output) {
            result.error = "failed while writing temporary setup preference";
            output.close();
            std::filesystem::remove(temporary, error);
            return result;
        }
        output.close();
        if (!output) {
            result.error = "failed while closing temporary setup preference";
            std::filesystem::remove(temporary, error);
            return result;
        }
    }
    if (!replace_file(temporary, result.path, error)) {
        result.error = "could not replace setup preference: " + error.message();
        std::filesystem::remove(temporary, error);
        return result;
    }
    result.saved = true;
    return result;
}

SetupSelectionIndices resolve_setup_selection_indices(
    const std::span<const std::string> tracks,
    const std::span<const std::string> cars,
    const std::optional<SetupSelectionPreference>& saved,
    const std::string_view initial_track,
    const std::string_view initial_car) noexcept
{
    SetupSelectionIndices result{};
    result.track = find_index(tracks, initial_track, 0U);
    result.car = find_index(cars, initial_car, 0U);
    if (!saved) return result;
    result.track = find_index(tracks, saved->track, result.track);
    result.car = find_index(cars, saved->car, result.car);
    return result;
}

} // namespace viper
