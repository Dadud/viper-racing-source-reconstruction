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

enum class PhysicsTaskRecordStreamReadStatus : std::uint8_t {
    complete,
    end_of_stream,
    incomplete_cursor,
    incomplete_record_header,
    incomplete_record_payload,
    trailing_bytes_after_end_of_stream,
    cursor_mismatch,
    cursor_not_representable,
    unknown_record_tag,
};

struct DecodedPhysicsTaskRecord {
    std::uint32_t record_tag{};
    std::uint32_t cursor_offset{};
    std::uint32_t record_size{};
    std::uint32_t cursor_advance{};
    std::size_t record_bytes_offset{};
};

struct PhysicsTaskRecordStreamRead {
    PhysicsTaskRecordStreamReadStatus status{
        PhysicsTaskRecordStreamReadStatus::end_of_stream};
    std::vector<DecodedPhysicsTaskRecord> records;
    std::size_t consumed_bytes{};
    std::uint32_t final_cursor{};
    std::optional<std::size_t> failed_record_index;
};

// Serializes complete, already reconstructed records using the native stream
// framing. Invalid or unknown records stop the walk without appending them.
[[nodiscard]] PhysicsTaskRecordSerialization serialize_physics_task_records(
    std::span<const PhysicsTaskRecordInput> records,
    std::uint32_t initial_cursor = 0U);

// Owns an explicitly supplied, already ordered sequence of complete records
// for one task-stream session. Records are committed only after their framing
// and cursor advance have been validated. This does not infer serialization
// order or model the separate fixed-step interpolation ring.
class PhysicsTaskRecordStreamSession {
public:
    explicit PhysicsTaskRecordStreamSession(std::uint32_t initial_cursor = 0U)
        : initial_cursor_(initial_cursor), final_cursor_(initial_cursor) {}

    // Appends one complete record. The returned serialization describes this
    // append; on failure the session remains unchanged and failed_record_index
    // identifies the position the rejected record would have occupied.
    [[nodiscard]] PhysicsTaskRecordSerialization append_record(
        std::span<const std::byte> record_bytes);

    // Returns the complete stream accumulated so far.
    [[nodiscard]] PhysicsTaskRecordSerialization snapshot() const;

    [[nodiscard]] std::uint32_t final_cursor() const noexcept {
        return final_cursor_;
    }

private:
    std::uint32_t initial_cursor_{};
    std::uint32_t final_cursor_{};
    std::vector<SerializedPhysicsTaskRecord> records_;
    std::vector<std::byte> bytes_;
};

// Decodes the cursor-prefixed complete-record framing written above. Cursor
// values are signed in the native reader, so a negative DWORD terminates input.
[[nodiscard]] PhysicsTaskRecordStreamRead read_physics_task_record_stream(
    std::span<const std::byte> bytes);

} // namespace viper
