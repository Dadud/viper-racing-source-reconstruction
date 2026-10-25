#include "viper/route_opponent_ai.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

viper::RouteMotionState motion(
    const std::size_t segment,
    const float fraction,
    const float speed,
    const bool finished = false)
{
    viper::RouteMotionState state{};
    state.route_segment = segment;
    state.route_fraction = fraction;
    state.speed = speed;
    state.race.finished = finished;
    return state;
}

} // namespace

int main()
{
    try {
        const std::vector<std::array<float, 2>> route{
            {0.0F, 0.0F}, {100.0F, 0.0F},
            {100.0F, 100.0F}, {0.0F, 100.0F}};

        std::array<viper::RouteMotionState, 3> traffic{
            motion(0U, 0.0F, 20.0F),
            motion(0U, 0.10F, 0.0F),
            motion(2U, 0.5F, 12.0F)};
        const viper::RouteOpponentAiState fresh{};
        const auto close_follow = viper::decide_route_opponent_action(
            fresh, route, traffic, 0U);
        const auto repeated = viper::decide_route_opponent_action(
            fresh, route, traffic, 0U);
        require(close_follow.valid && close_follow.leader_index == 1U &&
                close_follow.distance_to_leader > 9.9F &&
                close_follow.distance_to_leader < 10.1F &&
                close_follow.brake > 0.0F && close_follow.throttle == 0.0F,
            "close slower leader must trigger a staged braking decision");
        require(close_follow.brake == repeated.brake &&
                close_follow.throttle == repeated.throttle &&
                close_follow.next_state.braking_latched ==
                    repeated.next_state.braking_latched,
            "identical traffic snapshots must produce deterministic decisions");

        traffic[0].speed = 4.2F;
        const auto held_latch = viper::decide_route_opponent_action(
            close_follow.next_state, route, traffic, 0U);
        require(held_latch.next_state.braking_latched &&
                held_latch.brake > 0.0F,
            "braking latch must persist until the target speed is reached");
        traffic[0].speed = 0.0F;
        const auto released_latch = viper::decide_route_opponent_action(
            held_latch.next_state, route, traffic, 0U);
        require(!released_latch.next_state.braking_latched &&
                released_latch.brake == 0.0F,
            "braking latch must release after the car falls below target speed");

        const std::array<viper::RouteMotionState, 1> alone{
            motion(0U, 0.0F, 0.0F)};
        const auto open_route = viper::decide_route_opponent_action(
            fresh, route, alone, 0U);
        require(open_route.valid && open_route.throttle == 1.0F &&
                open_route.brake == 0.0F &&
                open_route.leader_index ==
                    std::numeric_limits<std::size_t>::max(),
            "open route must accelerate toward the policy's prototype target");

        const std::array<viper::RouteMotionState, 2> finished_leader{
            motion(0U, 0.0F, 0.0F), motion(0U, 0.01F, 0.0F, true)};
        const auto ignores_finished = viper::decide_route_opponent_action(
            fresh, route, finished_leader, 0U);
        require(ignores_finished.valid &&
                ignores_finished.leader_index ==
                    std::numeric_limits<std::size_t>::max() &&
                ignores_finished.throttle == 1.0F,
            "finished entrants must not become traffic leaders");

        const auto finished_self = viper::decide_route_opponent_action(
            {true}, route, finished_leader, 1U);
        require(finished_self.valid && finished_self.throttle == 0.0F &&
                finished_self.brake == 0.0F &&
                !finished_self.next_state.braking_latched,
            "finished self must have neutral output and cleared staged state");

        const auto bad_index = viper::decide_route_opponent_action(
            fresh, route, alone, 1U);
        require(!bad_index.valid, "invalid self index must fail closed");
        auto bad_motion = alone;
        bad_motion[0].speed = std::numeric_limits<float>::quiet_NaN();
        require(!viper::decide_route_opponent_action(
                    fresh, route, bad_motion, 0U).valid,
            "non-finite self speed must fail closed");
        auto bad_route = route;
        bad_route[0][0] = std::numeric_limits<float>::infinity();
        require(!viper::decide_route_opponent_action(
                    fresh, bad_route, alone, 0U).valid,
            "non-finite route geometry must fail closed");

        std::cout << "route opponent AI regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
