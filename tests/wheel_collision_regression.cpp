#include "viper/wheel_body_coupling.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace {
void require(const bool condition)
{
    if (!condition) std::abort();
}

viper::WheelCollisionImpulseInput make_input()
{
    viper::WheelCollisionImpulseInput input{};
    input.body_center = {0.0F, 0.0F, 0.0F};
    input.contact_point = {1.0F, 0.0F, 0.0F};
    input.body_velocity = {0.0F, -1.0F, 0.0F};
    input.contact_normal = {0.0F, 1.0F, 0.0F};
    input.body_mass = 1.0F;
    input.angular_response_matrix = {1.0F, 0.0F, 0.0F,
                                     0.0F, 1.0F, 0.0F,
                                     0.0F, 0.0F, 1.0F};
    input.contact_depth = 0.1F;
    input.contact_threshold_response_0x28 = 0.5F;
    return input;
}
}

int main()
{
    auto input = make_input();
    auto response = viper::prepare_wheel_collision_impulse(input);
    require(response.applied);
    require(std::abs(response.normal_response - 0.5F) < 1e-6F);
    require(std::abs(response.duration - 100.0F) < 1e-5F);

    viper::NcarPointImpulseAccumulator accumulator{};
    std::uint8_t damage_flag = 0U;
    auto application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.point_impulse_accumulated);
    require(!application.damage_gate_input_unresolved);
    require(std::abs(accumulator.impulse_time_0x294[1] - 50.0F) < 1e-4F);
    require(std::abs(accumulator.duration_0x2ac - 100.0F) < 1e-5F);

    // The high-response hook is reached, but an unknown native global cannot
    // authorize a byte write in the portable model.
    input.body_velocity.y = -10000.0F;
    accumulator = {};
    damage_flag = 0U;
    application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.response.damage_hook_reached);
    require(application.damage_gate_input_unresolved);
    require(!application.damage_flag_written);
    require(damage_flag == 0U);
    require(application.point_impulse_accumulated);

    input.query_flag_nonzero_0x50878c = true;
    accumulator = {};
    application = viper::apply_wheel_collision_impulse(
        accumulator, damage_flag, input);
    require(application.response.damage_gate_input_known);
    require(application.damage_flag_written);
    require(damage_flag == 1U);
}
