#include "wizard/ai.hpp"
#include "wizard/network.hpp"
#include <iostream>
#include <thread>
using namespace wizard;
int main(int argc, char **argv) {
    try {
        if (argc != 7)
            throw std::runtime_error("Usage: wizard_network_probe host|guest assets output port "
                                     "normal|surrender|reconnect address");
        auto c = loadContent(argv[2]);
        net::Peer peer(c, std::filesystem::path(argv[3]));
        PlayerDeck d;
        d.cards = c.deck;
        auto port = std::stoul(argv[4]);
        if (!port || port > 65535)
            throw std::runtime_error("Invalid port");
        bool host = std::string(argv[1]) == "host";
        std::string scenario = argv[5];
        if (host)
            peer.host(static_cast<unsigned short>(port), d);
        else
            peer.join(argv[6], static_cast<unsigned short>(port));
        bool deck = host, reconnected = false;
        std::size_t commands = 0;
        std::string lastNotice;
        auto terminal = std::chrono::steady_clock::time_point{};
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(180);
        while (std::chrono::steady_clock::now() < until) {
            peer.tick();
            if (lastNotice != peer.notice()) {
                lastNotice = peer.notice();
                std::cerr << "status: " << lastNotice << " revision=" << peer.revision()
                          << " result=" << (peer.started() ? peer.view().result : -1) << '\n';
            }
            if (peer.ended() && (!peer.started() || peer.view().result == -1))
                throw std::runtime_error(peer.notice());
            if (peer.connected() && !peer.started()) {
                if (!deck) {
                    peer.deck(d);
                    deck = true;
                }
                const auto &room = peer.room();
                auto player = room.at("hostPlayer").get<int>();
                if (!host)
                    player = 1 - player;
                if (!room.at("ready").at(player).get<bool>()) {
                    peer.ready();
                }
            }
            if (peer.started() && (peer.view().result != -1 || !peer.blocked())) {
                const auto &v = peer.view();
                if (v.result != -1) {
                    if (host) {
                        if (terminal == std::chrono::steady_clock::time_point{})
                            terminal = std::chrono::steady_clock::now();
                        if (!peer.replayDelivered()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(2));
                            continue;
                        }
                    }
                    std::string e;
                    if (peer.save(e)) {
                        peer.tick(); // flush the guest's delivery acknowledgement before exit
                        auto record = readJson(argv[3]);
                        auto result = replay(c, record);
                        std::cout << Json{{"scenario", scenario},
                                          {"host", host},
                                          {"result", result.state().result},
                                          {"digest", result.digest()},
                                          {"commands", record.at("commands").size()},
                                          {"reconnected", reconnected}}
                                         .dump()
                                  << '\n';
                        return 0;
                    }
                } else if (!host && scenario == "surrender" && commands >= 2) {
                    auto r = peer.submit(Surrender{});
                    if (!r.accepted)
                        throw std::runtime_error(r.error);
                    ++commands;
                } else if (!host && scenario == "reconnect" && commands >= 3 && !reconnected) {
                    peer.reconnect();
                    reconnected = true;
                } else if (auto a = ai::choose(v, ai::Difficulty::Normal)) {
                    auto r = peer.submit(a->command);
                    if (!r.accepted)
                        throw std::runtime_error(r.error);
                    ++commands;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        throw std::runtime_error("Network probe timed out: " + peer.notice());
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
