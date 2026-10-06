#include "wizard/presentation.hpp"
#include <algorithm>
#include <cmath>

namespace wizard::ui {
namespace {
bool visibleCard(const CardView &c, PlayerId viewer) {
    return c.instance.zone != Zone::Deck && (c.instance.zone != Zone::Hand || c.instance.owner == viewer);
}
const CardView *find(const GameView &v, CardId id) {
    for (const auto &c : v.cards)
        if (c.instance.id == id && visibleCard(c, v.viewer))
            return &c;
    return nullptr;
}
} // namespace
float MatchCue::progress() const {
    return std::clamp(age / duration, 0.f, 1.f);
}
bool MatchCue::visible() const {
    return age >= 0 && age < duration;
}
void MatchPresentation::clear() {
    cues_.clear();
    latest_ = next_;
}
bool MatchPresentation::busy() const {
    return !cues_.empty();
}
void MatchPresentation::update(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
        return;
    for (auto &cue : cues_)
        cue.age += seconds;
    cues_.erase(
        std::remove_if(cues_.begin(), cues_.end(), [](const MatchCue &c) { return c.age >= c.duration; }),
        cues_.end());
}
void MatchPresentation::sampleLatest(float seconds) {
    cues_.erase(std::remove_if(cues_.begin(), cues_.end(), [&](const MatchCue &c) { return c.id < latest_; }),
                cues_.end());
    for (auto &cue : cues_)
        cue.age = std::max(0.f, seconds);
}
void MatchPresentation::observe(const GameView &before, const GameView &after, bool reduced) {
    if (before.viewer != after.viewer || after.viewer < 0 || after.viewer > 1 ||
        before.events.size() > after.events.size()) {
        clear();
        return;
    }
    latest_ = next_;
    auto add = [&](CueKind kind, const CardView *card, PlayerId player, const std::string &text,
                   int amount = 0) -> MatchCue & {
        if (cues_.size() >= 48)
            cues_.erase(cues_.begin());
        MatchCue cue;
        cue.id = next_++;
        cue.kind = kind;
        cue.player = player;
        cue.text = text;
        cue.amount = amount;
        if (card) {
            cue.card = *card;
            cue.source = card->instance.id;
            cue.from = cue.to = card->instance.zone;
            cue.host = card->instance.host;
        }
        cue.duration = reduced ? .28f : kind == CueKind::Move || kind == CueKind::Draw ? .55f : .85f;
        cues_.push_back(std::move(cue));
        return cues_.back();
    };
    if (before.phase != after.phase || before.active != after.active)
        add(CueKind::Phase, nullptr, after.active, phaseName(after.phase) + "阶段");
    for (const auto &c : after.cards) {
        if (!visibleCard(c, after.viewer))
            continue;
        const auto *old = find(before, c.instance.id);
        if (!reduced && (!old || old->instance.zone != c.instance.zone)) {
            if (c.instance.zone == Zone::Hand && c.instance.owner == after.viewer) {
                auto &cue = add(CueKind::Draw, &c, c.instance.owner, "抽牌");
                cue.from = Zone::Deck;
                cue.to = Zone::Hand;
            } else if (c.instance.zone != Zone::Hand && !c.instance.base) {
                auto &cue = add(CueKind::Move, &c, c.instance.owner, c.definition.name);
                cue.from = old ? old->instance.zone : Zone::Hand;
                cue.to = c.instance.zone;
                cue.host = c.instance.host ? c.instance.host : c.instance.sourceFormation;
                for (std::size_t i = before.events.size(); i < after.events.size(); ++i)
                    if (after.events[i].kind == "command" && after.events[i].card == c.instance.id &&
                        (after.events[i].audience == -1 || after.events[i].audience == after.viewer))
                        cue.used = true;
            }
        }
        if (old && old->instance.spell == SpellState::Analyzing && c.instance.spell == SpellState::Ready)
            add(CueKind::Ready, &c, c.instance.owner, "解析完成");
    }
    // Opponent draws are public counts only; never copy the hidden card or its identifier.
    if (!reduced) {
        const int enemy = 1 - after.viewer;
        const int drawn = before.players[enemy].deckCount - after.players[enemy].deckCount;
        if (drawn > 0) {
            auto &cue = add(CueKind::Draw, nullptr, enemy, "对手抽牌", drawn);
            cue.from = Zone::Deck;
            cue.to = Zone::Hand;
        }
    }
    CardId damageSource = 0;
    for (std::size_t i = before.events.size(); i < after.events.size(); ++i) {
        const auto &e = after.events[i];
        if (e.audience != -1 && e.audience != after.viewer)
            continue;
        const auto *card = find(after, e.card);
        if (!card)
            card = find(before, e.card);
        if (e.kind == "effect" && e.text.rfind("造成 ", 0) == 0)
            damageSource = e.card;
        if (!card)
            continue;
        if (e.kind == "pay" && e.text.rfind("准备支付", 0) == 0)
            add(CueKind::Prepare, card, card->instance.owner, "准备施法");
        if (e.kind == "link_declared" && e.text.rfind("响应", 0) == 0) {
            auto &cue = add(CueKind::Response, card, card->instance.owner, "连锁响应");
            if (after.chain)
                for (const auto &link : after.chain->links)
                    if (link.item.source == e.card && link.kind == LinkKind::Response)
                        for (const auto &target : after.chain->links)
                            if (target.id == link.item.targetLink)
                                cue.target = target.item.source;
            if (!cue.target && before.chain && before.chain->links.size() == 1)
                cue.target = before.chain->links.front().item.source;
        }
        if (e.kind == "link_skipped")
            add(CueKind::Cancel, card, card->instance.owner, "取消 / 失效");
        if (e.kind == "link_resolved") {
            bool spell = false;
            const auto *old = find(before, e.card);
            if (old && ((old->instance.zone == Zone::Casting && old->instance.spell == SpellState::Pending) ||
                        (old->instance.zone == Zone::Hand && old->definition.type == CardType::Word &&
                         old->definition.immediate)))
                spell = true;
            if (before.chain)
                for (const auto &link : before.chain->links)
                    if (link.item.source == e.card && link.kind == LinkKind::Spell)
                        spell = true;
            if (spell)
                add(CueKind::Release, card, card->instance.owner, "释放法术");
        }
    }
    for (PlayerId p = 0; p < 2; ++p) {
        const auto &a = before.players[p], &b = after.players[p];
        const int life = b.life - a.life, mana = b.mana - a.mana, load = b.load - a.load;
        if (life) {
            auto &cue = add(life < 0 ? CueKind::Damage : CueKind::Heal, find(after, damageSource), p,
                            life < 0 ? "生命" : "回复", life);
            cue.duration = reduced ? .28f : 1.05f;
        }
        if (mana)
            add(CueKind::Mana, nullptr, p, "魔素", mana);
        if (load)
            add(CueKind::Load, nullptr, p, "荷载", load);
    }
}
} // namespace wizard::ui
