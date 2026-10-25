#include "viper/model_resource.hpp"
#include "viper/ncar_state.hpp"

#include <algorithm>
#include <array>
#include <bit>
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

        viper::NcarBodyConstructorObservation body_constructor_observation{};
        body_constructor_observation.source_values_0x08_to_0x14 =
            {4.0F, 0.5F, 0.5F, 0.5F};
        body_constructor_observation.source_record_word_0x00 = 0x50434152U;
        body_constructor_observation.serialized_object_stream_offset = 0xAD4U;
        const auto body_constructor_plan =
            viper::plan_ncar_body_constructor(body_constructor_observation);
        viper::NcarBodyRuntimeObjectState body_runtime_state{};
        require(viper::materialize_ncar_body_constructor(
                    body_runtime_state, body_constructor_plan),
            "shared body constructor writes must materialize into persistent byte state");
        const auto read_body_dword = [&](const std::size_t offset) {
            std::uint32_t value{};
            for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
                value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                    body_runtime_state.bytes[offset + byte])) << (byte * 8U);
            }
            return value;
        };
        require(read_body_dword(0x00U) == 0x004c85e0U &&
                read_body_dword(0x08U) == 0xAD4U &&
                read_body_dword(0x0cU) == 0x50434152U &&
                read_body_dword(0x38U) == std::bit_cast<std::uint32_t>(1.0F) &&
                read_body_dword(0x58U) == std::bit_cast<std::uint32_t>(1.0F) &&
                read_body_dword(0x5cU) == 0U && read_body_dword(0x234U) == 0U &&
                near(std::bit_cast<float>(read_body_dword(0x1f0U)), 1.818F) &&
                body_runtime_state.initialized_bytes[0x234U] != 0U &&
                body_runtime_state.initialized_bytes[0x50cU] == 0U,
            "body constructor state must preserve initialized pose/velocity while leaving runtime-only fields unknown");
        for (std::size_t index = 0U; index < 9U; ++index) {
            const auto expected = std::bit_cast<std::uint32_t>(
                *body_constructor_plan.matrix_0x210[index]);
            require(viper::read_ncar_body_object_u32(body_runtime_state,
                        static_cast<std::uint32_t>(0x210U + index * 4U)) == expected &&
                    viper::read_ncar_body_object_u32(body_runtime_state,
                        static_cast<std::uint32_t>(0x264U + index * 4U)) == expected,
                "body constructor must persist all nine lanes of both derived matrices");
        }
        require(viper::read_ncar_body_object_u32(body_runtime_state, 0x234U) == 0U &&
                !viper::read_ncar_body_object_u32(body_runtime_state, 0x50cU),
            "body reads must expose initialized velocity but keep unwritten derived state unknown");

        // The inherited ACAR constructor enters this same body base path,
        // copies record +0x18..+0x44 through 0x433C00, and stores record
        // ordinal +0x194 at body +0x510. Keep that supported subset distinct
        // from full ACAR object/callback construction.
        std::array<std::byte, 0x1e4U> acar_runtime_record{};
        const auto write_record_u32 = [&](const std::size_t offset,
                                          const std::uint32_t value) {
            for (std::size_t byte = 0U; byte < sizeof(value); ++byte) {
                acar_runtime_record[offset + byte] = static_cast<std::byte>(
                    (value >> (byte * 8U)) & 0xffU);
            }
        };
        write_record_u32(0x00U, 0x41434152U);
        write_record_u32(0x04U, 0x1e4U);
        for (const auto [offset, value] : std::array<std::pair<std::size_t, float>, 4>{{
                 {0x08U, 4.0F}, {0x0cU, 0.5F}, {0x10U, 0.5F}, {0x14U, 0.5F}}}) {
            write_record_u32(offset, std::bit_cast<std::uint32_t>(value));
        }
        std::array<float, 12> acar_transform{};
        for (std::size_t lane = 0U; lane < acar_transform.size(); ++lane) {
            acar_transform[lane] = static_cast<float>(lane + 1U);
            write_record_u32(0x18U + lane * sizeof(float),
                std::bit_cast<std::uint32_t>(acar_transform[lane]));
        }
        write_record_u32(0x194U, 3U);
        const auto acar_body_observation =
            viper::make_ncar_body_constructor_observation_from_runtime_record(
                acar_runtime_record, 0x1200U, false);
        require(acar_body_observation.has_value(),
            "complete ACAR runtime records must expose shared body constructor inputs");
        viper::NcarBodyRuntimeObjectState acar_body_state{};
        const auto acar_body_plan =
            viper::plan_ncar_body_constructor(*acar_body_observation);
        const auto acar_common_defaults =
            viper::plan_ncar_constructor_subobject_defaults(3U);
        const std::array<viper::NcarObjectFieldWrite, 1> acar_vtable{{
            {0x00U, 4U, 0x004c7b78U}}};
        require(viper::materialize_ncar_body_constructor(
                    acar_body_state, acar_body_plan) &&
                viper::apply_ncar_body_object_writes(acar_body_state,
                    acar_vtable) &&
                viper::apply_ncar_body_object_writes(acar_body_state,
                    acar_common_defaults.embedded_curve_selector_writes) &&
                viper::apply_ncar_body_object_writes(acar_body_state,
                    acar_common_defaults.source_record_identity_writes) &&
                viper::copy_ncar_runtime_record_transform_to_body(
                    acar_body_state, acar_transform),
            "ACAR records must materialize their inherited shared body constructor subset");
        require(viper::read_ncar_body_object_u32(acar_body_state, 0x00U) ==
                    0x004c7b78U &&
                viper::read_ncar_body_object_u32(acar_body_state, 0x510U) == 3U &&
                viper::read_ncar_body_object_u32(acar_body_state, 0x38U) ==
                    std::bit_cast<std::uint32_t>(acar_transform[0]) &&
                viper::read_ncar_body_object_u32(acar_body_state, 0x5cU) ==
                    std::bit_cast<std::uint32_t>(acar_transform[9]) &&
                viper::read_ncar_body_object_u32(acar_body_state, 0x64U) ==
                    std::bit_cast<std::uint32_t>(acar_transform[11]),
            "ACAR body pose and participant ordinal must come from its serialized runtime record");

        const std::array<viper::NcarObjectFieldWrite, 1> pcar_helper_byte{{
            {0xd74U, 1U, 1U}}};
        require(viper::apply_ncar_body_object_writes(
                    body_runtime_state, pcar_helper_byte) &&
                viper::read_ncar_body_object_u8(body_runtime_state, 0xd74U) == 1U,
            "PCAR constructor helper result must persist in the byte-backed body at +0xD74");
        const auto constructor_subobjects =
            viper::plan_ncar_constructor_subobject_defaults();
        require(constructor_subobjects.constructor_tail_source_unknown &&
                constructor_subobjects.source_record_identity_writes.empty() &&
                !constructor_subobjects.embedded_curve_selector_writes.empty() &&
                viper::apply_ncar_body_object_writes(body_runtime_state,
                    constructor_subobjects.embedded_curve_selector_writes) &&
                read_body_dword(0xbfcU) == 0x004c8780U &&
                read_body_dword(0xc98U) == 0x004c8778U &&
                read_body_dword(0xcb0U) == 0U &&
                !viper::read_ncar_body_object_u32(body_runtime_state, 0xc00U) &&
                !viper::read_ncar_body_object_u32(body_runtime_state, 0xc9cU) &&
                !viper::read_ncar_body_object_u32(body_runtime_state, 0xcc4U),
            "known curve/selector defaults must preserve native links and leave the runtime table unknown");
        const auto constructor_subobjects_with_identity =
            viper::plan_ncar_constructor_subobject_defaults(7U);
        require(!constructor_subobjects_with_identity.constructor_tail_source_unknown &&
                constructor_subobjects_with_identity.source_record_identity_writes.size() == 2U &&
                viper::apply_ncar_body_object_writes(body_runtime_state,
                    constructor_subobjects_with_identity.source_record_identity_writes) &&
                read_body_dword(0x510U) == 7U && read_body_dword(0xd78U) == 7U,
            "body constructor must copy the serialized car ordinal into both identity fields");
        const std::array<viper::NcarObjectFieldWrite, 2> selector_offset_writes{{
            {0xca0U, 4U, 0x3e587910U},
            {0xcb0U, 4U, 0U},
        }};
        require(viper::apply_ncar_body_object_writes(
                    body_runtime_state, selector_offset_writes) &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xca0U) ==
                    0x3e587910U &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xcb0U) == 0U,
            "selector setup denominator at child +8 must remain distinct from selection denominator at child +0x18");
        const std::array<viper::NcarObjectFieldWrite, 2> pcar_derived_writes{{
            {0x50cU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
            {0x00U, 4U, 0x004c8188U},
        }};
        require(viper::apply_ncar_body_object_writes(
                    body_runtime_state, pcar_derived_writes) &&
                read_body_dword(0x00U) == 0x004c8188U &&
                read_body_dword(0x50cU) == std::bit_cast<std::uint32_t>(1.0F) &&
                body_runtime_state.initialized_bytes[0x50cU] != 0U,
            "derived PCAR constructor stores must update the same persistent object");
        const auto pcar_initialization =
            viper::plan_ncar_object_initialization({});
        const auto pcar_e44_write = std::find_if(
            pcar_initialization.writes_before_backend_refresh.begin(),
            pcar_initialization.writes_before_backend_refresh.end(),
            [](const viper::NcarObjectFieldWrite& write) {
                return write.offset == 0xe44U && write.width_bytes == 4U;
            });
        require(pcar_e44_write !=
                    pcar_initialization.writes_before_backend_refresh.end() &&
                pcar_e44_write->value_bits == 0xc2c80000U &&
                viper::apply_ncar_body_object_writes(
                    body_runtime_state,
                    pcar_initialization.writes_before_backend_refresh) &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xe44U) ==
                    0xc2c80000U &&
                body_runtime_state.initialized_bytes[0xe47U] != 0U,
            "PCAR constructor 0x433AA0 must persist its exact -100.0 timestamp sentinel at +0xE44");
        const std::array<viper::NcarObjectFieldWrite, 4> pcar_scalar_writes{{
            {0xde0U, 4U, std::bit_cast<std::uint32_t>(0.25F)},
            {0xde4U, 4U, std::bit_cast<std::uint32_t>(0.5F)},
            {0xde8U, 4U, std::bit_cast<std::uint32_t>(0.75F)},
            {0xdecU, 4U, std::bit_cast<std::uint32_t>(1.0F)},
        }};
        require(viper::apply_ncar_body_object_writes(
                    body_runtime_state, pcar_scalar_writes) &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xde0U) ==
                    std::bit_cast<std::uint32_t>(0.25F) &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xde4U) ==
                    std::bit_cast<std::uint32_t>(0.5F) &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xde8U) ==
                    std::bit_cast<std::uint32_t>(0.75F) &&
                viper::read_ncar_body_object_u32(body_runtime_state, 0xdecU) ==
                    std::bit_cast<std::uint32_t>(1.0F),
            "PCAR callback scalar outputs must persist in the byte-backed body object");
        require(!viper::read_ncar_body_object_u32(body_runtime_state,
                    static_cast<std::uint32_t>(viper::ncar_body_runtime_object_bytes)),
            "body reads must reject out-of-bounds offsets");
        const auto prior_body_bytes = body_runtime_state.bytes;
        const auto prior_body_mask = body_runtime_state.initialized_bytes;
        const std::array<viper::NcarObjectFieldWrite, 1> invalid_body_write{{
            {static_cast<std::uint32_t>(viper::ncar_body_runtime_object_bytes),
                4U, 0U},
        }};
        require(!viper::apply_ncar_body_object_writes(
                    body_runtime_state, invalid_body_write) &&
                body_runtime_state.bytes == prior_body_bytes &&
                body_runtime_state.initialized_bytes == prior_body_mask,
            "invalid body writes must be rejected without partially mutating persistent state");

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

        viper::NcarSampleRecord sample{};
        sample.raw_bytes.fill(std::byte{0xA5});
        sample.raw_bytes[0] = std::byte{0x5A};
        viper::NcarSampleProducerObjectState body{};
        body.value_0xd70 = 12.5F;
        body.value_0xd64 = 0x12345678U;
        body.value_0x500 = -3.25F;
        body.value_0xde4 = 8.75F;
        body.flag_0xe28 = 1U;
        body.value_0xc34 = 0xABCDEF01U;
        body.flag_0xec0 = 1U;
        body.wheel_damage_0x574 = {2U, 3U, 4U, 5U};
        body.position_0x5c = {10.0F, 20.0F, 30.0F};
        // A scaled, slightly skewed native basis exercises orthonormalization.
        body.orientation_0x38 = {2.0F, 0.0F, 0.0F,
                                 0.0F, 4.0F, 0.2F,
                                 0.0F, 0.0F, 3.0F};
        const auto capture = viper::capture_ncar_sample_record(
            sample, body, 42.5F, false);
        require(capture.captured && capture.orientation_normalized &&
                near(std::bit_cast<float>(sample.sample_time_bits_0x04), 42.5F) &&
                sample.value_bits_0x08 == std::bit_cast<std::uint32_t>(8.75F) &&
                sample.signed_scalar_bits_0x0c == std::bit_cast<std::uint32_t>(-3.25F) &&
                sample.value_bits_0x10 == 0xABCDEF01U &&
                sample.small_integer_0x14 == 0x12345678U &&
                sample.scaled_byte_value_bits_0x18 == std::bit_cast<std::uint32_t>(12.5F) &&
                sample.vector_bits_0x1c == std::array<std::uint32_t, 3>{
                    std::bit_cast<std::uint32_t>(10.0F),
                    std::bit_cast<std::uint32_t>(20.0F),
                    std::bit_cast<std::uint32_t>(30.0F)} &&
                sample.flag_0x38 == 1U && sample.flag_0x39 == 1U &&
                sample.gate_bytes_0x3a == std::array<std::uint8_t, 4>{2U, 3U, 4U, 5U} &&
                sample.raw_bytes[0] == std::byte{0x5A} && body.flag_0xec0 == 0U,
            "timed producer capture must copy native fields and clear the one-shot flag");
        const auto& basis = body.orientation_0x38;
        const auto dot = [](const std::size_t first_axis, const std::size_t second_axis,
                            const std::array<float, 9>& matrix) {
            return matrix[first_axis * 3U] * matrix[second_axis * 3U] +
                matrix[first_axis * 3U + 1U] * matrix[second_axis * 3U + 1U] +
                matrix[first_axis * 3U + 2U] * matrix[second_axis * 3U + 2U];
        };
        require(near(dot(0U, 0U, basis), 1.0F) &&
                near(dot(1U, 1U, basis), 1.0F) &&
                near(dot(2U, 2U, basis), 1.0F) &&
                near(dot(0U, 1U, basis), 0.0F) &&
                near(dot(0U, 2U, basis), 0.0F) &&
                near(dot(1U, 2U, basis), 0.0F),
            "producer capture must store a normalized orthogonal orientation basis");

        auto rejected_sample = sample;
        auto degenerate_body = body;
        degenerate_body.flag_0xec0 = 1U;
        degenerate_body.orientation_0x38 = {};
        const auto rejected_capture = viper::capture_ncar_sample_record(
            rejected_sample, degenerate_body, 43.0F, false);
        require(!rejected_capture.captured && rejected_sample.raw_bytes == sample.raw_bytes &&
                degenerate_body.flag_0xec0 == 1U &&
                degenerate_body.orientation_0x38 == std::array<float, 9>{},
            "degenerate producer orientation must not partially mutate state");

        std::cout << "NCAR geometry regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
