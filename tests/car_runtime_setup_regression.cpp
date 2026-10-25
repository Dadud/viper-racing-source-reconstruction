#include "viper/world_track_objects.hpp"
#include "viper/wheel_body_coupling.hpp"
#include "viper/ncar_state.hpp"
#include "viper/model_resource.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void write_u32_le(std::vector<std::byte>& bytes, const std::size_t offset,
                  const std::uint32_t value)
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
    }
}

} // namespace

int main()
{
    try {
        constexpr std::size_t setup_size =
            viper::CarFileRuntimeMappingObservation::configuration_word_count *
            sizeof(std::uint32_t);
        std::vector<std::byte> setup(setup_size);
        std::array<std::uint32_t,
            viper::CarFileRuntimeMappingObservation::configuration_word_count> words{};
        for (std::size_t index = 0; index < words.size(); ++index) {
            words[index] = 0x3f000000U + static_cast<std::uint32_t>(index);
            write_u32_le(setup, index * sizeof(std::uint32_t), words[index]);
        }
        words[0x22U] = std::bit_cast<std::uint32_t>(1.0F);
        write_u32_le(setup, 0x22U * sizeof(std::uint32_t), words[0x22U]);

        viper::TireDefinition tire_definition{};
        tire_definition.parameters = {
            200.0F, 60.0F, 15.0F, 2.0F, 3.0F, 0.1F, 0.2F, 0.3F, 0.4F,
            2.0F, 4.0F, 4.0F, 10.0F, 2.0F, 8.0F, 5.0F, 30.0F,
            7.0F, 8.0F, 9.0F, 30.0F, 11.0F};
        const auto tire_object = viper::initialize_tire_response_object(
            tire_definition, 0.0254F);
        const auto tire_curves = viper::derive_tire_response_curves(tire_definition);
        const auto tire_terms = viper::derive_tire_trigonometric_terms(tire_definition);
        require(tire_object.words[0] == tire_curves.quadratic.quadratic &&
                tire_object.words[1] == tire_curves.quadratic.linear &&
                tire_object.words[2] == tire_curves.linear.slope &&
                tire_object.words[3] == tire_curves.linear.intercept &&
                tire_object.words[4] == 7.0F && tire_object.words[5] == 8.0F &&
                tire_object.words[6] == 9.0F &&
                tire_object.words[7] == tire_terms.coefficient_0x1c &&
                tire_object.words[8] == tire_terms.coefficient_0x20 &&
                tire_object.words[9] == tire_terms.coefficient_0x24 &&
                tire_object.words[10] == tire_terms.coefficient_0x28 &&
                tire_object.words[11] == 0.0254F &&
                tire_object.words[12] == 2.0F * 0.4545F &&
                tire_object.words[13] == 3.0F * 0.042279862F &&
                tire_object.words[14] == tire_curves.input_limit_at_0x38 &&
                tire_object.words[15] == 11.0F,
            "0x435BF0 must populate all sixteen tire-object DWORD slots");

        viper::CarFileResourcePayloads resources{};
        viper::CarFilePayload car_file{};
        car_file.resource_version = viper::CarFilePayload::supported_resource_version;
        car_file.raw_words.fill(0x3f800000U);
        car_file.raw_words[12U] = std::bit_cast<std::uint32_t>(200.0F);
        car_file.raw_words[13U] = std::bit_cast<std::uint32_t>(25.0F);
        car_file.raw_words[0x66U] = std::bit_cast<std::uint32_t>(10.0F);
        car_file.raw_words[0x67U] = std::bit_cast<std::uint32_t>(10.0F);
        // A zero setup-count discriminator selects the configured setup words.
        car_file.raw_words[0x3aU] = 0U;
        resources.car_file = car_file;

        const auto preparation =
            viper::prepare_car_file_runtime_mapping_from_setup_payload(
                resources, setup, {}, false, false);
        require(preparation.input_complete && preparation.mapping.has_value(),
            "a full 0x8c setup payload and CARF v6 must prepare a runtime mapping");
        const auto matching_write = std::find_if(
            preparation.mapping->writes.begin(), preparation.mapping->writes.end(),
            [](const viper::CarFileRuntimeFieldWrite& write) {
                return write.byte_offset == 0x94U;
            });
        require(matching_write != preparation.mapping->writes.end() &&
                matching_write->raw_word == words[0x14U],
            "setup source field 0x14 must reach the mapped vehicle field");

        std::vector<std::byte> runtime_record(
            viper::CarFileRuntimeMapping::runtime_record_size);
        require(viper::apply_car_file_runtime_mapping(
                    runtime_record, *preparation.mapping),
            "prepared CARF mapping must apply to the bounded runtime record");
        const auto body_observation =
            viper::make_ncar_body_constructor_observation_from_runtime_record(
                runtime_record, std::nullopt, false);
        require(body_observation.has_value() &&
                !body_observation->serialized_object_stream_offset.has_value(),
            "body constructor planning must preserve the unavailable serialized argument");
        const auto body_plan = viper::plan_ncar_body_constructor(*body_observation);
        require(std::fabs(body_plan.scaled_dimensions_0x1f0_to_0x1fc[0] -
                    body_observation->source_values_0x08_to_0x14[0] * 0.4545F) <
                    1.0e-6F &&
                std::fabs(body_plan.reciprocal_dimensions_0x200_to_0x20c[0] -
                    1.0F / body_plan.scaled_dimensions_0x1f0_to_0x1fc[0]) <
                    1.0e-6F &&
                std::none_of(body_plan.inherited_base_writes_0x4375c0.begin(),
                    body_plan.inherited_base_writes_0x4375c0.end(),
                    [](const viper::NcarObjectFieldWrite& write) {
                        return write.offset == 0x08U;
                    }),
            "body dimension derivation must remain usable without inventing inherited +0x08");
        const auto body_with_stream_offset =
            viper::make_ncar_body_constructor_observation_from_runtime_record(
                runtime_record, 0x12345678U, false);
        require(body_with_stream_offset.has_value(),
            "body constructor adapter must accept a supplied serialized stream offset");
        const auto offset_plan = viper::plan_ncar_body_constructor(
            *body_with_stream_offset);
        const auto offset_write = std::find_if(
            offset_plan.inherited_base_writes_0x4375c0.begin(),
            offset_plan.inherited_base_writes_0x4375c0.end(),
            [](const viper::NcarObjectFieldWrite& write) {
                return write.offset == 0x08U;
            });
        require(offset_write != offset_plan.inherited_base_writes_0x4375c0.end() &&
                offset_write->value_bits == 0x12345678U,
            "body base constructor must preserve the serialized object stream offset at +0x08");
        const std::array<float, 12> placement_transform{
            0.0F, -1.0F, 0.0F,
            1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F,
            12.5F, 3.25F, -44.0F};
        viper::NcarBodyRuntimeObjectState placed_body{};
        require(viper::copy_ncar_runtime_record_transform_to_body(
                    placed_body, placement_transform),
            "PCAR 0x433C00 transform handoff must populate body state");
        for (std::size_t index = 0; index < placement_transform.size(); ++index) {
            const auto body_word = viper::read_ncar_body_object_u32(
                placed_body, static_cast<std::uint32_t>(0x38U + index * 4U));
            require(body_word.has_value() &&
                    *body_word == std::bit_cast<std::uint32_t>(
                        placement_transform[index]),
                "0x433C00 must preserve all 12 runtime transform DWORDs in order");
        }
        require(viper::read_ncar_body_object_u32(placed_body, 0x68U) ==
                    std::nullopt && placed_body.initialized_bytes[0x68U] == 0U,
            "placement copy must stop after the three body-position DWORDs");

        viper::NcarBodyRuntimeObjectState kinematics_body{};
        std::vector<viper::NcarObjectFieldWrite> kinematics_writes;
        const auto add_float = [&kinematics_writes](
            const std::uint32_t offset, const float value) {
            kinematics_writes.push_back({offset, 4U,
                std::bit_cast<std::uint32_t>(value)});
        };
        const auto add_vector = [&add_float](
            const std::uint32_t offset, const std::array<float, 3>& values) {
            for (std::size_t axis = 0; axis < values.size(); ++axis) {
                add_float(offset + static_cast<std::uint32_t>(axis * 4U), values[axis]);
            }
        };
        const auto add_matrix = [&add_float](
            const std::uint32_t offset, const std::array<float, 9>& values) {
            for (std::size_t index = 0; index < values.size(); ++index) {
                add_float(offset + static_cast<std::uint32_t>(index * 4U), values[index]);
            }
        };
        const std::array<float, 9> orientation_input{
            1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
        const std::array<float, 9> angular_matrix_input{
            2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F, 10.0F};
        const std::array<float, 9> body_matrix_input{
            11.0F, 12.0F, 13.0F, 14.0F, 15.0F, 16.0F, 17.0F, 18.0F, 19.0F};
        add_matrix(0x038U, orientation_input);
        add_vector(0x05cU, {21.0F, 22.0F, 23.0F});
        add_float(0x200U, 0.125F);
        add_matrix(0x210U, body_matrix_input);
        add_vector(0x234U, {31.0F, 32.0F, 33.0F});
        add_vector(0x240U, {41.0F, 42.0F, 43.0F});
        add_vector(0x24cU, {51.0F, 52.0F, 53.0F});
        add_vector(0x258U, {61.0F, 62.0F, 63.0F});
        add_matrix(0x264U, angular_matrix_input);
        kinematics_writes.push_back({0x10U, 2U, 0x12ffU});
        kinematics_writes.push_back({0x12U, 2U, 0xabcdU});
        require(viper::apply_ncar_body_object_writes(
                    kinematics_body, kinematics_writes),
            "the test body must receive only explicitly initialized integrator fields");
        const auto kinematics_input =
            viper::read_ncar_body_integration_kinematics_input(kinematics_body);
        require(kinematics_input.has_value() &&
                kinematics_input->position.x == 21.0F &&
                kinematics_input->linear_velocity.z == 33.0F &&
                kinematics_input->accumulated_linear_force.y == 52.0F &&
                kinematics_input->inverse_mass == 0.125F &&
                kinematics_input->angular_velocity.x == 41.0F &&
                kinematics_input->accumulated_angular_moment.z == 63.0F &&
                kinematics_input->orientation_matrix_0x0e == orientation_input &&
                kinematics_input->angular_transform_matrix_0x99 == angular_matrix_input &&
                kinematics_input->body_matrix_0x210 == body_matrix_input,
            "body kinematics adapter must read the exact 0x43ECE0 offsets");
        auto incomplete_kinematics_body = kinematics_body;
        incomplete_kinematics_body.initialized_bytes[0x210U + 8U * 4U] = 0U;
        require(!viper::read_ncar_body_integration_kinematics_input(
                    incomplete_kinematics_body).has_value(),
            "kinematics input must stay unavailable when any consumed matrix byte is unknown");

        viper::BodyIntegrationKinematicsResult kinematics_result{};
        kinematics_result.linear.position = {101.0F, 102.0F, 103.0F};
        kinematics_result.linear.velocity = {111.0F, 112.0F, 113.0F};
        kinematics_result.angular.angular_velocity = {121.0F, 122.0F, 123.0F};
        kinematics_result.orientation.orientation_matrix = {
            1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F, 9.0F};
        kinematics_result.basis.basis_matrix = {
            9.0F, 8.0F, 7.0F, 6.0F, 5.0F, 4.0F, 3.0F, 2.0F, 1.0F};
        require(viper::apply_ncar_body_integration_kinematics_result(
                    kinematics_body, kinematics_result),
            "body integration result must be writable to the persistent PCAR owner");
        require(viper::read_ncar_body_object_u32(kinematics_body, 0x05cU) ==
                    std::bit_cast<std::uint32_t>(101.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x234U + 8U) ==
                    std::bit_cast<std::uint32_t>(113.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x240U + 4U) ==
                    std::bit_cast<std::uint32_t>(122.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x264U) ==
                    std::bit_cast<std::uint32_t>(9.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x210U) ==
                    std::bit_cast<std::uint32_t>(11.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x24cU) ==
                    std::bit_cast<std::uint32_t>(0.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x258U + 8U) ==
                    std::bit_cast<std::uint32_t>(0.0F) &&
                viper::read_ncar_body_object_u32(kinematics_body, 0x10U) ==
                    0xabcd1300U,
            "body kinematics apply must store pose, matrices, resets, and wrapped 16-bit counter");
        auto missing_counter_body = kinematics_body;
        missing_counter_body.initialized_bytes[0x11U] = 0U;
        require(!viper::apply_ncar_body_integration_kinematics_result(
                    missing_counter_body, kinematics_result) &&
                viper::read_ncar_body_object_u32(missing_counter_body, 0x05cU) ==
                    std::bit_cast<std::uint32_t>(101.0F),
            "an unavailable native counter must reject result application atomically");
        const auto body_shift_write = std::find_if(
            preparation.mapping->writes.begin(), preparation.mapping->writes.end(),
            [](const viper::CarFileRuntimeFieldWrite& write) {
                return write.byte_offset == 0x128U;
            });
        require(body_shift_write != preparation.mapping->writes.end() &&
                body_shift_write->raw_word == car_file.raw_words[0x66U],
            "CARF v6 word 0x66 must map directly to runtime record +0x128");
        const auto mapped_body_shift = [&runtime_record] {
            std::uint32_t raw{};
            for (std::size_t byte_index = 0; byte_index < sizeof(raw); ++byte_index) {
                raw |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                    runtime_record[0x128U + byte_index])) << (byte_index * 8U);
            }
            return std::bit_cast<float>(raw);
        }();
        require(mapped_body_shift == 10.0F,
            "mapped runtime record must retain the native +0x128 body-shift scalar");
        viper::ModelVertexBounds model_bounds{};
        model_bounds.minimum = {-1.0F, -1.0F, -1.0F};
        model_bounds.maximum = {1.0F, 1.0F, 3.0F};
        model_bounds.vertex_count = 2U;
        viper::NcarResetSphereBoundsObservation sphere_observation{};
        sphere_observation.extents_pointer_present = true;
        sphere_observation.precomputed_model_bounds = model_bounds;
        sphere_observation.car_record_0x58 = 200.0F;
        sphere_observation.car_record_0x5c = 25.0F;
        sphere_observation.car_record_0x128 = mapped_body_shift;
        const auto sphere_geometry =
            viper::derive_ncar_reset_sphere_geometry(sphere_observation);
        require(sphere_geometry.inputs_complete &&
                std::fabs(sphere_geometry.local_geometry.local_b0_after_body_shift -
                    (-1.0F - 10.0F * 0.0254F)) < 1.0e-5F &&
                std::fabs(sphere_geometry.local_geometry.local_bc_after_clamp - 1.0F) <
                    1.0e-5F,
            "the CARF-derived runtime +0x128 value must reach native proxy bounds");
        const auto callback_setup =
            viper::initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
                runtime_record);
        require(callback_setup.has_value() &&
                std::fabs(callback_setup->value_0xd84 - 3.81F) < 1.0e-5F &&
                std::fabs(callback_setup->value_0xd90 + 1.27F) < 1.0e-5F &&
                std::fabs(callback_setup->angle_0xdcc - 0.174F) < 1.0e-5F,
            "mapped CARF width/ratio/value must reach the exact constructor callback fields");

        std::vector<std::byte> anchor_record(0x148U);
        write_u32_le(anchor_record, 0x58U, std::bit_cast<std::uint32_t>(200.0F));
        write_u32_le(anchor_record, 0x5cU, std::bit_cast<std::uint32_t>(25.0F));
        write_u32_le(anchor_record, 0x128U, std::bit_cast<std::uint32_t>(10.0F));
        std::array<viper::ModelVertex, 5> anchor_vertices{};
        const std::array<std::array<float, 3>, 4> expected_candidates{{
            {-0.4F, -0.254F, 3.81F},
            {0.4F, -0.254F, 3.81F},
            {-0.4F, -0.254F, -1.27F},
            {0.4F, -0.254F, -1.27F}}};
        for (std::size_t vertex = 0; vertex < expected_candidates.size(); ++vertex) {
            std::copy(expected_candidates[vertex].begin(),
                expected_candidates[vertex].end(),
                anchor_vertices[vertex].fields.begin());
        }
        std::copy(expected_candidates[0].begin(), expected_candidates[0].end(),
            anchor_vertices[4].fields.begin());
        const auto anchors = viper::select_ncar_wheel_vertex_anchors_from_runtime_record(
            anchor_record, anchor_vertices);
        require(anchors.has_value(),
            "complete mapped runtime fields must permit mesh-derived wheel anchors");
        for (std::size_t wheel = 0; wheel < expected_candidates.size(); ++wheel) {
            require(anchors->candidate_positions[wheel] == expected_candidates[wheel] &&
                    anchors->vertex_indices[wheel] == wheel &&
                    anchors->nearest_distances[wheel] == 0.0F,
                "wheel anchor formulas and nearest FNIM vertices must match constructor order");
        }
        require(anchors->vertex_indices[0] == 0U,
            "strict nearest-distance comparison must keep the first tied model vertex");
        const auto four_wheels = viper::initialize_ncar_four_wheel_core({
            anchor_record, anchor_vertices, 0.5F, false});
        require(four_wheels.input_complete &&
                four_wheels.anchors.vertex_indices == anchors->vertex_indices &&
                std::all_of(four_wheels.wheels.begin(), four_wheels.wheels.end(),
                    [](const auto& wheel) { return wheel.has_value(); }) &&
                four_wheels.wheels[0]->selected_model_vertex == expected_candidates[0] &&
                four_wheels.wheels[3]->selected_model_vertex == expected_candidates[3],
            "four-wheel orchestration must feed each nearest FNIM anchor to its matching constructor");
        const auto empty_mesh_anchors = viper::select_ncar_wheel_vertex_anchors_from_runtime_record(
            anchor_record, std::span<const viper::ModelVertex>{});
        require(empty_mesh_anchors.has_value() &&
                std::all_of(empty_mesh_anchors->vertex_indices.begin(),
                    empty_mesh_anchors->vertex_indices.end(),
                    [](const auto& index) { return !index.has_value(); }),
            "empty FNIM vertices must leave all wheel anchor selections unset");
        require(!viper::select_ncar_wheel_vertex_anchors_from_runtime_record(
                    std::span<const std::byte>(anchor_record).first(0x12bU),
                    anchor_vertices),
            "runtime records missing bytes of +0x128 must be rejected");
        const auto no_anchor_batch = viper::initialize_ncar_four_wheel_core({
            anchor_record, std::span<const viper::ModelVertex>{}, 0.5F, false});
        require(!no_anchor_batch.input_complete &&
                std::none_of(no_anchor_batch.wheels.begin(), no_anchor_batch.wheels.end(),
                    [](const auto& wheel) { return wheel.has_value(); }),
            "four-wheel orchestration must remain incomplete when FNIM anchors are unavailable");

        std::vector<std::byte> wheel_record(0x148U);
        const auto write_float = [&wheel_record](const std::size_t offset,
                                                  const float value) {
            write_u32_le(wheel_record, offset, std::bit_cast<std::uint32_t>(value));
        };
        write_float(0x50U, 5.0F);
        write_float(0x54U, 6.0F);
        write_float(0x58U, 200.0F);
        write_float(0x5cU, 25.0F);
        write_float(0x60U, 8.0F);
        write_float(0x64U, 9.0F);
        write_float(0xccU, 0.1F);
        write_float(0xd0U, 0.2F);
        write_float(0xd4U, 0.01F);
        write_float(0xd8U, 0.02F);
        write_float(0xdcU, 0.03F);
        write_float(0xe0U, 0.04F);
        write_float(0xecU, 10.0F);
        write_float(0xf0U, 20.0F);
        write_float(0xf4U, 30.0F);
        write_float(0xf8U, 40.0F);
        write_float(0x100U, 0.3F);
        write_float(0x104U, 0.4F);
        write_float(0x128U, 10.0F);
        write_float(0x130U, 5.0F);
        write_float(0x134U, 7.0F);
        write_float(0x138U, 2.0F);
        write_float(0x13cU, 6.0F);
        write_float(0x140U, 8.0F);
        write_float(0x144U, 4.0F);
        const auto expected_surface_coefficients = [](const float track_scale) {
            const auto helper = [track_scale](const float stack_value_0x18) {
                return (static_cast<long double>(stack_value_0x18) +
                    static_cast<long double>(4.44444465637207F)) *
                    static_cast<long double>(track_scale) *
                    static_cast<long double>(1.2499999968440534e-7F);
            };
            const auto first = helper(26.66666603088379F);
            const auto second = helper(71.11111450195312F);
            return std::array<float, 2>{
                static_cast<float>((first * 71.11111450195312L -
                    26.66666603088379L * second) * -1.1865233318530954e-5L),
                static_cast<float>((second * 711.111083984375L -
                    first * 5056.79052734375L) * -1.1865233318530954e-5L)};
        };
        const auto front_tire_setup = viper::initialize_ncar_wheel_tire_setup(
            wheel_record, 0U, tire_definition, false);
        const auto rear_tire_setup = viper::initialize_ncar_wheel_tire_setup(
            wheel_record, 2U, tire_definition, false);
        require(front_tire_setup.has_value() && rear_tire_setup.has_value() &&
                std::fabs(front_tire_setup->loader_dimension_argument - 5.0F) < 1.0e-5F &&
                std::fabs(rear_tire_setup->loader_dimension_argument - 6.0F) < 1.0e-5F &&
                std::fabs(front_tire_setup->tire_response_object_0x2c - 0.025F) < 1.0e-6F &&
                std::fabs(rear_tire_setup->tire_response_object_0x2c - 0.03F) < 1.0e-6F &&
                front_tire_setup->tire_response_object_0x2c ==
                    front_tire_setup->response_object.words[11] &&
                rear_tire_setup->tire_response_object_0x2c ==
                    rear_tire_setup->response_object.words[11] &&
                !viper::initialize_ncar_wheel_tire_setup(
                    std::span<const std::byte>(wheel_record).first(0x13fU),
                    2U, tire_definition, false) &&
                !viper::initialize_ncar_wheel_tire_setup(
                    wheel_record, 4U, tire_definition, false),
            "wheel tire setup must select axle-specific scale and initialize TIRE slot +0x2C");
        const std::array<float, 3> front_anchor{-0.4F, -0.254F, 3.81F};
        const auto front_wheel = viper::initialize_ncar_wheel_core({
            wheel_record, 0U, front_anchor, 0.5F, false});
        const float front_base_height = 8.0F * 0.0254F;
        const float front_track_scale = 0.1F * 175.19684F;
        const auto front_surface_coefficients =
            expected_surface_coefficients(front_track_scale);
        const float front_height_correction = static_cast<float>(
            (static_cast<long double>(0.5F * 0.25F * 4.905F) -
             static_cast<long double>(front_base_height * 2.0F) * 0.5L *
                front_track_scale) / front_track_scale);
        require(front_wheel.has_value() && front_wheel->front_axle &&
                front_wheel->side_sign == -1.0F &&
                front_wheel->selected_model_vertex == front_anchor &&
                std::fabs(front_wheel->value_0x5c - 0.02575F) < 1.0e-6F &&
                std::fabs(front_wheel->value_0x44 -
                    (front_base_height * 2.0F + front_height_correction)) < 1.0e-6F &&
                std::fabs(front_wheel->value_0x3c - front_track_scale) < 1.0e-6F &&
                std::fabs(front_wheel->angle_0x60 - 0.17453292F) < 1.0e-6F &&
                std::fabs(front_wheel->angle_0x64 - 0.52359876F) < 1.0e-6F &&
                std::fabs(front_wheel->candidate_position[0] + 0.0635F) < 1.0e-6F &&
                std::fabs(front_wheel->candidate_position[2] - 3.81F) < 1.0e-6F &&
                std::fabs(front_wheel->position_relative_to_selected_vertex[0] -
                    0.3365F) < 1.0e-6F &&
                front_wheel->value_0x40 == 0.0F &&
                front_wheel->value_0x4c == front_track_scale &&
                front_wheel->coefficient_0x9c == front_surface_coefficients[0] &&
                front_wheel->coefficient_0xa0 == front_surface_coefficients[1] &&
                std::fabs(front_wheel->suspension_piecewise_terms.positive_slope -
                    ((0.01F / 0.0254F * 2.67F * 0.13333334F -
                      0.01F / 0.0254F * 4.45F) * -1.1538461F)) < 1.0e-4F &&
                std::fabs(front_wheel->suspension_piecewise_terms.nonpositive_slope -
                    ((0.03F / 0.0254F * 4.45F * 0.13333334F -
                      0.03F / 0.0254F * 4.45F) * -1.1538461F)) < 1.0e-4F,
            "0x441C10 front-wheel position, geometry, and steering stores must be reconstructed");
        viper::NcarWheelRuntimeObjectState materialized_front_wheel{};
        const auto front_wheel_base = 0x554U;
        const auto front_wheel_constructor = viper::plan_ncar_wheel_constructor(
            front_wheel_base, viper::NcarWheelConstructorObservation{});
        require(front_wheel.has_value() &&
                viper::materialize_ncar_wheel_constructor(materialized_front_wheel,
                    front_wheel_base, front_wheel_constructor) &&
                viper::apply_ncar_wheel_core_initialization(materialized_front_wheel,
                    front_wheel_base, *front_wheel),
            "resolved wheel-core constructor outputs must apply to persistent storage");
        const auto read_front_wheel_u32 = [&materialized_front_wheel](
                const std::size_t offset) {
            std::uint32_t bits{};
            for (std::size_t byte = 0; byte < 4U; ++byte) {
                bits |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                    materialized_front_wheel.bytes[offset + byte])) << (byte * 8U);
            }
            return bits;
        };
        require(read_front_wheel_u32(0x03cU) == std::bit_cast<std::uint32_t>(
                    front_wheel->value_0x3c) &&
                read_front_wheel_u32(0x044U) == std::bit_cast<std::uint32_t>(
                    front_wheel->value_0x44) &&
                read_front_wheel_u32(0x024U) == std::bit_cast<std::uint32_t>(
                    front_wheel->suspension_piecewise_terms.positive_slope) &&
                read_front_wheel_u32(0x09cU) == std::bit_cast<std::uint32_t>(
                    front_wheel->coefficient_0x9c) &&
                materialized_front_wheel.initialized_bytes[0x03cU] != 0U &&
                materialized_front_wheel.initialized_bytes[0x0a0U + 3U] != 0U,
            "live wheel bytes must retain exact constructor geometry and spring fields");
        const std::array<float, 12> projection_transform{
            0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
            1.0F, 3.0F, 2.0F, 0.0F, 0.0F, 0.0F};
        require(viper::project_body_point_velocity_0x442530(
                    {10.0F, 20.0F, 30.0F}, projection_transform) == 140.0F,
            "wheel surface projection must preserve native +0x50/+0x58/+0x54 operand order");
        viper::FourWheelBodyStepInput body_step_input{};
        body_step_input.wheel_contact_pipeline_enabled = true;
        body_step_input.integrator_prefix_input.state_0x4fc = 1U;
        body_step_input.kinematics.linear_velocity = {1.0F, 2.0F, 3.0F};
        body_step_input.kinematics.angular_velocity = {0.0F, 0.0F, 2.0F};
        body_step_input.wheel_contact_states[0].contact_point_0x130 = {4.0F, 5.0F, 0.0F};
        auto& body_step_wheel = body_step_input.wheel_update_pipeline_inputs[0];
        body_step_wheel.point_transform.body_transform_0x38_to_0x64 =
            projection_transform;
        body_step_wheel.suspension.base.spring_scale_0x3c = 1.0F;
        const auto body_step = viper::advance_four_wheel_body_step(body_step_input);
        require(body_step.wheel_point_velocities_0x43e9b0[0].x == -9.0F &&
                body_step.wheel_point_velocities_0x43e9b0[0].y == 10.0F &&
                body_step.wheel_point_velocities_0x43e9b0[0].z == 3.0F,
            "four-wheel step must calculate point velocity from prior contact state");
        require(body_step.wheel_contact_pipeline_results[0]
                    .suspension.surface.absolute_motion_projection == 20.0F,
            "four-wheel step must project point velocity through the body fields");
        const auto polarity_seeds =
            viper::make_ncar_surface_polarity_seed_pair_0x4193c0(
                30001U, static_cast<std::uint32_t>(-29999));
        require(polarity_seeds.stack_argument_0_second_sample_remainder == -29999 &&
                polarity_seeds.stack_argument_1_first_sample_remainder == 1,
            "polarity seed wrapper must use signed low-dword remainders and native push order");
        const auto polarity_table =
            viper::generate_ncar_surface_polarity_table_0x419430(0, 0);
        const auto repeated_polarity_table =
            viper::generate_ncar_surface_polarity_table_0x419430(0, 0);
        require(polarity_table == repeated_polarity_table &&
                std::all_of(polarity_table.begin(), polarity_table.end(),
                    [](const float value) {
                        return value >= 0.0F && value < 1.0F;
                    }) &&
                polarity_table !=
                    viper::generate_ncar_surface_polarity_table_0x419430(1, 0),
            "polarity table recurrence must be deterministic, bounded, and seed-sensitive");
        viper::NcarSurfacePolarityState initialized_polarity{};
        viper::initialize_ncar_surface_polarity_state_0x419430(
            initialized_polarity, 0, 0);
        require(initialized_polarity.initialized &&
                initialized_polarity.table_0x4f0910 == polarity_table &&
                initialized_polarity.index_a_0x4f0aa4 == 0x60U &&
                initialized_polarity.index_b_0x4f0aa8 == 0x20U &&
                std::bit_cast<std::uint32_t>(initialized_polarity.phase_0x4f0908) ==
                    0x3cb0f880U &&
                std::bit_cast<std::uint32_t>(initialized_polarity.increment_0x4f0a94) ==
                    0x3ee99762U &&
                std::bit_cast<std::uint32_t>(initialized_polarity.wrap_0x4f0a98) ==
                    0x3f7ffffdU,
            "polarity initializer must populate all recovered table and phase state");
        const auto minimum_timer_seed =
            viper::make_ncar_surface_polarity_seed_pair_0x4193c0(
                0x80000000U, 0xffffffffU);
        require(minimum_timer_seed.stack_argument_1_first_sample_remainder == -23648 &&
                minimum_timer_seed.stack_argument_0_second_sample_remainder == -1,
            "polarity seed wrapper must sign-interpret low dwords and preserve IDIV remainder sign");
        viper::NcarSurfacePolarityState polarity_state{};
        polarity_state.initialized = true;
        polarity_state.index_a_0x4f0aa4 = 0x60U;
        polarity_state.index_b_0x4f0aa8 = 0x20U;
        polarity_state.phase_0x4f0908 = 0.02F;
        polarity_state.increment_0x4f0a94 = 0.1F;
        polarity_state.wrap_0x4f0a98 = 0.999F;
        polarity_state.table_0x4f0910[0x60U] = 0.1F;
        polarity_state.table_0x4f0910[0x20U] = 0.8F;
        const auto polarity_value = viper::advance_ncar_surface_polarity_value_0x4193f0(
            polarity_state, 2);
        require(polarity_value == 1 &&
                polarity_state.index_a_0x4f0aa4 == 0x5fU &&
                polarity_state.index_b_0x4f0aa8 == 0x1fU &&
                std::abs(polarity_state.table_0x4f0910[0x60U] - 0.3F) < 1.0e-6F &&
                std::abs(polarity_state.phase_0x4f0908 - 0.919F) < 1.0e-6F,
            "native surface phase must update table, indices, accumulator, and scaled integer result");
        polarity_state.index_a_0x4f0aa4 = 0U;
        polarity_state.index_b_0x4f0aa8 = 0U;
        const auto wrapped_phase = viper::advance_ncar_surface_polarity_phase_0x419620(
            polarity_state);
        require(wrapped_phase.has_value() && *wrapped_phase > 0.0F &&
                *wrapped_phase < 1.0F &&
                polarity_state.index_a_0x4f0aa4 == 0x60U &&
                polarity_state.index_b_0x4f0aa8 == 0x60U,
            "native surface phase indices must wrap from zero back to table entry 0x60");
        viper::NcarSurfacePolarityState uninitialized_polarity{};
        require(!viper::advance_ncar_surface_polarity_phase_0x419620(
                    uninitialized_polarity).has_value(),
            "surface phase update must reject missing random table initialization");
        const auto right_front_wheel = viper::initialize_ncar_wheel_core({
            wheel_record, 1U, {0.4F, -0.254F, 3.81F}, 0.5F, false});
        require(right_front_wheel.has_value() && right_front_wheel->front_axle &&
                right_front_wheel->side_sign == 1.0F &&
                std::fabs(right_front_wheel->angle_0x60 + 0.17453292F) < 1.0e-6F &&
                std::fabs(right_front_wheel->candidate_position[0] - 0.0635F) < 1.0e-6F,
            "right-front wheel must invert steering sign and use the positive lateral offset");
        const auto rear_wheel = viper::initialize_ncar_wheel_core({
            wheel_record, 2U, {-0.4F, -0.254F, -1.27F}, 0.5F, false});
        const float rear_base_height = 9.0F * 0.0254F;
        const float rear_track_scale = 0.2F * 175.19684F;
        const auto rear_surface_coefficients =
            expected_surface_coefficients(rear_track_scale);
        const float rear_height_correction = static_cast<float>(
            (static_cast<long double>(0.5F * 0.75F * 4.905F) -
             static_cast<long double>(rear_base_height * 2.0F) * 0.5L *
                rear_track_scale) / rear_track_scale);
        require(rear_wheel.has_value() && !rear_wheel->front_axle &&
                rear_wheel->side_sign == -1.0F &&
                std::fabs(rear_wheel->value_0x5c - 0.05128F) < 1.0e-6F &&
                std::fabs(rear_wheel->value_0x44 -
                    (rear_base_height * 2.0F + rear_height_correction)) < 1.0e-6F &&
                std::fabs(rear_wheel->angle_0x60 - 0.34906584F) < 1.0e-6F &&
                std::fabs(rear_wheel->angle_0x64 - 0.69813168F) < 1.0e-6F &&
                std::fabs(rear_wheel->candidate_position[2] + 1.27F) < 1.0e-6F &&
                rear_wheel->coefficient_0x9c == rear_surface_coefficients[0] &&
                rear_wheel->coefficient_0xa0 == rear_surface_coefficients[1] &&
                std::fabs(rear_wheel->suspension_piecewise_terms.positive_slope -
                    ((0.02F / 0.0254F * 2.67F * 0.13333334F -
                      0.02F / 0.0254F * 4.45F) * -1.1538461F)) < 1.0e-4F &&
                std::fabs(rear_wheel->suspension_piecewise_terms.nonpositive_slope -
                    ((0.04F / 0.0254F * 4.45F * 0.13333334F -
                      0.04F / 0.0254F * 4.45F) * -1.1538461F)) < 1.0e-4F,
            "0x441C10 rear-wheel setup must select the rear axle and handedness fields");
        const auto right_rear_wheel = viper::initialize_ncar_wheel_core({
            wheel_record, 3U, {0.4F, -0.254F, -1.27F}, 0.5F, false});
        require(right_rear_wheel.has_value() && !right_rear_wheel->front_axle &&
                right_rear_wheel->side_sign == 1.0F &&
                std::fabs(right_rear_wheel->angle_0x60 + 0.34906584F) < 1.0e-6F &&
                std::fabs(right_rear_wheel->candidate_position[0] - 0.0762F) < 1.0e-6F,
            "right-rear wheel must select rear steering fields and positive lateral offset");
        require(!viper::initialize_ncar_wheel_core({
                    std::span<const std::byte>(wheel_record).first(0x147U),
                    0U, front_anchor, 0.5F, false}) &&
                !viper::initialize_ncar_wheel_core({
                    wheel_record, 4U, front_anchor, 0.5F, false}),
            "wheel core initialization must reject short records and invalid indices");
        require(!viper::initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
                    std::span<const std::byte>(runtime_record).first(0x12fU)),
            "runtime records missing bytes of +0x12C must be rejected");

        for (std::uint32_t wheel = 0U; wheel < 4U; ++wheel) {
            const auto wheel_base = 0x554U + 0x1a4U * wheel;
            const auto wheel_plan = viper::plan_ncar_wheel_constructor(
                wheel_base, viper::NcarWheelConstructorObservation{});
            const auto has_write = [&wheel_plan](const std::uint32_t offset,
                                                  const std::uint32_t bits) {
                return std::any_of(wheel_plan.writes.begin(), wheel_plan.writes.end(),
                    [offset, bits](const viper::NcarObjectFieldWrite& write) {
                        return write.offset == offset && write.value_bits == bits;
                    });
            };
            require(wheel_plan.unknown_fill_offsets.empty() &&
                    has_write(wheel_base + 0x18cU, 0x44e10000U) &&
                    has_write(wheel_base + 0x194U, 0x43910000U) &&
                    has_write(wheel_base + 0x150U, 0x3f800000U),
                "each persistent player wheel slot must retain its native constructor defaults");
            viper::NcarWheelRuntimeObjectState wheel_object{};
            require(viper::materialize_ncar_wheel_constructor(
                        wheel_object, wheel_base, wheel_plan),
                "fully resolved ordinary wheel constructor writes must materialize");
            const auto read_u32 = [&wheel_object](const std::size_t offset) {
                std::uint32_t bits{};
                for (std::size_t byte = 0; byte < 4U; ++byte) {
                    bits |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                        wheel_object.bytes[offset + byte])) << (byte * 8U);
                }
                return bits;
            };
            require(read_u32(0x18cU) == 0x44e10000U &&
                    read_u32(0x194U) == 0x43910000U &&
                    wheel_object.initialized_bytes[0x18cU] != 0U &&
                    wheel_object.initialized_bytes[0x1a3U] == 0U,
                "wheel storage must retain constructor bytes and distinguish unwritten bytes");
        }
        viper::NcarWheelRuntimeObjectState untouched_wheel{};
        const auto unresolved_wheel_plan = viper::plan_ncar_wheel_constructor(
            0x554U, {true, std::nullopt});
        require(!viper::materialize_ncar_wheel_constructor(
                    untouched_wheel, 0x554U, unresolved_wheel_plan) &&
                std::none_of(untouched_wheel.initialized_bytes.begin(),
                    untouched_wheel.initialized_bytes.end(),
                    [](const std::uint8_t value) { return value != 0U; }),
            "unresolved compatibility fills must not partially materialize wheel state");

        setup.pop_back();
        const auto short_preparation =
            viper::prepare_car_file_runtime_mapping_from_setup_payload(
                resources, setup, {}, false, false);
        require(!short_preparation.input_complete && !short_preparation.mapping,
            "a short CCS/CSU setup buffer must not produce a partial mapping");

        std::cout << "car runtime setup regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
