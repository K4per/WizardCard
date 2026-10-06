static Rect menuRow(int n) {
    return {{970, 330 + 86.f * static_cast<float>(n)}, {460, 62}};
}
static Rect deckRow(int p) {
    return {{530, 280 + 155.f * static_cast<float>(p)}, {540, 48}};
}
static Rect baseRow(int p) {
    return {{530, 340 + 155.f * static_cast<float>(p)}, {540, 42}};
}
static Rect settingValue(int n) {
    return {{850, 233 + 70.f * static_cast<float>(n)}, {350, 46}};
}
static Rect volumeMinus(int n) {
    return {{948, 233 + 70.f * static_cast<float>(n)}, {64, 46}};
}
static Rect volumePlus(int n) {
    return {{1062, 233 + 70.f * static_cast<float>(n)}, {64, 46}};
}
const Rect pageBack_{{385, 810}, {250, 48}}, setupStart_{{820, 810}, {370, 48}},
    seedBox_{{530, 627}, {360, 48}};
const Rect applySettings_{{385, 640}, {250, 48}}, resetSettings_{{665, 640}, {230, 48}},
    closeSettings_{{930, 640}, {270, 48}};
const Rect replaySettings_{{385, 740}, {230, 48}}, surrenderSettings_{{650, 740}, {180, 48}},
    leaveSettings_{{865, 740}, {335, 48}};
const Rect confirmYes_{{820, 575}, {300, 52}}, confirmNo_{{480, 575}, {300, 52}};
const Rect resultRestart_{{558, 553}, {230, 48}}, resultMenu_{{812, 553}, {230, 48}};
const Rect motionButton_{{1370, 78}, {195, 32}}, motionSettings_{{850, 583}, {350, 46}};
app::Settings draftSettings_;
std::string seedText_{"42"};
bool seedFocused_{}, windowChange_{};
int aiDifficulty_{1};
std::size_t aiPreset_{1};
const Rect tutorialEntry_{{530, 745}, {540, 48}};
#include "ai_pages.inc"
std::set<DecisionKind> exercisedPauseKinds_;
std::set<std::size_t> exercisedSelectionKinds_;
std::vector<std::string> bases() const {
    std::vector<std::string> out;
    for (const auto &[id, d] : content_.catalog.cards)
        if (d.baseEligible)
            out.push_back(id);
    return out;
}
void pageHeading(sf::RenderTarget &t, const std::string &title, const std::string &subtitle) {
    frame(t, {{350, 115}, {900, 785}});
    label(t, title, 385, 145, 32, gold);
    label(t, subtitle, 385, 193, 16, muted);
}
void titleBackdrop(sf::RenderTarget &t) {
    // Fill the physical viewport, including the margins used by other aspect ratios.
    const auto logical = t.getView();
    t.setView(t.getDefaultView());
    const auto size = t.getSize();
    for (int n = 0; n < 100; ++n) {
        const auto shade = static_cast<std::uint8_t>(n < 50 ? n / 8 : (100 - n) / 8);
        box(t,
            {{0, static_cast<float>(size.y) * n / 100.f},
             {static_cast<float>(size.x), static_cast<float>(size.y) / 100.f + 1}},
            {static_cast<std::uint8_t>(10 + shade), static_cast<std::uint8_t>(19 + shade),
             static_cast<std::uint8_t>(27 + shade)});
    }
    t.setView(logical);
    const sf::Vector2f center{470, 500};
    for (int layer = 12; layer > 0; --layer) {
        const float radius = 220 + layer * 20.f;
        sf::CircleShape glow(radius, 96);
        glow.setOrigin({radius, radius});
        glow.setPosition(center);
        glow.setFillColor({56, 124, 122, 3});
        t.draw(glow);
    }
    for (const float radius : {295.f, 318.f, 365.f}) {
        sf::CircleShape circle(radius, 120);
        circle.setOrigin({radius, radius});
        circle.setPosition(center);
        circle.setFillColor(sf::Color::Transparent);
        circle.setOutlineColor({171, 131, 69, 45});
        circle.setOutlineThickness(1.5f);
        t.draw(circle);
    }
    sf::VertexArray sigil(sf::PrimitiveType::Lines, 12);
    std::array<sf::Vector2f, 6> points;
    for (int n = 0; n < 6; ++n) {
        const float angle = n * 3.14159265f / 3.f - 3.14159265f / 2.f;
        points[n] = center + sf::Vector2f{std::cos(angle), std::sin(angle)} * 295.f;
    }
    for (int n = 0; n < 6; ++n) {
        sigil[n * 2] = {points[n], {171, 131, 69, 32}};
        sigil[n * 2 + 1] = {points[(n + 2) % 6], {171, 131, 69, 32}};
    }
    t.draw(sigil);
    box(t, {{115, 903}, {1370, 1}}, {171, 131, 69, 65});
}
void drawPage(sf::RenderTarget &t) {
    const auto page = app_.page();
    if (page == app::Page::Menu) {
        titleBackdrop(t);
        auto titleText = [&](const std::string &s, float y, unsigned size, sf::Color color) {
            sf::Text measured(resources_.font(), utf8(s), size);
            const auto bounds = measured.getLocalBounds();
            label(t, s, 470 - bounds.size.x / 2 - bounds.position.x, y, size, color);
        };
        if (!drawTitleLogo(t)) {
            titleText("W I Z A R D C A R D", 352, 19, mint);
            titleText("巫 师 牌", 402, 80, {224, 190, 128});
        }
        titleText("巫 师 牌 · 本 地 奥 术 对 决", 610, 23, ink);
        titleText("构筑法阵 · 编织言灵 · 掌控连锁", 673, 19, muted);
        const std::array<std::string, 5> items{"单人测试对战（换手）", "单人AI对战", "卡组构筑", "游戏设置",
                                               "退出游戏"};
        for (int n = 0; n < 5; ++n)
            button(t, menuRow(n), items[static_cast<std::size_t>(n)], n == 0, true, true);
        label(t, "ALPHA-v1.0  /  " + std::string(programVersion), 115, 926, 14, muted);
        label(t, "本地对战", 1390, 926, 14, muted);
        if (!status_.empty())
            wrapped(t, resources_, status_, {970, 793}, 14, muted, 34);
        return;
    }
    if (page == app::Page::HotseatSetup || page == app::Page::AiSetup) {
        const bool ai = page == app::Page::AiSetup;
        pageHeading(t, ai ? "单人 AI 对战" : "换手对战配置",
                    ai ? "选择本方卡组、代表性AI卡组与难度；教学使用固定引导局。"
                       : "双方分别选择卡组；基础阵法在卡组编辑页面设置。");
        for (int p = 0; p < 2; ++p) {
            const auto preset = ai && p == 1 ? aiChoice() : selectedChoice(p);
            label(t, ai ? (p == 0 ? "本方" : "对手") : (p == 0 ? "玩家一" : "玩家二"), 385,
                  291 + 155.f * static_cast<float>(p), 23, gold);
            button(t, deckRow(p), "卡组：" + shortText(preset.name, 22) + "  ›");
            label(t, "基础阵法：" + content_.catalog.at(preset.deck.baseFormation).name,
                  530, 348 + 155.f * static_cast<float>(p), 19, mint);
            label(t, "30 张主卡组 · 同名最多 3 张 · 开局容量 " +
                std::to_string(content_.catalog.at(preset.deck.baseFormation).capacity) + "+" +
                std::to_string(content_.catalog.baseLoadCapacity) + "=" +
                std::to_string(content_.catalog.at(preset.deck.baseFormation).capacity + content_.catalog.baseLoadCapacity),
                530, 392 + 155.f * static_cast<float>(p), 14, muted);
        }
        if (ai) {
            label(t, "难度", 385, 636, 19, gold);
            button(t, seedBox_,
                   std::array<std::string, 3>{"简单", "普通", "困难"}.at(
                       static_cast<std::size_t>(aiDifficulty_)) +
                       "  ›");
            const auto choice = aiChoice();
            wrapped(t, resources_, choice.description, {385, 688}, 14, muted, 53);
            button(t, tutorialEntry_, "固定引导教学 · 开始学习", true);
        } else {
            label(t, "对局种子", 385, 638, 19, gold);
            box(t, seedBox_, panel, seedFocused_ ? mint : gold);
            label(t, seedText_ + (seedFocused_ ? "|" : ""), 545, 636, 20, ink);
            button(t, {{930, 627}, {270, 48}}, "更换种子");
            label(t, "相同种子和卡组可重放相同的开局。", 530, 692, 15, muted);
        }
        button(t, pageBack_, "返回主菜单");
        button(t, setupStart_, "开始对战", true);
    } else if (page == app::Page::Decks || page == app::Page::DeckEditor ||
               page == app::Page::ConfirmDeckDelete || page == app::Page::ConfirmDeckDiscard) {
        drawDecks(t);
        return;
    } else if (page == app::Page::Settings) {
        const bool inMatch = app_.hasMatch();
        pageHeading(t, inMatch ? "对局设置 · 已暂停" : "游戏设置",
                    "点击应用保存修改；返回将放弃尚未应用的修改。");
        for (int n = 0; n < 2; ++n) {
            int value = n ? draftSettings_.soundVolume : draftSettings_.masterVolume;
            label(t, n ? "音效音量" : "总音量", 385, 241 + 70.f * static_cast<float>(n), 20, gold);
            label(t, std::to_string(value) + " %", 850, 242 + 70.f * static_cast<float>(n), 20, mint);
            button(t, volumeMinus(n), "−", false, true, true);
            button(t, volumePlus(n), "+", false, true, true);
        }
        label(t, "窗口模式", 385, 381, 20, gold);
        button(t, settingValue(2),
               std::array<std::string, 3>{"窗口", "无边框", "全屏"}.at(
                   static_cast<std::size_t>(draftSettings_.windowMode)) +
                   "  ›");
        label(t, "分辨率", 385, 451, 20, gold);
        button(t, settingValue(3),
               std::to_string(draftSettings_.width) + " × " + std::to_string(draftSettings_.height) +
                   (draftSettings_.windowMode == app::WindowMode::Borderless ? " · 跟随桌面" : "  ›"),
               false, draftSettings_.windowMode != app::WindowMode::Borderless);
        label(t, "空阶段自动通过", 385, 521, 20, gold);
        button(t, settingValue(4), draftSettings_.automaticPhases ? "开启" : "关闭");
        label(t, "对局动画", 385, 591, 20, gold);
        button(t, motionSettings_, draftSettings_.reducedMotion ? "精简反馈" : "完整动画");
        button(t, applySettings_, "应用并保存", true);
        button(t, resetSettings_, "恢复默认");
        button(t, closeSettings_, inMatch ? "继续游戏" : "返回主菜单");
        if (inMatch) {
            button(t, replaySettings_, "保存复盘");
            button(t, surrenderSettings_, "投降", false, view_.result == -1);
            button(t, leaveSettings_, "返回主菜单");
            label(t, "对局状态与换手遮挡将保留，继续后恢复当前选择。", 385, 707, 15, muted);
        }
    } else if (page == app::Page::ConfirmLeave || page == app::Page::ConfirmSurrender) {
        frame(t, {{440, 300}, {720, 360}});
        label(t, page == app::Page::ConfirmSurrender ? "确认投降？" : "结束当前对局？", 480, 344, 30, gold);
        wrapped(t, resources_,
                page == app::Page::ConfirmSurrender ? "投降后保存复盘并显示对局结果。"
                                                    : "进行中的对局将以当前玩家投降结束，保存复盘后离开。",
                {480, 405}, 20, ink, 30);
        button(t, confirmNo_, "取消并继续");
        button(t, confirmYes_, "确认并保存", true);
    }
    if (!status_.empty())
        wrapped(t, resources_, status_, {385, 838}, 13, muted, 57);
}
void openSettings() {
    resetPointer();
    app_.openSettings();
    draftSettings_ = app_.settings();
    seedFocused_ = false;
}
void viewport(sf::RenderWindow &window) {
    auto size = window.getSize();
    if (!size.x || !size.y)
        return;
    sf::View logical(Rect({0, 0}, {1600, 1000}));
    float r = static_cast<float>(size.x) / static_cast<float>(size.y), b = 1.6f;
    logical.setViewport(r > b ? Rect({(1 - b / r) / 2, 0}, {b / r, 1})
                              : Rect({0, (1 - r / b) / 2}, {1, r / b}));
    window.setView(logical);
}
void applyWindow(sf::RenderWindow &window) {
    auto settings = app_.settings();
    sf::VideoMode mode({settings.width, settings.height});
    if (settings.windowMode == app::WindowMode::Borderless)
        mode = sf::VideoMode::getDesktopMode();
    if (settings.windowMode == app::WindowMode::Fullscreen && !mode.isValid()) {
        settings.windowMode = app::WindowMode::Windowed;
        status_ = "全屏分辨率不可用，已恢复窗口模式。";
        std::string error;
        if (!app_.applySettings(settings, error))
            status_ += " " + error;
    }
    window.create(mode, utf8("巫师牌 · Alpha v1.0"),
                  settings.windowMode == app::WindowMode::Borderless ? sf::Style::None : sf::Style::Default,
                  settings.windowMode == app::WindowMode::Fullscreen ? sf::State::Fullscreen
                                                                     : sf::State::Windowed);
    if (!window.isOpen())
        throw std::runtime_error("无法创建游戏窗口，请检查显示设置。");
    sf::Image icon;
    if (icon.loadFromFile(assets_ / "art/branding/wizardcard-app-v1.png"))
        window.setIcon(icon.getSize(), icon.getPixelsPtr());
    window.setFramerateLimit(60);
    viewport(window);
    resources_.setSoundVolume(settings.effectiveSoundVolume());
    windowChange_ = false;
    resetPointer();
}
void cycleResolution() {
    if (draftSettings_.windowMode == app::WindowMode::Borderless)
        return;
    std::vector<sf::Vector2u> sizes{{1280, 800}, {1600, 1000}, {1920, 1080}};
    if (draftSettings_.windowMode == app::WindowMode::Fullscreen) {
        sizes.clear();
        for (const auto &mode : sf::VideoMode::getFullscreenModes())
            if (mode.size.x >= 960 && mode.size.x <= 3840 && mode.size.y >= 600 && mode.size.y <= 2160 &&
                std::find(sizes.begin(), sizes.end(), mode.size) == sizes.end())
                sizes.push_back(mode.size);
    }
    if (sizes.empty()) {
        status_ = "没有可用的全屏分辨率。";
        return;
    }
    auto i = std::find(sizes.begin(), sizes.end(), sf::Vector2u{draftSettings_.width, draftSettings_.height});
    auto next = i == sizes.end() || ++i == sizes.end() ? sizes.front() : *i;
    draftSettings_.width = next.x;
    draftSettings_.height = next.y;
}
bool startConfigured() {
    try {
        if (seedText_.empty() || seedText_.size() > 10 ||
            seedText_.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("请输入有效的对局种子。");
        auto seed = std::stoull(seedText_);
        if (seed > 4294967295ull)
            throw std::runtime_error("对局种子不能超过 4294967295。");
        MatchConfig config;
        config.seed = static_cast<std::uint32_t>(seed);
        const bool aiMode = app_.page() == app::Page::AiSetup;
        for (int p = 0; p < 2; ++p) {
            config.players[p] = aiMode && p == 1 ? aiChoice().deck : selectedChoice(p).deck;
        }
        std::string error;
        if (!app_.start(config, error,
                        {aiMode ? app::MatchMode::Ai : app::MatchMode::Hotseat,
                         static_cast<ai::Difficulty>(aiDifficulty_)})) {
            status_ = error;
            cue(ui::SoundId::Reject);
            return false;
        }
        resetMatchUi();
        return true;
    } catch (const std::exception &e) {
        status_ = e.what();
        cue(ui::SoundId::Reject);
        return false;
    }
}
void resetMatchUi() {
    presentation_.clear();
    motionOrigins_.clear();
    pendingAi_.reset();
    aiWait_ = 0;
    handOrder_ = {};
    offsets_.clear();
    interaction_.cancel();
    status_.clear();
    showLog_ = false;
    seedFocused_ = false;
    refresh(true);
}
void escaped() {
    if (app_.page() != app::Page::Match || mouseDown_ || interaction_.selected() || interaction_.pending() ||
        showLog_)
        cue(ui::SoundId::Cancel);
    if (app_.page() == app::Page::DeckEditor && deckPressed_) {
        resetPointer();
        return;
    }
    resetPointer();
    const auto page = app_.page();
    if (page == app::Page::DeckEditor) {
        if (showDeckProblems_) {
            showDeckProblems_ = false;
            return;
        }
        if (deckFocus_ != DeckFocus::None)
            deckFocus_ = DeckFocus::None;
        else
            requestDeckLeave();
        return;
    }
    if (page == app::Page::ConfirmDeckDelete) {
        app_.navigate(app::Page::Decks);
        return;
    }
    if (page == app::Page::ConfirmDeckDiscard) {
        app_.navigate(app::Page::DeckEditor);
        deckLeaveQuits_ = false;
        return;
    }
    if (page == app::Page::Settings) {
        app_.closeSettings();
        return;
    }
    if (page == app::Page::ConfirmLeave || page == app::Page::ConfirmSurrender) {
        app_.cancelConfirmation();
        return;
    }
    if (page != app::Page::Match) {
        app_.navigate(app::Page::Menu);
        seedFocused_ = false;
        return;
    }
    if (showLog_ || interaction_.selected() || interaction_.pending()) {
        interaction_.cancel();
        showLog_ = false;
        return;
    }
    openSettings();
}
void seedInput(char32_t code) {
    if (app_.page() != app::Page::HotseatSetup || !seedFocused_)
        return;
    if (code == 8) {
        if (!seedText_.empty())
            seedText_.pop_back();
    } else if (code >= U'0' && code <= U'9' && seedText_.size() < 10)
        seedText_ += static_cast<char>(code);
}
void clickPage(sf::Vector2f p) {
    const auto page = app_.page();
    if (page == app::Page::Menu) {
        for (int n = 0; n < 5; ++n)
            if (hit(p, menuRow(n))) {
                status_.clear();
                if (n == 0)
                    app_.navigate(app::Page::HotseatSetup);
                if (n == 1)
                    app_.navigate(app::Page::AiSetup);
                if (n == 2)
                    app_.navigate(app::Page::Decks);
                if (n == 3)
                    openSettings();
                if (n == 4)
                    app_.requestLeave(true);
                return;
            }
    } else if (page == app::Page::HotseatSetup || page == app::Page::AiSetup) {
        if (hit(p, pageBack_)) {
            app_.navigate(app::Page::Menu);
            seedFocused_ = false;
            return;
        }
        for (int player = 0; player < 2; ++player) {
            if (hit(p, deckRow(player))) {
                if (page == app::Page::AiSetup && player == 1) {
                    aiPreset_ = (aiPreset_ + 1) % app_.presets().size();
                } else
                    nextChoice(player);
                return;
            }
        }
        if (page == app::Page::AiSetup) {
            if (hit(p, seedBox_))
                aiDifficulty_ = (aiDifficulty_ + 1) % 3;
            if (hit(p, setupStart_))
                startConfigured();
            if (hit(p, tutorialEntry_))
                startTutorial();
            return;
        }
        seedFocused_ = hit(p, seedBox_);
        if (hit(p, {{930, 627}, {270, 48}}))
            seedText_ = std::to_string(
                static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (hit(p, setupStart_))
            startConfigured();
    } else if (page == app::Page::Decks || page == app::Page::DeckEditor ||
               page == app::Page::ConfirmDeckDelete || page == app::Page::ConfirmDeckDiscard) {
        clickDecks(p);
        return;
    } else if (page == app::Page::Settings) {
        for (int n = 0; n < 2; ++n) {
            int &value = n ? draftSettings_.soundVolume : draftSettings_.masterVolume;
            if (hit(p, volumeMinus(n)))
                value = std::max(0, value - 10);
            if (hit(p, volumePlus(n)))
                value = std::min(100, value + 10);
        }
        if (hit(p, settingValue(2))) {
            draftSettings_.windowMode =
                static_cast<app::WindowMode>((static_cast<int>(draftSettings_.windowMode) + 1) % 3);
            if (draftSettings_.windowMode == app::WindowMode::Borderless) {
                auto size = sf::VideoMode::getDesktopMode().size;
                draftSettings_.width = size.x;
                draftSettings_.height = size.y;
            } else if (draftSettings_.windowMode == app::WindowMode::Fullscreen &&
                       !sf::VideoMode({draftSettings_.width, draftSettings_.height}).isValid())
                cycleResolution();
        }
        if (hit(p, settingValue(3)))
            cycleResolution();
        if (hit(p, settingValue(4)))
            draftSettings_.automaticPhases = !draftSettings_.automaticPhases;
        if (hit(p, motionSettings_))
            draftSettings_.reducedMotion = !draftSettings_.reducedMotion;
        if (hit(p, resetSettings_)) {
            draftSettings_ = {};
            status_ = "默认设置已选定，点击应用保存。";
        }
        if (hit(p, applySettings_)) {
            if (draftSettings_.windowMode == app::WindowMode::Fullscreen &&
                !sf::VideoMode({draftSettings_.width, draftSettings_.height}).isValid()) {
                status_ = "请选择当前屏幕支持的全屏分辨率。";
                cue(ui::SoundId::Reject);
                return;
            }
            std::string error;
            const auto previous = app_.settings();
            if (app_.applySettings(draftSettings_, error)) {
                if (previous.reducedMotion != draftSettings_.reducedMotion) {
                    presentation_.clear();
                    motionOrigins_.clear();
                }
                resources_.setSoundVolume(app_.settings().effectiveSoundVolume());
                cue(ui::SoundId::Confirm);
                status_ = "设置已保存。";
                windowChange_ = previous.width != draftSettings_.width ||
                                previous.height != draftSettings_.height ||
                                previous.windowMode != draftSettings_.windowMode;
            } else {
                status_ = error;
                cue(ui::SoundId::Reject);
            }
        }
        if (hit(p, closeSettings_)) {
            app_.closeSettings();
            status_.clear();
            return;
        }
        if (app_.hasMatch()) {
            if (hit(p, replaySettings_))
                save();
            if (hit(p, surrenderSettings_))
                app_.requestSurrender();
            if (hit(p, leaveSettings_))
                app_.requestLeave();
        }
    } else if (page == app::Page::ConfirmLeave || page == app::Page::ConfirmSurrender) {
        if (hit(p, confirmNo_)) {
            app_.cancelConfirmation();
            return;
        }
        if (hit(p, confirmYes_)) {
            std::string error;
            if (page == app::Page::ConfirmLeave) {
                if (app_.confirmLeave(viewer_, error)) {
                    interaction_.cancel();
                    resetPointer();
                    status_ = "对局记录已保存。";
                } else
                    status_ = error;
            } else {
                if (app_.confirmSurrender(viewer_, error)) {
                    refresh();
                    status_ = "对局记录已保存。";
                } else
                    status_ = error;
            }
        }
    }
}
void exerciseMenu(sf::RenderWindow &window) {
    auto require = [](bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    };
    auto tap = [&](Rect rect) {
        draw(window);
        auto p = rect.position + sf::Vector2f{20, 20};
        press(p);
        releasePointer(p);
    };
    require(app_.page() == app::Page::Menu && !app_.hasMatch(), "menu must start without a match");
    tap(menuRow(3));
    require(app_.page() == app::Page::Settings, "menu settings entry missing");
    draftSettings_ = {};
    draftSettings_.width = 1280;
    draftSettings_.height = 800;
    tap(volumeMinus(0));
    tap(settingValue(4));
    tap(motionSettings_);
    tap(applySettings_);
    if (windowChange_)
        applyWindow(window);
    require(window.getSize() == sf::Vector2u{1280, 800}, "resolution was not applied");
    require(app_.settings().masterVolume == 70 && !app_.settings().automaticPhases &&
                app_.settings().reducedMotion,
            "settings controls did not apply");
    app::Application loaded(content_, app_.userDirectory());
    require(loaded.settings().masterVolume == 70 && loaded.settings().reducedMotion,
            "settings did not persist");
    tap(settingValue(2));
    tap(applySettings_);
    if (windowChange_)
        applyWindow(window);
    require(app_.settings().windowMode == app::WindowMode::Borderless &&
                window.getSize() == sf::VideoMode::getDesktopMode().size,
            "borderless mode did not apply");
    tap(settingValue(2));
    tap(applySettings_);
    if (windowChange_)
        applyWindow(window);
    require(app_.settings().windowMode == app::WindowMode::Fullscreen, "fullscreen mode did not apply");
    draftSettings_.windowMode = app::WindowMode::Windowed;
    draftSettings_.width = 1280;
    draftSettings_.height = 800;
    tap(applySettings_);
    if (windowChange_)
        applyWindow(window);
    tap(closeSettings_);
    tap(menuRow(0));
    tap(deckRow(1));
    tap(setupStart_);
    require(app_.hasMatch() && scene_ == Scene::Handoff, "setup did not start with privacy handoff");
    require(app_.configuration().players[0].cards != app_.configuration().players[1].cards,
            "independent deck selection lost");
    const auto digest = app_.match().engine().digest();
    const auto epoch = app_.generation();
    tap(settingsButton);
    require(app_.page() == app::Page::Settings && scene_ == Scene::Handoff, "settings revealed handoff");
    require(!app_.submit(viewer_, AdvancePhase{view_.phaseGate}, epoch).accepted,
            "paused submission accepted");
    tap(replaySettings_);
    tap(closeSettings_);
    require(scene_ == Scene::Handoff && app_.match().engine().digest() == digest,
            "resume changed handoff state");
    tap(handoffButton);
    require(scene_ == Scene::Match, "handoff acceptance failed");
    tap(settingsButton);
    tap(leaveSettings_);
    tap(confirmNo_);
    require(app_.page() == app::Page::Settings, "leave cancel lost settings context");
    tap(surrenderSettings_);
    tap(confirmYes_);
    require(scene_ == Scene::Result, "settings surrender did not show result");
    tap(resultRestart_);
    require(scene_ == Scene::Handoff && app_.generation() != epoch, "result restart failed");
    require(!app_.submit(viewer_, Surrender{}, epoch).accepted, "old match operation accepted");
    tap(settingsButton);
    tap(leaveSettings_);
    tap(confirmYes_);
    require(app_.page() == app::Page::Menu && !app_.hasMatch(), "saved return to menu failed");
    tap(menuRow(1));
    tap(seedBox_);
    tap(setupStart_);
    require(app_.hasMatch() && app_.mode() == app::MatchMode::Ai && viewer_ == 0 && scene_ != Scene::Handoff,
            "AI setup did not start a private single-player match");
    tap(settingsButton);
    tap(leaveSettings_);
    tap(confirmYes_);
    tap(menuRow(2));
    tap(deckImportChoice_);
    require(app_.page() == app::Page::Decks, "deck entry missing");
    tap(deckListBack_);
    // Actual OS-close route preserves the live match until its confirmation is accepted.
    tap(menuRow(0));
    tap(setupStart_);
    const auto closeDigest = app_.match().engine().digest();
    app_.requestLeave(true);
    tap(confirmNo_);
    require(!app_.quitRequested() && app_.match().engine().digest() == closeDigest,
            "close cancellation changed match");
    tap(settingsButton);
    tap(leaveSettings_);
    tap(confirmYes_);
    std::cout << "Menu/settings UI smoke passed: persistence, resolution, privacy, independent decks, pause, "
                 "cancel, surrender, restart and saved return\n";
}
void pauseAtDecision(sf::RenderWindow &window, const Command &command) {
    const auto digest = app_.match().engine().digest();
    const auto decision = view_.decision ? view_.decision->id : 0;
    const auto scene = scene_;
    const auto selected = interaction_.selected();
    const auto step = interaction_.step();
    const auto pending = interaction_.pending() ? encodeCommand(interaction_.pending()->command) : Json{};
    draw(window);
    auto p = settingsButton.position + sf::Vector2f{20, 20};
    press(p);
    releasePointer(p);
    if (app_.page() != app::Page::Settings || app_.submit(viewer_, command, app_.generation()).accepted)
        throw std::runtime_error("pending decision did not pause");
    const auto frozen = presentation_.cues();
    updatePresentation(1);
    if (presentation_.cues().size() != frozen.size())
        throw std::runtime_error("settings expired animations");
    for (std::size_t n = 0; n < frozen.size(); ++n)
        if (presentation_.cues()[n].age != frozen[n].age)
            throw std::runtime_error("settings advanced animation clock");
    draw(window);
    p = closeSettings_.position + sf::Vector2f{20, 20};
    press(p);
    releasePointer(p);
    const auto restored = interaction_.pending() ? encodeCommand(interaction_.pending()->command) : Json{};
    if (app_.page() != app::Page::Match || scene_ != scene ||
        (view_.decision ? view_.decision->id : 0) != decision || app_.match().engine().digest() != digest ||
        interaction_.selected() != selected || interaction_.step() != step || restored != pending)
        throw std::runtime_error("settings changed pending decision");
}

#include "decks.inc"
