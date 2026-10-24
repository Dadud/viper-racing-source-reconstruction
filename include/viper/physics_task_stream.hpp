#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

// Return the class-specific cursor advance used by 0x4209E0 for a serialized
// physics-task record tag. This is distinct from the number of bytes copied
// into the task buffer and from the constructed object's allocation size.
[[nodiscard]] std::optional<std::uint32_t>
physics_task_stream_advance_for_tag(std::uint32_t record_tag) noexcept;

struct PhysicsTaskStreamRecordOffset {
    std::uint32_t record_tag{};
    std::uint32_t stream_offset{};
    std::uint32_t cursor_advance{};
};

enum class PhysicsTaskStreamPlanStatus {
    complete,
    unknown_record_tag,
    cursor_overflow,
};

struct PhysicsTaskStreamPlan {
    PhysicsTaskStreamPlanStatus status{PhysicsTaskStreamPlanStatus::complete};
    std::uint32_t initial_cursor{};
    std::uint32_t final_cursor{};
    std::vector<PhysicsTaskStreamRecordOffset> records;
    std::optional<std::size_t> failed_record_index;
};

// Folds records in their already-observed serialization order. The initial
// native cursor and record order are explicit inputs; this function does not
// infer either from track.obt lines or from object allocation sizes.
[[nodiscard]] PhysicsTaskStreamPlan plan_physics_task_stream_offsets(
    std::span<const std::uint32_t> ordered_record_tags,
    std::uint32_t initial_cursor = 0U);

struct PhysicsTaskRecordInput {
    // Complete native record bytes, beginning with its FourCC and declared
    // record size. Bytes are retained verbatim after the four-byte cursor.
    std::span<const std::byte> record_bytes;
};

enum class PhysicsTaskRecordSerializationStatus : std::uint8_t {
    complete,
    malformed_record_header,
    declared_size_mismatch,
    unknown_record_tag,
    cursor_overflow,
    output_size_overflow,
};

struct SerializedPhysicsTaskRecord {
    std::uint32_t record_tag{};
    std::uint32_t cursor_offset{};
    std::uint32_t record_size{};
    std::uint32_t cursor_advance{};
};

struct PhysicsTaskRecordSerialization {
    PhysicsTaskRecordSerializationStatus status{
        PhysicsTaskRecordSerializationStatus::complete};
    std::vector<SerializedPhysicsTaskRecord> records;
    // Native in-band format: little-endian cursor DWORD followed by the exact
    // complete record bytes. The class advance affects the next cursor only.
    std::vector<std::byte> bytes;
    std::uint32_t initial_cursor{};
    std::uint32_t final_cursor{};
    std::optional<std::size_t> failed_record_index;
};

// Serializes complete, already reconstructed records using the native stream
// framing. Invalid or unknown records stop the walk without appending them.
[[nodiscard]] PhysicsTaskRecordSerialization serialize_physics_task_records(
    std::span<const PhysicsTaskRecordInput> records,
    std::uint32_t initial_cursor = 0U);

} // namespace viper
