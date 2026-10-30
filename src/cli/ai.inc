// No-window verification uses the same projection-only decision module as the client.
int runAi(const Content &content, const std::filesystem::path &assets, int argc, char **argv) {
    if (argc < 4)
        throw std::runtime_error("ai-match/tutorial-smoke requires a recording path");
    const auto path = std::filesystem::path(argv[3]);
    if (std::string(argv[1]) == "tutorial-smoke") {
        app::Application application(content, path.parent_path() / "tutorial-smoke-data",
                                     app::loadPresets(content, assets));
        std::string error;
        if (!application.startTutorial(error))
            throw std::runtime_error(error);
        std::size_t steps = 0;
        while (application.tutorial()->lesson != app::Lesson::Complete) {
            if (++steps > 500)
                throw std::runtime_error("tutorial did not complete");
            auto v = application.match().engine().viewFor(0);
            if (!v.opening && application.tutorial()->canContinue(v)) {
                application.continueTutorial();
                continue;
            }
            CommandResult result;
            if (application.aiTurn()) {
                const auto plan = application.planAi();
                if (!plan)
                    throw std::runtime_error("tutorial AI has no action");
                result = application.commitAi(*plan);
            } else {
                ai::CandidateSource candidates = [&](const auto &visitor) { application.visitActions(0, visitor); };
                const auto selected = ai::choose(v, candidates, ai::Difficulty::Normal);
                if (!selected)
                    throw std::runtime_error("student has no action at lesson " +
                                             application.tutorial()->title());
                result = application.submit(0, selected->command, application.generation());
            }
            if (!result.accepted)
                throw std::runtime_error(result.error);
        }
        writeJson(path, application.match().recording());
        const auto verified = replay(content, application.match().recording());
        std::cout << "tutorial complete; " << steps
                  << " steps; countered=" << application.tutorial()->countered
                  << "; enemy life=" << verified.state().players[1].life << "; digest=" << verified.digest()
                  << '\n';
        return 0;
    }
    const auto presets = app::loadPresets(content, assets);
    const auto difficulty = static_cast<ai::Difficulty>(argc > 5 ? std::stoi(argv[5]) : 1);
    (void)ai::difficultyLabel(difficulty);
    MatchConfig config;
    config.seed = argc > 4 ? static_cast<std::uint32_t>(std::stoul(argv[4])) : 42;
    config.players[0] = presets.at(std::min<std::size_t>(1, presets.size() - 1)).deck;
    config.players[1] =
        presets.at(std::min<std::size_t>(static_cast<std::size_t>(difficulty) + 1, presets.size() - 1)).deck;
    MatchSession session(content, config);
    int commands = 0;
    for (; session.engine().state().result == -1; ++commands) {
        if (commands >= 4000) {
            const auto &state = session.engine().state();
            throw std::runtime_error("AI duel did not terminate within verification budget at turn " +
                                     std::to_string(state.globalTurn) + " phase " + phaseName(state.phase) +
                                     ", life " + std::to_string(state.players[0].life) + "/" +
                                     std::to_string(state.players[1].life) + ", digest " + session.engine().digest());
        }
        const auto &s = session.engine().state();
        const int actor = s.decision ? s.decision->player : s.active;
        const auto selected =
            ai::choose(session.viewFor(actor), [&](const auto &visitor) { session.engine().visitLegalActions(actor, visitor); },
                       actor == 0 ? ai::Difficulty::Normal : difficulty);
        if (!selected)
            throw std::runtime_error("AI has no legal action");
        const auto result = session.submit(actor, selected->command);
        if (!result.accepted)
            throw std::runtime_error(result.error);
    }
    session.save(path);
    const auto verified = replay(content, session.recording());
    std::cout << "AI duel " << ai::difficultyLabel(difficulty) << ": " << commands
              << " commands; result=" << verified.state().result << "; digest=" << verified.digest() << '\n';
    return 0;
}
