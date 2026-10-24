#include "viper/resource_bundle.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace viper {
namespace {

constexpr std::uint32_t kRst0 = resource_type_code('R', 'S', 'T', '0');
constexpr std::size_t kHeaderSize = 16;
constexpr std::size_t kEntrySize = 36;
constexpr std::size_t kChunkHeaderSize = 8;

std::uint32_t read_u32_le(const std::vector<std::byte>& bytes,
                          const std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t)) {
        throw std::invalid_argument("truncated RST0 bundle integer");
    }

    return static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3])) << 24U);
}

bool ascii_case_equal(const std::string_view left,
                      const std::string_view right) noexcept
{
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        auto a = static_cast<unsigned char>(left[i]);
        auto b = static_cast<unsigned char>(right[i]);
        if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
        if (a != b) return false;
    }
    return true;
}

} // namespace

std::optional<StampPayload> parse_stamp_payload(
    const std::span<const std::byte> payload,
    const bool relocate_pointer_offsets)
{
    constexpr std::size_t pointer_table_offset = 0x81cU;
    constexpr std::size_t factor_a_offset = 0x04U;
    constexpr std::size_t factor_b_offset = 0x10U;
    constexpr std::size_t minimum_header_bytes = 0x14U;
    const auto read_payload_u32 = [payload](const std::size_t offset)
        -> std::optional<std::uint32_t> {
        if (offset > payload.size() || payload.size() - offset < 4U) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset])) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 1U])) << 8U) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 2U])) << 16U) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 3U])) << 24U);
    };
    if (payload.size() < minimum_header_bytes) return std::nullopt;
    const auto factor_a = read_payload_u32(factor_a_offset);
    const auto factor_b = read_payload_u32(factor_b_offset);
    if (!factor_a || !factor_b) return std::nullopt;

    StampPayload result{};
    result.factor_0x04 = *factor_a;
    result.factor_0x10 = *factor_b;
    const auto product_bits = *factor_a * *factor_b;
    result.pointer_entry_count = std::bit_cast<std::int32_t>(product_bits);
    result.pointer_offsets_relocated = relocate_pointer_offsets;
    if (payload.size() < pointer_table_offset) return result;
    result.embedded_data_offset = static_cast<std::uint32_t>(pointer_table_offset);
    if (result.pointer_entry_count <= 0) {
        result.pointer_table_complete = true;
        return result;
    }

    const auto count = static_cast<std::size_t>(result.pointer_entry_count);
    if (count > std::numeric_limits<std::size_t>::max() /
                    sizeof(std::uint32_t)) {
        return result;
    }
    const auto table_bytes = count * sizeof(std::uint32_t);
    if (table_bytes > payload.size() - pointer_table_offset) return result;
    const auto data_offset = pointer_table_offset + table_bytes;
    if (data_offset > std::numeric_limits<std::uint32_t>::max()) return result;
    result.embedded_data_offset = static_cast<std::uint32_t>(data_offset);
    result.pointer_entries.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto table_offset = pointer_table_offset +
            index * sizeof(std::uint32_t);
        const auto raw_offset = read_payload_u32(table_offset);
        if (!raw_offset) return result;
        const auto relocated_offset =
            static_cast<std::uint64_t>(data_offset) + *raw_offset;
        const bool offset_fits_target =
            relocated_offset <= std::numeric_limits<std::uint32_t>::max();
        const bool points_into_embedded_data = offset_fits_target &&
            relocated_offset >= data_offset && relocated_offset < payload.size();
        result.pointer_entries.push_back({
            static_cast<std::uint32_t>(table_offset), *raw_offset,
            relocate_pointer_offsets && offset_fits_target
                ? static_cast<std::uint32_t>(relocated_offset)
                : 0U,
            relocate_pointer_offsets, points_into_embedded_data,
        });
    }
    result.pointer_table_complete = true;
    return result;
}

std::optional<StampFontIndex> parse_stamp_font_index(
    const std::span<const std::byte> payload)
{
    constexpr std::size_t glyph_count_offset = 0x10U;
    constexpr std::size_t glyph_table_offset = 0x414U;
    const auto read_payload_u32 = [payload](const std::size_t offset)
        -> std::optional<std::uint32_t> {
        if (offset > payload.size() || payload.size() - offset < 4U) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset])) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 1U])) << 8U) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 2U])) << 16U) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 3U])) << 24U);
    };
    const auto raw_count = read_payload_u32(glyph_count_offset);
    if (!raw_count) return std::nullopt;

    StampFontIndex result{};
    result.glyph_record_count = std::bit_cast<std::int32_t>(*raw_count);
    if (result.glyph_record_count <= 0) {
        result.glyph_table_complete = true;
        return result;
    }
    const auto count = static_cast<std::size_t>(result.glyph_record_count);
    if (count > std::numeric_limits<std::size_t>::max() /
                    sizeof(std::uint32_t)) {
        return result;
    }
    const auto table_bytes = count * sizeof(std::uint32_t);
    if (payload.size() < glyph_table_offset ||
        table_bytes > payload.size() - glyph_table_offset) {
        return result;
    }

    for (std::size_t index = 0; index < count; ++index) {
        const auto encoded = read_payload_u32(
            glyph_table_offset + index * sizeof(std::uint32_t));
        if (!encoded) return result;
        const auto character = static_cast<std::uint8_t>(*encoded);
        const auto byte_1 = std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(*encoded >> 8U));
        const auto byte_2 = std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(*encoded >> 16U));
        const auto byte_3 = std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(*encoded >> 24U));
        const auto metric_sum = static_cast<std::uint16_t>(
            static_cast<std::int32_t>(byte_1) +
            static_cast<std::int32_t>(byte_2) +
            static_cast<std::int32_t>(byte_3));
        auto& output = result.characters[character];
        output.glyph_index = static_cast<std::uint16_t>(index);
        output.metric_0x204 = std::bit_cast<std::int16_t>(metric_sum);
        output.metric_0x404 = static_cast<std::int16_t>(byte_1);
        output.populated = true;
    }
    result.glyph_table_complete = true;
    return result;
}

std::optional<StampImage> parse_stamp_image(
    const std::span<const std::byte> payload,
    const bool relocate_pointer_offsets)
{
    constexpr std::size_t header_size = 0x14U;
    constexpr std::size_t palette_0_offset = 0x14U;
    constexpr std::size_t palette_1_offset = 0x214U;
    constexpr std::size_t palette_entries = 256U;
    constexpr std::size_t palette_bytes = palette_entries * 2U;
    const auto read_u32 = [payload](const std::size_t offset)
        -> std::optional<std::uint32_t> {
        if (offset > payload.size() || payload.size() - offset < 4U) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset])) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 1U])) << 8U) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 2U])) << 16U) |
               (static_cast<std::uint32_t>(
                   std::to_integer<std::uint8_t>(payload[offset + 3U])) << 24U);
    };
    if (payload.size() < header_size ||
        payload.size() < palette_1_offset + palette_bytes) {
        return std::nullopt;
    }
    const auto width = read_u32(0x00U);
    const auto height = read_u32(0x04U);
    const auto origin_x = read_u32(0x08U);
    const auto origin_y = read_u32(0x0cU);
    const auto frame_count = read_u32(0x10U);
    if (!width || !height || !origin_x || !origin_y || !frame_count) {
        return std::nullopt;
    }

    StampImage result{};
    result.width = std::bit_cast<std::int32_t>(*width);
    result.height = std::bit_cast<std::int32_t>(*height);
    result.origin_x = std::bit_cast<std::int32_t>(*origin_x);
    result.origin_y = std::bit_cast<std::int32_t>(*origin_y);
    result.frame_count = std::bit_cast<std::int32_t>(*frame_count);
    if (result.width <= 0 || result.height <= 0 || result.frame_count <= 0) {
        return result;
    }

    for (std::size_t plane = 0; plane < result.palette_planes.size(); ++plane) {
        const auto plane_offset = plane == 0 ? palette_0_offset
                                             : palette_1_offset;
        for (std::size_t index = 0; index < palette_entries; ++index) {
            const auto offset = plane_offset + index * 2U;
            result.palette_planes[plane][index] = static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(payload[offset]) |
                (static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(payload[offset + 1U])) << 8U));
        }
    }

    const auto stamp = parse_stamp_payload(payload, relocate_pointer_offsets);
    if (!stamp.has_value()) return std::nullopt;
    const auto expected_rows = static_cast<std::uint64_t>(result.height) *
        static_cast<std::uint64_t>(result.frame_count);
    result.pointer_table_complete = stamp->pointer_table_complete &&
        expected_rows == stamp->pointer_entries.size();
    result.rows_complete = result.pointer_table_complete &&
        stamp->pointer_offsets_relocated && relocate_pointer_offsets;
    if (!result.pointer_table_complete || !result.rows_complete) return result;

    result.encoded_rows.resize(static_cast<std::size_t>(result.frame_count));
    for (auto& frame_rows : result.encoded_rows) {
        frame_rows.resize(static_cast<std::size_t>(result.height));
    }
    for (std::size_t row_index = 0;
         row_index < stamp->pointer_entries.size(); ++row_index) {
        const auto& pointer = stamp->pointer_entries[row_index];
        if (!pointer.target_within_payload ||
            pointer.relocated_payload_offset >= payload.size()) {
            result.rows_complete = false;
            continue;
        }
        auto cursor = static_cast<std::size_t>(
            pointer.relocated_payload_offset);
        std::uint64_t decoded_width = 0;
        auto& encoded = result.encoded_rows[
            row_index / static_cast<std::size_t>(result.height)][
            row_index % static_cast<std::size_t>(result.height)];
        while (decoded_width < static_cast<std::uint64_t>(result.width)) {
            if (cursor >= payload.size()) {
                result.rows_complete = false;
                break;
            }
            const auto run = std::to_integer<std::uint8_t>(payload[cursor++]);
            const auto count = static_cast<std::uint64_t>(run & 0x7fU);
            if (count == 0U) {
                result.rows_complete = false;
                break;
            }
            const auto used_count = std::min(
                count, static_cast<std::uint64_t>(result.width) - decoded_width);
            encoded.push_back(run);
            if ((run & 0x80U) != 0U) {
                if (used_count > payload.size() - cursor) {
                    result.rows_complete = false;
                    encoded.clear();
                    break;
                }
                for (std::uint64_t pixel = 0; pixel < used_count; ++pixel) {
                    encoded.push_back(std::to_integer<std::uint8_t>(
                        payload[cursor + static_cast<std::size_t>(pixel)]));
                }
                cursor += static_cast<std::size_t>(used_count);
            }
            decoded_width += used_count;
        }
    }
    return result;
}

LegacyResourceRegistrationPlan plan_legacy_resource_registration(
    const LegacyResourceRegistrationObservation& observation) noexcept
{
    LegacyResourceRegistrationPlan plan{};
    plan.type_code = observation.type_code;
    plan.payload_size = observation.payload_size;
    if (!observation.allocation_succeeded) {
        plan.panic_reporter_called = true;
        plan.continued_after_panic = observation.panic_reporter_returned;
        plan.status = observation.panic_reporter_returned
            ? LegacyResourceRegistrationStatus::allocation_failed_reported
            : LegacyResourceRegistrationStatus::panic_did_not_return;
        return plan;
    }

    plan.constructor_called = true;
    if (observation.constructor_panicked) {
        plan.panic_reporter_called = true;
        plan.continued_after_panic = observation.panic_reporter_returned;
        if (!observation.panic_reporter_returned) {
            plan.status = LegacyResourceRegistrationStatus::panic_did_not_return;
            return plan;
        }
    }
    if (!observation.constructor_returned) {
        plan.status = LegacyResourceRegistrationStatus::panic_did_not_return;
        return plan;
    }
    if (!observation.object_index.has_value()) {
        plan.status = LegacyResourceRegistrationStatus::index_not_observed;
        return plan;
    }

    plan.registered_index = observation.object_index;
    plan.returned_handle = *observation.object_index + 1U;
    plan.registry_slot_written = true;
    plan.status = LegacyResourceRegistrationStatus::registered;
    return plan;
}

ResourceBundle ResourceBundle::open(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("unable to open resource bundle: " + path.string());
    }

    const auto end = input.tellg();
    if (end < std::streampos{0}) {
        throw std::runtime_error("resource bundle size is not representable: " + path.string());
    }
    const auto size = static_cast<std::streamoff>(end);
    if (static_cast<std::uintmax_t>(size) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        size > std::numeric_limits<std::streamsize>::max()) {
        throw std::runtime_error("resource bundle size is not representable: " + path.string());
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    if (!input) {
        throw std::runtime_error("unable to read resource bundle: " + path.string());
    }
    return parse(std::move(bytes));
}

ResourceBundle ResourceBundle::parse(std::vector<std::byte> bytes)
{
    if (bytes.size() < kHeaderSize) {
        throw std::invalid_argument("resource bundle is shorter than its RST0 header");
    }
    if (read_u32_le(bytes, 0) != kRst0) {
        throw std::invalid_argument("resource bundle does not have the RST0 signature");
    }

    const auto entry_count = read_u32_le(bytes, 4);
    const auto buffered_count = read_u32_le(bytes, 8);
    const auto buffered_boundary = read_u32_le(bytes, 12);
    if (buffered_count > entry_count) {
        throw std::invalid_argument("RST0 buffered-resource count exceeds entry count");
    }

    const auto remaining = bytes.size() - kHeaderSize;
    if (entry_count > remaining / kEntrySize) {
        throw std::invalid_argument("truncated RST0 resource index");
    }
    const auto table_size = static_cast<std::size_t>(entry_count) * kEntrySize;
    std::size_t chunk_offset = kHeaderSize + table_size;

    ResourceBundle bundle;
    bundle.bytes_ = std::move(bytes);
    bundle.buffered_resource_count_ = buffered_count;
    bundle.entries_.reserve(entry_count);

    for (std::uint32_t i = 0; i < entry_count; ++i) {
        const auto record = kHeaderSize + static_cast<std::size_t>(i) * kEntrySize;
        const auto name_begin = bundle.bytes_.begin() + static_cast<std::ptrdiff_t>(record);
        const auto name_end = name_begin + 16;
        const auto terminator = std::find(name_begin, name_end, std::byte{0});
        std::string name;
        name.reserve(static_cast<std::size_t>(terminator - name_begin));
        for (auto it = name_begin; it != terminator; ++it) {
            name.push_back(static_cast<char>(std::to_integer<unsigned char>(*it)));
        }
        if (name.empty()) {
            throw std::invalid_argument("RST0 resource has an empty name");
        }

        ResourceEntry entry;
        entry.name = std::move(name);
        entry.type_code = read_u32_le(bundle.bytes_, record + 16);
        entry.attributes = read_u32_le(bundle.bytes_, record + 20);
        entry.payload_size = read_u32_le(bundle.bytes_, record + 24);
        if (chunk_offset > bundle.bytes_.size() ||
            bundle.bytes_.size() - chunk_offset < kChunkHeaderSize ||
            entry.payload_size > bundle.bytes_.size() - chunk_offset - kChunkHeaderSize) {
            throw std::invalid_argument("truncated RST0 resource chunk: " + entry.name);
        }

        std::copy_n(bundle.bytes_.begin() + static_cast<std::ptrdiff_t>(chunk_offset),
                    kChunkHeaderSize, entry.chunk_header.begin());
        entry.payload_offset = chunk_offset + kChunkHeaderSize;
        bundle.entries_.push_back(std::move(entry));
        chunk_offset += kChunkHeaderSize + bundle.entries_.back().payload_size;

        if (i + 1 == buffered_count && chunk_offset != buffered_boundary) {
            throw std::invalid_argument("RST0 mapped-resource boundary does not match index");
        }
    }

    if (chunk_offset != bundle.bytes_.size()) {
        throw std::invalid_argument("RST0 resources do not account for the full bundle");
    }
    if (buffered_count == 0 && kHeaderSize + table_size != buffered_boundary) {
        throw std::invalid_argument("RST0 mapped-resource boundary does not match index");
    }
    return bundle;
}

const std::vector<ResourceEntry>& ResourceBundle::entries() const noexcept
{
    return entries_;
}

std::uint32_t ResourceBundle::buffered_resource_count() const noexcept
{
    return buffered_resource_count_;
}

std::span<const std::byte> ResourceBundle::payload(const ResourceEntry& entry) const noexcept
{
    if (entry.payload_offset > bytes_.size() ||
        entry.payload_size > bytes_.size() - entry.payload_offset) {
        return {};
    }
    return {bytes_.data() + entry.payload_offset, entry.payload_size};
}

const ResourceEntry* ResourceBundle::find(const std::string_view name,
                                          const std::uint32_t type_code) const noexcept
{
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const auto& entry) {
        return entry.type_code == type_code && ascii_case_equal(entry.name, name);
    });
    return it == entries_.end() ? nullptr : &*it;
}

std::optional<std::span<const std::byte>> ResourceBundle::find_payload(
    const std::string_view name, const std::uint32_t type_code) const noexcept
{
    const auto* entry = find(name, type_code);
    if (entry == nullptr) return std::nullopt;
    return payload(*entry);
}

ResourceGetPlan plan_resource_get(const ResourceGetObservation& observation)
{
    ResourceGetPlan plan;
    plan.calls.push_back(0x4151A0);
    plan.calls.push_back(0x417B70);
    const bool skip_loaded_search = observation.normalized_name_first_byte == '~';
    if (!skip_loaded_search) {
        if (!observation.loaded_set_entry_found) {
            plan.input_complete = false;
            return plan;
        }
        plan.loaded_set_search_performed = true;
    }
    const bool loaded_match = !skip_loaded_search && *observation.loaded_set_entry_found;
    if (!loaded_match) {
        if (!observation.external_resolver_entry_found) {
            plan.input_complete = false;
            return plan;
        }
        plan.external_resolver_called = true;
        plan.calls.push_back(0x417EF0);
        if (!*observation.external_resolver_entry_found) {
            plan.exit = ResourceGetExit::missing_resource;
        }
    }

    if (plan.exit != ResourceGetExit::missing_resource) {
        if (!observation.entry) {
            plan.input_complete = false;
            return plan;
        }
        const auto& entry = *observation.entry;
        if (entry.type_code != observation.requested_type_code) {
            plan.calls.push_back(0x411000);
            plan.type_mismatch_panic_called = true;
            plan.exit = ResourceGetExit::type_mismatch;
            if (!observation.missing_resource_log_suppressed_by_0x4d35a8) {
                plan.calls.push_back(0x410EA0);
                plan.error_string_addresses.push_back(0x4D3760);
                plan.missing_resource_error_reported = true;
            }
        } else {
            plan.resource_used_bit_set = true;
            plan.attributes_after = entry.attributes | 0x80000000U;
            plan.first_use_reported = (entry.attributes & 0x80000000U) == 0;
            plan.payload_size = entry.attributes & 0x7FFFFFFFU;
            plan.owner_pointer = entry.owner_pointer;
            plan.reference_count_was_zero = entry.reference_count == 0;
            const bool increment = entry.reference_count == 0 ||
                                   entry.chunk_header_first_dword_nonzero;
            plan.reference_count_after = entry.reference_count + (increment ? 1U : 0U);
            plan.payload_address = entry.chunk_header_address + 8U;
            plan.exit = ResourceGetExit::returned_resource;
        }
    } else if (!observation.missing_resource_log_suppressed_by_0x4d35a8) {
        plan.calls.push_back(0x410EA0);
        plan.error_string_addresses.push_back(0x4D3760);
        plan.missing_resource_error_reported = true;
    }

    // Both the miss and type-mismatch paths clear the one-shot loading gate.
    // Successful lookup releases the resource lock and clears it as well.
    plan.loading_gate_cleared = true;
    plan.calls.push_back(0x4151F0);
    return plan;
}

ResourceFileLoadPlan plan_resource_file_load(
    const ResourceFileLoadObservation& observation)
{
    ResourceFileLoadPlan plan;
    const auto append_path = [&plan](const std::optional<std::string>& prefix,
                                     const std::string_view suffix) {
        if (!prefix) {
            plan.constructed_paths_complete = false;
            plan.constructed_paths.push_back(std::nullopt);
        } else {
            plan.constructed_paths.push_back(*prefix + std::string(suffix));
        }
    };
    const bool tilde = !observation.resource_name.empty() &&
                       observation.resource_name.front() == '~';
    bool opened = false;
    if (tilde) {
        if (!observation.tilde_path_opened) {
            plan.input_complete = false;
            return plan;
        }
        plan.path = ResourceFileLoadPath::tilde_game_root;
        plan.attempted_paths.push_back(plan.path);
        append_path(observation.game_root_prefix,
                    std::string_view(observation.resource_name).substr(1U));
        plan.calls.push_back(0x411430);
        opened = *observation.tilde_path_opened;
    } else {
        if (!observation.primary_path_opened) {
            plan.input_complete = false;
            return plan;
        }
        plan.path = ResourceFileLoadPath::primary_resource_path;
        plan.attempted_paths.push_back(plan.path);
        append_path(observation.primary_resource_path_prefix, observation.resource_name);
        plan.calls.push_back(0x411430);
        opened = *observation.primary_path_opened;
        if (!opened) {
            if (!observation.secondary_path_opened) {
                plan.input_complete = false;
                return plan;
            }
            plan.path = ResourceFileLoadPath::secondary_resource_path;
            plan.attempted_paths.push_back(plan.path);
            append_path(observation.secondary_resource_path_prefix, observation.resource_name);
            plan.calls.push_back(0x411430);
            opened = *observation.secondary_path_opened;
        }
    }

    if (!opened) {
        plan.exit = ResourceFileLoadExit::open_failed;
        return plan;
    }
    plan.opened_path_source_name = observation.resource_name;
    if (!observation.header_read_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    plan.calls.push_back(0x411560);
    if (!*observation.header_read_succeeded || !observation.header_magic) {
        if (*observation.header_read_succeeded && !observation.header_magic) {
            plan.input_complete = false;
        }
        plan.exit = ResourceFileLoadExit::invalid_header;
        plan.calls.push_back(0x411500);
        plan.file_handle_closed = true;
        return plan;
    }
    if (*observation.header_magic != 0x52455330U) {
        plan.exit = ResourceFileLoadExit::invalid_header;
        plan.calls.push_back(0x411500);
        plan.file_handle_closed = true;
        return plan;
    }
    if (!observation.file_size_bytes) {
        plan.input_complete = false;
        return plan;
    }
    plan.calls.push_back(0x414170);
    plan.allocation_sizes.push_back(0x3cU);
    if (!observation.allocations_succeeded[0]) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.allocations_succeeded[0]) {
        plan.exit = ResourceFileLoadExit::allocation_failed;
        plan.allocation_failure_reaches_null_dereference = true;
        return plan;
    }

    const auto slash = observation.resource_name.find_last_of('\\');
    std::string base_name = slash == std::string::npos
                                ? observation.resource_name
                                : observation.resource_name.substr(slash + 1U);
    plan.linked_resource_name = base_name + '*';
    plan.linked_resource_name_overflows_0x24_buffer =
        base_name.size() + 1U > 0x24U;

    plan.calls.push_back(0x414170);
    plan.allocation_sizes.push_back(0x24U);
    if (!observation.allocations_succeeded[1]) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.allocations_succeeded[1]) {
        plan.exit = ResourceFileLoadExit::allocation_failed;
        plan.allocation_failure_reaches_null_dereference = true;
        return plan;
    }
    plan.header_word_1 = observation.header_word_1;
    plan.header_word_2 = observation.header_word_2;
    plan.input_complete = observation.header_word_1.has_value() &&
                          observation.header_word_2.has_value();
    const auto payload_bytes = *observation.file_size_bytes - 0x14;
    const auto read_bytes = *observation.file_size_bytes - 0x0c;
    plan.resource_payload_bytes = payload_bytes;
    plan.file_data_allocation_bytes = read_bytes;
    plan.file_read_bytes = read_bytes;
    plan.calls.push_back(0x411540);
    plan.calls.push_back(0x414170);
    plan.allocation_sizes.push_back(static_cast<std::uint32_t>(read_bytes));
    if (!observation.allocations_succeeded[2] || !observation.data_read_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.allocations_succeeded[2]) {
        plan.calls.push_back(0x411560);
        if (!*observation.data_read_succeeded) {
            plan.exit = ResourceFileLoadExit::read_failed;
            plan.allocation_failure_reaches_null_dereference = false;
            plan.calls.insert(plan.calls.end(), {0x414420, 0x414420, 0x414420, 0x411500});
            plan.file_handle_closed = true;
            return plan;
        }
        plan.exit = ResourceFileLoadExit::allocation_failed;
        plan.allocation_failure_reaches_null_dereference = true;
        return plan;
    }
    plan.calls.push_back(0x411560);
    if (!*observation.data_read_succeeded) {
        plan.exit = ResourceFileLoadExit::read_failed;
        plan.calls.insert(plan.calls.end(), {0x414420, 0x414420, 0x414420, 0x411500});
        plan.file_handle_closed = true;
        return plan;
    }
    plan.resource_linked_into_loaded_list = true;
    plan.exit = ResourceFileLoadExit::loaded;
    plan.calls.push_back(0x411500);
    plan.file_handle_closed = true;
    return plan;
}

ResourceMappingPlan plan_resource_mapping(
    const ResourceMappingObservation& observation)
{
    ResourceMappingPlan plan;
    plan.create_file_mapping_called = true;
    plan.calls.push_back(0x411BE0);
    if (!observation.create_file_mapping_succeeded) {
        plan.input_complete = false;
        return plan;
    }

    bool use_fallback = !*observation.create_file_mapping_succeeded;
    if (use_fallback) {
        plan.calls.push_back(0x416050);
        plan.calls.push_back(0x410EA0);
        plan.error_string_address = 0x4D1C80;
    } else {
        plan.map_view_called = true;
        if (!observation.map_view_succeeded) {
            plan.input_complete = false;
            return plan;
        }
        if (*observation.map_view_succeeded) {
            plan.exit = ResourceMappingExit::direct_file_mapping;
            plan.mapped_base_available = true;
            plan.mapping_handle_returned = true;
            plan.ownership_flag_returned = false;
            return plan;
        }
        plan.mapping_handle_closed_after_view_failure = true;
        plan.calls.push_back(0x416050);
        plan.calls.push_back(0x410EA0);
        plan.error_string_address = 0x4D1C64;
        use_fallback = true;
    }

    if (use_fallback) {
        plan.fallback_memory_map_selected = true;
        plan.fallback_file_open_called = true;
        plan.fallback_path_slot = observation.file_handle_slot - 1U;
        plan.calls.push_back(0x411B10);
        plan.calls.push_back(0x411430);
        if (!observation.fallback_file_opened) {
            plan.input_complete = false;
            return plan;
        }
        if (!*observation.fallback_file_opened) {
            plan.exit = ResourceMappingExit::fallback_path_missing;
            plan.fallback_missing_file_panic_called = true;
            plan.calls.push_back(0x411000);
            plan.error_string_address = 0x4D1C3C;
            plan.mapping_handle_returned = false;
            plan.ownership_flag_returned = true;
            return plan;
        }
        if (!observation.fallback_file_size_bytes) {
            plan.input_complete = false;
            return plan;
        }
        plan.fallback_file_data_bytes = observation.fallback_file_size_bytes;
        plan.fallback_allocation_bytes = observation.fallback_file_size_bytes;
        plan.calls.push_back(0x411540);
        plan.calls.push_back(0x414170);
        if (!observation.fallback_allocation_succeeded ||
            !observation.fallback_read_succeeded) {
            plan.input_complete = false;
            return plan;
        }
        plan.calls.push_back(0x411560);
        plan.fake_mapping_read_result_ignored = true;
        plan.calls.push_back(0x411500);
        plan.calls.push_back(0x410EA0);
        plan.error_string_address = 0x4D1C1C;
        plan.exit = ResourceMappingExit::fallback_memory_map;
        plan.mapped_base_available = *observation.fallback_allocation_succeeded;
        plan.mapping_handle_returned = false;
        plan.ownership_flag_returned = true;
    }
    return plan;
}

ResourceUnmapPlan plan_resource_unmap(const ResourceUnmapObservation& observation)
{
    ResourceUnmapPlan plan;
    plan.calls.push_back(0x411CB0);
    if (observation.fake_memory_map) {
        plan.fake_memory_buffer_freed = true;
        plan.calls.push_back(0x411BC0);
        plan.calls.push_back(0x414420);
    } else {
        plan.view_unmapped = true;
        plan.mapping_handle_closed = true;
        plan.mapping_handle_cleared = true;
    }
    return plan;
}

Rst0BundleLoadPlan plan_rst0_bundle_load(
    const Rst0BundleLoadObservation& observation)
{
    Rst0BundleLoadPlan plan;
    plan.calls.push_back(0x411560);
    if (!observation.header_read_succeeded ||
        (*observation.header_read_succeeded && !observation.signature)) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.header_read_succeeded || *observation.signature != 0x52535430U) {
        plan.exit = Rst0BundleLoadExit::rejected_header;
        return plan;
    }
    if (!observation.entry_count || !observation.buffered_chunk_count ||
        !observation.mapped_file_offset) {
        plan.input_complete = false;
        return plan;
    }

    plan.calls.push_back(0x414170);
    if (!observation.descriptor_allocation_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.descriptor_allocation_succeeded) {
        plan.exit = Rst0BundleLoadExit::allocation_fault;
        plan.allocation_failure_can_fault = true;
        return plan;
    }
    plan.resource_name_exceeds_0x3c_descriptor =
        observation.resource_name.size() + 1U > 0x3cU;

    const std::uint32_t buffered_bytes = *observation.mapped_file_offset - 0x10U;
    plan.buffered_region_read_bytes = buffered_bytes;
    plan.calls.push_back(0x414170);
    if (!observation.buffered_region_allocation_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.buffered_region_allocation_succeeded) {
        plan.exit = Rst0BundleLoadExit::allocation_fault;
        plan.allocation_failure_can_fault = true;
        return plan;
    }
    plan.calls.push_back(0x411560);
    plan.ignored_buffered_region_read_result =
        observation.buffered_region_read_succeeded;

    auto mapping_observation = observation.mapping;
    mapping_observation.file_handle_slot = observation.file_handle_slot;
    plan.mapping = plan_resource_mapping(mapping_observation);
    if (!plan.mapping->input_complete) {
        plan.input_complete = false;
        return plan;
    }

    const std::uint32_t index_bytes = *observation.entry_count * 0x24U;
    plan.index_table_bytes = index_bytes;
    const std::size_t entries_to_project = std::min<std::size_t>(
        *observation.entry_count, observation.indexed_payload_sizes.size());
    if (entries_to_project < *observation.entry_count) plan.input_complete = false;
    plan.chunks.reserve(entries_to_project);
    std::uint32_t relative_chunk_offset = index_bytes;
    for (std::size_t index = 0; index < entries_to_project; ++index) {
        const bool buffered = index < *observation.buffered_chunk_count;
        const std::uint32_t file_offset = 0x10U + relative_chunk_offset;
        plan.chunks.push_back({
            static_cast<std::uint32_t>(index),
            buffered ? Rst0ChunkStorage::initial_buffer : Rst0ChunkStorage::file_mapping,
            file_offset,
            buffered ? relative_chunk_offset : 0U,
            observation.indexed_payload_sizes[index],
            buffered,
        });
        relative_chunk_offset += observation.indexed_payload_sizes[index] + 8U;
    }
    plan.bundle_linked_into_loaded_set = true;
    plan.initial_reference_count = 1U;
    plan.exit = Rst0BundleLoadExit::loaded;
    return plan;
}

ResourceSetAcquirePlan plan_resource_set_acquire(
    const ResourceSetAcquireObservation& observation)
{
    ResourceSetAcquirePlan plan;
    if (!observation.existing_set_found) {
        plan.input_complete = false;
        return plan;
    }
    plan.calls.push_back(0x417430);
    if (*observation.existing_set_found) {
        plan.calls.push_back(0x4C6B90);
        if (!observation.existing_reference_count) {
            plan.input_complete = false;
            return plan;
        }
        plan.reference_count_before = observation.existing_reference_count;
        plan.reference_count_after = *observation.existing_reference_count + 1U;
        plan.exit = ResourceSetAcquireExit::existing_set_reused;
        plan.returned_success = true;
        return plan;
    }

    if (!observation.primary_path_opened) {
        plan.input_complete = false;
        return plan;
    }
    plan.calls.push_back(0x411430);
    if (observation.primary_path_prefix) {
        plan.attempted_paths.push_back(*observation.primary_path_prefix +
                                       observation.resource_set_name);
    }
    bool opened = *observation.primary_path_opened;
    if (!opened) {
        if (!observation.secondary_path_opened) {
            plan.input_complete = false;
            return plan;
        }
        plan.calls.push_back(0x411430);
        if (observation.secondary_path_prefix) {
            plan.attempted_paths.push_back(*observation.secondary_path_prefix +
                                           observation.resource_set_name);
        }
        opened = *observation.secondary_path_opened;
    }
    if (!observation.primary_path_prefix || (!opened && !observation.secondary_path_prefix)) {
        plan.input_complete = false;
    }
    if (!opened) {
        plan.exit = ResourceSetAcquireExit::file_open_failed;
        return plan;
    }
    if (!observation.opened_file_handle_slot || !observation.bundle_load) {
        plan.input_complete = false;
        return plan;
    }

    auto load_observation = *observation.bundle_load;
    load_observation.file_handle_slot = *observation.opened_file_handle_slot;
    load_observation.resource_name = observation.resource_set_name;
    plan.bundle_load = plan_rst0_bundle_load(load_observation);
    plan.input_complete = plan.input_complete && plan.bundle_load->input_complete;
    if (plan.bundle_load->exit == Rst0BundleLoadExit::loaded) {
        plan.exit = ResourceSetAcquireExit::bundle_loaded;
        plan.returned_success = true;
        plan.file_handle_retained_by_bundle = true;
        plan.reference_count_after = plan.bundle_load->initial_reference_count;
    } else if (plan.bundle_load->exit == Rst0BundleLoadExit::rejected_header) {
        plan.calls.push_back(0x411500);
        plan.opened_file_handle_closed = true;
        plan.exit = ResourceSetAcquireExit::bundle_rejected;
    } else if (plan.bundle_load->exit == Rst0BundleLoadExit::allocation_fault) {
        plan.exit = ResourceSetAcquireExit::allocation_fault;
    } else {
        plan.input_complete = false;
    }
    return plan;
}

ResourceReleasePlan plan_resource_release(
    const ResourceReleaseObservation& observation)
{
    ResourceReleasePlan plan;
    plan.calls.push_back(0x417EE0);
    if (observation.chunk_header_owner_is_null) {
        plan.exit = ResourceReleaseExit::null_owner;
        return plan;
    }
    if (!observation.reference_count_before) {
        plan.input_complete = false;
        return plan;
    }

    auto name_end = observation.entry_name.find('\0');
    if (name_end == std::string::npos) name_end = observation.entry_name.size();
    plan.wildcard_entry = name_end != 0 && observation.entry_name[name_end - 1U] == '*';
    const auto before = *observation.reference_count_before;
    if (before == 0) {
        plan.zero_reference_count_panic_called = true;
        plan.calls.push_back(0x411000);
        plan.reference_count_after = 0;
    } else {
        plan.reference_count_after = before - 1U;
    }

    if (!plan.wildcard_entry || *plan.reference_count_after != 0) {
        plan.exit = ResourceReleaseExit::reference_released;
        plan.returned_zero = *plan.reference_count_after == 0;
        return plan;
    }

    plan.wildcard_owner_set_lookup_called = true;
    plan.calls.push_back(0x4181E0);
    if (!observation.wildcard_owner_set_found) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.wildcard_owner_set_found) {
        plan.exit = ResourceReleaseExit::wildcard_owner_set_missing;
        plan.wildcard_owner_set_missing_panic_called = true;
        plan.calls.push_back(0x411000);
        plan.returned_zero = true;
        return plan;
    }

    plan.exit = ResourceReleaseExit::wildcard_set_unloaded;
    plan.loaded_set_node_removed = true;
    plan.resource_data_free_calls = 1;
    plan.entry_descriptor_freed = true;
    plan.set_node_freed = true;
    plan.returned_zero = true;
    plan.calls.insert(plan.calls.end(), {0x417EE0, 0x414420, 0x414420, 0x414420});
    return plan;
}

ResourceSetRequestPlan plan_resource_set_request(
    const ResourceSetAcquireObservation& observation)
{
    ResourceSetRequestPlan plan;
    plan.calls.push_back(0x4151A0);
    plan.lock_acquired = true;
    plan.acquisition = plan_resource_set_acquire(observation);
    plan.calls.insert(plan.calls.end(), plan.acquisition->calls.begin(),
                      plan.acquisition->calls.end());
    if (!plan.acquisition->input_complete ||
        plan.acquisition->exit == ResourceSetAcquireExit::allocation_fault ||
        plan.acquisition->exit == ResourceSetAcquireExit::incomplete) {
        plan.input_complete = false;
        return plan;
    }
    if (!plan.acquisition->returned_success) {
        plan.calls.push_back(0x411000);
        plan.missing_resource_panic_called = true;
    }
    plan.calls.push_back(0x4151F0);
    plan.lock_released = true;
    plan.returned_success = plan.acquisition->returned_success;
    return plan;
}

ResourceSweepPlan plan_resource_set_sweep(
    const ResourceSweepObservation& observation)
{
    ResourceSweepPlan plan;
    if (observation.wrapper_address == 0x417870U) {
        plan.callback_address = 0x417820U;
    } else if (observation.wrapper_address == 0x4178B0U) {
        plan.callback_address = 0x417790U;
    } else {
        plan.input_complete = false;
        return plan;
    }

    for (const auto& node : observation.loaded_nodes_in_list_order) {
        ++plan.nodes_visited;
        if (!node.entry_name_ends_with_wildcard) {
            plan.input_complete = false;
            continue;
        }
        if (!*node.entry_name_ends_with_wildcard) {
            ResourceSweepCallback callback;
            callback.node_identifier = node.node_identifier;
            callback.callback_address = plan.callback_address;
            if (plan.callback_address == 0x417820U) {
                if (!node.mapped_base_is_nonnull) {
                    plan.input_complete = false;
                } else if (*node.mapped_base_is_nonnull) {
                    if (!node.fake_memory_map) {
                        plan.input_complete = false;
                    } else {
                        callback.unmap = plan_resource_unmap({*node.fake_memory_map});
                    }
                }
            } else {
                if (!node.remap_observation || !node.entry_count ||
                    !node.buffered_chunk_count) {
                    plan.input_complete = false;
                } else {
                    auto mapping_observation = *node.remap_observation;
                    callback.remapping = plan_resource_mapping(mapping_observation);
                    if (!callback.remapping->input_complete ||
                        !callback.remapping->mapped_base_available) {
                        plan.input_complete = false;
                    }
                    const auto count = std::min<std::size_t>(
                        *node.entry_count, node.entry_attributes.size());
                    if (count < *node.entry_count) plan.input_complete = false;
                    std::uint32_t file_offset = *node.entry_count * 0x24U + 0x10U;
                    for (std::size_t index = 0; index < count; ++index) {
                        if (index < *node.buffered_chunk_count) {
                            callback.retargeted_buffered_chunk_file_offsets.push_back(
                                file_offset);
                        }
                        file_offset += (node.entry_attributes[index] & 0x7FFFFFFFU) + 8U;
                    }
                }
            }
            plan.callbacks.push_back(std::move(callback));
        }
    }
    return plan;
}

LegacyFileReadPlan plan_legacy_file_read(
    const LegacyFileReadObservation& observation)
{
    LegacyFileReadPlan plan;
    plan.requested_bytes = observation.requested_bytes;
    plan.calls.push_back(0x411560U);
    if (observation.handle_slot == 0 || !observation.read_file_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    plan.handle_table_index = observation.handle_slot - 1U;
    if (!*observation.read_file_succeeded) {
        plan.exit = LegacyFileReadExit::api_failure;
        plan.get_last_error_called = true;
        plan.panic_helper_called = true;
        plan.diagnostic_format_address = 0x4D1AE8U;
        plan.calls.push_back(0x411000U);
        plan.input_complete = observation.last_error.has_value();
        return plan;
    }
    if (!observation.bytes_read) {
        plan.input_complete = false;
        return plan;
    }
    plan.bytes_read = observation.bytes_read;
    if (*observation.bytes_read == observation.requested_bytes) {
        plan.exit = LegacyFileReadExit::exact_read;
        plan.returned_success = true;
        return plan;
    }
    plan.exit = LegacyFileReadExit::short_read;
    plan.panic_helper_called = true;
    plan.diagnostic_format_address = 0x4D1AC8U;
    plan.calls.push_back(0x411000U);
    return plan;
}

LegacyFileRegisterPlan plan_legacy_file_register(
    const LegacyFileRegisterObservation& observation)
{
    LegacyFileRegisterPlan plan;
    plan.calls.push_back(0x4112E0U);
    plan.calls.push_back(0x4151A0U);
    plan.lock_acquired = true;
    if (!observation.handle_available || observation.current_slot_count > 32U) {
        plan.input_complete = false;
    } else {
        std::optional<std::uint32_t> index;
        if (observation.current_slot_count < 32U) {
            index = observation.current_slot_count;
        } else if (observation.slots_occupied.size() >= 32U) {
            for (std::uint32_t candidate = 0; candidate < 32U; ++candidate) {
                if (!observation.slots_occupied[candidate]) {
                    index = candidate;
                    break;
                }
            }
        } else {
            plan.input_complete = false;
        }
        if (index) {
            plan.table_index = *index;
            plan.one_based_slot = *index + 1U;
            plan.slot_path_copied = true;
            plan.handle_stored = true;
            if (observation.path.size() >=
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                plan.input_complete = false;
            } else {
                plan.path_copy_size_bytes = static_cast<std::uint32_t>(
                    observation.path.size() + 1U);
                plan.path_copy_overruns_path_buffer =
                    *plan.path_copy_size_bytes > plan.path_buffer_bytes;
            }
        } else if (plan.input_complete) {
            plan.panic_helper_called = true;
            plan.calls.push_back(0x411000U);
        }
    }
    plan.calls.push_back(0x4151F0U);
    plan.lock_released = true;
    return plan;
}

LegacyFileOpenPlan plan_legacy_file_open(
    const LegacyFileOpenObservation& observation)
{
    LegacyFileOpenPlan plan;
    plan.calls.push_back(0x411430U);
    plan.create_file_called = true;
    if (!observation.create_file_succeeded) {
        plan.returned_zero = true;
        return plan;
    }
    auto registration_observation = observation.registration;
    registration_observation.path = observation.path;
    registration_observation.handle_available = true;
    plan.registration = plan_legacy_file_register(registration_observation);
    plan.calls.insert(plan.calls.end(), plan.registration->calls.begin(),
                      plan.registration->calls.end());
    plan.input_complete = plan.registration->input_complete;
    if (plan.registration->one_based_slot) {
        plan.returned_one_based_slot = plan.registration->one_based_slot;
    } else if (plan.registration->input_complete) {
        plan.returned_zero = true;
    }
    return plan;
}

LegacyFileClosePlan plan_legacy_file_close(
    const LegacyFileCloseObservation& observation)
{
    LegacyFileClosePlan plan;
    plan.calls.push_back(0x411500U);
    if (observation.one_based_slot == 0 || observation.one_based_slot > 32U) {
        plan.input_complete = false;
        return plan;
    }
    plan.table_index = observation.one_based_slot - 1U;
    plan.close_handle_called = true;
    plan.slot_invalidated = true;
    plan.caller_slot_zeroed = true;
    return plan;
}

LegacyFileSizePlan plan_legacy_file_size(
    const LegacyFileSizeObservation& observation)
{
    LegacyFileSizePlan plan;
    plan.calls.push_back(0x411540U);
    if (observation.one_based_slot == 0 || observation.one_based_slot > 32U) {
        plan.input_complete = false;
        return plan;
    }
    plan.table_index = observation.one_based_slot - 1U;
    plan.get_file_size_called = true;
    if (!observation.low_size) {
        plan.input_complete = false;
        return plan;
    }
    plan.returned_low_size = observation.low_size;
    return plan;
}

} // namespace viper
