#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace viper {

// Resolves a concrete setup choice, or the `random` selector by choosing the
// Nth installed entry while skipping the selector sentinel itself.
[[nodiscard]] std::optional<std::size_t> resolve_track_choice_index(
    std::span<const std::string> tracks,
    std::string_view selected_track,
    std::size_t random_choice_index) noexcept;

} // namespace viper
