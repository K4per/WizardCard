// Included inside Client. The AI's projection and pending command never enter the render view.
std::optional<app::AiPlan> pendingAi_;
float aiWait_{};
const Rect tutorialContinue_{{37, 267}, {93, 34}}, tutorialRestart_{{136, 267}, {91, 34}};
app::DeckPreset aiChoice() const {
    return app_.presets().at(aiPreset_ % app_.presets().size());
}
bool startTutorial() {
    std::string error;
    if (!app_.startTutorial(error)) {
        status_ = error;
        return false;
    }
    resetMatchUi();
    return true;
}
void restartTutorial() {
    std::string error;
    app_.requestLeave();
    if (!app_.confirmLeave(0, error)) {
        status_ = error;
        return;
    }
    startTutorial();
}
void tickAi(float dt) {
    if (app_.paused() || !app_.aiTurn()) {
        pendingAi_.reset();
        aiWait_ = 0;
        return;
    }
    if (mouseDown_ || showLog_)
        return;
    if (presentation_.busy() && !smokeDriver_)
        return;
    if (!pendingAi_) {
        pendingAi_ = app_.planAi();
        aiWait_ = 0;
    }
    if (!pendingAi_) {
        status_ = "AI暂时没有可提交的操作。";
        return;
    }
    aiWait_ += dt;
    if (aiWait_ < (app_.mode() == app::MatchMode::Tutorial ? .9f : .45f))
        return;
    const auto result = app_.commitAi(*pendingAi_);
    pendingAi_.reset();
    aiWait_ = 0;
    if (!result.accepted) {
        status_ = result.error;
        return;
    }
    refresh();
    std::string error;
    status_ = app_.saveReplay(error) ? "AI操作完成" : error;
}
void drawTutorial(sf::RenderTarget &t) {
    const auto *lesson = app_.tutorial();
    if (!lesson || scene_ == Scene::Result)
        return;
    frame(t, {{25, 132}, {215, 178}}, lesson->lesson == app::Lesson::Complete ? mint : gold);
    label(t, "教学 " + std::to_string(static_cast<int>(lesson->lesson) + 1) + "/12 · " + lesson->title(), 37,
          141, 13, gold);
    wrapped(t, resources_, lesson->hint(view_), {37, 168}, 11, ink, 17);
    button(t, tutorialContinue_, lesson->lesson == app::Lesson::Intro ? "开始" : "继续", true,
           lesson->canContinue(view_));
    button(t, tutorialRestart_, "重开教学");
}
void exerciseAi(sf::RenderWindow &window) {
    auto require = [](bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    };
    auto tap = [&](Rect r) {
        draw(window);
        auto p = r.position + sf::Vector2f{12, 15};
        press(p);
        releasePointer(p);
    };
    auto assertPrivacy = [&] {
        require(viewer_ == 0 && scene_ != Scene::Handoff, "AI match switched the rendered viewer");
        for (const auto &c : view_.cards)
            require(c.instance.zone != Zone::Deck && (c.instance.zone != Zone::Hand || c.instance.owner == 0),
                    "AI private cards entered the render view");
        for (const auto &e : view_.events)
            require(e.audience == -1 || e.audience == 0, "AI private log leaked");
        for (const auto &cue : presentation_.cues())
            if (cue.card)
                require(cue.card->instance.zone != Zone::Deck &&
                            (cue.card->instance.zone != Zone::Hand || cue.card->instance.owner == 0),
                        "AI private card entered an animation snapshot");
    };
    auto humanAction = [&](GameView projected) {
        if (const auto *lesson = app_.tutorial()) {
            projected.actions.erase(
                std::remove_if(projected.actions.begin(), projected.actions.end(),
                               [&](const LegalAction &a) { return !lesson->allows(projected, a.command); }),
                projected.actions.end());
        }
        const auto chosen = ai::choose(projected, ai::Difficulty::Normal);
        require(bool(chosen), "student has no available tutorial step");
        // Compute the expected digest in an isolated session, then execute through actual UI controls.
        auto expected = app_.match();
        const auto result = expected.submit(0, chosen->command);
        require(result.accepted, "student's chosen command was not legal");
        drive(window, Json{{"actor", 0},
                           {"command", encodeCommand(chosen->command)},
                           {"digest", expected.engine().digest()}});
    };
    for (int d = 0; d < 3; ++d) {
        tap(menuRow(1));
        aiDifficulty_ = d;
        aiPreset_ = static_cast<std::size_t>(d + 1) % app_.presets().size();
        tap(setupStart_);
        require(app_.hasMatch() && app_.difficulty() == static_cast<ai::Difficulty>(d),
                "difficulty did not apply");
        if (d == 0) {
            const auto digest = app_.match().engine().digest();
            const bool previous = app_.settings().reducedMotion;
            tap(motionButton_);
            require(app_.settings().reducedMotion != previous && app_.match().engine().digest() == digest,
                    "animation toggle changed match or did not apply");
            app::Application loaded(content_, app_.userDirectory());
            require(loaded.settings().reducedMotion == app_.settings().reducedMotion,
                    "in-match animation preference did not persist");
            tap(motionButton_);
        }
        bool pausedAi = false;
        for (int n = 0; app_.match().engine().state().result == -1; ++n) {
            require(n < 2500, "AI UI duel did not terminate");
            assertPrivacy();
            if (app_.aiTurn()) {
                if (!pausedAi) {
                    tickAi(0);
                    require(bool(pendingAi_), "no pending AI plan");
                    const auto savedPlan = *pendingAi_;
                    const auto digest = app_.match().engine().digest();
                    tap(settingsButton);
                    tickAi(2);
                    require(app_.match().engine().digest() == digest && !app_.commitAi(savedPlan).accepted,
                            "settings did not pause pending AI");
                    tap(closeSettings_);
                    pausedAi = true;
                }
                tickAi(1);
            } else
                humanAction(view_);
            draw(window);
        }
        require(pausedAi && scene_ == Scene::Result, "AI duel never reached a private result");
        require(replay(content_, app_.match().recording()).digest() == app_.match().engine().digest(),
                "AI UI replay mismatch");
        tap(resultRestart_);
        require(app_.mode() == app::MatchMode::Ai && app_.difficulty() == static_cast<ai::Difficulty>(d),
                "AI restart lost mode or difficulty");
        tap(settingsButton);
        tap(leaveSettings_);
        tap(confirmYes_);
    }
    tap(menuRow(1));
    tap(tutorialEntry_);
    require(app_.tutorial() && app_.configuration().seed == 39, "fixed tutorial did not start");
    const auto first = app_.match().engine().digest();
    tap(tutorialRestart_);
    require(app_.match().engine().digest() == first, "tutorial restart changed opening");
    std::set<app::Lesson> visited;
    for (int n = 0; app_.tutorial()->lesson != app::Lesson::Complete; ++n) {
        require(n < 500, "tutorial did not complete");
        assertPrivacy();
        visited.insert(app_.tutorial()->lesson);
        if (app_.tutorial()->canContinue(view_))
            tap(tutorialContinue_);
        else if (app_.aiTurn())
            tickAi(1);
        else
            humanAction(view_);
        draw(window);
    }
    require(visited.count(app::Lesson::Response) && app_.tutorial()->countered && view_.players[1].life == startingLife - 10,
            "tutorial skipped real counter or damage");
    require(replay(content_, app_.match().recording()).digest() == app_.match().engine().digest(),
            "tutorial UI replay mismatch");
    tap(settingsButton);
    tap(leaveSettings_);
    tap(confirmYes_);
    std::cout
        << "AI/tutorial UI smoke passed: three difficulties, private single-player view, pending-AI settings "
           "pause, full duels/replays, restart, fixed guided tutorial with real cancellation and release\n";
}
