#include "viper/race_participants.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void write_u32_le(std::span<std::byte> bytes, const std::size_t offset,
                  const std::uint32_t value)
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
    }
}

void write_f32_word(viper::CarFilePayload& payload,
                    const std::size_t byte_offset, const float value)
{
    payload.raw_words[byte_offset / sizeof(std::uint32_t)] =
        std::bit_cast<std::uint32_t>(value);
}

void test_ai_configured_setup()
{
    using namespace viper;
    std::array<std::byte, 0x8cU> base{};
    write_u32_le(base, 0U, std::bit_cast<std::uint32_t>(2.0F));
    write_u32_le(base, 0x48U, std::bit_cast<std::uint32_t>(3.0F));
    write_u32_le(base, 0x4cU, std::bit_cast<std::uint32_t>(5.0F));
    write_u32_le(base, 0x08U, 0xdeadbeefU);

    AiConfiguredSetupObservation observation{};
    observation.base_setup_payload = base;
    observation.driver_tuning = {2.0F, 3.0F, 4.0F, 5.0F, 7.0F};
    observation.configuration_data_available = false;
    const auto fallback = build_ai_configured_setup(observation);
    require(fallback.input_complete && fallback.track_driver_scalars_written,
        "known AI setup inputs must produce a complete configured setup");
    require(std::bit_cast<float>(fallback.setup_words[1]) == 8.0F &&
            std::bit_cast<float>(fallback.setup_words[0x12U]) == 63.0F &&
            std::bit_cast<float>(fallback.setup_words[0x13U]) == 105.0F,
        "AIDR track and driver scalars must update their observed setup words");
    require(fallback.setup_words[2U] == 0xdeadbeefU,
        "unmapped AI setup words must retain the base payload value");
    for (std::size_t index = 0;
         index < fallback.coefficient_plan.fallback_coefficient_bits.size();
         ++index) {
        require(fallback.setup_words[0x14U + index] ==
                fallback.coefficient_plan.fallback_coefficient_bits[index],
            "fallback coefficients must be merged at setup words 0x14..0x1a");
    }

    auto car_file = CarFilePayload{};
    car_file.resource_version = CarFilePayload::supported_resource_version;
    car_file.raw_words[0x108U / 4U] = 2U;
    write_f32_word(car_file, 0x1b4U, 1.0F);
    write_f32_word(car_file, 0x1b0U, 0.0F);
    write_f32_word(car_file, 0x1acU, 0.0F);
    write_f32_word(car_file, 0xccU, 1.0F);
    write_f32_word(car_file, 0xc8U, 1.0F);
    write_f32_word(car_file, 0x4cU, 1.0F);
    observation.configuration_data_available = true;
    observation.loaded_car_file = car_file;
    observation.compatibility_division_helper_enabled = false;
    const auto configured = build_ai_configured_setup(observation);
    require(configured.input_complete &&
            configured.coefficient_plan.coefficient_values_calculated &&
            configured.coefficient_plan.coefficient_values.size() == 2U,
        "valid AI CARF configuration must produce its coefficient outputs");
    for (std::size_t index = 0;
         index < configured.coefficient_plan.coefficient_values.size(); ++index) {
        require(configured.setup_words[0x14U + index] == std::bit_cast<std::uint32_t>(
                    configured.coefficient_plan.coefficient_values[index]),
            "CARF coefficients must be merged at their observed setup words");
    }

    observation.base_setup_payload = std::span<const std::byte>(base).first(0x88U);
    const auto short_base = build_ai_configured_setup(observation);
    require(!short_base.input_complete,
        "truncated AI base setup must fail closed");
}

} // namespace

int main()
{
    try {
        const auto order = viper::initialize_single_race_driver_order();
        for (std::size_t index = 0; index < order.size(); ++index)
            require(order[index] == static_cast<std::int32_t>(index),
                "native default driver order must be the identity sequence");

        constexpr std::array<std::string_view, 7> ai_names{
            "AI 0", "AI 1", "AI 2", "AI 3", "AI 4", "AI 5", "AI 6"};
        viper::SingleRaceEntrantObservation observation{};
        observation.configured_car_count = 8;
        observation.driver_order_values = order;
        observation.ai_names_by_slot = ai_names;
        observation.player_car_resource_name = "viper";
        observation.resolved_player_name = "Player";
        observation.player_paintjob_value = 0;

        const auto plan = viper::build_single_race_entrants(observation);
        require(plan.input_complete,
            "complete default setup observation must produce complete entrant rows");
        require(plan.player_record_index == 7U,
            "fresh eight-car single race must put driver-order value 7 in row 8");
        require(plan.records.size() == 8U,
            "eight-car single race must construct eight participant records");
        require(plan.records[7].marker_value == 0U &&
            plan.records[7].car_resource_name == "viper",
            "eighth participant row must be the selected human Viper");
        require(plan.records[0].marker_value == 1U &&
            plan.records[0].display_name == "AI 0" &&
            plan.records[0].word_at_0xc0 == 0U,
            "first participant row must use the first AI driver slot");

        viper::SingleRaceSetupPostReadObservation setup_observation{};
        setup_observation.selected_track_resource_name = "bemidji";
        setup_observation.difficulty_resource_index = 0;
        setup_observation.participant_count = plan.participant_count;
        setup_observation.entrants = plan.records;
        const auto setup = viper::plan_single_race_setup_post_read(
            setup_observation);
        require(setup.input_complete && setup.entrant_initializers.size() == 8U,
            "post-read setup must initialize all eight configured rows");
        require(setup.setup_call_order == std::array<std::uint32_t, 3>{
                0x0040a7b0U, 0x0040a710U, 0x0040a620U},
            "row initialization must follow the native post-read call order");
        require(setup.entrant_initializers[0].initializer_address == 0x0041a000U &&
                setup.entrant_initializers[0].vehicle_record_offset_0x34_used,
            "AI row setup must dispatch through 0x41A000 using row +0x34");
        require(setup.entrant_initializers[7].initializer_address == 0x0045efd0U &&
                setup.entrant_initializers[7].vehicle_record_offset_0x34_used,
            "human row setup must dispatch through 0x45EFD0 using row +0x34");
        test_ai_configured_setup();
        std::cout << "race participant regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "race participant regression failure: " << error.what() << '\n';
        return 1;
    }
}
