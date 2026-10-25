#include "viper/network_session.hpp"

#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

int main() {
    using namespace viper::net;
    constexpr std::uint16_t port=39173;
    std::exception_ptr host_error;
    std::thread host([&] {
        try {
            auto session=Session::host(port);
            StateSnapshot snapshot;
            if (!session.receive_state(snapshot) || snapshot.sequence!=1 || snapshot.simulation_tick!=10 || snapshot.payload!=std::vector<std::uint8_t>{1,2}) throw std::runtime_error("host did not receive first client snapshot");
            if (!session.receive_state(snapshot) || snapshot.sequence!=3 || snapshot.simulation_tick!=30) throw std::runtime_error("host did not receive newest client snapshot");
            if (session.receive_state(snapshot)) throw std::runtime_error("host accepted stale client snapshot");
            const std::array<std::uint8_t,2> reply{8,9};
            session.send_state(1,40,reply);
        } catch (...) { host_error=std::current_exception(); }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    try {
        auto client=Session::join("127.0.0.1",port);
        const std::array<std::uint8_t,2> first{1,2}, third{3,4}, stale{2,2};
        client.send_state(1,10,first); client.send_state(3,30,third); client.send_state(2,20,stale);
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
