#include "wizard/core/card_rules.hpp"
#include "wizard/core/engine.hpp"
#include "engine_internal.hpp"

namespace wizard::card_rules {
std::string activationError(const GameState &s, const CardCatalog &cat, PlayerId p, CardId id) {
    return activationError(s, cat, p, id, 0);
}
}

namespace wizard {
bool GameEngine::cardEffect() {
    return resolveResourceEffect() || resolveFormationEffect() || resolveSpellEffect() || resolveRecipeEffect();
}
bool GameEngine::chooseCardEffect(PlayerId actor, ChoiceValue choice) {
    return chooseResourceEffect(actor, choice) || chooseFormationEffect(actor, choice) || chooseSpellEffect(actor, choice) || chooseRecipeEffect(actor, choice);
}
namespace {

void shuffleBloodDeck(MutationContext &s, PlayerId p) {
    auto &deck = s.players[p].deck;
    for (std::size_t n = deck.size(); n > 1; --n) std::swap(deck[n - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(n))]);
}
}
bool GameEngine::resolveResourceEffect() {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto p = f.item.owner;
    const auto finish = [&] { ++f.cursor; f.resume<RepeatedChoiceContinuation>().remaining = -1; f.resume<RepeatedChoiceContinuation>().completed = 0; };
    if (e.kind == EffectKind::ChangeLifeLimit) {
        card_rules::changeLifeLimit(s, catalog(), p, e.amount); finish(); return true;
    }
    if (e.kind == EffectKind::BloodDamage) {
        const int amount = std::min(e.amount, maximumLife - card_rules::lifeLimit(s, catalog(), p));
        EffectResolver::apply(s, catalog(), f.item, {EffectKind::Damage, amount, DamageType::Necrotic});
        // Search is dependent on this damage actually being applied.
        f.resume<RepeatedChoiceContinuation>().completed = amount > 0 && !card_rules::damageImmune(s, catalog(), f.item.targetPlayer >= 0 ? f.item.targetPlayer : p) ? 1 : 0;
        ++f.cursor;
        return true;
    }
    if (e.kind == EffectKind::BloodSearch) {
        const auto &source = catalog().at(s.cards.at(f.item.source).definition);
        const bool debt = hasCapability(source, Capability::BloodDebt);
        const bool repayment = hasCapability(source, Capability::BloodRepayment);
        const bool optional = debt || repayment;
        if (optional && !f.resume<RepeatedChoiceContinuation>().completed) { finish(); return true; }
        std::vector<CardId> options;
        for (auto id : s.players[p].deck) {
            const auto &d = catalog().at(s.cards.at(id).definition);
            if ((debt && hasCapability(d, Capability::BloodRepayment)) ||
                (repayment && hasCapability(d, Capability::BloodDebt)) ||
                (!optional && card_rules::member(d, "血契"))) options.push_back(id);
        }
        if (options.empty()) finish();
        else decision(p, DecisionKind::EffectChoice, std::move(options), optional);
        return true;
    }
    if (e.kind == EffectKind::BloodTube) {
        if (!card_rules::canPayLife(s, p, 2)) finish();
        else typedDecision(p, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::Confirm)}, true);
        return true;
    }
    if (e.kind == EffectKind::BloodHeartSeal) {
        if (!card_rules::canPayLife(s, p, 2)) finish();
        else typedDecision(p, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::Confirm)}, true);
        return true;
    }
    if (e.kind == EffectKind::BloodClot) {
        bool prevented = false;
        if (s.chain) for (auto &link : s.chain->links) if (link.id == f.item.targetLink) {
            const auto victim = link.item.targetPlayer < 0 ? link.item.owner : link.item.targetPlayer;
            for (auto &effect : link.item.effects)
                if ((effect.kind == EffectKind::Damage || effect.kind == EffectKind::BloodDamage) &&
                    (effect.recipient == EffectRecipient::Opponent ? 1 - link.item.owner : victim) == p && effect.amount > 0) {
                    effect.amount = 0; prevented = true;
                    link.item.invalidateEffectsKey();
                    break;
                }
        }
        if (prevented) {
            card_rules::changeLifeLimit(s, catalog(), p, -4);
            ResourceRules::drawOne(s, catalog(), p);
        }
        finish(); return true;
    }
    return false;
}
bool GameEngine::chooseResourceEffect(PlayerId actor, ChoiceValue choice) {
    const auto selected = static_cast<CardId>(choice.value);
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto kind = f.item.effects[f.cursor].kind;
    if (kind == EffectKind::BloodSearch) {
        if (selected) {
            card_rules::returnCard(s, catalog(), selected, true); shuffleBloodDeck(s, actor);
        }
    } else if (kind == EffectKind::BloodTube) {
        if (selected) {
            card_rules::payLife(s, catalog(), actor, 2, f.item.source);
            EffectResolver::apply(s, catalog(), f.item, {EffectKind::GainMana, 2});
        }
    } else if (kind == EffectKind::BloodHeartSeal) {
        if (selected) {
            card_rules::payLife(s, catalog(), actor, 2, f.item.source);
            const auto host = s.cards.find(s.cards.at(f.item.source).host);
            if (host != s.cards.end() && s.chain) for (auto &link : s.chain->links)
                if (link.item.source == host->first) for (auto &e : link.item.effects)
                    if (e.kind == EffectKind::Damage || e.kind == EffectKind::BloodDamage) e.amount += 8;
            if (host != s.cards.end() && s.chain)
                for (auto &link : s.chain->links) if (link.item.source == host->first) link.item.invalidateEffectsKey();
        }
    } else return false;
    ++f.cursor; f.resume<RepeatedChoiceContinuation>().remaining = -1; f.resume<RepeatedChoiceContinuation>().completed = 0;
    return true;
}
}


#include "wizard/core/card_rules.hpp"
#include "wizard/core/engine.hpp"
#include "engine_internal.hpp"

namespace wizard::card_rules {
namespace {
bool trait(const CardDefinition &d, Capability capability) {
    return hasCapability(d, capability);
}
bool sevenFormation(const CardInstance &c, const CardCatalog &cat) {
    return c.zone == Zone::Analysis && !c.faceDown && cat.at(c.definition).type == CardType::Formation &&
        member(cat.at(c.definition), "七星");
}
}
int aspectMode(const GameState &s, const CardCatalog &cat, PlayerId p) {
    if (!std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
            return entry.second.owner == p && sevenFormation(entry.second, cat);
        })) return 0;
    bool bright = s.players[p].ownTurn % 2 == 1;
    if (s.players[p].sevenShifted && s.players[p].sevenShiftUntilOwnTurn > s.players[p].ownTurn) bright = !bright;
    return bright ? 1 : 2;
}
bool effectImmune(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto found = s.cards.find(id);
    if (found == s.cards.end()) return false;
    const auto &c = found->second;
    const auto &d = cat.at(c.definition);
    if (trait(d, Capability::EffectImmune)) return true;
    if (trait(d, Capability::GreatStorm)) return true;
    if (!field(c) || !member(d, "七星")) return false;
    return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &provider = entry.second;
        return entry.first != id && provider.owner == c.owner && provider.zone == Zone::Analysis &&
            !provider.faceDown && hasCapability(cat.at(provider.definition), Capability::SevenStarsYaoguang);
    });
}
std::vector<CardId> formationUpgradeSources(const GameState &s, const CardCatalog &cat, PlayerId p, bool kaiyang) {
    std::vector<CardId> out;
    for (const auto &[id, c] : s.cards) {
        if (c.owner != p || !sevenFormation(c, cat) ||
            (kaiyang && !hasCapability(cat.at(c.definition), Capability::SevenStarsKaiyang))) continue;
        const int next = Rules::maxRank(s, cat, id) + 1;
        if (!kaiyang && next >= 7) continue; // Yaoguang's exclusive condition is never bypassed.
        const bool available = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
            const auto &candidate = entry.second;
            const auto &d = cat.at(candidate.definition);
            return candidate.owner == p && candidate.zone == Zone::SideDeck && d.type == CardType::Formation &&
                member(d, "七星") && (kaiyang ? hasCapability(d, Capability::SevenStarsYaoguang) : d.level == next);
        });
        if (available) out.push_back(id);
    }
    return out;
}
void queueFormationSetup(MutationContext &s, const CardCatalog &cat, CardId id) {
    const auto &c = s.cards.at(id);
    if (!sevenFormation(c, cat) || hasCapability(cat.at(c.definition), Capability::QixingTianshu) ||
        hasCapability(cat.at(c.definition), Capability::SevenStarsKaiyang)) return;
    auto trigger = detail::snapshot(c, cat.at(c.definition));
    trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
    trigger.effects = {{EffectKind::SevenSetup}};
    s.queue.append(std::move(trigger));
}
bool validLinkedResponse(const GameState &s, const CardCatalog &cat, PlayerId p, LinkId target) {
    if (!s.chain) return false;
    for (std::size_t index = 1; index < s.chain->links.size(); ++index) {
        const auto &link = s.chain->links[index];
        const auto &previous = s.chain->links[index - 1];
        if (link.id != target) continue;
        const auto source = s.cards.find(link.item.source);
        const auto prior = s.cards.find(previous.item.source);
        return link.item.owner != p && source != s.cards.end() && previous.item.owner == p &&
            prior != s.cards.end() && member(cat.at(prior->second.definition), "七星") &&
            (link.kind == LinkKind::Spell || link.kind == LinkKind::Response) &&
            !link.canceled && !link.sourceLost && !effectImmune(s, cat, source->first);
    }
    return false;
}
}

namespace wizard {
namespace {

bool sevenKind(EffectKind kind) {
    return kind >= EffectKind::SevenAwakening && kind <= EffectKind::SevenKaiyang;
}
void finishSeven(EffectFrame &f) { ++f.cursor; f.resume<RepeatedChoiceContinuation>().remaining = -1; f.resume<RepeatedChoiceContinuation>().completed = 0; }
void shuffleSevenDeck(MutationContext &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t n = deck.size(); n > 1; --n) std::swap(deck[n - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(n))]);
}
std::vector<CardId> upgrades(const GameState &s, const CardCatalog &cat, PlayerId p, CardId previous, bool kaiyang) {
    std::vector<CardId> out;
    const auto old = s.cards.find(previous);
    if (old == s.cards.end() || old->second.zone != Zone::Analysis) return out;
    const int next = Rules::maxRank(s, cat, previous) + 1;
    for (const auto &[id, c] : s.cards) {
        const auto &d = cat.at(c.definition);
        if (c.owner == p && c.zone == Zone::SideDeck && d.type == CardType::Formation &&
            card_rules::member(d, "七星") && (kaiyang ? hasCapability(d, Capability::SevenStarsYaoguang) : d.level == next && next < 7)) out.push_back(id);
    }
    return out;
}
void replaceSeven(MutationContext &s, const CardCatalog &cat, CardId oldId, CardId replacement) {
    // Replacement follows the shared leave tree: runes and hosted analysis
    // spells leave, while casting spells retain their independent field zone.
    StateMaintenance::leave(s, oldId, &cat);
    auto &next = s.cards.at(replacement);
    next.zone = Zone::Analysis; next.fieldOrder = s.nextFieldOrder++;
    next.host = next.sourceFormation = 0; next.stormPoints = 0; next.stormReleaseProgress = 0;
    next.faceDown = false; next.base = false; next.spell = SpellState::None;
    next.analysisLoad = next.castLoad = next.analysisProgress = next.remaining = 0;
    s.events.push_back({"formation_replaced", "七星阵法替换", -1, replacement});
    card_rules::queueFormationSetup(s, cat, replacement);
}
void bottomViewed(MutationContext &s, PlayerId owner, const std::vector<CardId> &viewed, CardId selected) {
    auto &deck = s.players[owner].deck;
    std::vector<CardId> bottom;
    for (auto id : viewed) {
        deck.erase(std::remove(deck.begin(), deck.end(), id), deck.end());
        if (id != selected) bottom.push_back(id);
    }
    // Deck.back() is the top. Inserting the viewed bottom-to-top vector at
    // begin() preserves the original relative order of every unselected card.
    deck.insert(deck.begin(), bottom.begin(), bottom.end());
    if (selected) {
        auto &card = s.cards.at(selected);
        card.zone = Zone::Hand; card.faceDown = card.publiclyRevealed = false;
        card.host = card.sourceFormation = 0; card.spell = SpellState::None;
        s.events.push_back({"search", "遥鉴：从查看卡牌加入手牌", owner, selected});
    }
    s.events.push_back({"deck_bottom", "遥鉴：其余卡牌按原顺序置底（不洗牌）", owner});
}
void concealGaze(MutationContext &s, const CardCatalog &cat, CardId target, CardId host, int ring) {
    auto &card = s.cards.at(target);
    card.zone = Zone::Analysis; card.host = card.sourceFormation = host;
    card.ringIndex = card.sourceRingIndex = ring; card.faceDown = card.wasAmbushed = true;
    card.publiclyRevealed = false; card.ambushedTurn = s.globalTurn;
    card.spell = SpellState::None; card.analysisProgress = card.remaining = 0;
    card.castLoad = 0; card.targetCard = 0; card.targets.clear(); card.targetPlayer = -1;
    card.canceledTurn = -1; card.analysisStarted = s.players[controllerOf(card)].ownTurn;
    for (auto &payment : card.payments) payment.canceled = true;
    s.events.push_back({"conceal", "七星的注视：无效法术并重新盖伏", -1, target});
    (void)cat;
}
}
std::optional<std::string> GameEngine::sevenAbility(PlayerId actor, const ActivateAbility &cmd) {
    if (cmd.ability != "seven_kaiyang") return std::nullopt;
    (void)actor;
    (void)cmd;
    return "seven_kaiyang_prepare_only";
}
bool GameEngine::resolveFormationEffect() {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto effect = f.item.effects[f.cursor];
    if (!sevenKind(effect.kind)) return false;
    const auto owner = f.item.owner;
    const auto source = s.cards.find(f.item.source);
    const int mode = card_rules::aspectMode(s, catalog(), owner);
    if (effect.kind == EffectKind::SevenRuin || effect.kind == EffectKind::SevenPunishment) {
        const auto trigger = f.item;
        const bool both = source != s.cards.end() && hasCapability(catalog().at(source->second.definition), Capability::QixingRuin);
        finishSeven(f);
        if (!mode) return true;
        const auto type = mode == 1 ? DamageType::Radiant : DamageType::Necrotic;
        if (both) {
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, effect.amount, type, EffectRecipient::Owner});
            if (s.result != -1) return true;
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, effect.amount, type, EffectRecipient::Opponent});
        } else {
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, effect.amount, type, EffectRecipient::Opponent});
        }
        StateMaintenance::check(s, catalog()); return true;
    }
    if (effect.kind == EffectKind::SevenShift && mode == 1) {
        auto &player = s.players[owner]; player.sevenShifted = !player.sevenShifted;
        player.sevenShiftUntilOwnTurn = player.ownTurn + 1;
        s.events.push_back({"seven_shift", "七星耀/黯效果交换，至下个己方回合开始", -1, f.item.source});
        finishSeven(f); return true;
    }
    if (effect.kind == EffectKind::SevenSetup && source != s.cards.end() &&
        hasCapability(catalog().at(source->second.definition), Capability::SevenStarsYaoguang)) {
        const auto sourceId = f.item.source;
        finishSeven(f);
        card_rules::heal(s, catalog(), owner, maximumLife, sourceId);
        s.players[owner].mana = 0;
        for (int i = 0; i < 5 && s.result == -1; ++i) ResourceRules::drawOne(s, catalog(), owner);
        StateMaintenance::check(s, catalog()); return true;
    }
    if (effect.kind == EffectKind::SevenGaze) {
        if (!f.resume<TargetWalkContinuation>().next) {
            if (!s.chain) { finishSeven(f); return true; }
            const auto targetLink = std::find_if(s.chain->links.begin(), s.chain->links.end(), [&](const ChainLink &l) { return l.id == f.item.targetLink; });
            if (targetLink == s.chain->links.end() || targetLink->canceled || targetLink->sourceLost ||
                card_rules::effectImmune(s, catalog(), targetLink->item.source)) { finishSeven(f); return true; }
            targetLink->canceled = true;
            f.item.targetCard = targetLink->item.source;
            f.resume<TargetWalkContinuation>().next = 0;
        }
        const auto target = s.cards.find(f.item.targetCard);
        if (target == s.cards.end() || !card_rules::field(target->second)) { finishSeven(f); return true; }
        const auto id = target->first;
        const auto oldHost = s.cards.find(target->second.sourceFormation);
        const int oldRing = target->second.sourceRingIndex;
        if (oldHost != s.cards.end() && oldHost->second.zone == Zone::Analysis && oldRing >= 0 && oldRing < Rules::rings(s, catalog(), oldHost->first) && Rules::canHost(s, catalog(), id, oldHost->first, true) && std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                const auto &other = entry.second;
                return entry.first != id && other.zone == Zone::Analysis && other.host == oldHost->first && other.ringIndex == oldRing;
            })) {
            concealGaze(s, catalog(), id, oldHost->first, oldRing); finishSeven(f); return true;
        }
        std::vector<CardId> hosts;
        for (const auto &[hostId, host] : s.cards)
            if (controllerOf(host) == controllerOf(target->second) && host.zone == Zone::Analysis && catalog().at(host.definition).type == CardType::Formation && Rules::canHost(s, catalog(), id, hostId, true) && Rules::freeRing(s, catalog(), hostId) >= 0) hosts.push_back(hostId);
        if (hosts.empty()) { finishSeven(f); StateMaintenance::leave(s, id, &catalog()); return true; }
        decision(controllerOf(target->second), DecisionKind::EffectChoice, std::move(hosts)); return true;
    }
    if ((effect.kind == EffectKind::SevenInsight && mode == 1) ||
        (effect.kind == EffectKind::SevenShift && mode == 2) || effect.kind == EffectKind::SevenUpgrade || effect.kind == EffectKind::SevenKaiyang) {
        const bool kaiyang = effect.kind == EffectKind::SevenKaiyang;
        auto options = !f.resume<EntityChoiceContinuation>().entity ? card_rules::formationUpgradeSources(s, catalog(), owner, kaiyang)
                                           : upgrades(s, catalog(), owner, *f.resume<EntityChoiceContinuation>().entity, kaiyang);
        if (options.empty()) { finishSeven(f); return true; }
        decision(owner, DecisionKind::EffectChoice, std::move(options), kaiyang && !f.resume<EntityChoiceContinuation>().entity); return true;
    }
    if (effect.kind == EffectKind::SevenInsight && mode == 2) {
        if (!f.resume<EntityChoiceContinuation>().inspected) {
            const auto &deck = s.players[owner].deck;
            const auto amount = std::min<std::size_t>(7, deck.size());
            f.item.targetCards.assign(deck.end() - static_cast<std::ptrdiff_t>(amount), deck.end());
            for (auto id : f.item.targetCards) s.events.push_back({"inspect", "七星遥鉴：查看卡组顶部卡牌", owner, id});
            f.resume<EntityChoiceContinuation>().inspected = true;
        }
        std::vector<CardId> options;
        for (auto id : f.item.targetCards)
            if (s.cards.count(id) && card_rules::member(catalog().at(s.cards.at(id).definition), "七星")) options.push_back(id);
        if (options.empty()) { bottomViewed(s, owner, f.item.targetCards, 0); finishSeven(f); return true; }
        decision(owner, DecisionKind::EffectChoice, std::move(options)); return true;
    }
    if (effect.kind == EffectKind::SevenAwakening && f.resume<RepeatedChoiceContinuation>().completed == 1) {
        typedDecision(owner, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::SevenDamage), ChoiceValue::mode(ChoiceMode::SevenLoad)}); return true;
    }
    std::vector<CardId> options;
    if (effect.kind == EffectKind::SevenReturn) {
        if (f.resume<RepeatedChoiceContinuation>().completed >= std::max(1, effect.amount)) {
            shuffleSevenDeck(s, owner); finishSeven(f); return true;
        }
        for (const auto &[id, card] : s.cards) if (card.owner == owner && card.zone == Zone::Ash &&
            card_rules::member(catalog().at(card.definition), "七星")) options.push_back(id);
        if (options.empty()) { shuffleSevenDeck(s, owner); finishSeven(f); return true; }
        decision(owner, DecisionKind::EffectChoice, std::move(options), f.resume<RepeatedChoiceContinuation>().completed > 0); return true;
    }
    if (effect.kind == EffectKind::SevenAwakening || effect.kind == EffectKind::SevenSetup) {
        const bool setup = effect.kind == EffectKind::SevenSetup;
        const auto *sourceDefinition = source == s.cards.end() ? nullptr : &catalog().at(source->second.definition);
        const bool ash = setup ? (sourceDefinition && (hasCapability(*sourceDefinition, Capability::QixingTianquan) ||
            hasCapability(*sourceDefinition, Capability::QixingYuheng))) : mode == 2;
        if (!setup && !mode) { finishSeven(f); return true; }
        for (const auto &[id, card] : s.cards) {
            if (card.owner != owner || card.zone != (ash ? Zone::Ash : Zone::Deck)) continue;
            const auto &d = catalog().at(card.definition);
            const bool shiftOnly = setup && sourceDefinition &&
                (hasCapability(*sourceDefinition, Capability::QixingTianxuan) || hasCapability(*sourceDefinition, Capability::QixingTianquan));
            if (shiftOnly ? hasCapability(d, Capability::QixingShift) : card_rules::member(d, "七星")) options.push_back(id);
        }
        if (options.empty()) { finishSeven(f); return true; }
        decision(owner, DecisionKind::EffectChoice, std::move(options)); return true;
    }
    finishSeven(f); return true;
}
bool GameEngine::chooseFormationEffect(PlayerId actor, ChoiceValue choice) {
    const auto selected = static_cast<CardId>(choice.value);
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto kind = f.item.effects[f.cursor].kind;
    if (!sevenKind(kind)) return false;
    const auto owner = f.item.owner;
    const auto source = s.cards.find(f.item.source);
    const int mode = card_rules::aspectMode(s, catalog(), owner);
    const auto listed = [&](const std::vector<CardId> &ids) {
        return std::find(ids.begin(), ids.end(), selected) != ids.end();
    };
    if (kind == EffectKind::SevenGaze) {
        const auto id = f.item.targetCard;
        const auto target = s.cards.find(selected);
        if (!s.cards.count(id) || target == s.cards.end() || controllerOf(target->second) != owner || target->second.zone != Zone::Analysis || target->second.faceDown || catalog().at(target->second.definition).type != CardType::Formation || !Rules::canHost(s, catalog(), id, selected, true) || Rules::freeRing(s, catalog(), selected) < 0)
            return false;
        const int ring = Rules::freeRing(s, catalog(), selected);
        concealGaze(s, catalog(), id, selected, ring); finishSeven(f); return true;
    }
    if ((kind == EffectKind::SevenInsight && mode == 1) || (kind == EffectKind::SevenShift && mode == 2) ||
        kind == EffectKind::SevenUpgrade || kind == EffectKind::SevenKaiyang) {
        const bool kaiyang = kind == EffectKind::SevenKaiyang;
        const auto options = !f.resume<EntityChoiceContinuation>().entity
            ? card_rules::formationUpgradeSources(s, catalog(), owner, kaiyang)
            : upgrades(s, catalog(), owner, *f.resume<EntityChoiceContinuation>().entity, kaiyang);
        if (kaiyang && !selected && !f.resume<EntityChoiceContinuation>().entity) {
            finishSeven(f);
            return true;
        }
        if (!listed(options)) return false;
        if (!f.resume<EntityChoiceContinuation>().entity) {
            f.resume<EntityChoiceContinuation>().entity = selected;
            if (kaiyang) {
                std::vector<CardId> hand;
                for (const auto &[id, card] : s.cards)
                    if (card.owner == owner && card.zone == Zone::Hand) hand.push_back(id);
                for (auto id : hand) StateMaintenance::leave(s, id, &catalog());
            }
        }
        else {
            const auto old = *f.resume<EntityChoiceContinuation>().entity;
            finishSeven(f); replaceSeven(s, catalog(), old, selected);
        }
        return true;
    }
    if (kind == EffectKind::SevenInsight) {
        if (!selected || !listed(f.item.targetCards)) return false;
        if (!s.cards.count(selected)) return false;
        bottomViewed(s, owner, f.item.targetCards, selected); finishSeven(f); return true;
    }
    if (kind == EffectKind::SevenReturn) {
        if (selected) {
            std::vector<CardId> options;
            for (const auto &[id, card] : s.cards)
                if (card.owner == owner && card.zone == Zone::Ash && card_rules::member(catalog().at(card.definition), "七星"))
                    options.push_back(id);
            if (!listed(options)) return false;
            const auto zone = s.cards.at(selected).zone;
            card_rules::returnCard(s, catalog(), selected, false);
            if (zone == Zone::Ash || zone == Zone::Deck) ++f.resume<RepeatedChoiceContinuation>().completed;
        }
        else { shuffleSevenDeck(s, owner); finishSeven(f); }
        return true;
    }
    if (kind == EffectKind::SevenAwakening && f.resume<RepeatedChoiceContinuation>().completed == 1) {
        if (choice != ChoiceValue::mode(ChoiceMode::SevenDamage) && choice != ChoiceValue::mode(ChoiceMode::SevenLoad)) return false;
        const auto trigger = f.item; finishSeven(f);
        if (choice == ChoiceValue::mode(ChoiceMode::SevenDamage)) EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, 7, DamageType::Radiant, EffectRecipient::Owner});
        else if (choice == ChoiceValue::mode(ChoiceMode::SevenLoad)) EffectResolver::apply(s, catalog(), trigger, {EffectKind::AddIndependent, 7, DamageType::Force, EffectRecipient::Owner});
        StateMaintenance::check(s, catalog()); return true;
    }
    if (kind == EffectKind::SevenAwakening || kind == EffectKind::SevenSetup) {
        if (!s.cards.count(selected)) return false;
        const bool setup = kind == EffectKind::SevenSetup;
        const auto *sourceDefinition = source == s.cards.end() ? nullptr : &catalog().at(source->second.definition);
        const bool ash = setup ? (sourceDefinition && (hasCapability(*sourceDefinition, Capability::QixingTianquan) ||
            hasCapability(*sourceDefinition, Capability::QixingYuheng))) : mode == 2;
        const bool shiftOnly = setup && sourceDefinition &&
            (hasCapability(*sourceDefinition, Capability::QixingTianxuan) || hasCapability(*sourceDefinition, Capability::QixingTianquan));
        const auto card = s.cards.find(selected);
        if (card == s.cards.end() || card->second.owner != owner || card->second.zone != (ash ? Zone::Ash : Zone::Deck) ||
            (shiftOnly ? !hasCapability(catalog().at(card->second.definition), Capability::QixingShift) : !card_rules::member(catalog().at(card->second.definition), "七星")))
            return false;
        const bool fromDeck = s.cards.at(selected).zone == Zone::Deck;
        card_rules::returnCard(s, catalog(), selected, true);
        if (fromDeck) shuffleSevenDeck(s, owner);
        if (kind == EffectKind::SevenAwakening && mode == 1) f.resume<RepeatedChoiceContinuation>().completed = 1;
        else finishSeven(f);
        return true;
    }
    (void)actor;
    return false;
}
}




#include "wizard/core/card_rules.hpp"
#include "engine_internal.hpp"
#include <set>

namespace wizard::card_rules {
bool isSpell(const CardDefinition &d) {
    return d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman;
}
bool hasActiveTrait(const GameState &s, const CardCatalog &cat, Capability capability, PlayerId owner) {
    return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &c = entry.second;
        return (owner < 0 || c.owner == owner) && !c.faceDown &&
            (c.zone == Zone::Analysis || c.zone == Zone::Casting) && c.spell == SpellState::Active &&
            trait(cat.at(c.definition), capability);
    });
}
std::string activationError(const GameState &s, const CardCatalog &cat, PlayerId owner, CardId source, CardId target) {
    const auto &sourceCard = s.cards.at(source);
    const auto &sourceDefinition = cat.at(sourceCard.definition);
    const int activationLife = activationLifeCost(sourceDefinition);
    if (activationLife || hasCapability(sourceDefinition, Capability::BloodLastStand) || member(sourceDefinition, "七星")) {
        if (activationLife && !canPayLife(s, owner, activationLife)) return "life_cost";
        if (hasCapability(sourceDefinition, Capability::BloodLastStand) && s.players[owner].life > 15)
            return "life_condition";
        if (!member(cat.at(sourceCard.definition), "七星")) return {};
        if (hasCapability(sourceDefinition, Capability::SevenStarsLamp)) {
            const bool yaoguang = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                return entry.second.owner == owner && entry.second.zone == Zone::Analysis &&
                    hasCapability(cat.at(entry.second.definition), Capability::SevenStarsYaoguang);
            });
            if (!yaoguang) return "qixing_yaoguang_required";
        } else if (!hasCapability(sourceDefinition, Capability::QixingShift) &&
                   !hasCapability(sourceDefinition, Capability::QixingGaze) &&
                   !hasCapability(sourceDefinition, Capability::QixingReturn) && !aspectMode(s, cat, owner))
            return "qixing_mode_required";
        if (hasCapability(sourceDefinition, Capability::QixingShift) && aspectMode(s, cat, owner) == 1 && !canPayLife(s, owner, 7)) return "life_cost";
    }

    const auto &d = cat.at(sourceCard.definition);
    if (trait(d, Capability::Unique) && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        return entry.first != source && card.owner == owner && card.definition == d.id &&
            (card.zone == Zone::Analysis || card.zone == Zone::Casting);
    })) return "unique: 己方场上已存在同名唯一卡牌";
    if (d.type == CardType::Word && std::any_of(s.players.begin(), s.players.end(), [](const PlayerState &p) {
        return p.wordLockUntilOwnDraw > p.ownTurn;
    })) return "word_lock: 昏昏入睡期间双方言灵不可发动";
    if (trait(d, Capability::ArcanaFoolCondition)) {
        int hand = 0;
        for (const auto &[id, c] : s.cards) {
            if (c.owner != owner) continue;
            if (c.zone == Zone::Hand) ++hand;
            if (id != source && c.zone == Zone::Analysis && isSpell(cat.at(c.definition)))
                return "arcana_fool_condition: 解析区存在其他法术";
        }
        if (hand < 5) return "arcana_fool_condition: 需要至少五张手牌";
    }
    if (trait(d, Capability::ArcanaHangedCost) && !s.players[owner].debugInfiniteLife && s.players[owner].life <= (0))
        return "life_cost: 生命不足以支付一半生命";
    if (hasCapability(d, Capability::ArcanaMagician)) {
        std::set<std::string> names;
        for (auto id : s.players[owner].deck) names.insert(cat.at(s.cards.at(id).definition).name);
        if (names.size() < 3) return "arcana_magician_condition: 卡组需要三种不同卡名";
    }
    if (hasCapability(d, Capability::ArcanaTower)) {
        const auto selected = s.cards.find(target);
        if (selected == s.cards.end() || selected->second.owner != owner ||
            selected->second.zone != Zone::Analysis || cat.at(selected->second.definition).type != CardType::Formation)
            return "target: 高塔必须指定己方阵法";
    }
    return {};
}
std::vector<CardId> costOptions(const GameState &s, const CardCatalog &cat, PlayerId owner, CardId source) {
    {
        const auto &c = s.cards.at(source);
        const int mode = card_rules::aspectMode(s, cat, owner);
        if ((hasCapability(cat.at(c.definition), Capability::QixingDivination) && mode == 1) ||
            (hasCapability(cat.at(c.definition), Capability::QixingShift) && mode == 2)) {
            std::vector<CardId> options;
            for (const auto &[id, material] : s.cards)
                if (id != source && material.owner == owner && material.zone == Zone::Hand) options.push_back(id);
            return options;
        }
    }
    const auto &d = cat.at(s.cards.at(source).definition);
    const bool priestess = trait(d, Capability::ArcanaPriestessCost);
    const bool emperor = trait(d, Capability::ArcanaEmperorCost);
    if (!priestess && !emperor) return {0};
    std::vector<CardId> out;
    for (const auto &[id, c] : s.cards) {
        if (id == source || c.owner != owner) continue;
        const auto &definition = cat.at(c.definition);
        if (priestess && definition.type == CardType::Seal && (c.zone == Zone::Hand || c.zone == Zone::Attached))
            out.push_back(id);
        if (emperor && isSpell(definition) && c.zone == Zone::Analysis) out.push_back(id);
    }
    return out;
}
bool validCost(const GameState &s, const CardCatalog &cat, PlayerId owner, CardId source, CardId material) {
    {
        const auto &c = s.cards.at(source);
        const int mode = aspectMode(s, cat, owner);
        const bool discard = (hasCapability(cat.at(c.definition), Capability::QixingDivination) && mode == 1) ||
            (hasCapability(cat.at(c.definition), Capability::QixingShift) && mode == 2);
        if (discard) {
            const auto found = s.cards.find(material);
            return material != source && found != s.cards.end() && found->second.owner == owner && found->second.zone == Zone::Hand;
        }
    }
    const auto options = costOptions(s, cat, owner, source);
    if (std::find(options.begin(), options.end(), material) == options.end()) return false;
    const auto &definition = cat.at(s.cards.at(source).definition);
    if (trait(definition, Capability::ArcanaPriestessCost) || trait(definition, Capability::ArcanaEmperorCost)) {
        return std::any_of(s.players[owner].deck.begin(), s.players[owner].deck.end(), [&](CardId id) {
            const auto &candidate = cat.at(s.cards.at(id).definition);
            return trait(definition, Capability::ArcanaPriestessCost) ? candidate.type == CardType::Seal :
                isSpell(candidate) && candidate.rank == cat.at(s.cards.at(material).definition).rank + 3;
        });
    }
    return true;
}
void eraseCard(MutationContext &s, const CardCatalog &cat, CardId id) {
    auto &card = s.cards.at(id);
    std::vector<CardId> children;
    for (const auto &[child, c] : s.cards)
        if (c.host == id && (c.zone == Zone::Attached || c.zone == Zone::Analysis)) children.push_back(child);
    for (auto child : children) StateMaintenance::leave(s, child, &cat);
    auto &deck = s.players[card.owner].deck;
    deck.erase(std::remove(deck.begin(), deck.end(), id), deck.end());
    if (s.chain) for (auto &link : s.chain->links) if (link.item.source == id) link.sourceLost = true;
    for (auto &link : s.deferredPreparations) if (link.item.source == id) link.sourceLost = true;
    card.zone = Zone::Exile;
    card.spell = SpellState::None;
    card.host = card.sourceFormation = 0;
    card.analysisLoad = card.castLoad = card.analysisProgress = card.remaining = 0;
    card.faceDown = card.wasAmbushed = false;
    card.publiclyRevealed = true;
    card.ringIndex = -1;
    s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &load) {
        return load.source == id && load.sourceBound;
    }), s.temporary.end());
    s.events.push_back({"exile", "抹除卡牌", -1, id});
}
void payExtraCost(MutationContext &s, const CardCatalog &cat, PlayerId owner, CardId source, CardId material) {
    const auto &card = s.cards.at(source);
    {
        if (const auto cost = activationLifeCost(cat.at(card.definition)); cost)
            payLife(s, cat, owner, cost, source);
        const int mode = aspectMode(s, cat, owner);
        if (hasCapability(cat.at(card.definition), Capability::QixingShift) && mode == 1) payLife(s, cat, owner, 7, source);
        if (((hasCapability(cat.at(card.definition), Capability::QixingDivination) && mode == 1) ||
             (hasCapability(cat.at(card.definition), Capability::QixingShift) && mode == 2)) && material)
            StateMaintenance::leave(s, material, &cat);
    }

    const auto &d = cat.at(s.cards.at(source).definition);
    if (material) {
        if (trait(d, Capability::ArcanaEmperorCost)) eraseCard(s, cat, material);
        else StateMaintenance::leave(s, material, &cat);
        s.cards.at(source).targets = {material};
    }
    if (trait(d, Capability::ArcanaHangedCost) && !s.players[owner].debugInfiniteLife) {
        const int amount = (s.players[owner].life + 1) / 2;
        s.players[owner].life -= amount;
        s.events.push_back({"life_cost", "倒吊人：支付一半生命（向上取整）", -1, source, amount});
        StateMaintenance::check(s, cat);
    }
}
void advanceAnalysis(MutationContext &s, const CardCatalog &cat, CardId id, int amount) {
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || amount <= 0) return;
    auto &card = found->second;
    if (card.zone != Zone::Analysis || card.faceDown || card.spell != SpellState::Analyzing) return;
    const auto &definition = cat.at(card.definition);
    const int increment = std::min(amount, std::max(0, definition.analysisTurns - card.analysisProgress));
    if (!increment) return;
    card.analysisProgress += increment;
    s.events.push_back({"analysis_advance", "解析时间减少", -1, id, increment});
    if (card.analysisProgress >= definition.analysisTurns) {
        card.spell = SpellState::Ready;
        s.events.push_back({"analysis_ready", "法术解析完成", -1, id});
        for (const auto &[formationId, formation] : s.cards)
            if (formation.owner == card.owner && formation.zone == Zone::Analysis &&
                trait(cat.at(formation.definition), Capability::ClearLoadOnAnalysis)) {
                auto trigger = detail::snapshot(formation, cat.at(formation.definition));
                trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
                trigger.effects = {{EffectKind::ClearAnyLoad, 1}};
                s.queue.append(std::move(trigger));
            }
    }
    if (!hasCapability(definition, Capability::ArcanaChariot) && definition.type == CardType::Analytic) {
        std::vector<CardId> chariots;
        for (const auto &[other, candidate] : s.cards)
            if (candidate.owner == card.owner && hasCapability(cat.at(candidate.definition), Capability::ArcanaChariot)) chariots.push_back(other);
        for (auto chariot : chariots) advanceAnalysis(s, cat, chariot, 1);
    }
}
void onAshEntered(MutationContext &s, const CardCatalog &cat, PlayerId owner) {

    std::vector<CardId> deaths;
    for (const auto &[id, card] : s.cards)
        if (hasCapability(cat.at(card.definition), Capability::ArcanaDeath) && card.owner == owner) deaths.push_back(id);
    for (auto id : deaths) advanceAnalysis(s, cat, id, 1);
}
void onFireDamage(MutationContext &s, const CardCatalog &cat) {

    std::vector<CardId> devils;
    for (const auto &[id, card] : s.cards)
        if (hasCapability(cat.at(card.definition), Capability::ArcanaDevil)) devils.push_back(id);
    for (auto id : devils) advanceAnalysis(s, cat, id, 1);
}
void onManaSpent(MutationContext &s, const CardCatalog &cat, PlayerId owner, int amount) {
    if (amount > 0 && hasActiveTrait(s, cat, Capability::ArcanaTemperance, owner)) {
        s.players[owner].mana = std::min(maximumMana, s.players[owner].mana + 1);
        s.events.push_back({"mana_refund", "节制：消耗魔素后获得1点魔素", -1, 0, 1});
    }
}
} // namespace wizard::card_rules

namespace wizard {
namespace {
bool arcanaKind(EffectKind kind) {
    return kind >= EffectKind::ClearAllAmbush && kind <= EffectKind::ArcanaWorld;
}
void shuffleArcanaDeck(MutationContext &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t n = deck.size(); n > 1; --n)
        std::swap(deck[n - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(n))]);
}
void deckToHand(MutationContext &s, PlayerId owner, CardId id) {
    auto &deck = s.players[owner].deck;
    deck.erase(std::remove(deck.begin(), deck.end(), id), deck.end());
    auto &card = s.cards.at(id);
    card.zone = Zone::Hand;
    card.faceDown = false;
    card.publiclyRevealed = false;
    s.events.push_back({"search", "检索卡牌加入手牌", -1, id});
}
bool immune(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &d = cat.at(s.cards.at(id).definition);
    return card_rules::effectImmune(s, cat, id) ||
        hasCapability(d, Capability::EffectImmune) ||
        hasCapability(d, Capability::GreatStorm);
}
}
bool GameEngine::resolveSpellEffect() {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto effect = f.item.effects[f.cursor];
    if (!arcanaKind(effect.kind)) return false;
    const auto owner = f.item.owner;
    const auto finish = [&] { ++f.cursor; f.resume<RepeatedChoiceContinuation>().remaining = -1; f.resume<RepeatedChoiceContinuation>().completed = 0; };
    if (effect.kind == EffectKind::ClearAllAmbush) {
        std::vector<CardId> cards;
        for (const auto &[id, card] : s.cards)
            if (card.zone == Zone::Analysis && card.faceDown && !immune(s, catalog(), id)) cards.push_back(id);
        for (auto id : cards) StateMaintenance::leave(s, id, &catalog());
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaFool) {
        auto &deck = s.players[owner].deck;
        for (auto &[id, card] : s.cards) if (card.owner == owner && card.zone == Zone::Hand) {
            card.zone = Zone::Deck; deck.push_back(id);
        }
        shuffleArcanaDeck(s, owner);
        for (int i = 0; i < effect.amount && s.result == -1; ++i) ResourceRules::drawOne(s, catalog(), owner);
        finish(); return true;
    }
    if (effect.kind == EffectKind::PreventNextDamage) {
        s.players[owner].nextDamageImmune = true;
        s.events.push_back({"next_damage_immunity", "教皇：免疫下一次伤害，结束阶段开始时失效", -1, f.item.source});
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaLovers && s.cards.at(f.item.source).zone != Zone::Ash) {
        s.cards.at(f.item.source).remaining = 1;
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaHermit) {
        for (auto &[id, card] : s.cards) if (card.zone == Zone::Analysis &&
            card_rules::isSpell(catalog().at(card.definition)) && !immune(s, catalog(), id)) {
            card.faceDown = card.wasAmbushed = true;
            card.publiclyRevealed = false;
            card.ambushedTurn = s.globalTurn;
            card.spell = SpellState::None;
            card.analysisProgress = 0;
            s.events.push_back({"conceal", "隐士：法术重新盖伏", -1, id});
        }
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaHangedMan) {
        for (PlayerId p : {0, 1}) {
            std::vector<CardId> ashes;
            for (auto &[id, card] : s.cards) if (card.owner == p && card.zone == Zone::Ash) {
                if (catalog().at(card.definition).type == CardType::Formation) {

                }
                else ashes.push_back(id);
            }
            for (auto id : s.players[p].deck) {
                s.cards.at(id).zone = Zone::Ash;
                card_rules::onAshEntered(s, catalog(), p);
            }
            s.players[p].deck = ashes;
            for (auto id : ashes) { s.cards.at(id).zone = Zone::Deck; s.cards.at(id).faceDown = false; }
            shuffleArcanaDeck(s, p);
        }
        s.events.push_back({"deck_ash_exchange", "倒吊人：双方交换卡组与灰烬区"});
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaMagician && f.resume<RepeatedChoiceContinuation>().completed >= 3) {
        decision(1 - owner, DecisionKind::EffectChoice, f.item.targetCards, false);
        return true;
    }
    if (effect.kind == EffectKind::ArcanaTower && !f.resume<RepeatedChoiceContinuation>().completed) {
        const auto target = s.cards.find(f.item.targetCard);
        if (target == s.cards.end() || controllerOf(target->second) != owner || target->second.zone != Zone::Analysis ||
            catalog().at(target->second.definition).type != CardType::Formation || immune(s, catalog(), target->first)) {
            finish(); return true;
        }
        // Resolve destruction through the shared effect path, including immunity
        // and the Storm Heart owner's optional replacement decision.
        f.item.targetCards = {target->first};
        f.resume<RepeatedChoiceContinuation>().completed = 1;
        f.insertBeforeCurrent({EffectKind::Destroy, 0});
        return true;
    }
    if (effect.kind == EffectKind::ArcanaTower &&
        (f.item.targetCards.empty() || s.cards.at(f.item.targetCards.front()).zone != Zone::Ash)) {
        finish(); return true;
    }
    PlayerId chooser = owner;
    bool optional = false;
    std::vector<CardId> options;
    if (effect.kind == EffectKind::ArcanaDeath) {
        typedDecision(owner, DecisionKind::DamageChoice,
                      {ChoiceValue::mode(ChoiceMode::ArcanaDeathNecrotic),
                       ChoiceValue::mode(ChoiceMode::ArcanaDeathRadiant)}); return true;
    }
    if (effect.kind == EffectKind::ArcanaDevil) {
        auto link = std::find_if(s.chain->links.begin(), s.chain->links.end(), [&](const ChainLink &l) {
            return l.id == f.item.targetLink;
        });
        if (link == s.chain->links.end() || link->canceled || link->sourceLost) { finish(); return true; }
        for (const auto &set : detail::sets(s, link->item.target, 1, link->item.owner))
            if (!set.empty() && set.front() != link->item.targetCard &&
                std::find(link->item.targetCards.begin(), link->item.targetCards.end(), set.front()) == link->item.targetCards.end())
                options.push_back(set.front());
    } else if (effect.kind == EffectKind::ArcanaJustice) {
        int counts[2]{};
        for (const auto &[id, c] : s.cards)
            if (c.zone == Zone::Analysis && card_rules::isSpell(catalog().at(c.definition))) ++counts[c.owner];
        if (counts[0] == counts[1]) { finish(); return true; }
        chooser = counts[0] > counts[1] ? 0 : 1;
        for (const auto &[id, c] : s.cards)
            if (controllerOf(c) == chooser && c.zone == Zone::Analysis && card_rules::isSpell(catalog().at(c.definition))) options.push_back(id);
    } else if (effect.kind == EffectKind::ArcanaLovers) {
        chooser = 1 - owner;
        for (const auto &[id, c] : s.cards)
            if (controllerOf(c) == chooser && c.zone == Zone::Analysis && card_rules::isSpell(catalog().at(c.definition))) options.push_back(id);
    } else if (effect.kind == EffectKind::ArcanaTower) {
        optional = true;
        for (const auto &[id, c] : s.cards)
            if (controllerOf(c) != owner && c.zone == Zone::Analysis && catalog().at(c.definition).type == CardType::Formation &&
                !immune(s, catalog(), id) && !card_rules::protectedFormation(s, catalog(), id) &&
                !catalog().at(c.definition).opponentDestroyProtected &&
                !hasCapability(catalog().at(c.definition), Capability::EffectIndestructible)) options.push_back(id);
    } else {
        if (effect.kind == EffectKind::ArcanaWorld) {
            optional = true;
            if (!f.resume<CatalogWalkContinuation>().next) f.resume<CatalogWalkContinuation>().next = 0;
            if (*f.resume<CatalogWalkContinuation>().next >= 4) { shuffleArcanaDeck(s, owner); finish(); return true; }
        }
        std::set<std::string> selectedNames;
        if (effect.kind == EffectKind::ArcanaMagician)
            for (auto id : f.item.targetCards) selectedNames.insert(catalog().at(s.cards.at(id).definition).name);
        for (auto id : s.players[owner].deck) {
            const auto &d = catalog().at(s.cards.at(id).definition);
            bool eligible = effect.kind == EffectKind::ArcanaPriestess ? d.type == CardType::Seal :
                effect.kind == EffectKind::ArcanaEmperor ? !f.item.targetCards.empty() && card_rules::isSpell(d) &&
                    d.rank == catalog().at(s.cards.at(f.item.targetCards.front()).definition).rank + 3 :
                effect.kind == EffectKind::ArcanaMagician ? !selectedNames.count(d.name) :
                effect.kind == EffectKind::ArcanaWorld ?
                    (*f.resume<CatalogWalkContinuation>().next == 0 ? d.type == CardType::Analytic :
                     *f.resume<CatalogWalkContinuation>().next == 1 ? d.type == CardType::Word && !hasCapability(d, Capability::ArcanaWorld) :
                     *f.resume<CatalogWalkContinuation>().next == 2 ? d.type == CardType::Talisman : d.type == CardType::Seal) : false;
            if (eligible) options.push_back(id);
        }
    }
    if (options.empty()) {
        if (effect.kind == EffectKind::ArcanaWorld) { ++*f.resume<CatalogWalkContinuation>().next; return true; }
        if (effect.kind == EffectKind::ArcanaMagician && !f.item.targetCards.empty()) {
            const auto chosen = detail::bounded(s, static_cast<std::uint32_t>(f.item.targetCards.size()));
            const auto cards = f.item.targetCards;
            for (std::size_t i = 0; i < cards.size(); ++i)
                if (i == chosen) deckToHand(s, owner, cards[i]); else card_rules::eraseCard(s, catalog(), cards[i]);
        }
        finish(); return true;
    }
    decision(chooser, DecisionKind::EffectChoice, std::move(options), optional);
    return true;
}
bool GameEngine::chooseSpellEffect(PlayerId actor, ChoiceValue choice) {
    const auto selected = static_cast<CardId>(choice.value);
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    if (!arcanaKind(e.kind)) return false;
    const auto owner = f.item.owner;
    const auto finish = [&] { ++f.cursor; f.resume<RepeatedChoiceContinuation>().remaining = -1; f.resume<RepeatedChoiceContinuation>().completed = 0; };
    if (e.kind == EffectKind::ArcanaDeath) {
        const auto damageType = choice == ChoiceValue::mode(ChoiceMode::ArcanaDeathNecrotic)
            ? DamageType::Necrotic : DamageType::Radiant;
        auto damage = Effect{EffectKind::Damage, e.amount, damageType};
        EffectResolver::apply(s, catalog(), f.item, damage);
        if (s.effect) finish();
        return true;
    }
    if (e.kind == EffectKind::ArcanaMagician) {
        if (f.resume<RepeatedChoiceContinuation>().completed >= 3) {
            const auto cards = f.item.targetCards;
            for (auto id : cards)
                if (id == selected) deckToHand(s, owner, id); else card_rules::eraseCard(s, catalog(), id);
            shuffleArcanaDeck(s, owner); finish(); return true;
        }
        f.item.targetCards.push_back(selected);
        s.cards.at(selected).publiclyRevealed = true;
        s.events.push_back({"reveal", "魔术师：展示候选卡牌", -1, selected});
        if (++f.resume<RepeatedChoiceContinuation>().completed < 3) return true;
        return true;
        const auto chosen = detail::bounded(s, static_cast<std::uint32_t>(f.item.targetCards.size()));
        const auto cards = f.item.targetCards;
        for (std::size_t i = 0; i < cards.size(); ++i)
            if (i == chosen) deckToHand(s, owner, cards[i]); else card_rules::eraseCard(s, catalog(), cards[i]);
        shuffleArcanaDeck(s, owner); finish(); return true;
    }
    if (e.kind == EffectKind::ArcanaPriestess || e.kind == EffectKind::ArcanaEmperor) {
        deckToHand(s, owner, selected); shuffleArcanaDeck(s, owner); finish(); return true;
    }
    if (e.kind == EffectKind::ArcanaWorld) {
        if (selected) deckToHand(s, owner, selected);
        ++*f.resume<CatalogWalkContinuation>().next; return true;
    }
    if (e.kind == EffectKind::ArcanaTower) {
        finish();
        if (selected) {
            f.item.targetCard = selected;
            f.item.targetCards = {selected};
            f.insertBeforeCurrent({EffectKind::Destroy, 0});
        }
        return true;
    }
    if (e.kind == EffectKind::ArcanaJustice || e.kind == EffectKind::ArcanaLovers) {
        if (selected) StateMaintenance::leave(s, selected, &catalog());
        if (e.kind != EffectKind::ArcanaJustice) finish();
        return true;
    }
    if (e.kind == EffectKind::ArcanaDevil) {
        auto link = std::find_if(s.chain->links.begin(), s.chain->links.end(), [&](const ChainLink &l) {
            return l.id == f.item.targetLink;
        });
        if (link != s.chain->links.end()) {
            link->item.targetCard = selected;
            if (link->item.targetCards.empty()) link->item.targetCards = {selected};
            else link->item.targetCards.front() = selected;
            if (link->item.target == TargetKind::Player)
                link->item.targetPlayer = static_cast<PlayerId>(choice.value);
            s.events.push_back({"retarget", "恶魔：改写对方法术目标", -1, selected});
            EffectResolver::apply(s, catalog(), f.item, {EffectKind::AddIndependent, 6, DamageType::Force, EffectRecipient::Owner});
        }
        if (s.effect) finish();
        return true;
    }
    (void)actor;
    return false;
}
} // namespace wizard


#include "wizard/core/card_rules.hpp"
#include "wizard/core/engine.hpp"
#include "wizard/core/card_rules.hpp"
#include "engine_internal.hpp"
#include <algorithm>
#include <set>

namespace wizard::card_rules {
bool hasFormation(const GameState &s, const CardCatalog &cat, PlayerId p, Capability capability) {
    return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        return card.owner == p && card.zone == Zone::Analysis &&
               cat.at(card.definition).type == CardType::Formation &&
               hasCapability(cat.at(card.definition), capability);
    });
}
bool isFireSpell(const CardDefinition &d) {
    if (hasCapability(d, Capability::FireDamageSpell))
        return isSpell(d);
    const auto fire = [](const Effect &e) {
        if (e.damageType != DamageType::Fire) return false;
        if (e.kind == EffectKind::Damage || e.kind == EffectKind::ChooseDamage ||
            e.kind == EffectKind::RayVolley) return true;
        // These v2.6 structured damage effects are adapted to regular Damage
        // effects by EffectResolver before they resolve.
        return e.kind == EffectKind::V260Structured &&
            (e.semantic == "area_damage" || e.semantic == "both_damage");
    };
    return isSpell(d) && (std::any_of(d.effects.begin(), d.effects.end(), fire) ||
        std::any_of(d.responses.begin(), d.responses.end(), [&](const ResponseAbility &ability) {
            return std::any_of(ability.effects.begin(), ability.effects.end(), fire);
        }));
}
bool activationAllowed(const GameState &s, const CardCatalog &cat, PlayerId p, const CardDefinition &d) {
    if (hasCapability(d, Capability::SiqueRequiresCampfire) &&
        !hasFormation(s, cat, p, Capability::SiqueCampfire)) return false;
    if (hasCapability(d, Capability::SiqueRequiresCauldron) &&
        !hasFormation(s, cat, p, Capability::SiqueCauldron)) return false;
    // The cauldron restricts activations to the Sique series, while still
    // allowing fire-damage spells as its explicit exception.
    return !hasFormation(s, cat, p, Capability::SiqueCauldron) || member(d, "味契") || isFireSpell(d);
}
bool protectedFormation(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto card = s.cards.find(id);
    return card != s.cards.end() && hasCapability(cat.at(card->second.definition), Capability::SiqueCampfire) &&
           hasFormation(s, cat, card->second.owner, Capability::SiqueCauldron);
}
int attachedCount(const GameState &s, const CardCatalog &cat, PlayerId p, Capability capability) {
    return static_cast<int>(std::count_if(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        return card.owner == p && card.zone == Zone::Attached &&
               hasCapability(cat.at(card.definition), capability);
    }));
}
int detachCost(const CardDefinition &d) {
    for (int count = 1; count <= 3; ++count)
        if ((count == 1 && hasCapability(d, Capability::SiqueDetach1)) ||
            (count == 2 && hasCapability(d, Capability::SiqueDetach2)) ||
            (count == 3 && hasCapability(d, Capability::SiqueDetach3)))
            return count;
    return 0;
}
bool validDetach(const GameState &s, const CardCatalog &cat, PlayerId p, const CardDefinition &d,
                 const std::vector<CardId> &ids) {
    if (static_cast<int>(ids.size()) != detachCost(d)) return false;
    std::set<CardId> unique;
    for (auto id : ids) {
        auto card = s.cards.find(id);
        if (!unique.insert(id).second || card == s.cards.end() || card->second.owner != p ||
            card->second.zone != Zone::Attached || cat.at(card->second.definition).type != CardType::Seal ||
            !member(cat.at(card->second.definition), "味契")) return false;
    }
    return true;
}
std::vector<std::vector<CardId>> detachOptions(const GameState &s, const CardCatalog &cat, PlayerId p,
                                              const CardDefinition &d) {
    std::vector<std::vector<CardId>> out;
    std::vector<CardId> pool, selected;
    const int count = detachCost(d);
    for (const auto &[id, card] : s.cards)
        if (card.owner == p && card.zone == Zone::Attached && cat.at(card.definition).type == CardType::Seal &&
            member(cat.at(card.definition), "味契")) pool.push_back(id);
    auto visit = [&](auto &&self, std::size_t start) -> void {
        if (static_cast<int>(selected.size()) == count) { out.push_back(selected); return; }
        for (std::size_t i = start; i < pool.size(); ++i) {
            selected.push_back(pool[i]); self(self, i + 1); selected.pop_back();
        }
    };
    visit(visit, 0);
    return out;
}
bool canPay(const GameState &s, PlayerId p, int amount, const CardCatalog *cat) {
    if (cat) return card_rules::canPay(s, *cat, p, amount);
    if (amount < 0) return false;
    if (s.players[p].debugInfiniteMana) return true;
    if (s.players[p].siqueLifePaymentTurn && s.players[p].siqueLifePaymentTurn == s.globalTurn) {
        // Pre-2.5.3 recordings retain their original unrestricted substitution.
        return !cat || s.players[1 - p].debugInfiniteLife ||
            s.players[1 - p].life > amount;
    }
    return ResourceRules::canPayMana(s, p, amount);
}
void pay(MutationContext &s, PlayerId p, int amount, const CardCatalog *cat) {
    if (cat) { card_rules::pay(s, *cat, p, amount); return; }
    if (s.players[p].debugInfiniteMana) return;
    if (s.players[p].siqueLifePaymentTurn && s.players[p].siqueLifePaymentTurn == s.globalTurn) {
        if (s.players[1 - p].debugInfiniteLife) return;
        if (cat && !canPay(s, p, amount, cat)) return;
        s.players[1 - p].life -= amount;
        s.events.push_back({"life_payment", "神恩炖肉：宣告时以对方普通生命按1:1支付魔素费用", -1, 0, amount});
    } else {
        ResourceRules::payMana(s, p, amount);
        if (amount > 0 && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
            const auto &card = entry.second;
            return cat && card.owner == p && hasCapability(cat->at(card.definition), Capability::ArcanaTemperance) && !card.faceDown &&
                (card.zone == Zone::Analysis || card.zone == Zone::Casting) && card.spell == SpellState::Active;
        })) {
            s.players[p].mana = std::min(maximumMana, s.players[p].mana + 1);
            s.events.push_back({"mana_refund", "节制：消耗魔素后获得1点魔素", -1, 0, 1});
        }
    }
}
void attached(MutationContext &s, const CardCatalog &cat, CardId id) {
    auto &card = s.cards.at(id);
    if (card.siquePairTurn == s.globalTurn && card.siquePartner) {
        const auto partner = s.cards.find(card.siquePartner);
        if (partner != s.cards.end() && partner->second.zone == Zone::Hand && controllerOf(partner->second) == controllerOf(card))
            StateMaintenance::leave(s, partner->first, &cat);
    }
    const auto &def = cat.at(card.definition);
    if (member(def, "味契") && !def.effects.empty()) {
        Trigger t;
        t.id = s.nextTrigger++; t.batch = s.nextBatch++;
        t.owner = card.owner; t.source = id; t.effects = def.effects;
        s.queue.append(std::move(t));
    }
}
bool threatensFormation(const GameState &s, const CardCatalog &cat, const ChainLink &link, PlayerId owner) {
    const bool destroys = std::any_of(link.item.effects.begin(), link.item.effects.end(), [](const Effect &e) {
        return e.kind == EffectKind::Destroy || e.kind == EffectKind::DestroyAllEnemyField;
    });
    if (!destroys) return false;
    auto target = [&](CardId id) {
        const auto found = s.cards.find(id);
        return found != s.cards.end() && found->second.owner == owner && found->second.zone == Zone::Analysis &&
            cat.at(found->second.definition).type == CardType::Formation && member(cat.at(found->second.definition), "味契");
    };
    if (target(link.item.targetCard) || std::any_of(link.item.targetCards.begin(), link.item.targetCards.end(), target))
        return true;
    const bool broad = std::any_of(link.item.effects.begin(), link.item.effects.end(), [](const Effect &e) {
        return e.kind == EffectKind::DestroyAllEnemyField;
    });
    return broad && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) { return target(entry.first); });
}
} // namespace wizard::card_rules

namespace wizard {
namespace {
bool siqueChoice(EffectKind kind) {
    return kind == EffectKind::SiqueRecoverSpell || kind == EffectKind::SiqueRecoverSeal ||
        kind == EffectKind::SiqueMillSeal || kind == EffectKind::SiqueMillSpell ||
        kind == EffectKind::SiqueSearchPair || kind == EffectKind::SiqueAttachFromDeck ||
        kind == EffectKind::SiqueRecycle || kind == EffectKind::SiqueOptionalDraw ||
        kind == EffectKind::SiqueShuffleHands || kind == EffectKind::SiqueRoastRecover ||
        kind == EffectKind::SiqueBlowfire || kind == EffectKind::SiqueBackfire;
}
void shuffleSiqueDeck(MutationContext &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t size = deck.size(); size > 1; --size)
        std::swap(deck[size - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(size))]);
}
void finishSiqueEffect(EffectFrame &frame) {
    ++frame.cursor; frame.resume<RepeatedChoiceContinuation>().remaining = -1; frame.resume<RepeatedChoiceContinuation>().completed = 0;
    frame.item.targetCards.clear();
}
void finishRecycleEffect(EffectFrame &frame, bool preserveCount) {
    const auto count = frame.resume<RepeatedChoiceContinuation>().completed;
    finishSiqueEffect(frame);
    if (preserveCount) frame.resume<RepeatedChoiceContinuation>().completed = count;
}
} // namespace
bool GameEngine::resolveRecipeEffect() {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto owner = f.item.owner;
    const int maximumSelection = std::min(5, std::max(0, e.amount));
    if (e.kind == EffectKind::SiqueDamageCap) {
        s.players[owner].siqueDamageCap = e.amount;
        s.players[owner].siqueCapUntilOwnTurn = s.players[owner].ownTurn + 1;
        finishSiqueEffect(f); return true;
    }
    if (e.kind == EffectKind::SiqueLifePayment) {
        s.players[owner].siqueLifePaymentTurn = s.globalTurn;
        finishSiqueEffect(f); return true;
    }
    if (e.kind == EffectKind::SiqueDetachAll) {
        std::vector<CardId> seals;
        for (const auto &[id, card] : s.cards)
            if (card.zone == Zone::Attached && card.host == f.item.source) seals.push_back(id);
        for (auto id : seals) StateMaintenance::leave(s, id, &catalog());
        finishSiqueEffect(f); return true;
    }
    if (!siqueChoice(e.kind)) return false;
    if ((e.kind == EffectKind::SiqueBackfire || e.kind == EffectKind::SiqueRecycle) &&
        f.resume<RepeatedChoiceContinuation>().completed >= maximumSelection) {
        if (e.kind == EffectKind::SiqueRecycle) shuffleSiqueDeck(s, owner);
        const int count = f.resume<RepeatedChoiceContinuation>().completed;
        const auto trigger = f.item;
        finishRecycleEffect(f, e.kind == EffectKind::SiqueRecycle);
        if (e.kind == EffectKind::SiqueBackfire) {
            shuffleSiqueDeck(s, 1 - owner);
            EffectResolver::apply(s, catalog(), trigger,
                {EffectKind::Damage, count, DamageType::Fire, EffectRecipient::Opponent});
            if (s.result == -1) EffectResolver::apply(s, catalog(), trigger,
                {EffectKind::Heal, count * 2, DamageType::Force, EffectRecipient::Owner});
        }
        return true;
    }
    if (e.kind == EffectKind::SiqueShuffleHands) {
        if (!f.resume<PlayerWalkContinuation>().next) {
            f.item.targetCards = {0, 0};
            for (auto &[id, card] : s.cards) if (card.zone == Zone::Hand) {
                ++f.item.targetCards[card.owner]; card.zone = Zone::Deck;
                s.players[card.owner].deck.push_back(id);
                card.siquePartner = 0; card.siquePairTurn = 0;
            }
            shuffleSiqueDeck(s, 0); shuffleSiqueDeck(s, 1);
            f.resume<PlayerWalkContinuation>().next = 0;
        }
        if (*f.resume<PlayerWalkContinuation>().next >= 2) { finishSiqueEffect(f); return true; }
        const auto player = *f.resume<PlayerWalkContinuation>().next;
        if (!f.item.targetCards[player]) { ++*f.resume<PlayerWalkContinuation>().next; return true; }
        typedDecision(player, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::Draw)}, true);
        return true;
    }
    if (e.kind == EffectKind::SiqueOptionalDraw) {
        if (f.cursor > 0 && f.item.effects[f.cursor - 1].kind == EffectKind::SiqueRecycle && !f.resume<RepeatedChoiceContinuation>().completed) {
            finishSiqueEffect(f); return true;
        }
        typedDecision(owner, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::Draw)}, true); return true;
    }
    std::vector<CardId> choices;
    if (e.kind == EffectKind::SiqueAttachFromDeck && f.resume<EntityChoiceContinuation>().entity) {
        for (const auto &[id, card] : s.cards) {
            const auto &d = catalog().at(card.definition);
            if (card.owner == owner && card.zone == Zone::Analysis && d.type == CardType::Formation &&
                card_rules::member(d, "味契") && hasCapability(d, Capability::SiqueCauldron)) choices.push_back(id);
        }
    } else for (const auto &[id, card] : s.cards) {
        const auto &d = catalog().at(card.definition);
        const bool backfire = e.kind == EffectKind::SiqueBackfire;
        if (backfire ? card.owner == owner : card.owner != owner) continue;
        if (!backfire && e.kind != EffectKind::SiqueBlowfire && !card_rules::member(d, "味契")) continue;
        const bool rune = d.type == CardType::Seal;
        bool eligible = false;
        switch (e.kind) {
        case EffectKind::SiqueRecoverSpell: eligible = card.zone == Zone::Ash && card_rules::isSpell(d); break;
        case EffectKind::SiqueRecoverSeal: eligible = card.zone == Zone::Ash && rune; break;
        case EffectKind::SiqueMillSeal:
        case EffectKind::SiqueSearchPair:
        case EffectKind::SiqueAttachFromDeck: eligible = card.zone == Zone::Deck && rune; break;
        case EffectKind::SiqueMillSpell: eligible = card.zone == Zone::Deck && card_rules::isSpell(d); break;
        case EffectKind::SiqueRecycle: eligible = card.zone == Zone::Ash; break;
        case EffectKind::SiqueRoastRecover: eligible = card.zone == Zone::Ash; break;
        case EffectKind::SiqueBlowfire: eligible = card.zone == Zone::Hand; break;
        case EffectKind::SiqueBackfire: eligible = card.zone == Zone::Ash; break;
        default: break;
        }
        if (eligible) choices.push_back(id);
    }
    if (e.kind == EffectKind::SiqueSearchPair && f.resume<RepeatedChoiceContinuation>().completed >= 2) choices.clear();
    if ((e.kind == EffectKind::SiqueRecycle || e.kind == EffectKind::SiqueBackfire) &&
        f.resume<RepeatedChoiceContinuation>().completed >= maximumSelection) choices.clear();
    if (choices.empty()) {
        if (e.kind == EffectKind::SiqueBackfire) {
            shuffleSiqueDeck(s, 1 - owner);
            const int count = f.resume<RepeatedChoiceContinuation>().completed;
            auto trigger = f.item;
            finishSiqueEffect(f);
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, count, DamageType::Fire, EffectRecipient::Opponent});
            if (s.result == -1) EffectResolver::apply(s, catalog(), trigger,
                {EffectKind::Heal, count * 2, DamageType::Force, EffectRecipient::Owner});
            return true;
        }
        if (e.kind == EffectKind::SiqueSearchPair || e.kind == EffectKind::SiqueAttachFromDeck ||
            e.kind == EffectKind::SiqueMillSeal || e.kind == EffectKind::SiqueMillSpell ||
            e.kind == EffectKind::SiqueRecycle) shuffleSiqueDeck(s, owner);
        finishRecycleEffect(f, e.kind == EffectKind::SiqueRecycle); return true;
    }
    const bool mayStop = (e.kind == EffectKind::SiqueRecycle || e.kind == EffectKind::SiqueBackfire) &&
        (f.resume<RepeatedChoiceContinuation>().completed > 0);
    decision(owner, DecisionKind::EffectChoice, std::move(choices), mayStop ||
        e.kind == EffectKind::SiqueRoastRecover || e.kind == EffectKind::SiqueBlowfire);
    return true;
}
bool GameEngine::chooseRecipeEffect(PlayerId actor, ChoiceValue choice) {
    const auto selected = static_cast<CardId>(choice.value);
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto kind = f.item.effects[f.cursor].kind;
    if (!siqueChoice(kind)) return false;
    if (kind == EffectKind::SiqueBackfire) {
        if (!selected) {
            const int count = f.resume<RepeatedChoiceContinuation>().completed;
            const auto trigger = f.item;
            shuffleSiqueDeck(s, 1 - actor); finishSiqueEffect(f);
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, count, DamageType::Fire, EffectRecipient::Opponent});
            if (s.result == -1) EffectResolver::apply(s, catalog(), trigger,
                {EffectKind::Heal, count * 2, DamageType::Force, EffectRecipient::Owner});
        } else {
            auto &card = s.cards.at(selected);
            card_rules::returnCard(s, catalog(), selected, false);
            card.faceDown = false; card.siquePartner = 0; card.siquePairTurn = 0;
            ++f.resume<RepeatedChoiceContinuation>().completed;
        }
        return true;
    }
    if (kind == EffectKind::SiqueBlowfire) {
        const auto trigger = f.item;
        if (selected) StateMaintenance::leave(s, selected, &catalog());
        finishSiqueEffect(f);
        if (selected) EffectResolver::apply(s, catalog(), trigger,
            {EffectKind::Damage, 4, DamageType::Fire, EffectRecipient::Opponent});
        return true;
    }
    if (kind == EffectKind::SiqueShuffleHands) {
        const auto count = f.item.targetCards[static_cast<std::size_t>(*f.resume<PlayerWalkContinuation>().next)];
        ++*f.resume<PlayerWalkContinuation>().next;
        if (selected) for (CardId i = 0; i < count && s.result == -1; ++i) {
            ResourceRules::drawOne(s, catalog(), actor); StateMaintenance::check(s, catalog());
        }
        return true;
    }
    if (kind == EffectKind::SiqueOptionalDraw) {
        finishSiqueEffect(f);
        if (selected) ResourceRules::drawOne(s, catalog(), actor);
        StateMaintenance::check(s, catalog()); return true;
    }
    if (!selected) {
        shuffleSiqueDeck(s, actor);
        finishRecycleEffect(f, kind == EffectKind::SiqueRecycle);
        return true;
    }
    auto &card = s.cards.at(selected);
    auto &deck = s.players[actor].deck;
    if (kind == EffectKind::SiqueAttachFromDeck) {
        if (!f.resume<EntityChoiceContinuation>().entity) { f.resume<EntityChoiceContinuation>().entity = selected; return true; }
        const auto runeId = *f.resume<EntityChoiceContinuation>().entity;
        auto &rune = s.cards.at(runeId);
        deck.erase(std::remove(deck.begin(), deck.end(), runeId), deck.end());
        rune.zone = Zone::Attached; rune.host = selected;
        card_rules::attached(s, catalog(), runeId);
        shuffleSiqueDeck(s, actor); finishSiqueEffect(f); return true;
    }
    if (kind == EffectKind::SiqueMillSeal || kind == EffectKind::SiqueMillSpell) {
        StateMaintenance::leave(s, selected, &catalog());
        shuffleSiqueDeck(s, actor); finishSiqueEffect(f); return true;
    }
    if (kind == EffectKind::SiqueRecycle) {
        card_rules::returnCard(s, catalog(), selected, false);
        card.faceDown = false; card.siquePartner = 0; card.siquePairTurn = 0;
        ++f.resume<RepeatedChoiceContinuation>().completed; return true;
    }
    deck.erase(std::remove(deck.begin(), deck.end(), selected), deck.end());
    card_rules::returnCard(s, catalog(), selected, true);
    s.events.push_back({"sique_recover", "味契卡加入手牌", actor, selected});
    if (kind == EffectKind::SiqueSearchPair) {
        if (!f.item.targetCards.empty()) {
            const auto first = f.item.targetCards.front();
            card.siquePartner = first; card.siquePairTurn = s.globalTurn;
            s.cards.at(first).siquePartner = selected; s.cards.at(first).siquePairTurn = s.globalTurn;
        }
        f.item.targetCards.push_back(selected); ++f.resume<RepeatedChoiceContinuation>().completed;
        return true;
    }
    finishSiqueEffect(f); return true;
}
} // namespace wizard


