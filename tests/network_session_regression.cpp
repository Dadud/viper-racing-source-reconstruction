#include "viper/network_session.hpp"
#include "viper/network_race_state.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

int main() {
    using namespace viper::net;
    viper::RouteMotionState expected_race_state{};
    expected_race_state.route_segment = 2U;
    expected_race_state.race.lap_start_segment = 1U;
    expected_race_state.route_fraction = 0.25F;
    expected_race_state.steering = -0.5F;
    expected_race_state.speed = 12.5F;
    expected_race_state.race.completed_laps = 2U;
    expected_race_state.race.elapsed_seconds = 123.5F;
    expected_race_state.route_distance_travelled = -42.25F;
    expected_race_state.race.finished = true;
    const auto encoded_race_state = encode_race_state(expected_race_state);
    const std::array<std::uint8_t, race_state_payload_size>
        expected_race_state_bytes{
            'V', 'R', 'S', '1',
            0, 0, 0, 2, 0, 0, 0, 1,
            0x3e, 0x80, 0, 0, 0xbf, 0, 0, 0,
            0x41, 0x48, 0, 0, 0, 0, 0, 2,
            0x42, 0xf7, 0, 0, 0xc2, 0x29, 0, 0,
            1, 0, 0, 0};
    viper::RouteMotionState decoded_race_state{};
    if (encoded_race_state != expected_race_state_bytes ||
        !decode_race_state(encoded_race_state, decoded_race_state) ||
        decoded_race_state.route_segment != expected_race_state.route_segment ||
        decoded_race_state.race.lap_start_segment !=
            expected_race_state.race.lap_start_segment ||
        decoded_race_state.route_fraction != expected_race_state.route_fraction ||
        decoded_race_state.steering != expected_race_state.steering ||
        decoded_race_state.speed != expected_race_state.speed ||
        decoded_race_state.race.completed_laps !=
            expected_race_state.race.completed_laps ||
        decoded_race_state.race.elapsed_seconds !=
            expected_race_state.race.elapsed_seconds ||
        decoded_race_state.route_distance_travelled !=
            expected_race_state.route_distance_travelled ||
        decoded_race_state.race.finished != expected_race_state.race.finished) {
        std::cerr << "race state codec did not preserve route and finish state\n";
        return 1;
    }
    auto malformed_race_state = encoded_race_state;
    malformed_race_state[36] = 2U;
    if (decode_race_state(malformed_race_state, decoded_race_state)) {
        std::cerr << "race state codec accepted unknown flag bits\n";
        return 1;
    }
    auto out_of_range_race_state = expected_race_state;
    out_of_range_race_state.speed = 39.0F;
    bool rejected_invalid_race_state = false;
    try { (void)encode_race_state(out_of_range_race_state); }
    catch (const std::invalid_argument&) { rejected_invalid_race_state = true; }
    if (!rejected_invalid_race_state) {
        std::cerr << "race state codec accepted speed outside route model bounds\n";
        return 1;
    }
    const RaceConfiguration expected_configuration{
        "bemidji", "viper", 3U, 0U, 8U, 2U};
    const auto encoded_configuration = encode_race_configuration(
        expected_configuration);
    const std::array<std::uint8_t, 23> expected_configuration_bytes{
        'V', 'R', 'C', '2', 7, 5, 0, 3, 0, 8, 2,
        'b', 'e', 'm', 'i', 'd', 'j', 'i', 'v', 'i', 'p', 'e', 'r'};
    RaceConfiguration decoded_configuration;
    if (!std::equal(encoded_configuration.begin(), encoded_configuration.end(),
            expected_configuration_bytes.begin(), expected_configuration_bytes.end()) ||
        !decode_race_configuration(encoded_configuration, decoded_configuration) ||
        decoded_configuration != expected_configuration) {
        std::cerr << "race configuration codec did not preserve the setup\n";
        return 1;
    }
    auto peer_configuration = expected_configuration;
    peer_configuration.player_slot = 7U;
    auto reused_slot_configuration = expected_configuration;
    if (!compatible_race_configurations(expected_configuration,
            peer_configuration) ||
        compatible_race_configurations(expected_configuration,
            reused_slot_configuration)) {
        std::cerr << "race configuration did not enforce distinct peer slots\n";
        return 1;
    }
    auto malformed_configuration = encoded_configuration;
    malformed_configuration[4] = 33U;
    if (decode_race_configuration(malformed_configuration, decoded_configuration)) {
        std::cerr << "race configuration codec accepted an oversized track name\n";
        return 1;
    }
    bool rejected_invalid_configuration = false;
    try {
        (void)encode_race_configuration({"../bemidji", "viper", 3U, 0U, 8U});
    } catch (const std::invalid_argument&) {
        rejected_invalid_configuration = true;
    }
    if (!rejected_invalid_configuration) {
        std::cerr << "race configuration codec accepted a path-like resource name\n";
        return 1;
    }
    try {
        (void)encode_race_configuration(
            {"bemidji", "viper", 3U, 0U, 8U, 8U});
        std::cerr << "race configuration accepted an out-of-range player slot\n";
        return 1;
    } catch (const std::invalid_argument&) {
    }
    constexpr std::uint16_t port=39173;
    std::exception_ptr host_error;
    std::thread host([&] {
        try {
            auto session=Session::host(port);
            StateSnapshot snapshot;
            if (!session.receive_state(snapshot) || snapshot.sequence!=0xfffffffeu || snapshot.simulation_tick!=10 || snapshot.payload!=std::vector<std::uint8_t>{1,2}) throw std::runtime_error("host did not receive first client snapshot");
            if (!session.receive_state(snapshot) || snapshot.sequence!=0xffffffffu || snapshot.simulation_tick!=20) throw std::runtime_error("host did not receive pre-wrap client snapshot");
            if (!session.receive_state(snapshot) || snapshot.sequence!=0 || snapshot.simulation_tick!=30) throw std::runtime_error("host did not accept wrapped client sequence");
            if (session.receive_state(snapshot)) throw std::runtime_error("host accepted stale client snapshot");
            const std::array<std::uint8_t,2> reply{8,9};
            session.send_state(1,40,reply);
        } catch (...) { host_error=std::current_exception(); }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    try {
        auto client=Session::join("127.0.0.1",port);
        const std::array<std::uint8_t,2> first{1,2}, second{3,4}, third{5,6}, stale{7,8};
        client.send_state(0xfffffffeu,10,first);
        client.send_state(0xffffffffu,20,second);
        client.send_state(0,30,third);
        client.send_state(0xffffffffu,20,stale);
        StateSnapshot received;
        if (!client.receive_state(received) || received.sequence!=1 || received.simulation_tick!=40 || received.payload!=std::vector<std::uint8_t>{8,9}) throw std::runtime_error("client did not receive host snapshot");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        if (host.joinable()) host.join();
        return 1;
    }
    host.join();
    if (host_error) {
        try { std::rethrow_exception(host_error); } catch (const std::exception& e) { std::cerr << e.what() << '\n'; }
        return 1;
    }
    return 0;
}
