#include "viper/world_track_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t byte_at(const viper::IdealLineTrackData::Record& row,
                     const std::size_t offset)
{
    return std::to_integer<std::uint8_t>(row[offset]);
}

void set_byte(viper::IdealLineResourcePayload::Record& row,
              const std::size_t offset, const std::uint8_t value)
{
    row[offset] = static_cast<std::byte>(value);
}

} // namespace

int main()
{
    try {
        viper::IdealLineResourcePayload payload;
        payload.resource_version =
            viper::IdealLineResourcePayload::supported_resource_version;
        payload.line_count = 3;
        payload.records.resize(3);
        set_byte(payload.records[0], 0x2a, 99);
        set_byte(payload.records[1], 0x2a, 2);
        set_byte(payload.records[2], 0x2a, 4);

        const auto normalized = viper::normalize_ideal_line_resource(payload, 1);
        require(normalized.has_value(), "valid ILIN rows rejected");
        require(normalized->category_labels_normalized,
                "cross-row category labels were not normalized");
        require(!normalized->category_label_input_incomplete,
                "complete ILIN category labels marked incomplete");
        require(byte_at(normalized->records[0], 0x2a) == 1,
                "first ILIN category label was not initialized");
        // Orientation mode keeps row zero fixed and reverses the remaining
        // rows before the native cross-row category scan.
        require(byte_at(normalized->records[1], 0x2a) == 1 &&
                    byte_at(normalized->records[2], 0x2a) == 2,
                "cross-row category labels did not follow native row order");

        viper::IdealLineResourcePayload one_row;
        one_row.resource_version =
            viper::IdealLineResourcePayload::supported_resource_version;
        one_row.line_count = 1;
        one_row.records.resize(1);
        one_row.trailing_bytes.resize(0x2b);
        one_row.trailing_bytes[0x2a] = std::byte{3};
        const auto with_tail = viper::normalize_ideal_line_resource(one_row, 1);
        require(with_tail && with_tail->category_labels_normalized &&
                    !with_tail->category_label_input_incomplete,
                "single-row trailing category byte was not safely modeled");

        one_row.trailing_bytes.clear();
        const auto without_tail = viper::normalize_ideal_line_resource(one_row, 1);
        require(without_tail && without_tail->category_label_input_incomplete &&
                    !without_tail->category_labels_normalized,
                "missing single-row overread input was not reported");

        viper::CarTrackLineObservation participant_car{};
        participant_car.configured_car_index = 0U;
        participant_car.configured_car_count = 2U;
        participant_car.raw_subtype =
            static_cast<std::uint32_t>(viper::TrackCarSubtype::pcar);
        participant_car.model_name = "viper";
        participant_car.force_default_placement_0x4dc1b0 = true;
        const auto player_car = viper::plan_car_track_line(participant_car);
        require(player_car.disposition ==
                    viper::CarTrackLineDisposition::construction_ready &&
                    player_car.record_tag == 0x50434152U &&
                    player_car.subtype == viper::TrackCarSubtype::pcar,
                "human participant marker must select the configured PCAR class");
        participant_car.configured_car_index = 1U;
        participant_car.raw_subtype =
            static_cast<std::uint32_t>(viper::TrackCarSubtype::acar);
        const auto ai_car = viper::plan_car_track_line(participant_car);
        require(ai_car.disposition ==
                    viper::CarTrackLineDisposition::construction_ready &&
                    ai_car.record_tag == 0x41434152U &&
                    ai_car.subtype == viper::TrackCarSubtype::acar &&
                    ai_car.calls_acar_helper_0x41a2a0,
                "AI participant marker must select the configured ACAR class");

        std::cout << "world track objects regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
