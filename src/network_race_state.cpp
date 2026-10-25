#include "viper/network_race_state.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace viper::net {
namespace {
void write_float(std::uint8_t* output, const float value) noexcept
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

bool valid_resource_stem(const std::string_view value) noexcept
{
    return !value.empty() && value.size() <= 32U &&
        std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        });
}

void write_u32(std::uint8_t* output, const std::uint32_t value) noexcept
{
    output[0] = static_cast<std::uint8_t>(value >> 24U);
    output[1] = static_cast<std::uint8_t>(value >> 16U);
    output[2] = static_cast<std::uint8_t>(value >> 8U);
    output[3] = static_cast<std::uint8_t>(value);
}

std::uint32_t read_u32(const std::uint8_t* input) noexcept
{
    return (static_cast<std::uint32_t>(input[0]) << 24U) |
        (static_cast<std::uint32_t>(input[1]) << 16U) |
        (static_cast<std::uint32_t>(input[2]) << 8U) |
        static_cast<std::uint32_t>(input[3]);
}

bool valid_race_state(const RouteMotionState& state) noexcept
{
    return state.route_segment <= std::numeric_limits<std::uint32_t>::max() &&
        state.race.lap_start_segment <=
            std::numeric_limits<std::uint32_t>::max() &&
        std::isfinite(state.route_fraction) && state.route_fraction >= 0.0F &&
        state.route_fraction <= 1.0F && std::isfinite(state.steering) &&
        state.steering >= -1.0F && state.steering <= 1.0F &&
        std::isfinite(state.speed) && state.speed >= -8.0F &&
        state.speed <= 38.0F && state.race.completed_laps <= 99U &&
        std::isfinite(state.race.elapsed_seconds) &&
        state.race.elapsed_seconds >= 0.0F &&
        std::isfinite(state.route_distance_travelled);
}
}

std::array<std::uint8_t, race_state_payload_size> encode_race_state(
    const RouteMotionState& state)
{
    if (!valid_race_state(state))
        throw std::invalid_argument("race state is outside protocol bounds");
    std::array<std::uint8_t, race_state_payload_size> bytes{};
    bytes[0] = 'V'; bytes[1] = 'R'; bytes[2] = 'S'; bytes[3] = '1';
    write_u32(bytes.data() + 4U, static_cast<std::uint32_t>(state.route_segment));
    write_u32(bytes.data() + 8U,
        static_cast<std::uint32_t>(state.race.lap_start_segment));
    write_float(bytes.data() + 12U, state.route_fraction);
    write_float(bytes.data() + 16U, state.steering);
    write_float(bytes.data() + 20U, state.speed);
    write_u32(bytes.data() + 24U, state.race.completed_laps);
    write_float(bytes.data() + 28U, state.race.elapsed_seconds);
    write_float(bytes.data() + 32U, state.route_distance_travelled);
    bytes[36] = state.race.finished ? 1U : 0U;
    return bytes;
}

bool decode_race_state(const std::span<const std::uint8_t> bytes,
                       RouteMotionState& state) noexcept
{
    if (bytes.size() != race_state_payload_size || bytes[0] != 'V' ||
        bytes[1] != 'R' || bytes[2] != 'S' || bytes[3] != '1' ||
        bytes[36] > 1U || bytes[37] != 0U || bytes[38] != 0U ||
        bytes[39] != 0U) return false;

    RouteMotionState decoded{};
    decoded.route_segment = read_u32(bytes.data() + 4U);
    decoded.race.lap_start_segment = read_u32(bytes.data() + 8U);
    decoded.route_fraction = read_float(bytes.data() + 12U);
    decoded.steering = read_float(bytes.data() + 16U);
    decoded.speed = read_float(bytes.data() + 20U);
    decoded.race.completed_laps = read_u32(bytes.data() + 24U);
    decoded.race.elapsed_seconds = read_float(bytes.data() + 28U);
    decoded.route_distance_travelled = read_float(bytes.data() + 32U);
    decoded.race.finished = bytes[36] != 0U;
    if (!valid_race_state(decoded)) return false;
    state = decoded;
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
