#include "wizard/bridge.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace wizard::bridge {
namespace {
Json ok(Json data = Json::object()) { return {{"ok", true}, {"data", std::move(data)}}; }
Json fail(const std::string &message) { return {{"ok", false}, {"errorCode", "local_ui_error"}, {"error", message}}; }
std::uint64_t exactId(const Json &value) {
    const auto text = value.get<std::string>();
    std::uint64_t id{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), id);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("ID需要精确十进制字符串");
    return id;
}
CardId exactCard(const Json &value) {
    const auto id = exactId(value);
    if (id > std::numeric_limits<CardId>::max()) throw std::invalid_argument("卡牌实例ID超出范围");
    return static_cast<CardId>(id);
}
}
Json LocalSession::library() const {
    if (!match_.application) return fail("请先加载内容");
    if (decks_.libraryCache) return *decks_.libraryCache;
    Json cards = Json::array(), presets = Json::array(), drafts = Json::array();
    for (const auto &[id, definition] : decks_.content->catalog.cards) {
        (void)id;
        auto card = cardDefinitionDto(definition);
        card["typeLabel"] = app::cardTypeLabel(definition.type);
        card["costLabel"] = app::primaryCostLabel(definition.type);
        card["rarityLabel"] = app::rarityLabel(definition.rarity);
        card["tags"] = app::cardTags(definition);
        cards.push_back(std::move(card));
    }
    for (const auto &preset : match_.application->presets())
        presets.push_back({{"id", preset.id}, {"name", preset.name}, {"description", preset.description}});
    for (const auto &draft : match_.application->decks().drafts()) {
        auto data = app::encodeDraft(draft);
        data["problems"] = app::constructionProblems(decks_.content->catalog, draft);
        data["matchCompatible"] = app::deckProblems(decks_.content->catalog, draft).empty();
        drafts.push_back(std::move(data));
    }
    Json playable = Json::array();
    for (const auto &preset : match_.application->playableDecks())
        playable.push_back({{"id", preset.id}, {"name", preset.name}, {"description", preset.description}});
    decks_.libraryCache = ok({{"cards", cards}, {"presets", presets}, {"drafts", drafts}, {"playable", playable},
        {"settings", app::encodeSettings(match_.application->settings())},
        {"writable", match_.application->decks().writable()}, {"notice", match_.application->notice()}});
    return *decks_.libraryCache;
}
Json LocalSession::createDraft(const std::string &presetId, int format) const {
    if (!match_.application) return fail("请先加载内容");
    if (format < 1 || format > 3) return fail("卡组草稿版本无效");
    auto draft = match_.application->decks().create();
    draft.format = format;
    if (!presetId.empty()) {
        const auto &presets = match_.application->presets();
        const auto found = std::find_if(presets.begin(), presets.end(), [&](const app::DeckPreset &p) { return p.id == presetId; });
        if (found == presets.end()) return fail("未找到预设");
        draft.name = found->name + " · 副本";
        draft.baseFormation = found->deck.baseFormation;
        for (const auto &id : found->deck.cards) ++draft.cards[id];
        if (format >= 2)
            for (const auto &id : found->deck.sideDeck) ++draft.sideCards[id];
    }
    if (format >= 2) {
        for (auto it = draft.cards.begin(); it != draft.cards.end();) {
            const auto found = decks_.content->catalog.cards.find(it->first);
            if (found != decks_.content->catalog.cards.end() && found->second.type == CardType::Formation) {
                draft.sideCards[it->first] += it->second;
                it = draft.cards.erase(it);
            } else ++it;
        }
        if (format == 2 && !draft.baseFormation.empty() && !draft.sideCards.count(draft.baseFormation))
            draft.sideCards[draft.baseFormation] = 1;
    }
    if (format == 3) draft.baseFormation.clear();
    return ok(app::encodeDraft(draft));
}
Json LocalSession::validateDraft(const Json &data) const {
    if (!match_.application) return fail("请先加载内容");
    try {
        const auto draft = app::decodeDraft(data);
        return ok({{"problems", app::constructionProblems(decks_.content->catalog, draft)},
                   {"matchCompatible", app::deckProblems(decks_.content->catalog, draft).empty()}});
    }
    catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::saveDraft(const Json &data) {
    if (!match_.application) return fail("请先加载内容");
    try {
        std::string error;
        if (!match_.application->decks().save(app::decodeDraft(data), error)) return fail(error);
        decks_.libraryCache.reset();
        return library();
    } catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::eraseDraft(const std::string &id) {
    if (!match_.application) return fail("请先加载内容");
    std::string error;
    if (!match_.application->decks().erase(id, error)) return fail(error);
    decks_.libraryCache.reset();
    return library();
}
Json LocalSession::query(const Json &data) const {
    if (!decks_.content) return fail("请先加载内容");
    try {
        app::CardQuery query;
        query.name = data.value("name", std::string{});
        query.rarity = data.value("rarity", std::string{});
        if (data.contains("type")) {
            const auto type = data.at("type").get<int>();
            if (type < 0 || type > static_cast<int>(CardType::Talisman)) return fail("类型筛选无效");
            query.type = static_cast<CardType>(type);
        }
        if (data.contains("cost")) query.cost = data.at("cost").get<int>();
        if (data.contains("castCost")) query.castCost = data.at("castCost").get<int>();
        if (data.contains("rank")) query.rank = data.at("rank").get<int>();
        if (data.contains("faction")) query.factions = data.at("faction").get<std::vector<std::string>>();
        if (data.contains("speed")) query.speeds = data.at("speed").get<std::vector<std::string>>();
        if (data.contains("damageType")) query.damageTypes = data.at("damageType").get<std::vector<std::string>>();
        if (data.contains("tags")) query.tags = data.at("tags").get<std::vector<std::string>>();
        return ok({{"ids", app::queryCards(decks_.content->catalog, query)}});
    } catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::applySettings(const Json &data) {
    if (!match_.application) return fail("请先加载内容");
    try {
        std::string error;
        if (!match_.application->applySettings(app::decodeSettings(data), error)) return fail(error);
        decks_.libraryCache.reset();
        return ok(app::encodeSettings(match_.application->settings()));
    } catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::leaveMatch(PlayerId viewer) {
    if (!visibleTo(viewer)) return fail("没有此查看者的对局");
    std::string error;
    match_.application->requestLeave();
    if (!match_.application->confirmLeave(viewer, error)) {
        match_.application->cancelConfirmation();
        return fail(error);
    }
    ++match_.generation; match_.pending.reset(); match_.interaction.cancel(); match_.interactionViewer = -1;
    return ok();
}
Json LocalSession::interact(PlayerId viewer, const Json &request, const std::string &generation,
                            const std::string &revision) {
    if (!current(generation, revision) || !visibleTo(viewer)) return fail("选择已过期或查看者无效");
    if (match_.application->paused()) return fail("对局已暂停");
    try {
        if (match_.interactionViewer != viewer || match_.interactionGeneration != match_.generation || match_.interactionRevision != match_.application->revision()) {
            match_.interaction.update(view(viewer), [this, viewer](const ActionIntent &intent) {
                return match_.application->queryActions(viewer, match_.application->generation(), match_.application->revision(), intent);
            });
            match_.interactionViewer = viewer; match_.interactionGeneration = match_.generation; match_.interactionRevision = match_.application->revision();
        }
        const auto operation = request.value("operation", std::string("state"));
        bool selected = true;
        if (operation == "select") selected = match_.interaction.select(exactCard(request.at("card")));
        else if (operation == "activate") selected = match_.interaction.activate(request.at("group").get<std::size_t>());
        else if (operation == "pick") selected = match_.interaction.pick(exactCard(request.at("card")));
        else if (operation == "pick_link") selected = match_.interaction.pickLink(exactId(request.at("link")));
        else if (operation == "drop") {
            const auto destination = request.at("destination").get<int>();
            if (destination < 0 || destination > static_cast<int>(Zone::Resolving)) return fail("区域无效");
            selected = match_.interaction.drop(exactCard(request.at("card")), exactCard(request.at("target")), static_cast<Zone>(destination));
        } else if (operation == "cancel") match_.interaction.cancel();
        else if (operation != "state") return fail("未知选择操作");
        if (!selected) return fail("当前选择无效");
        match_.pending.reset();
        if (match_.interaction.pending()) match_.pending = Pending{viewer, match_.interaction.pending()->command, match_.generation, match_.application->revision()};
        Json groups = Json::array(), candidates = Json::array(), links = Json::array();
        for (const auto &group : match_.interaction.groups()) groups.push_back(group.title);
        for (const auto id : match_.interaction.candidates()) candidates.push_back(std::to_string(id));
        for (const auto id : match_.interaction.linkCandidates()) links.push_back(std::to_string(id));
        return ok({{"step", static_cast<int>(match_.interaction.step())}, {"selected", std::to_string(match_.interaction.selected())},
            {"groups", groups}, {"candidates", candidates}, {"links", links},
            {"pending", match_.interaction.pending() ? Json(match_.interaction.pending()->label) : Json()},
            {"command", match_.interaction.pending() ? commandDto(match_.interaction.pending()->command) : Json()}});
    } catch (const std::exception &e) { return fail(e.what()); }
}

Json LocalSession::networkRequest(const Json &request) {
    if (!match_.application) return fail("请先加载内容");
    try {
        const auto operation = request.at("operation").get<std::string>();
        const auto chooseDeck = [&]() -> PlayerDeck {
            const auto decks = match_.application->playableDecks();
            const auto id = request.value("deck", std::string("default"));
            const auto found = std::find_if(decks.begin(), decks.end(),
                [&](const app::DeckPreset &preset) { return preset.id == id; });
            if (found == decks.end()) throw std::invalid_argument("未找到合法卡组");
            return found->deck;
        };
        if (operation == "host" || operation == "join") {
            const auto port = request.value("port", 4242);
            if (port < 1 || port > 65535) return fail("端口必须在1到65535之间");
            const auto address = request.value("address", std::string("127.0.0.1"));
            std::string error;
            if (!match_.application->startNetwork(operation == "host", address,
                                            static_cast<unsigned short>(port), chooseDeck(), error))
                return fail(error);
            ++match_.generation;
            match_.pending.reset();
            match_.interaction.cancel();
        } else {
            auto *room = match_.application->network();
            if (!room) return fail("当前没有联机房间");
            if (operation == "deck") room->deck(chooseDeck());
            else if (operation == "configure")
                room->configure(request.value("hostFirst", true), request.value("automatic", true));
            else if (operation == "ready") room->ready();
            else if (operation == "reconnect") room->reconnect();
            else if (operation == "leave") {
                std::string error;
                if (!match_.application->leaveNetwork(error, request.value("force", false))) return fail(error);
                ++match_.generation;
                match_.pending.reset();
                match_.interaction.cancel();
                return ok();
            } else return fail("未知联机操作");
        }
        return pollNetwork();
    } catch (const std::exception &e) { return fail(e.what()); }
}

Json LocalSession::pollNetwork() {
    if (!match_.application || !match_.application->network()) return ok({{"exists", false}});
    try {
        match_.application->tickNetwork();
        if (match_.application->networkReset()) {
            ++match_.generation;
            match_.pending.reset();
            match_.interaction.cancel();
        }
        auto *room = match_.application->network();
        if (!room) return ok({{"exists", false}, {"ended", true}, {"generation", std::to_string(match_.generation)}});
        const auto localPlayer = room->started() ? room->view().viewer : -1;
        // A joining peer has no room snapshot until the TCP handshake completes.
        // Keep the bridge shape stable so the frontend can render "connecting"
        // instead of trying to cast JSON null to a Dictionary.
        const auto rawRoom = room->room();
        const Json safeRoom = rawRoom.is_object() ? rawRoom : Json::object();
        return ok({{"exists", true}, {"room", safeRoom}, {"host", room->isHost()},
                   {"connected", room->connected()}, {"started", room->started()},
                   {"blocked", room->blocked()}, {"ended", room->ended()},
                   {"replayDelivered", room->replayDelivered()}, {"notice", room->notice()},
                   {"viewer", localPlayer}, {"generation", std::to_string(match_.generation)},
                   {"revision", std::to_string(match_.application->revision())}});
    } catch (const std::exception &e) { return fail(e.what()); }
}
} // namespace wizard::bridge
