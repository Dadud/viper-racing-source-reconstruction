#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <array>

namespace viper {

class ResourceBundle;
struct ResourceEntry;
struct IdealLineTrackData;

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

// Builds the two driver-specific line candidates used by 0x419EC0.
// Notes-directory lines use .ilq; track-sidecar lines use .ilg.
[[nodiscard]] std::optional<std::string> make_ai_line_path(
    const AINoteLookupKey& key, AINotePathKind kind);

enum class AILineOverrideInventory : std::uint8_t { none, present, incomplete };

// Conservative inventory of loose .ilq/.ilg/.dnt files and indexed overrides
// in the data tree. Expected .ilg/.dnt entries in drivers.res are handled by key.
// Any unreadable directory or bundle makes the inventory incomplete.
[[nodiscard]] AILineOverrideInventory inventory_ai_line_overrides(
    const std::filesystem::path& root);

enum class AILineSelectionStatus : std::uint8_t {
    driver_line, default_line, unresolved_overrides, invalid_key
};
struct AILineSelection {
    AILineSelectionStatus status{AILineSelectionStatus::invalid_key};
    const ResourceEntry* driver_entry{}; // owned by the supplied driver bundle
};
// Resolves the stock driver-bundle candidate only after higher-priority
// override sources were inventoried. Payload validation remains the caller's job.
[[nodiscard]] AILineSelection select_stock_ai_line(
    const ResourceBundle& drivers, const AINoteLookupKey& key,
    AILineOverrideInventory overrides);

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
// The native loader additionally checks the line checksum and count; use
// ai_note_matches_line for that successful acceptance path.
[[nodiscard]] std::optional<AINoteResource> parse_ai_note_resource(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

[[nodiscard]] bool ai_note_matches_lookup_key(
    const AINoteResource& note,
    const AINoteLookupKey& key) noexcept;

// 0x49D800: CRC-16/XMODEM (initial zero) used by the AI line check.
[[nodiscard]] std::uint16_t ai_note_checksum(std::span<const std::byte> bytes) noexcept;
// Models the successful 0x41F6E0 key/count/checksum acceptance path.
[[nodiscard]] bool ai_note_matches_line(const AINoteResource& note,
    const AINoteLookupKey& key, const IdealLineTrackData& line) noexcept;
// Decodes the two raw DWORDs per line node passed to 0x429440.
[[nodiscard]] std::optional<std::vector<std::array<std::uint32_t, 2>>>
read_ai_note_node_pairs(const AINoteResource& note);

} // namespace viper
