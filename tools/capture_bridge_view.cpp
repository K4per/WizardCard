#include "wizard/bridge.hpp"
#include <algorithm>
#include <iostream>

// Select a real complex public board from a replay for visual layout review.
int main(int argc, char **argv) {
    try {
        if (argc != 4) throw std::runtime_error("wizard_capture_view ASSETS REPLAY OUTPUT");
        const auto content = wizard::loadContent(argv[1]);
        const auto record = wizard::readJson(argv[2]);
        (void)wizard::replay(content, record);
        wizard::MatchConfig config;
        config.seed = record.at("seed");
        for (int p = 0; p < 2; ++p) {
            config.players[p].baseFormation = record.at("players").at(p).at("baseFormation");
            config.players[p].cards = record.at("players").at(p).at("cards").get<std::vector<std::string>>();
        }
        wizard::GameEngine engine(content.catalog, config);
        wizard::Json best;
        int bestScore = -1, step = 0;
        for (const auto &row : record.at("commands")) {
            const auto result = engine.submit(row.at("actor"), wizard::decodeCommand(row.at("command")));
            if (!result.accepted) throw std::runtime_error(result.error);
            ++step;
            const auto v = engine.viewFor(0);
            if (v.result != -1 || !v.chain || v.chain->mode != wizard::ChainMode::Building ||
                v.players[0].handCount < 3 || v.players[0].deckCount < 5 || v.players[1].deckCount < 5) continue;
            int score = static_cast<int>(v.chain->links.size()) * 30;
            for (const auto &c : v.cards)
                if (c.instance.zone != wizard::Zone::Hand && c.instance.zone != wizard::Zone::Ash)
                    score += c.definition.type == wizard::CardType::Formation ? 3 : 5;
            if (score <= bestScore) continue;
            bestScore = score;
            best = wizard::bridge::viewDto(v);
            best["capturedStep"] = step;
            best["sourceReplay"] = std::filesystem::path(argv[2]).filename().u8string();
            best["sourceDigest"] = engine.digest();
            best["rulesVersion"] = content.catalog.rulesVersion;
            best["cardSetVersion"] = content.catalog.cardSetVersion;
        }
        if (best.is_null()) throw std::runtime_error("recording has no suitable cast-window board");
        wizard::writeJson(argv[3], best);
        std::cout << "Captured real viewer 0 board at step " << best.at("capturedStep") << '\n';
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
