#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

struct AINoteLookupKey {
    std::uint32_t entrant_group{}; // Native per-entrant value reduced modulo 7.
    std::uint32_t difficulty{};    // Native difficulty/profile selector.
    std::uint32_t track_index{};   // tracks.tab row, with 8 added for reverse mode.
    std::string_view car_stem;
};

enum class AINotePathKind : std::uint8_t {
    track_sidecar,
    notes_directory,
};

// Builds the filename key used by 0x41F5E0/0x41F630. Hex fields are lowercase,
// unpadded, and concatenated as %x%x%x%s.
[[nodiscard]] std::optional<std::string> make_ai_note_path(
    const AINoteLookupKey& key,
    AINotePathKind kind);

struct AINoteResource {
    static constexpr std::uint32_t resource_type_code = 0x41444e54U; // ADNT
    static constexpr std::size_t header_bytes = 0x40U;
    static constexpr std::size_t car_name_bytes = 0x20U;

    std::uint32_t track_index{};
    std::uint32_t entrant_group{};
    std::uint32_t difficulty{};
    std::string car_stem;
    std::uint16_t redundant_check{};
    std::uint16_t ideal_line_check{};
    std::uint32_t ideal_line_node_count{};
    std::uint32_t node_payload_bytes{};
    std::vector<std::byte> node_payload;
};

// Parses the ADNT v7 resource header and bounded node payload. The native
// loader additionally checks ideal-line checksums and a redundant check word;
// those computations are kept separate until their helper semantics are
// completely reconstructed.
[[nodiscard]] std::optional<AINoteResource> parse_ai_note_resource(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

[[nodiscard]] bool ai_note_matches_lookup_key(
    const AINoteResource& note,
    const AINoteLookupKey& key) noexcept;

} // namespace viper
