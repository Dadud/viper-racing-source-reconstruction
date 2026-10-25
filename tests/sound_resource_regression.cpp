#include "viper/sound_resource.hpp"
#include "viper/engine_sound_setup.hpp"
#include "viper/resource_bundle.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void write_u16(std::vector<std::byte>& bytes, const std::size_t offset,
               const std::uint16_t value)
{
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void write_u32(std::vector<std::byte>& bytes, const std::size_t offset,
               const std::uint32_t value)
{
    for (std::size_t i = 0; i < 4U; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8U)) & 0xffU);
}

bool run()
{
    std::vector<std::byte> payload(32U);
    write_u32(payload, 0U, 8U);
    write_u16(payload, 4U, 1U);
    write_u16(payload, 6U, 1U);
    write_u32(payload, 8U, 22050U);
    write_u32(payload, 12U, 44100U);
    write_u16(payload, 16U, 2U);
    write_u16(payload, 18U, 16U);
    payload[20U] = std::byte{0x12};
    payload[27U] = std::byte{0x34};
    const auto parsed = viper::parse_pcm_sfx0_resource(payload);
    if (!parsed || parsed->format_tag != 1U || parsed->channels != 1U ||
        parsed->sample_rate != 22050U || parsed->bits_per_sample != 16U ||
        parsed->pcm_data.size() != 8U || parsed->opaque_tail.size() != 4U ||
        parsed->pcm_data.front() != std::byte{0x12} ||
        parsed->pcm_data.back() != std::byte{0x34}) {
        std::cerr << "valid PCM SFX0 payload was not parsed exactly\n";
        return false;
    }

    auto truncated = payload;
    truncated.resize(27U);
    if (viper::parse_pcm_sfx0_resource(truncated)) {
        std::cerr << "truncated PCM sample data was accepted\n";
        return false;
    }
    auto wrong_rate = payload;
    write_u32(wrong_rate, 12U, 48000U);
    if (viper::parse_pcm_sfx0_resource(wrong_rate)) {
        std::cerr << "inconsistent average byte rate was accepted\n";
        return false;
    }
    auto wrong_format = payload;
    write_u16(wrong_format, 4U, 2U);
    if (viper::parse_pcm_sfx0_resource(wrong_format)) {
        std::cerr << "non-PCM format was accepted\n";
        return false;
    }
    auto misaligned = payload;
    write_u32(misaligned, 0U, 7U);
    if (viper::parse_pcm_sfx0_resource(misaligned)) {
        std::cerr << "sample data not aligned to PCM frames was accepted\n";
        return false;
    }

    std::vector<std::byte> engine_setup(36U);
    write_u32(engine_setup, 0U, 1U);
    for (std::size_t field = 0; field < 7U; ++field) {
        write_u32(engine_setup, 4U + field * 4U,
                  std::bit_cast<std::uint32_t>(static_cast<float>(field) + 0.25F));
    }
    engine_setup[32U] = std::byte{0xa5};
    const auto parsed_setup = viper::parse_engine_sound_setup(engine_setup);
    if (!parsed_setup || parsed_setup->band_count != 1U ||
        parsed_setup->bands[0][6] != 6.25F ||
        parsed_setup->opaque_tail.size() != 4U ||
        parsed_setup->opaque_tail.front() != std::byte{0xa5}) {
        std::cerr << "valid engine ESFX setup was not parsed exactly\n";
        return false;
    }
    auto truncated_setup = engine_setup;
    truncated_setup.resize(31U);
    if (viper::parse_engine_sound_setup(truncated_setup)) {
        std::cerr << "truncated engine ESFX record was accepted\n";
        return false;
    }
    auto oversized_setup = engine_setup;
    write_u32(oversized_setup, 0U, 8U);
    if (viper::parse_engine_sound_setup(oversized_setup)) {
        std::cerr << "engine ESFX setup beyond native seven-band capacity was accepted\n";
        return false;
    }
    auto nonfinite_setup = engine_setup;
    write_u32(nonfinite_setup, 4U, 0x7fc00000U);
    if (viper::parse_engine_sound_setup(nonfinite_setup)) {
        std::cerr << "non-finite engine ESFX tuning value was accepted\n";
        return false;
    }

    // 0x46C930: b0 is inclusive and b3 exclusive; the middle ratio drives
    // the configured b4->b5 interpolation.
    viper::EngineSoundBandInput band_input{
        {0.0F, 1.0F, 3.0F, 5.0F, 10.0F, 20.0F, 2.0F},
        2.0F, 0.0F, true};
    auto band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.status != viper::EngineSoundBandStatus::evaluated ||
        band_eval.branch != viper::EngineSoundBandBranch::central_ramp ||
        band_eval.value != 15.0F) {
        std::cerr << "ESFX central interpolation branch mismatch\n";
        return false;
    }
    band_input.control_value = -0.1F;
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.branch != viper::EngineSoundBandBranch::outside_gate ||
        band_eval.value != 0.0F) {
        std::cerr << "ESFX lower-bound gate mismatch\n";
        return false;
    }
    band_input.control_value = 5.0F;
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.branch != viper::EngineSoundBandBranch::outside_gate) {
        std::cerr << "ESFX exclusive upper-bound gate mismatch\n";
        return false;
    }
    band_input.control_value = 3.5F;
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.branch != viper::EngineSoundBandBranch::upper_ramp ||
        band_eval.value != 16.875F) {
        std::cerr << "ESFX upper-ramp branch mismatch\n";
        return false;
    }
    band_input.control_value = 0.5F;
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.branch != viper::EngineSoundBandBranch::lower_ramp ||
        band_eval.value != 3.75F) {
        std::cerr << "ESFX lower-ramp branch mismatch\n";
        return false;
    }
    band_input.control_value = 2.0F;
    band_input.division_helper_gate_clear = false;
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.status !=
        viper::EngineSoundBandStatus::unresolved_division_helper) {
        std::cerr << "unknown ESFX division-helper gate was treated as known\n";
        return false;
    }
    band_input.division_helper_gate_clear = true;
    band_input.band[2] = band_input.band[1];
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.status !=
        viper::EngineSoundBandStatus::unresolved_division_helper) {
        std::cerr << "zero ESFX denominator was treated as a finite update\n";
        return false;
    }
    band_input.band[2] = 3.0F;
    band_input.band[6] = std::numeric_limits<float>::infinity();
    band_eval = viper::evaluate_engine_sound_band_46c930(band_input);
    if (band_eval.status !=
        viper::EngineSoundBandStatus::invalid_or_nonfinite_input) {
        std::cerr << "non-finite ESFX input was not rejected\n";
        return false;
    }

    viper::EngineSoundUpdateInput update_input;
    update_input.band_input = {
        {0.0F, 1.0F, 3.0F, 5.0F, 10.0F, 20.0F, 2.0F},
        2.0F, 0.0F, true};
    update_input.object_scale = 0.1F;
    update_input.sound_object = viper::EngineSoundObjectState{
        0.0F, 0.0F, false, false, false, false, false};
    const auto update = viper::apply_engine_sound_update_46ca10(update_input);
    if (update.status != viper::EngineSoundUpdateStatus::updated ||
        update.sound_object.value_at_04 != 1.0F ||
        std::abs(update.sound_object.value_at_08 - 0.9F) > 0.00001F ||
        !update.sound_object.flag_at_29 || !update.sound_object.flag_at_2b ||
        !update.sound_object.flag_at_2c || !update.sound_object.flag_at_2d ||
        update.sound_object.flag_at_2a) {
        std::cerr << "ESFX 0x46CA10 object update mismatch\n";
        return false;
    }
    update_input.object_scale = -0.1F;
    const auto disabled_update =
        viper::apply_engine_sound_update_46ca10(update_input);
    if (disabled_update.status !=
            viper::EngineSoundUpdateStatus::sound_disabled_for_frame ||
        !disabled_update.sound_object.flag_at_2a) {
        std::cerr << "ESFX 0x46CA10 nonpositive gate mismatch\n";
        return false;
    }
    update_input.sound_object.reset();
    const auto missing_object_update =
        viper::apply_engine_sound_update_46ca10(update_input);
    if (missing_object_update.status !=
        viper::EngineSoundUpdateStatus::missing_sound_object) {
        std::cerr << "unknown ESFX sound-object state was guessed\n";
        return false;
    }
    return true;
}

bool validate_shipped_bundles(const std::filesystem::path& data_dir)
{
    constexpr auto sfx0 = viper::resource_type_code('S', 'F', 'X', '0');
    constexpr auto esfx = viper::resource_type_code('E', 'S', 'F', 'X');
    constexpr std::array<std::string_view, 5> cars{
        "exotic", "plane", "sedan", "sports", "viper"};
    for (const auto car : cars) {
        const auto bundle = viper::ResourceBundle::open(
            data_dir / (std::string(car) + ".car"));
        std::size_t sound_count = 0U;
        std::size_t setup_count = 0U;
        std::optional<std::uint32_t> declared_bands;
        for (const auto& entry : bundle.entries()) {
            if (entry.type_code == esfx) {
                const auto setup = viper::parse_engine_sound_setup(bundle.payload(entry));
                const auto expected_bands = car == "viper" ? 3U : 2U;
                const auto expected_tail = car == "viper" ? 140U : 168U;
                if (!setup || setup->band_count != expected_bands ||
                    setup->opaque_tail.size() != expected_tail) {
                    std::cerr << "invalid shipped engine ESFX setup: " << car
                              << '/' << entry.name << '\n';
                    return false;
                }
                declared_bands = setup->band_count;
                ++setup_count;
            }
            if (entry.type_code != sfx0) continue;
            const auto sound = viper::parse_pcm_sfx0_resource(bundle.payload(entry));
            if (!sound || sound->pcm_data.empty() ||
                sound->opaque_tail.size() != 4098U) {
                std::cerr << "invalid shipped SFX0 PCM resource: " << car
                          << '/' << entry.name << '\n';
                return false;
            }
            ++sound_count;
        }
        const auto expected_count = car == "viper" ? 4U : 3U;
        if (sound_count != expected_count) {
            std::cerr << "unexpected SFX0 resource count for " << car << ": "
                      << sound_count << '\n';
            return false;
        }
        if (setup_count != 1U) {
            std::cerr << "unexpected ESFX setup count for " << car << ": "
                      << setup_count << '\n';
            return false;
        }
        const auto idle_name = std::string(car) + "i.sfx";
        if (bundle.find(idle_name, sfx0) == nullptr) {
            std::cerr << "missing shipped idle sample for " << car << ": "
                      << idle_name << '\n';
            return false;
        }
        for (std::uint32_t band = 0U; band < *declared_bands; ++band) {
            const auto specific = std::string(car) + std::to_string(band) + ".sfx";
            const auto fallback = "engine" + std::to_string(band) + ".sfx";
            if (bundle.find(specific, sfx0) == nullptr &&
                bundle.find(fallback, sfx0) == nullptr) {
                std::cerr << "missing shipped engine band " << band << " for "
                          << car << " (tried " << specific << " and "
                          << fallback << ")\n";
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main(const int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "usage: viper_sound_resource_regression [DATA_DIR]\n";
        return 2;
    }
    if (!run()) return 1;
    if (argc == 2 && !validate_shipped_bundles(argv[1])) return 1;
    return 0;
}
