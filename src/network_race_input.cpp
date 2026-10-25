#include "viper/network_race_input.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace viper::net {
namespace {
void write_float(std::uint8_t* output, const float value)
{
    const auto bits = std::bit_cast<std::uint32_t>(value);
    for (unsigned i = 0; i < 4; ++i)
        output[i] = static_cast<std::uint8_t>(bits >> ((3U - i) * 8U));
}

float read_float(const std::uint8_t* input) noexcept
{
    std::uint32_t bits{};
    for (unsigned i = 0; i < 4; ++i)
        bits |= static_cast<std::uint32_t>(input[i]) << ((3U - i) * 8U);
    return std::bit_cast<float>(bits);
}

bool valid_resource_stem(const std::string_view value) noexcept
{
    return !value.empty() && value.size() <= 32U &&
        std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        });
}

void write_u16(std::uint8_t* output, const std::uint16_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value >> 8U);
    output[1] = static_cast<std::uint8_t>(value);
}

std::uint16_t read_u16(const std::uint8_t* input) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(input[0]) << 8U) | input[1]);
}
}

std::array<std::uint8_t, race_input_payload_size> encode_race_input(
    const RaceInput& input)
{
    if (!std::isfinite(input.steering) || !std::isfinite(input.throttle) ||
        !std::isfinite(input.brake) || input.steering < -1.0F ||
        input.steering > 1.0F || input.throttle < 0.0F ||
        input.throttle > 1.0F || input.brake < 0.0F || input.brake > 1.0F)
        throw std::invalid_argument("race input is outside normalized bounds");
    std::array<std::uint8_t, race_input_payload_size> bytes{};
    write_float(bytes.data(), input.steering);
    write_float(bytes.data() + 4, input.throttle);
    write_float(bytes.data() + 8, input.brake);
    return bytes;
}

bool decode_race_input(const std::span<const std::uint8_t> bytes,
                       RaceInput& input) noexcept
{
    if (bytes.size() != race_input_payload_size) return false;
    const RaceInput decoded{read_float(bytes.data()), read_float(bytes.data() + 4),
        read_float(bytes.data() + 8)};
    if (!std::isfinite(decoded.steering) || !std::isfinite(decoded.throttle) ||
        !std::isfinite(decoded.brake) || decoded.steering < -1.0F ||
        decoded.steering > 1.0F || decoded.throttle < 0.0F ||
        decoded.throttle > 1.0F || decoded.brake < 0.0F || decoded.brake > 1.0F)
        return false;
    input = decoded;
    return true;
}

std::vector<std::uint8_t> encode_race_configuration(
    const RaceConfiguration& configuration)
{
    if (!valid_resource_stem(configuration.track) ||
        !valid_resource_stem(configuration.car) ||
        configuration.laps < 1U || configuration.laps > 99U ||
        configuration.opponent_strength >= 8U ||
        configuration.entrant_count < 2U || configuration.entrant_count > 64U)
        throw std::invalid_argument("race configuration is outside protocol bounds");

    constexpr std::size_t header_size = 10U;
    std::vector<std::uint8_t> bytes(header_size + configuration.track.size() +
        configuration.car.size());
    bytes[0] = 'V'; bytes[1] = 'R'; bytes[2] = 'C'; bytes[3] = '1';
    bytes[4] = static_cast<std::uint8_t>(configuration.track.size());
    bytes[5] = static_cast<std::uint8_t>(configuration.car.size());
    write_u16(bytes.data() + 6U, configuration.laps);
    bytes[8] = configuration.opponent_strength;
    bytes[9] = configuration.entrant_count;
    std::copy(configuration.track.begin(), configuration.track.end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(header_size));
    std::copy(configuration.car.begin(), configuration.car.end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(header_size +
            configuration.track.size()));
    return bytes;
}

bool decode_race_configuration(const std::span<const std::uint8_t> bytes,
                               RaceConfiguration& configuration)
{
    constexpr std::size_t header_size = 10U;
    if (bytes.size() < header_size || bytes[0] != 'V' || bytes[1] != 'R' ||
        bytes[2] != 'C' || bytes[3] != '1')
        return false;
    const auto track_size = static_cast<std::size_t>(bytes[4]);
    const auto car_size = static_cast<std::size_t>(bytes[5]);
    if (track_size == 0U || track_size > 32U || car_size == 0U ||
        car_size > 32U || bytes.size() != header_size + track_size + car_size)
        return false;
    RaceConfiguration decoded;
    decoded.laps = read_u16(bytes.data() + 6U);
    decoded.opponent_strength = bytes[8];
    decoded.entrant_count = bytes[9];
    decoded.track.assign(reinterpret_cast<const char*>(bytes.data() + header_size),
        track_size);
    decoded.car.assign(reinterpret_cast<const char*>(bytes.data() + header_size +
        track_size), car_size);
    if (!valid_resource_stem(decoded.track) || !valid_resource_stem(decoded.car) ||
        decoded.laps < 1U || decoded.laps > 99U ||
        decoded.opponent_strength >= 8U || decoded.entrant_count < 2U ||
        decoded.entrant_count > 64U)
        return false;
    configuration = std::move(decoded);
    return true;
}

} // namespace viper::net
