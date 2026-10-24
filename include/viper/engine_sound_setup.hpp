#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace viper {

// The native sound manager reads a count followed by seven-float records.
// The values are kept positional until each field's meaning is confirmed.
struct EngineSoundSetupView {
    std::uint32_t band_count{};
    std::array<std::array<float, 7>, 7> bands{};
    std::span<const std::byte> opaque_tail;
};

[[nodiscard]] std::optional<EngineSoundSetupView> parse_engine_sound_setup(
    std::span<const std::byte> payload) noexcept;

} // namespace viper
