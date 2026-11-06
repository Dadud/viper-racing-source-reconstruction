#include "viper/resource_bundle.hpp"
#include "viper/pcar_control_update.hpp"
#include "viper/wheel_body_coupling.hpp"
#include "viper/world_track_objects.hpp"
#include "viper/acar_curve_tracker.hpp"
#include "viper/physics_task_factory.hpp"
#include "viper/ai_notes_resource.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::uint32_t word(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t result{};
    for (std::size_t i = 0; i < 4; ++i)
        result |= std::to_integer<std::uint32_t>(bytes[offset + i]) << (8U * i);
    return result;
}
void write_word(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (8U * i)) & 0xffU);
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        const std::filesystem::path root(argv[1]);
        require(viper::inventory_ai_line_overrides(root) == viper::AILineOverrideInventory::none,
            "supplied data must have no higher-priority AI line overrides");
        require(viper::inventory_ai_line_overrides(root / "missing_ai_line_directory") ==
                viper::AILineOverrideInventory::incomplete,
            "an unreadable AI resource inventory must not select a fallback");
        const auto test_directory = std::filesystem::temp_directory_path() /
            ("viper_ai_inventory_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        require(std::filesystem::create_directory(test_directory), "could not create inventory fixture");
        const auto override_path = test_directory / "example.ILQ";
        { std::ofstream override_file(override_path); require(override_file.good(), "could not create override fixture"); }
        const auto override_inventory = viper::inventory_ai_line_overrides(test_directory);
        std::filesystem::remove(override_path);
        std::filesystem::remove(test_directory);
        require(override_inventory == viper::AILineOverrideInventory::present,
            "loose AI override detection must be case insensitive");
        const auto driver_lines = viper::ResourceBundle::open(root / "drivers.res");
        std::size_t valid_driver_line_count{};
        std::size_t valid_driver_note_count{};
        for (const auto& entry : driver_lines.entries()) {
            if (entry.type_code == 0x494c494eU && entry.name.ends_with(".ilg")) {
                const auto raw = viper::parse_ideal_line_resource(entry.attributes, driver_lines.payload(entry));
                require(raw && viper::normalize_ideal_line_resource(*raw, 0U),
                    "driver-specific AI line is invalid");
                ++valid_driver_line_count;
            } else if (entry.type_code == viper::AINoteResource::resource_type_code &&
                       entry.name.ends_with(".dnt")) {
                require(viper::parse_ai_note_resource(entry.attributes, driver_lines.payload(entry)).has_value(),
                    "driver note resource is invalid");
                ++valid_driver_note_count;
            }
        }
        require(valid_driver_line_count > 0U && valid_driver_note_count > 0U,
            "driver bundle must expose AI lines and notes");
        const viper::AINoteLookupKey stock_key{0U, 0U, 0U, "viper"};
        const auto driver_line = viper::select_stock_ai_line(
            driver_lines, stock_key, viper::AILineOverrideInventory::none);
        require(driver_line.status == viper::AILineSelectionStatus::driver_line &&
                driver_line.driver_entry != nullptr, "stock driver line must resolve by exact key");
        const auto raw_driver_line = viper::parse_ideal_line_resource(
            driver_line.driver_entry->attributes, driver_lines.payload(*driver_line.driver_entry));
        const auto normalized_driver_line = raw_driver_line
            ? viper::normalize_ideal_line_resource(*raw_driver_line, 0U) : std::nullopt;
        require(normalized_driver_line &&
                viper::make_acar_model_node_observations(*normalized_driver_line).has_value(),
            "selected driver line must provide AI model nodes");
        const auto driver_curve = viper::make_track_curve_nodes(*normalized_driver_line);
        require(driver_curve && viper::initialize_acar_model_tracker(*driver_curve,
                    {0.0F, 0.0F}, {0.0F, 0.0F}).has_value(),
            "bundled driver line must support physical tracker initialization");
        const auto* driver_note_entry = driver_lines.find("000vipr.dnt", viper::AINoteResource::resource_type_code);
        require(driver_note_entry != nullptr, "stock driver note missing");
        const auto stock_note = viper::parse_ai_note_resource(
            driver_note_entry->attributes, driver_lines.payload(*driver_note_entry));
        require(stock_note && viper::ai_note_matches_line(*stock_note, stock_key, *normalized_driver_line),
            "stock driver note must match its selected line checksum and node count");
        const auto note_pairs = viper::read_ai_note_node_pairs(*stock_note);
        require(note_pairs && note_pairs->size() == normalized_driver_line->records.size() &&
                note_pairs->front()[0] == word(stock_note->node_payload, 0U) &&
                note_pairs->front()[1] == word(stock_note->node_payload, 4U),
            "validated driver note must supply a pair for each AI line node");
        const auto driver_nodes = viper::make_acar_model_node_observations(*normalized_driver_line);
        const auto descriptors = viper::make_acar_runtime_descriptors(*driver_nodes,
            std::span<const std::array<std::uint32_t, 2>>(*note_pairs), false);
        require(descriptors && descriptors->size() == note_pairs->size() &&
                descriptors->front().note_pair_0x04 == note_pairs->front() &&
                descriptors->back().note_pair_0x04 == note_pairs->back() &&
                descriptors->back().node_id_0x14 == driver_nodes->back().address,
            "validated stock notes must initialize the corresponding AI descriptors");
        const auto gated_descriptors = viper::make_acar_runtime_descriptors(*driver_nodes,
            std::span<const std::array<std::uint32_t, 2>>(*note_pairs), true);
        require(gated_descriptors && gated_descriptors->front().note_pair_0x04 ==
                std::array<std::uint32_t, 2>{},
            "gated note lookup must clear descriptor note pairs");
        require(viper::select_stock_ai_line(driver_lines, stock_key,
                    viper::AILineOverrideInventory::present).status ==
                    viper::AILineSelectionStatus::unresolved_overrides,
            "a higher-priority override must prevent selecting the stock line");
        require(viper::select_stock_ai_line(driver_lines,
                    {0U, 0U, 0U, "plane"}, viper::AILineOverrideInventory::none).status ==
                    viper::AILineSelectionStatus::default_line,
            "a missing stock driver line must select the default fallback");
        const auto common = viper::ResourceBundle::open(root / "common.res");
        const auto* tire_entry = common.find("def_tire.tir", 0x54495245U);
        require(tire_entry != nullptr, "default tire missing");
        const auto tire = viper::parse_tire_resource(common.payload(*tire_entry));
        require(tire.has_value(), "default tire invalid");
        for (const std::string name : {"bemidji", "dundas", "hastings", "heaven",
                "kenyon", "limbo", "nfield", "uptown"}) {
            const auto bundle = viper::ResourceBundle::open(root / (name + ".trk"));
            for (const auto* resource_name : {"default.ili", "rdefault.ili"}) {
                const auto* entry = bundle.find(resource_name, 0x494c494eU);
                require(entry != nullptr, "AI line missing from track bundle");
                const auto payload = viper::parse_ideal_line_resource(entry->attributes, bundle.payload(*entry));
                require(payload.has_value(), "AI line payload invalid");
                const auto line_data = viper::normalize_ideal_line_resource(*payload, 0U);
                require(line_data.has_value(), "AI line normalization failed");
                const auto nodes = viper::make_acar_model_node_observations(*line_data);
                require(nodes && nodes->size() == line_data->records.size() && !nodes->empty() &&
                        nodes->back().next_address == nodes->front().address,
                    "AI line descriptors must form a complete circular list");
                const auto descriptor_geometry = viper::make_acar_descriptor_geometry(*nodes);
                const auto threshold_candidate = viper::plan_acar_line_threshold_candidate(
                    *nodes, std::bit_cast<std::uint32_t>(1.0F));
                require(threshold_candidate.has_value(), "AI line threshold inputs incomplete");
                require(descriptor_geometry.size() == nodes->size(), "AI descriptor count mismatch");
                for (std::size_t i = 0; i < nodes->size(); ++i)
                    require(descriptor_geometry[i].node_id == (*nodes)[i].address &&
                            std::isfinite(descriptor_geometry[i].mapped_scalar) &&
                            descriptor_geometry[i].mapped_scalar >= 0.3F &&
                            descriptor_geometry[i].mapped_scalar <= 0.700001F,
                        "AI descriptor geometry must be finite and reference its source node");
            }
        }
        const auto track = viper::ResourceBundle::open(root / "bemidji.trk");
        const auto* line_entry = track.find("track.ild", 0x494c494eU);
        require(line_entry != nullptr, "Bemidji track spline missing");
        const auto line = viper::parse_ideal_line_resource(
            line_entry->attributes, track.payload(*line_entry));
        require(line.has_value(), "Bemidji track spline invalid");
        const auto normalized = viper::normalize_ideal_line_resource(*line, 0U);
        require(normalized.has_value(), "Bemidji track normalization failed");
        const auto track_curve = viper::make_track_curve_nodes(*normalized);
        require(track_curve.has_value(), "Bemidji track curve invalid");
        viper::TrackPositionIteratorState iterator{};
        require(viper::initialize_track_position_iterator(iterator, *track_curve, 0.0F, 0.0F) ==
                    viper::TrackPositionIteratorStatus::updated,
            "Bemidji track iterator initialization failed");
        for (const std::string car : {"viper", "sports", "sedan", "exotic", "plane"}) {
            const auto bundle = viper::ResourceBundle::open(root / (car + ".car"));
            const auto resources = viper::parse_car_file_resources(bundle, car, true);
            constexpr auto setup_type = viper::resource_type_code('C','C','S','0');
            auto setup = bundle.find_payload(car + ".ccs", setup_type);
            if (!setup) setup = common.find_payload("default.ccs", setup_type);
            require(resources.car_file.has_value() && setup.has_value(), "missing car or setup");
            const auto preparation = viper::prepare_car_file_runtime_mapping_from_setup_payload(
                resources, *setup, {}, false, false);
            require(preparation.input_complete && preparation.mapping.has_value(), "car mapping incomplete");
            std::vector<std::byte> record(viper::CarFileRuntimeMapping::runtime_record_size);
            require(viper::apply_car_file_runtime_mapping(record, *preparation.mapping), "car mapping failed");
            write_word(record, 0U, 0x50434152U);
            write_word(record, 4U, static_cast<std::uint32_t>(record.size()));
            for (std::size_t wheel = 0; wheel < 4U; ++wheel) {
                const auto base = 0x554U + 0x1a4U * static_cast<std::uint32_t>(wheel);
                viper::NcarWheelRuntimeObjectState object{};
                const auto setup = viper::initialize_ncar_wheel_tire_setup(record, wheel, *tire, false);
                require(setup && viper::apply_ncar_wheel_tire_response_scale(object, base, *setup),
                    "asset tire scale could not initialize wheel");
                const auto written = viper::read_ncar_wheel_object_u32(object, 0x90U);
                require(written && *written == std::bit_cast<std::uint32_t>(setup->tire_response_object_0x2c),
                    "asset tire scale must persist on each wheel");
            }
            const auto source = viper::read_car_curve_source_record(record);
            require(source.has_value(), "curve source missing");
            const auto curve = viper::configure_acar_update_curve(0U, *source, false);
            viper::NcarBodyRuntimeObjectState initialized_body{};
            require(viper::initialize_ncar_embedded_curve_and_selector(initialized_body, record),
                "asset embedded curve initialization failed");
            for (const auto& field : viper::make_configured_update_curve_object_writes(curve, 0xbfcU)) {
                if (field.width != 4U) continue;
                const auto stored = viper::read_ncar_body_object_u32(initialized_body, field.offset);
                require(stored && *stored == field.value_bits, "embedded curve field mismatch");
            }
            std::uint32_t expected_count{};
            while (expected_count < 7U &&
                    std::bit_cast<float>(word(record, 0x94U + expected_count * 4U)) > 0.01F)
                ++expected_count;
            const auto count = viper::read_ncar_body_object_u32(initialized_body, 0xcb8U);
            require(count && *count == expected_count, "embedded selector count mismatch");
            for (std::uint32_t i = 0; i < expected_count; ++i) {
                const auto entry = viper::read_ncar_body_object_u32(initialized_body, 0xcc4U + i * 4U);
                require(entry && *entry == word(record, 0x94U + i * 4U), "embedded selector entry mismatch");
            }
            require(!viper::initialize_ncar_embedded_curve_and_selector(
                    initialized_body, std::span(record).first(0x80U)),
                "truncated runtime records must fail embedded setup");
            const auto signal = viper::make_ncar_integrator_signal_state_from_runtime_record(record);
            const auto front = viper::derive_ncar_wheel_scalar_0x5c(record, 0U);
            const auto rear = viper::derive_ncar_wheel_scalar_0x5c(record, 2U);
            require(signal && front && rear, "constructor signal missing");
            std::array<viper::NcarWheelRuntimeObjectState, 4> signal_wheels{};
            for (std::size_t i = 0; i < signal_wheels.size(); ++i) {
                const auto base = 0x554U + 0x1a4U * static_cast<std::uint32_t>(i);
                const auto value = viper::derive_ncar_wheel_scalar_0x5c(record, i);
                require(value.has_value(), "wheel signal source missing");
                const viper::NcarObjectFieldWrite field{base + 0x5cU, 4U,
                    std::bit_cast<std::uint32_t>(*value)};
                require(viper::apply_ncar_wheel_object_writes(signal_wheels[i], base,
                    std::span(&field, 1U)), "wheel signal setup failed");
            }
            viper::NcarBodyRuntimeObjectState signal_body{};
            require(viper::initialize_ncar_wheel_signal_inputs(signal_body, signal_wheels, record),
                "wheel-derived signal initialization failed");
            require(viper::read_ncar_body_object_u32(signal_body, 0x5b0U) ==
                        std::bit_cast<std::uint32_t>(*front) &&
                    viper::read_ncar_body_object_u32(signal_body, 0x8f8U) ==
                        std::bit_cast<std::uint32_t>(*rear) &&
                    viper::read_ncar_body_object_u32(signal_body, 0xdc4U) == word(record, 0xb4U) &&
                    viper::read_ncar_body_object_u32(signal_body, 0xdd0U).has_value() &&
                    viper::read_ncar_body_object_u32(signal_body, 0xdd4U).has_value() &&
                    viper::read_ncar_body_object_u32(signal_body, 0xdd8U).has_value(),
                "wheel-derived signal fields must retain car-specific inputs");
            signal_wheels[0] = {};
            require(!viper::initialize_ncar_wheel_signal_inputs(signal_body, signal_wheels, record),
                "missing wheel source must fail signal initialization");
            const auto multiplier = viper::derive_ncar_integrator_signal_multiplier(
                *front, *rear, std::bit_cast<float>(word(record, 0xb4U)),
                std::bit_cast<float>(word(record, 0x90U)), std::bit_cast<float>(0x3dd67750U));
            require(multiplier.has_value(), "invalid signal multiplier");
            viper::NcarBodyRuntimeObjectState body{};
            const auto defaults = viper::plan_ncar_constructor_subobject_defaults();
            require(viper::apply_ncar_body_object_writes(body, defaults.writes), "body defaults failed");
            for (const auto& field : viper::make_configured_update_curve_object_writes(curve, 0xbfcU)) {
                const viper::NcarObjectFieldWrite write{field.offset, field.width, field.value_bits};
                require(viper::apply_ncar_body_object_writes(body, std::span(&write, 1U)), "curve write failed");
            }
            std::vector<viper::NcarObjectFieldWrite> writes;
            const auto f = [&writes](std::uint32_t offset, float value) {
                writes.push_back({offset, 4U, std::bit_cast<std::uint32_t>(value)});
            };
            for (const auto offset : {0x50U,0x58U,0x258U,0x25cU,0x260U,0xc0cU,0xd70U,0xde0U}) f(offset, 0.0F);
            f(0x54U, 1.0F); f(0xdecU, 1.0F); f(0xc8cU, signal->limit_scale_0xc8c);
            f(0xc90U, signal->callback_scale_0xc90); f(0xca0U, std::bit_cast<float>(0x3e587910U));
            f(0xca4U, 1.0F / std::bit_cast<float>(0x3e587910U));
            const float profile = std::bit_cast<float>(word(record, 0xc8U));
            f(0xcacU, std::fabs(profile) > std::bit_cast<float>(0x34000000U)
                ? profile * std::bit_cast<float>(0x3facfe73U) : std::bit_cast<float>(0x3b03126fU));
            f(0xdd0U, *multiplier);
            writes.push_back({0xbe4U,1U,0U}); writes.push_back({0x534U,1U,0U});
            for (std::uint32_t i = 0; i < 7U; ++i) {
                const auto entry = word(record, 0x94U + i * 4U);
                if (!(std::bit_cast<float>(entry) > 0.01F)) break;
                writes.push_back({0xcc4U + i * 4U, 4U, entry});
            }
            require(viper::apply_ncar_body_object_writes(body, writes), "body setup failed");
            std::array<viper::NcarWheelRuntimeObjectState, 4> wheels{};
            for (std::size_t i = 0; i < wheels.size(); ++i) {
                const auto base = 0x554U + 0x1a4U * static_cast<std::uint32_t>(i);
                const viper::NcarObjectFieldWrite spin{base + 0x128U, 4U, 0U};
                require(viper::apply_ncar_wheel_object_writes(wheels[i], base, std::span(&spin, 1U)), "wheel init failed");
            }
            require(viper::read_ncar_body_object_u32(body, 0xcc4U).has_value(),
                "first selector value missing");
            for (int tick = 0; tick < 64; ++tick) {
                if (tick == 32) {
                    const std::array<viper::NcarObjectFieldWrite, 3> controls{{
                        {0xcbcU, 4U, 1U},
                        {0xde0U, 4U, std::bit_cast<std::uint32_t>(0.75F)},
                        {0x534U, 1U, 1U}}};
                    require(viper::apply_ncar_body_object_writes(body, controls), "control write failed");
                }
                const auto step = viper::advance_ncar_drivetrain_control_in_body_and_wheels(body, wheels, record, 0x554U);
                if (!step) throw std::runtime_error(car + " drivetrain step " + std::to_string(tick) + " failed");
                const auto getter = viper::read_ncar_body_object_u32(body, 0xc0cU);
                require(getter && std::isfinite(std::bit_cast<float>(*getter)), "invalid curve getter");
                const viper::NcarObjectFieldWrite sample{0xd70U,4U,*getter};
                require(viper::apply_ncar_body_object_writes(body, std::span(&sample, 1U)), "getter sample failed");
            }
            require(viper::read_ncar_body_object_u8(body, 0xcf0U) == 1U,
                "selected controller did not become active");
            // Exercise the feedback path with resource-derived wheel core
            // values. The zero tire vector is an explicit fixture input;
            // this is not a ground-contact or vehicle-motion simulation.
            const auto body_observation =
                viper::make_ncar_body_constructor_observation_from_runtime_record(
                    record, 0U, false);
            require(body_observation.has_value(), "body constructor inputs missing");
            const auto body_plan = viper::plan_ncar_body_constructor(*body_observation);
            const auto wheel_core = viper::initialize_ncar_four_wheel_core({
                record, {}, body_plan.scaled_dimensions_0x1f0_to_0x1fc[0], false, false});
            require(wheel_core.input_complete, "wheel core setup incomplete");
            std::array<viper::NcarObjectFieldWrite, 12> pose{};
            for (std::size_t i = 0; i < 9U; ++i)
                pose[i] = {0x38U + static_cast<std::uint32_t>(i * 4U), 4U,
                    std::bit_cast<std::uint32_t>(i % 4U == 0U ? 1.0F : 0.0F)};
            for (std::size_t i = 0; i < 3U; ++i)
                pose[9U + i] = {0x234U + static_cast<std::uint32_t>(i * 4U), 4U, 0U};
            require(viper::apply_ncar_body_object_writes(body, pose), "body pose failed");
            for (std::size_t i = 0; i < wheels.size(); ++i) {
                const auto base = 0x554U + 0x1a4U * static_cast<std::uint32_t>(i);
                const auto wheel_plan = viper::plan_ncar_wheel_constructor(
                    base, viper::NcarWheelConstructorObservation{});
                require(wheel_core.wheels[i].has_value() &&
                    viper::materialize_ncar_wheel_constructor(wheels[i], base, wheel_plan) &&
                    viper::apply_ncar_wheel_core_initialization(wheels[i], base, *wheel_core.wheels[i]),
                    "wheel initialization failed");
                const std::array<viper::NcarObjectFieldWrite, 3> controls{{
                    {base + 0x160U, 1U, 0U}, {base + 0x15cU, 4U, 1U},
                    {base + 0x164U, 4U, 1U}}};
                require(viper::apply_ncar_wheel_object_writes(wheels[i], base, controls),
                    "wheel control initialization failed");
            }
            bool wheel_rate_changed = false;
            auto neutral_body = body;
            auto neutral_wheels = wheels;
            bool signal_response_differs = false;
            viper::PcarScalarFields neutral_fields{}, active_fields{};
            viper::PcarCallbackGlobals neutral_globals{}, active_globals{};
            active_globals.value_0x5093f4 = 0.75F;
            for (int tick = 0; tick < 64; ++tick) {
                viper::update_pcar_callback_scalar_prefix(neutral_fields, neutral_globals, 1.0F);
                viper::update_pcar_callback_scalar_prefix(active_fields, active_globals, 1.0F);
                const viper::NcarObjectFieldWrite neutral_control{
                    0xde0U, 4U, std::bit_cast<std::uint32_t>(neutral_fields.value_0xde0)};
                const viper::NcarObjectFieldWrite active_control{
                    0xde0U, 4U, std::bit_cast<std::uint32_t>(active_fields.value_0xde0)};
                require(viper::apply_ncar_body_object_writes(neutral_body, std::span(&neutral_control, 1U)) &&
                    viper::apply_ncar_body_object_writes(body, std::span(&active_control, 1U)),
                    "control signal write failed");
                const auto step = viper::advance_ncar_drivetrain_control_in_body_and_wheels(
                    body, wheels, record, 0x554U);
                if (!step) throw std::runtime_error(car + " coupled drivetrain step " + std::to_string(tick) + " failed");
                const auto neutral_step = viper::advance_ncar_drivetrain_control_in_body_and_wheels(
                    neutral_body, neutral_wheels, record, 0x554U);
                require(neutral_step.has_value(), "neutral drivetrain update failed");
                for (std::size_t i = 0; i < wheels.size(); ++i) {
                    const auto base = 0x554U + 0x1a4U * static_cast<std::uint32_t>(i);
                    require(viper::advance_ncar_wheel_update_prefix_in_object(wheels[i], base).has_value(),
                        "wheel prefix failed");
                    const auto spin = viper::advance_ncar_wheel_spin_preprojection_in_objects(wheels[i], base, body);
                    require(spin.has_value(), "wheel spin failed");
                    const auto rate = viper::advance_ncar_wheel_rate_finalization_in_object(
                        wheels[i], base, *spin, {}, {});
                    require(rate && std::isfinite(rate->rate_0x124), "wheel rate failed");
                    wheel_rate_changed |= rate->rate_0x124 != 0.0F;
                    require(viper::advance_ncar_wheel_update_prefix_in_object(neutral_wheels[i], base).has_value(),
                        "neutral wheel prefix failed");
                    const auto neutral_spin = viper::advance_ncar_wheel_spin_preprojection_in_objects(
                        neutral_wheels[i], base, neutral_body);
                    require(neutral_spin.has_value(), "neutral wheel spin failed");
                    const auto neutral_rate = viper::advance_ncar_wheel_rate_finalization_in_object(
                        neutral_wheels[i], base, *neutral_spin, {}, {});
                    require(neutral_rate.has_value(), "neutral wheel rate failed");
                    signal_response_differs |= rate->rate_0x124 != neutral_rate->rate_0x124;
                }
                const auto getter = viper::read_ncar_body_object_u32(body, 0xc0cU);
                require(getter.has_value(), "curve sample missing");
                const viper::NcarObjectFieldWrite sample{0xd70U, 4U, *getter};
                require(viper::apply_ncar_body_object_writes(body, std::span(&sample, 1U)), "curve sample failed");
                const auto neutral_getter = viper::read_ncar_body_object_u32(neutral_body, 0xc0cU);
                require(neutral_getter.has_value(), "neutral curve sample missing");
                const viper::NcarObjectFieldWrite neutral_sample{0xd70U, 4U, *neutral_getter};
                require(viper::apply_ncar_body_object_writes(neutral_body, std::span(&neutral_sample, 1U)),
                    "neutral curve sample failed");
            }
            require(wheel_rate_changed, "selected drivetrain did not change any wheel rate");
            require(signal_response_differs, "accelerator signal did not change any wheel rate");
            std::cout << car << ": 128 finite drivetrain updates; wheel rate changed; control difference="
                << signal_response_differs << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
