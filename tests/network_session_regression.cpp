#include "viper/network_session.hpp"
#include "viper/network_race_input.hpp"

#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

int main() {
    using namespace viper::net;
    const RaceInput expected_input{-0.5F, 0.75F, 0.125F};
    const auto encoded_input = encode_race_input(expected_input);
    const std::array<std::uint8_t, race_input_payload_size> expected_wire_bytes{
        0xbf, 0x00, 0x00, 0x00, 0x3f, 0x40,
        0x00, 0x00, 0x3e, 0x00, 0x00, 0x00};
    RaceInput decoded_input{};
    if (encoded_input != expected_wire_bytes ||
        !decode_race_input(encoded_input, decoded_input) ||
        decoded_input.steering != expected_input.steering ||
        decoded_input.throttle != expected_input.throttle ||
        decoded_input.brake != expected_input.brake ||
        decode_race_input(std::span<const std::uint8_t>{}, decoded_input)) {
        std::cerr << "race input codec did not preserve bounded controls\n";
        return 1;
    }
    bool rejected_out_of_range = false;
    try { (void)encode_race_input({1.1F, 0.0F, 0.0F}); }
    catch (const std::invalid_argument&) { rejected_out_of_range = true; }
    if (!rejected_out_of_range) {
        std::cerr << "race input codec accepted out-of-range steering\n";
        return 1;
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
