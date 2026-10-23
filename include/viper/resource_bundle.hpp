#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

[[nodiscard]] constexpr std::uint32_t resource_type_code(
    const char a, const char b, const char c, const char d) noexcept
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8U) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d));
}

struct ResourceEntry {
    std::string name;
    std::uint32_t type_code{};
    std::uint32_t attributes{};
    std::uint32_t payload_size{};
    std::array<std::byte, 8> chunk_header{};
    std::size_t payload_offset{};
};

enum class LegacyResourceRegistrationStatus : std::uint8_t {
    allocation_failed_reported,
    panic_did_not_return,
    index_not_observed,
    registered,
};

struct LegacyResourceRegistrationObservation {
    std::uint32_t type_code{};
    std::uint32_t payload_size{};
    bool allocation_succeeded{};
    bool constructor_panicked{};
    bool constructor_returned{};
    bool panic_reporter_returned{};
    std::optional<std::uint32_t> object_index;
};

struct LegacyResourceRegistrationPlan {
    LegacyResourceRegistrationStatus status{
        LegacyResourceRegistrationStatus::allocation_failed_reported};
    std::uint32_t allocation_bytes{0x40U};
    std::uint32_t type_code{};
    std::uint32_t payload_size{};
    std::optional<std::uint32_t> registered_index;
    std::optional<std::uint32_t> returned_handle;
    bool allocation_attempted{true};
    bool constructor_called{};
    bool panic_reporter_called{};
    bool continued_after_panic{};
    bool registry_slot_written{};
};

// Models the registry wrapper at 0x4C59B0. Constructor internals are supplied
// as observations; an observed object index is the DWORD read at object +0.
[[nodiscard]] LegacyResourceRegistrationPlan plan_legacy_resource_registration(
    const LegacyResourceRegistrationObservation& observation) noexcept;

class ResourceBundle {
public:
    [[nodiscard]] static ResourceBundle open(const std::filesystem::path& path);

    [[nodiscard]] static ResourceBundle parse(std::vector<std::byte> bytes);

    [[nodiscard]] const std::vector<ResourceEntry>& entries() const noexcept;
    [[nodiscard]] std::uint32_t buffered_resource_count() const noexcept;
    [[nodiscard]] std::span<const std::byte> payload(
        const ResourceEntry& entry) const noexcept;
    [[nodiscard]] const ResourceEntry* find(
        std::string_view name, std::uint32_t type_code) const noexcept;
    [[nodiscard]] std::optional<std::span<const std::byte>> find_payload(
        std::string_view name, std::uint32_t type_code) const noexcept;

private:
    std::vector<std::byte> bytes_;
    std::vector<ResourceEntry> entries_;
    std::uint32_t buffered_resource_count_{};
};

struct StampPointerEntry {
    std::uint32_t table_offset{};
    std::uint32_t raw_data_offset{};
    std::uint32_t relocated_payload_offset{};
    bool relocated{};
    bool target_within_payload{};
};

struct StampPayload {
    std::uint32_t factor_0x04{};
    std::uint32_t factor_0x10{};
    std::int32_t pointer_entry_count{};
    std::uint32_t embedded_data_offset{};
    bool pointer_table_complete{};
    bool pointer_offsets_relocated{};
    std::vector<StampPointerEntry> pointer_entries;
};

// Parses the observed STMP payload shape used by 0x44CFD0. The table count is
// the target's signed 32-bit product of payload dwords +0x04 and +0x10; pointer
// fixups are applied only when the caller observes the loader's relocation bit.
[[nodiscard]] std::optional<StampPayload> parse_stamp_payload(
    std::span<const std::byte> payload,
    bool relocate_pointer_offsets);

struct StampFontCharacterEntry {
    std::uint16_t glyph_index{0xffffU};
    std::int16_t metric_0x204{};
    std::int16_t metric_0x404{};
    bool populated{};
};

struct StampFontIndex {
    std::int32_t glyph_record_count{};
    bool glyph_table_complete{};
    std::array<StampFontCharacterEntry, 256> characters{};
};

// Projects the 0x44DA00 character-map build from the STMP dword at +0x10 and
// encoded glyph records beginning at +0x414. Metric fields remain unlabeled.
[[nodiscard]] std::optional<StampFontIndex> parse_stamp_font_index(
    std::span<const std::byte> payload);

enum class ResourceGetExit : std::uint8_t {
    incomplete,
    returned_resource,
    missing_resource,
    type_mismatch,
};

struct ResourceGetEntryObservation {
    std::uint32_t type_code{};
    std::uint32_t attributes{};
    std::uint32_t owner_pointer{};
    std::uint32_t chunk_header_address{};
    std::uint32_t reference_count{};
    bool chunk_header_first_dword_nonzero{};
};

struct ResourceGetObservation {
    // Name returned by 0x417B70 is parsed before its first byte is inspected.
    char normalized_name_first_byte{};
    std::uint32_t requested_type_code{};
    bool missing_resource_log_suppressed_by_0x4d35a8{};
    // Required unless a '~' name skips the loaded-set search. A '*' name still
    // searches the set after the manager strips its prefix.
    std::optional<bool> loaded_set_entry_found;
    // Required when a special name is used or no loaded-set entry matches.
    std::optional<bool> external_resolver_entry_found;
    // Details for whichever lookup path found the entry.
    std::optional<ResourceGetEntryObservation> entry;
};

struct ResourceGetPlan {
    bool input_complete{true};
    ResourceGetExit exit{ResourceGetExit::incomplete};
    bool loaded_set_search_performed{};
    bool external_resolver_called{};
    bool resource_used_bit_set{};
    std::optional<bool> first_use_reported;
    std::optional<bool> reference_count_was_zero{};
    std::optional<std::uint32_t> payload_size;
    std::optional<std::uint32_t> attributes_after;
    std::optional<std::uint32_t> owner_pointer;
    std::optional<std::uint32_t> payload_address;
    std::optional<std::uint32_t> reference_count_after;
    bool type_mismatch_panic_called{};
    bool missing_resource_error_reported{};
    bool loading_gate_cleared{};
    std::vector<std::uint32_t> error_string_addresses;
    std::vector<std::uint32_t> calls;
};

// Projects the loaded-bundle lookup side effects in 0x417CB0. Bundle-search,
// resolver, and entry state are observations; this function does no I/O.
[[nodiscard]] ResourceGetPlan plan_resource_get(
    const ResourceGetObservation& observation);

enum class ResourceFileLoadPath : std::uint8_t {
    incomplete,
    tilde_game_root,
    primary_resource_path,
    secondary_resource_path,
};

enum class ResourceFileLoadExit : std::uint8_t {
    incomplete,
    open_failed,
    invalid_header,
    allocation_failed,
    read_failed,
    loaded,
};

struct ResourceFileLoadObservation {
    std::string resource_name;
    std::optional<std::string> game_root_prefix;
    std::optional<std::string> primary_resource_path_prefix;
    std::optional<std::string> secondary_resource_path_prefix;
    std::optional<bool> tilde_path_opened;
    std::optional<bool> primary_path_opened;
    std::optional<bool> secondary_path_opened;
    std::optional<bool> header_read_succeeded;
    std::optional<std::uint32_t> header_magic;
    std::optional<std::uint32_t> header_word_1;
    std::optional<std::uint32_t> header_word_2;
    std::optional<std::int32_t> file_size_bytes;
    // Descriptor (0x3c), name/entry (0x24), and file-data (file size - 0x0c)
    // allocations, in that order when reached.
    std::array<std::optional<bool>, 3> allocations_succeeded{};
    std::optional<bool> data_read_succeeded;
};

struct ResourceFileLoadPlan {
    bool input_complete{true};
    ResourceFileLoadPath path{ResourceFileLoadPath::incomplete};
    ResourceFileLoadExit exit{ResourceFileLoadExit::incomplete};
    std::vector<ResourceFileLoadPath> attempted_paths;
    std::vector<std::optional<std::string>> constructed_paths;
    bool constructed_paths_complete{true};
    std::optional<std::string> opened_path_source_name;
    std::optional<std::string> linked_resource_name;
    std::optional<bool> linked_resource_name_overflows_0x24_buffer;
    std::optional<std::uint32_t> header_word_1;
    std::optional<std::uint32_t> header_word_2;
    std::optional<std::int32_t> resource_payload_bytes;
    std::optional<std::int32_t> file_data_allocation_bytes;
    std::optional<std::int32_t> file_read_bytes;
    std::optional<bool> allocation_failure_reaches_null_dereference;
    bool resource_linked_into_loaded_list{};
    bool file_handle_closed{};
    std::vector<std::uint32_t> allocation_sizes;
    std::vector<std::uint32_t> calls;
};

// Projects the RES0-backed file resolver in 0x417EF0. File, allocator, and
// read outcomes are observations; the function performs no file I/O.
[[nodiscard]] ResourceFileLoadPlan plan_resource_file_load(
    const ResourceFileLoadObservation& observation);

enum class ResourceMappingExit : std::uint8_t {
    incomplete,
    direct_file_mapping,
    fallback_memory_map,
    fallback_path_missing,
};

struct ResourceMappingObservation {
    std::uint32_t file_handle_slot{};
    std::optional<bool> create_file_mapping_succeeded;
    std::optional<bool> map_view_succeeded;
    std::optional<bool> fallback_file_opened;
    std::optional<std::uint32_t> fallback_file_size_bytes;
    std::optional<bool> fallback_allocation_succeeded;
    std::optional<bool> fallback_read_succeeded;
};

struct ResourceMappingPlan {
    bool input_complete{true};
    ResourceMappingExit exit{ResourceMappingExit::incomplete};
    bool create_file_mapping_called{};
    std::uint32_t file_mapping_protection{2U};
    bool map_view_called{};
    std::uint32_t map_view_access{4U};
    bool mapping_handle_closed_after_view_failure{};
    bool fallback_file_open_called{};
    std::optional<std::uint32_t> fallback_path_slot;
    std::optional<std::uint32_t> fallback_file_data_bytes;
    std::optional<std::uint32_t> fallback_allocation_bytes;
    std::optional<bool> fake_mapping_read_result_ignored;
    bool fallback_missing_file_panic_called{};
    bool fallback_memory_map_selected{};
    bool mapped_base_available{};
    std::optional<bool> mapping_handle_returned;
    std::optional<bool> ownership_flag_returned;
    std::optional<std::uint32_t> error_string_address;
    std::vector<std::uint32_t> calls;
};

// Projects 0x411BE0 and its fake-memory-map fallback 0x411B10. Win32 and
// filesystem outcomes are inputs; this model performs no OS calls.
[[nodiscard]] ResourceMappingPlan plan_resource_mapping(
    const ResourceMappingObservation& observation);

struct ResourceUnmapObservation {
    bool fake_memory_map{};
};

struct ResourceUnmapPlan {
    bool fake_memory_buffer_freed{};
    bool view_unmapped{};
    bool mapping_handle_closed{};
    bool base_pointer_cleared{true};
    bool mapping_handle_cleared{};
    std::vector<std::uint32_t> calls;
};

// Projects the ownership split in 0x411CB0/0x411BC0 for a non-null base.
[[nodiscard]] ResourceUnmapPlan plan_resource_unmap(
    const ResourceUnmapObservation& observation);

enum class Rst0BundleLoadExit : std::uint8_t {
    incomplete,
    rejected_header,
    allocation_fault,
    loaded,
};

enum class Rst0ChunkStorage : std::uint8_t {
    initial_buffer,
    file_mapping,
};

struct Rst0BundleLoadObservation {
    std::uint32_t file_handle_slot{};
    std::string resource_name;
    std::optional<bool> header_read_succeeded;
    std::optional<std::uint32_t> signature;
    std::optional<std::uint32_t> entry_count;
    std::optional<std::uint32_t> buffered_chunk_count;
    std::optional<std::uint32_t> mapped_file_offset;
    std::optional<bool> descriptor_allocation_succeeded;
    std::optional<bool> buffered_region_allocation_succeeded;
    // The original loader ignores this read helper's Boolean result.
    std::optional<bool> buffered_region_read_succeeded;
    std::vector<std::uint32_t> indexed_payload_sizes;
    ResourceMappingObservation mapping;
};

struct Rst0ChunkLocation {
    std::uint32_t index{};
    Rst0ChunkStorage storage{};
    std::uint32_t file_offset{};
    std::uint32_t initial_buffer_offset{};
    std::uint32_t payload_bytes{};
    bool first_chunk_header_dword_replaced_by_index_pointer{};
};

struct Rst0BundleLoadPlan {
    bool input_complete{true};
    Rst0BundleLoadExit exit{Rst0BundleLoadExit::incomplete};
    std::optional<std::uint32_t> buffered_region_read_bytes;
    std::optional<bool> ignored_buffered_region_read_result;
    std::optional<bool> resource_name_exceeds_0x3c_descriptor{};
    std::optional<std::uint32_t> index_table_bytes;
    std::optional<std::uint32_t> initial_reference_count;
    bool allocation_failure_can_fault{};
    bool bundle_linked_into_loaded_set{};
    std::vector<Rst0ChunkLocation> chunks;
    std::optional<ResourceMappingPlan> mapping;
    std::vector<std::uint32_t> calls;
};

// Projects 0x417550's RST0 load after the file has been opened by 0x417430.
// Header/index contents, allocations, reads, and mapping results are observed.
[[nodiscard]] Rst0BundleLoadPlan plan_rst0_bundle_load(
    const Rst0BundleLoadObservation& observation);

enum class ResourceSetAcquireExit : std::uint8_t {
    incomplete,
    existing_set_reused,
    file_open_failed,
    bundle_rejected,
    allocation_fault,
    bundle_loaded,
};

struct ResourceSetAcquireObservation {
    std::string resource_set_name;
    std::optional<bool> existing_set_found;
    std::optional<std::uint32_t> existing_reference_count;
    std::optional<std::string> primary_path_prefix;
    std::optional<std::string> secondary_path_prefix;
    std::optional<bool> primary_path_opened;
    std::optional<bool> secondary_path_opened;
    std::optional<std::uint32_t> opened_file_handle_slot;
    std::optional<Rst0BundleLoadObservation> bundle_load;
};

struct ResourceSetAcquirePlan {
    bool input_complete{true};
    ResourceSetAcquireExit exit{ResourceSetAcquireExit::incomplete};
    std::optional<std::uint32_t> reference_count_before;
    std::optional<std::uint32_t> reference_count_after;
    std::vector<std::string> attempted_paths;
    std::optional<Rst0BundleLoadPlan> bundle_load;
    bool opened_file_handle_closed{};
    bool file_handle_retained_by_bundle{};
    bool returned_success{};
    std::vector<std::uint32_t> calls;
};

// Composes the reuse/open/load/close control flow of 0x417430.
[[nodiscard]] ResourceSetAcquirePlan plan_resource_set_acquire(
    const ResourceSetAcquireObservation& observation);

enum class ResourceReleaseExit : std::uint8_t {
    incomplete,
    null_owner,
    reference_released,
    wildcard_set_unloaded,
    wildcard_owner_set_missing,
};

struct ResourceReleaseObservation {
    bool chunk_header_owner_is_null{};
    std::optional<std::uint32_t> reference_count_before;
    std::string entry_name;
    // Needed only when a wildcard entry reaches reference count zero.
    std::optional<bool> wildcard_owner_set_found;
};

struct ResourceReleasePlan {
    bool input_complete{true};
    ResourceReleaseExit exit{ResourceReleaseExit::incomplete};
    std::optional<std::uint32_t> reference_count_after;
    bool zero_reference_count_panic_called{};
    bool wildcard_entry{};
    bool wildcard_owner_set_lookup_called{};
    bool wildcard_owner_set_missing_panic_called{};
    bool loaded_set_node_removed{};
    std::uint32_t resource_data_free_calls{};
    bool entry_descriptor_freed{};
    bool set_node_freed{};
    bool returned_zero{};
    std::vector<std::uint32_t> calls;
};

// Projects 0x418160 and the wildcard-owned set removal through 0x4181E0/
// 0x418230. Pointer/refcount/list observations remain inputs.
[[nodiscard]] ResourceReleasePlan plan_resource_release(
    const ResourceReleaseObservation& observation);

struct ResourceSetRequestPlan {
    bool input_complete{true};
    bool lock_acquired{};
    bool missing_resource_panic_called{};
    bool lock_released{};
    bool returned_success{};
    std::optional<ResourceSetAcquirePlan> acquisition;
    std::vector<std::uint32_t> calls;
};

// Wraps 0x417430 in 0x417740's lock, panic, and unlock sequence.
[[nodiscard]] ResourceSetRequestPlan plan_resource_set_request(
    const ResourceSetAcquireObservation& observation);

struct ResourceSweepNodeObservation {
    std::uint32_t node_identifier{};
    std::optional<bool> entry_name_ends_with_wildcard;
    std::optional<bool> mapped_base_is_nonnull;
    std::optional<bool> fake_memory_map{};
    std::optional<std::uint32_t> entry_count;
    std::optional<std::uint32_t> buffered_chunk_count;
    std::vector<std::uint32_t> entry_attributes;
    std::optional<ResourceMappingObservation> remap_observation;
};

struct ResourceSweepObservation {
    std::uint32_t wrapper_address{}; // 0x417870 or 0x4178B0
    std::vector<ResourceSweepNodeObservation> loaded_nodes_in_list_order;
};

struct ResourceSweepCallback {
    std::uint32_t node_identifier{};
    std::uint32_t callback_address{};
    std::optional<ResourceUnmapPlan> unmap;
    std::optional<ResourceMappingPlan> remapping;
    std::vector<std::uint32_t> retargeted_buffered_chunk_file_offsets;
};

struct ResourceSweepPlan {
    bool input_complete{true};
    std::uint32_t callback_address{};
    std::uint32_t nodes_visited{};
    std::vector<ResourceSweepCallback> callbacks;
};

// Models the 0x417870/0x4178B0 wrappers, 0x417880 wildcard filter, and the
// observed unmap/remap callback effects. Runtime mapping results remain inputs.
[[nodiscard]] ResourceSweepPlan plan_resource_set_sweep(
    const ResourceSweepObservation& observation);

enum class LegacyFileReadExit : std::uint8_t {
    incomplete,
    exact_read,
    api_failure,
    short_read,
};

struct LegacyFileReadObservation {
    std::uint32_t handle_slot{};
    std::uint32_t requested_bytes{};
    std::string path;
    std::optional<bool> read_file_succeeded;
    std::optional<std::uint32_t> bytes_read;
    std::optional<std::uint32_t> last_error;
};

struct LegacyFileReadPlan {
    bool input_complete{true};
    LegacyFileReadExit exit{LegacyFileReadExit::incomplete};
    std::uint32_t handle_table_index{};
    std::uint32_t requested_bytes{};
    std::optional<std::uint32_t> bytes_read;
    bool get_last_error_called{};
    bool panic_helper_called{};
    bool returned_success{};
    std::optional<std::uint32_t> diagnostic_format_address;
    std::vector<std::uint32_t> calls;
};

// Models the exact-byte ReadFile wrapper 0x411560 using the 1-based legacy
// handle slot. OS results remain observations.
[[nodiscard]] LegacyFileReadPlan plan_legacy_file_read(
    const LegacyFileReadObservation& observation);

struct LegacyFileRegisterObservation {
    std::uint32_t current_slot_count{};
    std::vector<bool> slots_occupied;
    std::string path;
    bool handle_available{true};
};

struct LegacyFileRegisterPlan {
    bool input_complete{true};
    bool lock_acquired{};
    bool panic_helper_called{};
    bool slot_path_copied{};
    bool handle_stored{};
    bool lock_released{};
    std::optional<std::uint32_t> table_index;
    std::optional<std::uint32_t> one_based_slot;
    std::uint32_t path_entry_stride{0x104};
    std::uint32_t path_buffer_bytes{0x100};
    std::optional<std::uint32_t> path_copy_size_bytes;
    bool path_copy_overruns_path_buffer{};
    std::vector<std::uint32_t> calls;
};

// Models 0x4112E0's locked allocation/registration in the legacy 32-slot table.
[[nodiscard]] LegacyFileRegisterPlan plan_legacy_file_register(
    const LegacyFileRegisterObservation& observation);

struct LegacyFileOpenObservation {
    std::string path;
    bool create_file_succeeded{};
    LegacyFileRegisterObservation registration;
};

struct LegacyFileOpenPlan {
    bool input_complete{true};
    std::uint32_t desired_access{0x80000000};
    std::uint32_t share_mode{1};
    std::uint32_t creation_disposition{3};
    std::uint32_t flags_and_attributes{0x08000001};
    bool create_file_called{};
    bool returned_zero{};
    std::optional<std::uint32_t> returned_one_based_slot;
    std::optional<LegacyFileRegisterPlan> registration;
    std::vector<std::uint32_t> calls;
};

// Models 0x411430 from CreateFileA through handle-table registration.
[[nodiscard]] LegacyFileOpenPlan plan_legacy_file_open(
    const LegacyFileOpenObservation& observation);

struct LegacyFileCloseObservation {
    std::uint32_t one_based_slot{};
};

struct LegacyFileClosePlan {
    bool input_complete{true};
    std::uint32_t table_index{};
    bool close_handle_called{};
    bool slot_invalidated{};
    bool caller_slot_zeroed{};
    std::vector<std::uint32_t> calls;
};

// Models 0x411500; the actual native handle value remains a table observation.
[[nodiscard]] LegacyFileClosePlan plan_legacy_file_close(
    const LegacyFileCloseObservation& observation);

struct LegacyFileSizeObservation {
    std::uint32_t one_based_slot{};
    std::optional<std::uint32_t> low_size;
};

struct LegacyFileSizePlan {
    bool input_complete{true};
    std::uint32_t table_index{};
    bool get_file_size_called{};
    bool high_word_output_is_null{true};
    std::optional<std::uint32_t> returned_low_size;
    std::vector<std::uint32_t> calls;
};

// Models 0x411540; the low DWORD from GetFileSize is the only output observed.
[[nodiscard]] LegacyFileSizePlan plan_legacy_file_size(
    const LegacyFileSizeObservation& observation);

} // namespace viper
