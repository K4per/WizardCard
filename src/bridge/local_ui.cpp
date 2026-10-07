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
    if (!application_) return fail("请先加载内容");
    Json cards = Json::array(), presets = Json::array(), drafts = Json::array();
    for (const auto &[id, definition] : content_->catalog.cards) {
        (void)id;
        auto card = cardDefinitionDto(definition);
        card["typeLabel"] = app::cardTypeLabel(definition.type);
        card["costLabel"] = app::primaryCostLabel(definition.type);
        card["rarityLabel"] = app::rarityLabel(definition.rarity);
        card["tags"] = app::cardTags(definition);
        cards.push_back(std::move(card));
    }
    for (const auto &preset : application_->presets())
        presets.push_back({{"id", preset.id}, {"name", preset.name}, {"description", preset.description}});
    for (const auto &draft : application_->decks().drafts()) {
        auto data = app::encodeDraft(draft);
        data["problems"] = application_->decks().problems(draft);
        drafts.push_back(std::move(data));
    }
    Json playable = Json::array();
    for (const auto &preset : application_->playableDecks())
        playable.push_back({{"id", preset.id}, {"name", preset.name}, {"description", preset.description}});
    return ok({{"cards", cards}, {"presets", presets}, {"drafts", drafts}, {"playable", playable},
        {"settings", app::encodeSettings(application_->settings())},
        {"writable", application_->decks().writable()}, {"notice", application_->notice()}});
}
Json LocalSession::createDraft(const std::string &presetId) const {
    if (!application_) return fail("请先加载内容");
    auto draft = application_->decks().create();
    if (!presetId.empty()) {
        const auto &presets = application_->presets();
        const auto found = std::find_if(presets.begin(), presets.end(), [&](const app::DeckPreset &p) { return p.id == presetId; });
        if (found == presets.end()) return fail("未找到预设");
        draft.name = found->name + " · 副本";
        draft.baseFormation = found->deck.baseFormation;
        for (const auto &id : found->deck.cards) ++draft.cards[id];
    }
    return ok(app::encodeDraft(draft));
}
Json LocalSession::validateDraft(const Json &data) const {
    if (!application_) return fail("请先加载内容");
    try { return ok({{"problems", application_->decks().problems(app::decodeDraft(data))}}); }
    catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::saveDraft(const Json &data) {
    if (!application_) return fail("请先加载内容");
    try {
        std::string error;
        if (!application_->decks().save(app::decodeDraft(data), error)) return fail(error);
        return library();
    } catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::eraseDraft(const std::string &id) {
    if (!application_) return fail("请先加载内容");
    std::string error;
    if (!application_->decks().erase(id, error)) return fail(error);
    return library();
}
Json LocalSession::query(const Json &data) const {
    if (!content_) return fail("请先加载内容");
    try {
        app::CardQuery query;
        query.name = data.value("name", std::string{});
        query.rarity = data.value("rarity", std::string{});
        if (data.contains("type")) {
            const auto type = data.at("type").get<int>();
            if (type < 0 || type > 4) return fail("类型筛选无效");
            query.type = static_cast<CardType>(type);
        }
        if (data.contains("cost")) query.cost = data.at("cost").get<int>();
        if (data.contains("castCost")) query.castCost = data.at("castCost").get<int>();
        if (data.contains("rank")) query.rank = data.at("rank").get<int>();
        if (data.contains("tags")) query.tags = data.at("tags").get<std::vector<std::string>>();
        return ok({{"ids", app::queryCards(content_->catalog, query)}});
    } catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::applySettings(const Json &data) {
    if (!application_) return fail("请先加载内容");
    try {
        std::string error;
        if (!application_->applySettings(app::decodeSettings(data), error)) return fail(error);
        return ok(app::encodeSettings(application_->settings()));
    } catch (const std::exception &e) { return fail(e.what()); }
}
Json LocalSession::leaveMatch(PlayerId viewer) {
    if (!visibleTo(viewer)) return fail("没有此查看者的对局");
    std::string error;
    application_->requestLeave();
    if (!application_->confirmLeave(viewer, error)) {
        application_->cancelConfirmation();
        return fail(error);
    }
    ++generation_; pending_.reset(); interaction_.cancel(); interactionViewer_ = -1;
    return ok();
}
Json LocalSession::interact(PlayerId viewer, const Json &request, const std::string &generation,
                            const std::string &revision) {
    if (!current(generation, revision) || !visibleTo(viewer)) return fail("选择已过期或查看者无效");
    if (application_->paused()) return fail("对局已暂停");
    try {
        if (interactionViewer_ != viewer || interactionGeneration_ != generation_ || interactionRevision_ != application_->revision()) {
            interaction_.update(view(viewer));
            interactionViewer_ = viewer; interactionGeneration_ = generation_; interactionRevision_ = application_->revision();
        }
        const auto operation = request.value("operation", std::string("state"));
        bool selected = true;
        if (operation == "select") selected = interaction_.select(exactCard(request.at("card")));
        else if (operation == "activate") selected = interaction_.activate(request.at("group").get<std::size_t>());
        else if (operation == "pick") selected = interaction_.pick(exactCard(request.at("card")));
        else if (operation == "pick_link") selected = interaction_.pickLink(exactId(request.at("link")));
        else if (operation == "drop") {
            const auto destination = request.at("destination").get<int>();
            if (destination < 0 || destination > static_cast<int>(Zone::Resolving)) return fail("区域无效");
            selected = interaction_.drop(exactCard(request.at("card")), exactCard(request.at("target")), static_cast<Zone>(destination));
        } else if (operation == "cancel") interaction_.cancel();
        else if (operation != "state") return fail("未知选择操作");
        if (!selected) return fail("当前选择无效");
        pending_.reset();
        if (interaction_.pending()) pending_ = Pending{viewer, interaction_.pending()->command, generation_, application_->revision()};
        Json groups = Json::array(), candidates = Json::array(), links = Json::array();
        for (const auto &group : interaction_.groups()) groups.push_back(group.title);
        for (const auto id : interaction_.candidates()) candidates.push_back(std::to_string(id));
        for (const auto id : interaction_.linkCandidates()) links.push_back(std::to_string(id));
        return ok({{"step", static_cast<int>(interaction_.step())}, {"selected", std::to_string(interaction_.selected())},
            {"groups", groups}, {"candidates", candidates}, {"links", links},
            {"pending", interaction_.pending() ? Json(interaction_.pending()->label) : Json()},
            {"command", interaction_.pending() ? commandDto(interaction_.pending()->command) : Json()}});
    } catch (const std::exception &e) { return fail(e.what()); }
}
} // namespace wizard::bridge
