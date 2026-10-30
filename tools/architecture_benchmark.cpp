#include "wizard/application.hpp"
#include "wizard/ai.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/network.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

using namespace wizard;
using Timer = std::chrono::steady_clock;

namespace {
double elapsedMs(Timer::time_point start) {
    return std::chrono::duration<double, std::milli>(Timer::now() - start).count();
}

Json stats(std::vector<double> samples) {
    if (samples.empty()) return {{"samples", 0}};
    std::sort(samples.begin(), samples.end());
    return {{"samples", samples.size()}, {"p50_ms", samples[samples.size() / 2]},
            {"p95_ms", samples[std::min(samples.size() - 1, samples.size() * 95 / 100)]}};
}

std::uint64_t peakWorkingSet() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX memory{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), sizeof(memory)))
        return memory.PeakWorkingSetSize;
#endif
    return 0;
}
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "usage: wizard_architecture_benchmark <assets> <output.json>\n";
        return 2;
    }

    try {
        const std::filesystem::path assets = argv[1];
        const std::filesystem::path output = argv[2];
        const auto content = loadContent(assets);
        const auto presets = app::loadPresets(content, assets);
        Json measurements = Json::array();

        for (const auto seed : {42u, 71u, 314u}) {
            MatchConfig config;
            config.seed = seed;
            config.players[0] = presets.at(1).deck;
            config.players[1] = presets.at(2).deck;
            MatchSession session(content, config);
            std::vector<double> projections, queries, reads, submits;
            std::uint64_t projectionBytes{};
            for (std::size_t step = 0; step < 600 && session.engine().state().result == -1; ++step) {
                const auto &state = session.engine().state();
                const auto actor = state.decision ? state.decision->player : state.active;
                auto start = Timer::now();
                const auto view = session.viewFor(actor);
                const auto projectionMs = elapsedMs(start);
                projections.push_back(projectionMs);
                projectionBytes += net::encodeView(view).dump().size();

                start = Timer::now();
                const auto options = session.queryActions(actor, 1, session.revision(), ActionIntent{});
                const auto queryMs = elapsedMs(start);
                if (!options.error.empty()) throw std::runtime_error("action query failed: " + options.error);
                queries.push_back(queryMs);
                reads.push_back(projectionMs + queryMs);

                const ai::CandidateSource candidates = [&](const auto &visitor) {
                    session.engine().visitLegalActions(actor, visitor);
                };
                const auto choice = ai::choose(view, candidates, ai::Difficulty::Normal, 512);
                if (!choice) throw std::runtime_error("AI produced no candidate for seed " + std::to_string(seed));
                start = Timer::now();
                const auto result = session.submit(actor, choice->command);
                submits.push_back(elapsedMs(start));
                if (!result.accepted) throw std::runtime_error("AI command rejected for seed " +
                    std::to_string(seed) + " at step " + std::to_string(step) + ": " + result.error);
            }

            measurements.push_back({{"seed", seed}, {"commands", submits.size()},
                {"projection", stats(std::move(projections))}, {"query", stats(std::move(queries))},
                {"projectionPlusQuery", stats(std::move(reads))}, {"submit", stats(std::move(submits))},
                {"projectionBytes", projectionBytes}, {"peakWorkingSetBytes", peakWorkingSet()},
                {"turn", session.engine().state().globalTurn}, {"result", session.engine().state().result}});
        }

        std::filesystem::create_directories(output.parent_path());
        writeJson(output, Json{{"programVersion", programVersion}, {"rulesVersion", content.catalog.rulesVersion},
            {"mechanicsRevision", content.catalog.mechanicsRevision}, {"contentHash", content.catalog.contentHash},
            {"scenario", "normal-ai-fixed-seeds-600-commands"}, {"runs", measurements}});
        std::cout << readJson(output).dump(2) << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
