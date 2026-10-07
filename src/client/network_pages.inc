std::string networkAddress_{"127.0.0.1"}, networkPort_{"27861"};
int networkFocus_{};
std::string networkNotice_;
bool networkHostFirst_{true}, networkAutomatic_{true}, networkDeckSent_{};
const Rect networkAddressBox_{{530, 270}, {620, 48}}, networkPortBox_{{530, 340}, {260, 48}},
    networkCreate_{{385, 650}, {350, 52}}, networkJoin_{{820, 650}, {350, 52}},
    networkReady_{{820, 710}, {350, 52}}, networkSeat_{{530, 440}, {620, 48}},
    networkAuto_{{530, 510}, {620, 48}}, networkDeck_{{530, 580}, {620, 48}};
void networkText(char32_t code) {
    if (app_.page() != app::Page::NetworkSetup || !networkFocus_)
        return;
    auto &s = networkFocus_ == 1 ? networkAddress_ : networkPort_;
    if (code == 8) {
        if (!s.empty())
            s.pop_back();
    } else if ((code >= U'0' && code <= U'9') || (code == U'.' && networkFocus_ == 1))
        if (s.size() < (networkFocus_ == 1 ? 15u : 5u))
            s += static_cast<char>(code);
}
void drawNetwork(sf::RenderTarget &t) {
    auto *peer = app_.network();
    bool room = app_.page() == app::Page::NetworkRoom;
    pageHeading(t, room ? "真人联机 · 房间" : "真人联机 · 局域网",
                room ? "双方选择卡组并准备；更改配置会清除准备状态。"
                     : "房主创建房间，另一位玩家输入房主的局域网或虚拟局域网IPv4地址。");
    if (!room) {
        label(t, "房主地址", 385, 281, 20, gold);
        box(t, networkAddressBox_, panel, networkFocus_ == 1 ? mint : gold);
        label(t, networkAddress_ + (networkFocus_ == 1 ? "|" : ""), 545, 280, 20);
        label(t, "端口", 385, 351, 20, gold);
        box(t, networkPortBox_, panel, networkFocus_ == 2 ? mint : gold);
        label(t, networkPort_ + (networkFocus_ == 2 ? "|" : ""), 545, 350, 20);
        wrapped(t, resources_,
                "默认端口27861。创建房间时允许Windows防火墙的专用网络访问；对方使用你的网卡IPv4地址。127.0.0."
                "1仅用于同机测试。",
                {385, 420}, 18, muted, 47);
        button(t, networkCreate_, "创建房间", true);
        button(t, networkJoin_, "加入房间", true);
    } else if (peer) {
        label(t,
              peer->isHost() ? "你是房主 · 监听端口 " + networkPort_
                             : "房主：" + networkAddress_ + ":" + networkPort_,
              385, 270, 20, gold);
        label(t, peer->connected() ? "连接已建立" : "等待连接或重连", 385, 315, 20,
              peer->connected() ? mint : muted);
        const auto &r = peer->room();
        if (!r.is_null()) {
            auto ready = r.at("ready").get<std::array<bool, 2>>();
            auto hp = r.at("hostPlayer").get<int>();
            label(t,
                  std::string("房主：") + (ready[hp] ? "已准备" : "未准备") + "    对方：" +
                      (ready[1 - hp] ? "已准备" : "未准备"),
                  385, 365, 20);
            networkHostFirst_ = hp == 0;
            networkAutomatic_ = r.at("automatic").get<bool>();
        }
        button(t, networkSeat_, networkHostFirst_ ? "房主先手" : "客机先手", false,
               peer->isHost() && !peer->started());
        button(t, networkAuto_, networkAutomatic_ ? "空阶段自动通过：开启" : "空阶段自动通过：关闭", false,
               peer->isHost() && !peer->started());
        auto choice = selectedChoice(0);
        button(t, networkDeck_, "本方卡组：" + shortText(choice.name, 22) + " ›", false,
               peer->connected() && !peer->started());
        button(t, networkReady_, "准备对战", true, peer->connected() && !peer->started());
        wrapped(t, resources_, peer->notice(), {385, 780}, 15, muted, 50);
    }
    button(t, pageBack_, "返回主菜单");
    if (!status_.empty())
        wrapped(t, resources_, status_, {385, 866}, 14, muted, 52);
}
void clickNetwork(sf::Vector2f p) {
    try {
        if (hit(p, pageBack_)) {
            if (app_.network()) {
                std::string error;
                if (!app_.leaveNetwork(error))
                    status_ = error;
            } else
                app_.navigate(app::Page::Menu);
            return;
        }
        if (app_.page() == app::Page::NetworkSetup) {
            networkFocus_ = hit(p, networkAddressBox_) ? 1 : hit(p, networkPortBox_) ? 2 : 0;
            if (hit(p, networkCreate_) || hit(p, networkJoin_)) {
                if (networkPort_.empty())
                    throw std::runtime_error("请输入端口");
                auto port = std::stoul(networkPort_);
                if (!port || port > 65535)
                    throw std::runtime_error("端口必须为1–65535");
                std::string error;
                if (!app_.startNetwork(hit(p, networkCreate_), networkAddress_,
                                       static_cast<unsigned short>(port), selectedChoice(0).deck, error))
                    status_ = error;
                else {
                    networkDeckSent_ = hit(p, networkCreate_);
                    status_.clear();
                    networkFocus_ = 0;
                    view_ = {};
                    handOrder_ = {};
                    offsets_.clear();
                    interaction_.cancel();
                    presentation_.clear();
                }
            }
            return;
        }
        auto *peer = app_.network();
        if (!peer || peer->started())
            return;
        if (hit(p, networkSeat_) && peer->isHost())
            peer->configure(!networkHostFirst_, networkAutomatic_);
        if (hit(p, networkAuto_) && peer->isHost())
            peer->configure(networkHostFirst_, !networkAutomatic_);
        if (hit(p, networkDeck_) && peer->connected()) {
            nextChoice(0);
            peer->deck(selectedChoice(0).deck);
            networkDeckSent_ = true;
        }
        if (hit(p, networkReady_) && peer->connected()) {
            peer->ready();
            status_ = "已准备，等待对方";
        }
    } catch (const std::exception &e) {
        status_ = e.what();
    }
}
void tickNetworkUi() {
    if (!app_.network())
        return;
    bool changed = app_.tickNetwork();
    auto *peer = app_.network();
    if (!peer) {
        interaction_.cancel();
        presentation_.clear();
        resources_.stopSounds();
        return;
    }
    if (peer->connected() && !peer->started() && !networkDeckSent_) {
        try {
            peer->deck(selectedChoice(0).deck);
            networkDeckSent_ = true;
        } catch (const std::exception &e) {
            status_ = e.what();
        }
    }
    if (peer->notice() != networkNotice_) {
        networkNotice_ = peer->notice();
        status_ = networkNotice_;
    }
    if (changed) {
        bool reset = view_.cards.empty() || !peer->connected() || app_.networkReset();
        refresh(reset);
    }
    if (app_.hasMatch() && (!peer->connected() || peer->ended() || peer->closing())) {
        interaction_.cancel();
        resetPointer();
        presentation_.clear();
        status_ = peer->notice();
    }
}
std::string networkSmokeRole_, networkSmokeScenario_;
bool networkSmokeBegun_{}, networkSmokeSettings_{}, networkSmokeReconnected_{};
std::size_t networkSmokeActions_{};
std::size_t networkSmokeSettingsTicks_{};
std::chrono::steady_clock::time_point networkSmokeStart_, networkSmokePause_, networkSmokeTerminal_;
void tickNetworkSmoke(sf::RenderWindow &window) {
    if (networkSmokeRole_.empty())
        return;
    auto now = std::chrono::steady_clock::now();
    if (!networkSmokeBegun_) {
        networkSmokeBegun_ = true;
        networkSmokeStart_ = now;
        app_.navigate(app::Page::NetworkSetup);
        draw(window);
        click((networkSmokeRole_ == "host" ? networkCreate_ : networkJoin_).position + sf::Vector2f{10, 10});
        return;
    }
    if (now - networkSmokeStart_ > std::chrono::seconds(180))
        throw std::runtime_error("Network UI smoke timed out: " + status_);
    auto *peer = app_.network();
    if (!peer)
        throw std::runtime_error("Network UI room missing");
    if (peer->ended() && (!peer->started() || view_.result == -1))
        throw std::runtime_error(peer->notice());
    if (peer->connected() && !peer->started() && now - networkSmokeStart_ > std::chrono::milliseconds(500)) {
        auto r = peer->room();
        auto player = r.at("hostPlayer").get<int>();
        if (!peer->isHost())
            player = 1 - player;
        if (!r.at("ready").at(player).get<bool>()) {
            draw(window);
            click(networkReady_.position + sf::Vector2f{10, 10});
        }
        return;
    }
    if (!peer->started() || (peer->blocked() && view_.result == -1))
        return;
    if (view_.result != -1) {
        if (networkSmokeTerminal_ == std::chrono::steady_clock::time_point{})
            networkSmokeTerminal_ = now;
        if (peer->isHost() && !peer->replayDelivered())
            return;
        std::string error;
        if (!app_.saveReplay(error))
            return;
        app_.tickNetwork(); // flush delivery acknowledgement before the test window closes
        auto record = readJson(app_.replayPath());
        auto engine = replay(content_, record);
        writeJson(app_.userDirectory() / "network-ui-audit.json",
                  {{"role", networkSmokeRole_},
                   {"scenario", networkSmokeScenario_},
                   {"digest", engine.digest()},
                   {"result", engine.state().result},
                   {"commands", record.at("commands").size()},
                   {"uiActions", networkSmokeActions_},
                   {"settings", networkSmokeSettings_},
                   {"settingsTicks", networkSmokeSettingsTicks_},
                   {"reducedMotion", app_.settings().reducedMotion},
                   {"reconnected", networkSmokeReconnected_}});
        draw(window);
        sf::Texture screenshot;
        if (!screenshot.resize(window.getSize()))
            throw std::runtime_error("UI capture failed");
        screenshot.update(window);
        if (!screenshot.copyToImage().saveToFile(app_.userDirectory() / "network-result.png"))
            throw std::runtime_error("UI screenshot failed");
        std::cout << "Network UI smoke passed: " << networkSmokeActions_ << " real UI actions, digest "
                  << engine.digest() << '\n';
        window.close();
        return;
    }
    if (!networkSmokeSettings_) {
        networkSmokeSettings_ = true;
        openSettings();
        if (networkSmokeRole_ == "guest") {
            draftSettings_.reducedMotion = true;
            draw(window);
            click(applySettings_.position + sf::Vector2f{10, 10});
        }
        networkSmokePause_ = now;
        return;
    }
    if (app_.page() == app::Page::Settings) {
        ++networkSmokeSettingsTicks_;
        if (now - networkSmokePause_ > std::chrono::milliseconds(350))
            app_.closeSettings();
        return;
    }
    if (app_.page() != app::Page::Match)
        return;
    if (networkSmokeRole_ == "guest" && networkSmokeScenario_ == "reconnect" && networkSmokeActions_ >= 3 &&
        !networkSmokeReconnected_) {
        peer->reconnect();
        networkSmokeReconnected_ = true;
        return;
    }
    auto selection = ai::choose(view_, ai::Difficulty::Normal);
    if (!selection)
        return;
    if (networkSmokeRole_ == "guest" && networkSmokeScenario_ == "surrender" && networkSmokeActions_ >= 2)
        selection->command = Surrender{};
    draw(window);
    drive(window, {{"actor", viewer_}, {"command", encodeCommand(selection->command)}});
    ++networkSmokeActions_;
}
