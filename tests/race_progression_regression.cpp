#include "viper/race_progression.hpp"
#include "viper/race_loop_callbacks.hpp"

#include <cmath>
#include <iostream>
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

        const std::vector<std::array<float, 2>> square_route{
            {0.0F, 0.0F}, {10.0F, 0.0F},
            {10.0F, 10.0F}, {0.0F, 10.0F}};
        viper::RouteMotionState minimum_delta_motion{};
        viper::initialize_race_progression(minimum_delta_motion.race, 0U);
        (void)viper::advance_route_motion(minimum_delta_motion, square_route,
            false, false, false, false, 0.0F, 1U);
        require(std::abs(minimum_delta_motion.race.elapsed_seconds - 0.01F) < 1.0e-6F,
            "runtime race step must apply the native minimum frame delta");
        viper::RouteMotionState maximum_delta_motion{};
        viper::initialize_race_progression(maximum_delta_motion.race, 0U);
        (void)viper::advance_route_motion(maximum_delta_motion, square_route,
            false, false, false, false, 1.0F, 1U);
        require(std::abs(maximum_delta_motion.race.elapsed_seconds - 0.25F) < 1.0e-6F,
            "runtime race step must apply the native maximum frame delta");

        viper::RouteMotionState motion{};
        viper::initialize_race_progression(motion.race, 0U);
        viper::RouteMotionResult motion_result{};
        for (unsigned frame = 0U; frame < 6000U && !motion.race.finished; ++frame) {
            motion_result = viper::advance_route_motion(motion, square_route,
                false, false, true, false, 1.0F / 60.0F, 2U);
            require(motion_result.valid,
                "route integrator must produce finite poses during a closed-course run");
        }
        require(motion.race.finished && motion.race.completed_laps == 2U &&
                motion_result.finished && motion.race.elapsed_seconds > 0.0F,
            "the runtime route integrator must drive through the configured finish");

        viper::RouteMotionState braking_motion{};
        braking_motion.speed = 10.0F;
        viper::initialize_race_progression(braking_motion.race, 0U);
        (void)viper::advance_route_motion(braking_motion, square_route,
            false, false, false, true, 0.25F, 1U);
        require(braking_motion.speed >= 0.0F && braking_motion.speed < 10.0F,
            "the braking binding must slow a forward-moving car without selecting reverse");

        viper::RouteMotionState reverse_motion{};
        viper::initialize_race_progression(reverse_motion.race, 0U);
        const auto reverse_result = viper::advance_route_motion(reverse_motion,
            square_route, false, false, false, false, 0.25F, 1U, true);
        require(reverse_result.valid && reverse_motion.speed < 0.0F &&
                reverse_motion.route_segment == square_route.size() - 1U &&
                reverse_motion.route_fraction < 1.0F &&
                reverse_motion.race.completed_laps == 0U,
            "the separate reverse binding must move backward without counting a lap");

        viper::RouteMotionState steering_motion{};
        viper::initialize_race_progression(steering_motion.race, 0U);
        const std::vector<std::array<float, 2>> straight_route{
            {0.0F, 0.0F}, {100.0F, 0.0F}};
        const auto right_input = viper::advance_route_motion(steering_motion,
            straight_route, false, true, false, false, 0.05F, 1U);
        require(std::abs(steering_motion.steering - 0.25F) < 1.0e-6F &&
                std::abs(right_input.yaw - (1.57079632679F + 0.0875F)) < 1.0e-6F,
            "digital steering must use the recovered native response rate");
        (void)viper::advance_route_motion(steering_motion, straight_route,
            false, false, false, false, 0.05F, 1U);
        require(std::abs(steering_motion.steering - 0.125F) < 1.0e-6F,
            "released digital steering must decay at the recovered native rate");

        viper::RouteMotionState staged_motion{};
        staged_motion.route_segment = 3U;
        staged_motion.route_fraction = 0.9F;
        staged_motion.speed = 30.0F;
        viper::initialize_race_progression(staged_motion.race, 0U);
        const auto committed_before_query = staged_motion;
        auto candidate_motion = staged_motion;
        const auto staged_result = viper::advance_route_motion(candidate_motion,
            square_route, false, true, false, false, 0.05F, 2U);
        require(staged_result.valid && candidate_motion.race.elapsed_seconds > 0.0F &&
                candidate_motion.race.completed_laps == 1U,
            "staged advancement must expose a candidate lap crossing before terrain validation");
        require(!viper::commit_route_motion_after_terrain_query(
                    staged_motion, candidate_motion, false) &&
                staged_motion.route_segment == committed_before_query.route_segment &&
                staged_motion.route_fraction == committed_before_query.route_fraction &&
                staged_motion.steering == committed_before_query.steering &&
                staged_motion.speed == committed_before_query.speed &&
                staged_motion.race.elapsed_seconds ==
                    committed_before_query.race.elapsed_seconds &&
                staged_motion.race.completed_laps ==
                    committed_before_query.race.completed_laps &&
                staged_motion.race.finished == committed_before_query.race.finished,
            "failed terrain validation must roll back all player route and race progress");
        require(viper::commit_route_motion_after_terrain_query(
                    staged_motion, candidate_motion, true) &&
                staged_motion.route_fraction == candidate_motion.route_fraction &&
                staged_motion.steering == candidate_motion.steering &&
                staged_motion.race.elapsed_seconds ==
                    candidate_motion.race.elapsed_seconds &&
                staged_motion.race.completed_laps == 1U,
            "successful terrain validation must commit the complete candidate state");

        std::array<viper::RouteMotionState, 4> finishers{};
        finishers[0].race = {0U, 1U, 12.5F, true};
        finishers[1].race = {0U, 1U, 0.0F, false};
        finishers[2].race = {0U, 1U, 9.25F, true};
        finishers[3].race = {0U, 1U, 0.0F, false};
        auto finished_player = staged_motion;
        finished_player.race = {0U, 1U, 10.0F, true};
        const auto standings = viper::build_route_race_standings(
            finished_player, finishers, 3U);
        require(standings.size() == 3U &&
                standings[0].participant_index == 2U &&
                standings[1].player && standings[1].participant_index == 3U &&
                standings[2].participant_index == 0U,
            "race standings must sort only completed entrants by finish time and slot");
        std::cout << "race progression regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
