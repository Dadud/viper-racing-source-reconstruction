#include "viper/model_resource.hpp"
#include "viper/ncar_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(const float actual, const float expected)
{
    return std::fabs(actual - expected) < 1.0e-5F;
}

} // namespace

int main()
{
    try {
        std::array<viper::ModelVertex, 3> vertices{};
        vertices[0].fields = {0.0F, 0.0F, 0.0F};
        vertices[1].fields = {1.0F, 0.0F, 0.0F};
        vertices[2].fields = {0.0F, 0.0F, 1.0F};
        const auto model_bounds = viper::measure_model_vertex_bounds(vertices);

        viper::NcarResetSphereBoundsObservation observation{};
        observation.extents_pointer_present = true;
        observation.car_record_0x58 = 200.0F;
        observation.car_record_0x5c = 25.0F;
        observation.car_record_0x128 = 10.0F;
        observation.precomputed_model_bounds = model_bounds;
        const auto geometry = viper::derive_ncar_reset_sphere_geometry(observation);
        require(geometry.inputs_complete &&
                geometry.raw_bounds == std::array<float, 6>{
                    0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F},
            "precomputed FNIM bounds must feed the native reset-sphere geometry");
        require(near(geometry.local_geometry.local_b0_after_body_shift, -0.254F) &&
                near(geometry.local_geometry.local_98_after_body_shift, 1.27F) &&
                near(geometry.local_geometry.local_9c_after_body_shift, 2.27F),
            "explicit CARF/body source fields must apply to the model extents");

        const auto positions =
            viper::derive_ncar_reset_sphere_local_positions(geometry.local_geometry);
        require(near(positions[0][0], -0.6F) &&
                near(positions[0][1], 0.146F) &&
                near(positions[0][2], 1.87F) &&
                near(positions[11][0], 0.5F) &&
                near(positions[11][1], 0.246F) &&
                near(positions[11][2], 1.77F),
            "constructor sphere positions must consume the precomputed bounds");

        std::array<float, 12> identity{};
        identity[0] = identity[4] = identity[8] = 1.0F;
        viper::NcarResetBackendGroupObservation group{};
        group.group_object_present = true;
        group.parent_transform_present = true;
        group.parent_transform_0x00_to_0x2c = identity;
        group.source_bounds = observation;
        const auto group_plan = viper::plan_ncar_reset_backend_group(group);
        constexpr std::array<float, 12> radii{
            0.4F, 0.4F, 0.4F, 0.4F, 0.4F, 0.4F,
            0.6F, 0.6F, 0.5F, 0.5F, 0.5F, 0.5F};
        float expected_group_radius{};
        for (std::size_t index = 0; index < positions.size(); ++index) {
            const auto& point = positions[index];
            expected_group_radius = (std::max)(expected_group_radius,
                std::sqrt(point[0] * point[0] + point[1] * point[1] +
                    point[2] * point[2]) + radii[index]);
        }
        bool all_world_positions_match =
            group_plan.child_world_positions_0x2c.size() == positions.size();
        for (std::size_t index = 0;
             all_world_positions_match && index < positions.size(); ++index) {
            for (std::size_t axis = 0; axis < 3U; ++axis) {
                all_world_positions_match = all_world_positions_match &&
                    near(group_plan.child_world_positions_0x2c[index][axis],
                        positions[index][axis]);
            }
        }
        require(!group_plan.geometry_inputs_incomplete &&
                group_plan.child_calls_attempted == 12U &&
                group_plan.constructor_child_tag_writes_0x18 == 24U &&
                all_world_positions_match &&
                near(group_plan.group_radius_0x50, expected_group_radius),
            "backend-group planning must build the twelve mapped sphere children");

        std::cout << "NCAR geometry regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
