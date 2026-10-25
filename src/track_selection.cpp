#include "viper/track_selection.hpp"

namespace viper {

std::optional<std::size_t> resolve_track_choice_index(
    const std::span<const std::string> tracks,
    const std::string_view selected_track,
    std::size_t random_choice_index) noexcept
{
    if (selected_track != "random") {
        for (std::size_t index = 0; index < tracks.size(); ++index) {
            if (tracks[index] == selected_track) return index;
        }
        return std::nullopt;
    }

    for (std::size_t index = 0; index < tracks.size(); ++index) {
        if (tracks[index] == "random") continue;
        if (random_choice_index == 0U) return index;
        --random_choice_index;
    }
    return std::nullopt;
}

} // namespace viper
