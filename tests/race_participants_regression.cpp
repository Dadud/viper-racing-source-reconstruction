#include "viper/race_participants.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
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
        std::cout << "race participant regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "race participant regression failure: " << error.what() << '\n';
        return 1;
    }
}
