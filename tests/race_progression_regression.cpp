#include "viper/race_progression.hpp"
#include "viper/race_loop_callbacks.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <array>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        const auto tracking_point = viper::race_tracking_point_xz(
            {1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F},
            {10.0F, 20.0F, 30.0F}, {2.0F, 3.0F, 4.0F});
        require(tracking_point[0] == 52.0F && tracking_point[1] == 90.0F,
            "race tracking must transform the local point and pass world X and Z");
        viper::RaceInputBindingObservation keyboard_input{};
        const viper::RaceInputBinding up_key_binding{1U, 0x26U};
        keyboard_input.device_button_states[0x26U] = 1U;
        require(viper::evaluate_race_input_binding(up_key_binding, keyboard_input) == 1.0F &&
                viper::race_input_binding_is_active(up_key_binding, keyboard_input),
            "digital runtime key snapshots must pass through the recovered binding evaluator");
        keyboard_input.device_button_states[0x26U] = 0U;
        require(!viper::race_input_binding_is_active(up_key_binding, keyboard_input) &&
                !viper::race_input_binding_is_active({}, keyboard_input),
            "released and unbound controls must remain inactive");

        viper::RaceProgressionState state{};
        viper::PhysicalRaceLapState physical_laps{};
        require(!viper::observe_physical_race_category(physical_laps, true, 2U, 1.0F, 2U) &&
                !viper::observe_physical_race_category(physical_laps, false, 1U, 2.0F, 2U) &&
                physical_laps.native_crossing_count == 0U,
            "only a category change into one advances native lap state");
        require(!viper::observe_physical_race_category(physical_laps, true, 1U, 3.0F, 2U) &&
                physical_laps.native_crossing_count == 1U && !physical_laps.finished,
            "the first category-one transition starts native crossing state");
        require(viper::observe_physical_race_category(physical_laps, true, 1U, 13.0F, 2U) &&
                physical_laps.last_lap_time == 13.0F && !physical_laps.finished,
            "the second transition completes the first lap");
        require(viper::observe_physical_race_category(physical_laps, true, 1U, 25.0F, 2U) &&
                physical_laps.last_lap_time == 12.0F && physical_laps.finished &&
                !viper::observe_physical_race_category(physical_laps, true, 1U, 30.0F, 2U),
            "the target crossing must finish and freeze the lap state");
        viper::RaceProgressionState physical_clock{};
        for (int i = 0; i < 5; ++i)
            viper::advance_race_clock(physical_clock, 0.016F);
        require(std::abs(physical_clock.elapsed_seconds - 0.08F) < 1.0e-6F &&
                physical_clock.completed_laps == 0U && !physical_clock.finished,
            "physical simulation time must advance without route transitions");
        viper::initialize_race_progression(state, 1U);
        viper::advance_race_clock(state, 0.5F);
        require(!viper::observe_race_route_transition(state, 2U, true, 2U),
            "ordinary forward segment changes must not complete a lap");
        require(!viper::observe_race_route_transition(state, 0U, true, 2U),
            "crossing the route array boundary away from the lap start must not count");
        require(viper::observe_race_route_transition(state, 1U, true, 2U) &&
                state.completed_laps == 1U && !state.finished,
            "crossing the initialized lap start must count one lap");
        require(!viper::observe_race_route_transition(state, 1U, false, 2U),
            "reverse travel must not count as a completed lap");
        viper::advance_race_clock(state, 1.25F);
        require(viper::observe_race_route_transition(state, 1U, true, 2U),
            "the second forward crossing must reach the configured finish");
        require(state.finished && state.completed_laps == 2U &&
                std::abs(state.elapsed_seconds - 1.75F) < 1.0e-6F,
            "configured lap count must finish the race and retain elapsed time");
        viper::advance_race_clock(state, 4.0F);
        require(state.elapsed_seconds == 1.75F &&
                !viper::observe_race_route_transition(state, 1U, true, 2U),
            "finished race state must remain stable");

        const std::vector<std::array<float, 2>> straight_route{
            {0.0F, 0.0F}, {100.0F, 0.0F}};
        viper::RouteMotionState replicated_state{};
        replicated_state.route_segment = 0U;
        replicated_state.route_fraction = 0.25F;
        replicated_state.steering = 0.5F;
        replicated_state.speed = 12.0F;
        replicated_state.route_distance_travelled = 25.0F;
        viper::initialize_race_progression(replicated_state.race, 0U);
        viper::advance_race_clock(replicated_state.race, 1.5F);
        const auto replicated_before_projection = replicated_state;
        const auto projected_peer_state =
            viper::project_route_motion_state(replicated_state, straight_route);
        const auto repeated_projection =
            viper::project_route_motion_state(replicated_state, straight_route);
        require(projected_peer_state.valid &&
                std::abs(projected_peer_state.x - 25.0F) < 1.0e-5F &&
                std::abs(projected_peer_state.z + 0.0576F) < 1.0e-5F &&
                std::abs(projected_peer_state.yaw - 1.7457963F) < 1.0e-5F &&
                repeated_projection.x == projected_peer_state.x &&
                repeated_projection.z == projected_peer_state.z &&
                repeated_projection.yaw == projected_peer_state.yaw &&
                replicated_state.route_segment ==
                    replicated_before_projection.route_segment &&
                replicated_state.route_fraction ==
                    replicated_before_projection.route_fraction &&
                replicated_state.speed == replicated_before_projection.speed &&
                replicated_state.race.elapsed_seconds ==
                    replicated_before_projection.race.elapsed_seconds,
            "projecting a fixed replicated route snapshot must not advance simulation state");
        auto invalid_peer_state = replicated_state;
        invalid_peer_state.route_segment = straight_route.size();
        require(!viper::project_route_motion_state(invalid_peer_state,
                    straight_route).valid,
            "replicated route states must reject segments outside the local route");
        std::array<viper::RouteMotionState, 4> finishers{};
        finishers[0].race = {0U, 1U, 12.5F, true};
        finishers[1].race = {0U, 1U, 0.0F, false};
        finishers[2].race = {0U, 1U, 9.25F, true};
        finishers[3].race = {0U, 1U, 0.0F, false};
        viper::RouteMotionState finished_player{};
        finished_player.race = {0U, 1U, 10.0F, true};
        const auto standings = viper::build_route_race_standings(
            finished_player, finishers, 3U);
        require(standings.size() == 3U &&
                standings[0].participant_index == 2U &&
                standings[1].player && standings[1].participant_index == 3U &&
                standings[2].participant_index == 0U,
            "race standings must sort only completed entrants by finish time and slot");

        std::array<viper::RouteMotionState, 5> active_participants{};
        active_participants[0].route_distance_travelled = 12.0F;
        active_participants[1].route_distance_travelled = 18.0F;
        active_participants[2].route_distance_travelled = 18.0F;
        active_participants[3].route_distance_travelled =
            std::numeric_limits<float>::quiet_NaN();
        active_participants[4].race = {0U, 1U, 8.0F, true};
        active_participants[4].route_distance_travelled = 5.0F;
        viper::RouteMotionState active_player{};
        active_player.route_distance_travelled = 15.0F;
        const auto positions = viper::build_route_race_positions(
            active_player, active_participants, 0U);
        require(positions.size() == 4U && positions[0].finished &&
                positions[0].participant_index == 4U &&
                positions[1].participant_index == 1U &&
                positions[2].participant_index == 2U &&
                positions[3].player && positions[3].participant_index == 0U,
            "live positions must rank finishers first, then signed route distance with stable ties and skip invalid progress");
        std::cout << "race progression regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
