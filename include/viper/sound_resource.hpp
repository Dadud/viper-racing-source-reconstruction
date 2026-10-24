#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace viper {

// View of an SFX0 resource's PCM data. The source bundle must outlive the view.
// Bytes after pcm_data are retained as an opaque tail; their runtime meaning
// has not been established.
struct PcmSoundResourceView {
    std::uint16_t format_tag{};
    std::uint16_t channels{};
    std::uint32_t sample_rate{};
    std::uint32_t average_bytes_per_second{};
    std::uint16_t block_align{};
    std::uint16_t bits_per_sample{};
    std::span<const std::byte> pcm_data;
    std::span<const std::byte> opaque_tail;
};

// Parses the observed SFX0 payload header and accepts only internally
// consistent integer PCM resources. Does not interpret opaque_tail.
[[nodiscard]] std::optional<PcmSoundResourceView> parse_pcm_sfx0_resource(
    std::span<const std::byte> payload) noexcept;

} // namespace viper
