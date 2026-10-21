#include "wizard/core/arcana.hpp"
#include "wizard/core/sique.hpp"
#include "engine_internal.hpp"
#include <set>

namespace wizard::arcana {
namespace {
bool trait(const CardDefinition &d, const char *name) {
    return std::find(d.traits.begin(), d.traits.end(), name) != d.traits.end();
}
}
bool spell(const CardDefinition &d) {
    return d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman;
}
bool active(const GameState &s, const CardCatalog &cat, const char *name, PlayerId owner) {
    return cat.usesAlphaV252() && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &c = entry.second;
        return (owner < 0 || c.owner == owner) && !c.faceDown &&
            (c.zone == Zone::Analysis || c.zone == Zone::Casting) && c.spell == SpellState::Active &&
            trait(cat.at(c.definition), name);
    });
}
std::string activationError(const GameState &s, const CardCatalog &cat, PlayerId owner, CardId source, CardId target) {
    if (!cat.usesAlphaV252()) return {};
    const auto &d = cat.at(s.cards.at(source).definition);
    if (trait(d, "unique") && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        return entry.first != source && card.owner == owner && card.definition == d.id &&
            (card.zone == Zone::Analysis || card.zone == Zone::Casting);
    })) return "unique: 己方场上已存在同名唯一卡牌";
    if (d.type == CardType::Word && std::any_of(s.players.begin(), s.players.end(), [](const PlayerState &p) {
        return p.wordLockUntilOwnDraw > p.ownTurn;
    })) return "word_lock: 昏昏入睡期间双方言灵不可发动";
    if (trait(d, "arcana_fool_condition")) {
        int hand = 0;
        for (const auto &[id, c] : s.cards) {
            if (c.owner != owner) continue;
            if (c.zone == Zone::Hand) ++hand;
            if (id != source && c.zone == Zone::Analysis && spell(cat.at(c.definition)))
                return "arcana_fool_condition: 解析区存在其他法术";
        }
        if (hand < 5) return "arcana_fool_condition: 需要至少五张手牌";
    }
    if (trait(d, "arcana_hanged_cost") && s.players[owner].life <= 1)
        return "life_cost: 生命不足以支付一半生命";
    if (d.id == "arcana-magician") {
        std::set<std::string> names;
        for (auto id : s.players[owner].deck) names.insert(cat.at(s.cards.at(id).definition).name);
        if (names.size() < 3) return "arcana_magician_condition: 卡组需要三种不同卡名";
    }
    if (d.id == "arcana-tower") {
        const auto selected = s.cards.find(target);
        if (selected == s.cards.end() || selected->second.owner != owner ||
            selected->second.zone != Zone::Analysis || cat.at(selected->second.definition).type != CardType::Formation)
            return "target: 高塔必须指定己方阵法";
    }
    return {};
}
std::vector<CardId> costOptions(const GameState &s, const CardCatalog &cat, PlayerId owner, CardId source) {
    const auto &d = cat.at(s.cards.at(source).definition);
    const bool priestess = cat.usesAlphaV252() && trait(d, "arcana_priestess_cost");
    const bool emperor = cat.usesAlphaV252() && trait(d, "arcana_emperor_cost");
    if (!priestess && !emperor) return {0};
    std::vector<CardId> out;
    for (const auto &[id, c] : s.cards) {
        if (id == source || c.owner != owner) continue;
        const auto &definition = cat.at(c.definition);
        if (priestess && definition.type == CardType::Seal && (c.zone == Zone::Hand || c.zone == Zone::Attached))
            out.push_back(id);
        if (emperor && spell(definition) && c.zone == Zone::Analysis) out.push_back(id);
    }
    return out;
}
bool validCost(const GameState &s, const CardCatalog &cat, PlayerId owner, CardId source, CardId material) {
    const auto options = costOptions(s, cat, owner, source);
    if (std::find(options.begin(), options.end(), material) == options.end()) return false;
    const auto &definition = cat.at(s.cards.at(source).definition);
    if (trait(definition, "arcana_priestess_cost") || trait(definition, "arcana_emperor_cost")) {
        return std::any_of(s.players[owner].deck.begin(), s.players[owner].deck.end(), [&](CardId id) {
            const auto &candidate = cat.at(s.cards.at(id).definition);
            return trait(definition, "arcana_priestess_cost") ? candidate.type == CardType::Seal :
                spell(candidate) && candidate.rank == cat.at(s.cards.at(material).definition).rank + 3;
        });
    }
    return true;
}
void erase(GameState &s, const CardCatalog &cat, CardId id) {
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
void payCost(GameState &s, const CardCatalog &cat, PlayerId owner, CardId source, CardId material) {
    if (!cat.usesAlphaV252()) return;
    const auto &d = cat.at(s.cards.at(source).definition);
    if (material) {
        if (trait(d, "arcana_emperor_cost")) erase(s, cat, material);
        else StateMaintenance::leave(s, material, &cat);
        s.cards.at(source).targets = {material};
    }
    if (trait(d, "arcana_hanged_cost")) {
        const int amount = (s.players[owner].life + 1) / 2;
        s.players[owner].life -= amount;
        s.events.push_back({"life_cost", "倒吊人：支付一半生命（向上取整）", -1, source, amount});
    }
}
void advance(GameState &s, const CardCatalog &cat, CardId id, int amount) {
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
                trait(cat.at(formation.definition), "clear_load_on_analysis")) {
                auto trigger = detail::snapshot(formation, cat.at(formation.definition));
                trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
                trigger.effects = {{EffectKind::ClearAnyLoad, 1}};
                s.queue.append(std::move(trigger));
            }
    }
    if (cat.usesAlphaV252() && definition.id != "arcana-chariot") {
        std::vector<CardId> chariots;
        for (const auto &[other, candidate] : s.cards)
            if (candidate.owner == card.owner && candidate.definition == "arcana-chariot") chariots.push_back(other);
        for (auto chariot : chariots) advance(s, cat, chariot, 1);
    }
}
void ashEntered(GameState &s, const CardCatalog &cat) {
    if (!cat.usesAlphaV252()) return;
    std::vector<CardId> deaths;
    for (const auto &[id, card] : s.cards) if (card.definition == "arcana-death") deaths.push_back(id);
    for (auto id : deaths) advance(s, cat, id, 1);
}
void fireDamage(GameState &s, const CardCatalog &cat) {
    if (!cat.usesAlphaV252()) return;
    std::vector<CardId> devils;
    for (const auto &[id, card] : s.cards) if (card.definition == "arcana-devil") devils.push_back(id);
    for (auto id : devils) advance(s, cat, id, 1);
}
void manaSpent(GameState &s, const CardCatalog &cat, PlayerId owner, int amount) {
    if (amount > 0 && active(s, cat, "arcana_temperance", owner)) {
        s.players[owner].mana = std::min(maximumMana, s.players[owner].mana + 1);
        s.events.push_back({"mana_refund", "节制：消耗魔素后获得1点魔素", -1, 0, 1});
    }
}
} // namespace wizard::arcana

namespace wizard {
namespace {
bool arcanaKind(EffectKind kind) {
    return kind >= EffectKind::ClearAllAmbush && kind <= EffectKind::ArcanaWorld;
}
void shuffleArcanaDeck(GameState &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t n = deck.size(); n > 1; --n)
        std::swap(deck[n - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(n))]);
}
void deckToHand(GameState &s, PlayerId owner, CardId id) {
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
    return std::find(d.traits.begin(), d.traits.end(), "effect_immune") != d.traits.end();
}
}
bool GameEngine::arcanaEffect() {
    auto &s = state_;
    if (!catalog_.usesAlphaV252() || !s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto effect = f.item.effects[f.cursor];
    if (!arcanaKind(effect.kind)) return false;
    const auto owner = f.item.owner;
    const auto finish = [&] { ++f.cursor; f.clearRemaining = -1; f.resolvedCount = 0; };
    if (effect.kind == EffectKind::ClearAllAmbush) {
        std::vector<CardId> cards;
        for (const auto &[id, card] : s.cards)
            if (card.zone == Zone::Analysis && card.faceDown && !immune(s, catalog_, id)) cards.push_back(id);
        for (auto id : cards) StateMaintenance::leave(s, id, &catalog_);
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaFool) {
        auto &deck = s.players[owner].deck;
        for (auto &[id, card] : s.cards) if (card.owner == owner && card.zone == Zone::Hand) {
            card.zone = Zone::Deck; deck.push_back(id);
        }
        shuffleArcanaDeck(s, owner);
        for (int i = 0; i < effect.amount && s.result == -1; ++i) ResourceRules::drawOne(s, catalog_, owner);
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
            arcana::spell(catalog_.at(card.definition)) && !immune(s, catalog_, id)) {
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
                if (catalog_.at(card.definition).type == CardType::Formation) card.zone = Zone::SideDeck;
                else ashes.push_back(id);
            }
            for (auto id : s.players[p].deck) {
                s.cards.at(id).zone = Zone::Ash;
                arcana::ashEntered(s, catalog_);
            }
            s.players[p].deck = ashes;
            for (auto id : ashes) { s.cards.at(id).zone = Zone::Deck; s.cards.at(id).faceDown = false; }
            shuffleArcanaDeck(s, p);
        }
        s.events.push_back({"deck_ash_exchange", "倒吊人：双方交换卡组与灰烬区"});
        finish(); return true;
    }
    if (effect.kind == EffectKind::ArcanaTower && !f.resolvedCount) {
        const auto target = s.cards.find(f.item.targetCard);
        if (target == s.cards.end() || target->second.owner != owner || target->second.zone != Zone::Analysis ||
            catalog_.at(target->second.definition).type != CardType::Formation || immune(s, catalog_, target->first)) {
            finish(); return true;
        }
        // Resolve destruction through the shared effect path, including immunity
        // and the Storm Heart owner's optional replacement decision.
        f.item.targetCards = {target->first};
        f.resolvedCount = 1;
        f.item.effects.insert(f.item.effects.begin() + static_cast<std::ptrdiff_t>(f.cursor),
                             {EffectKind::Destroy, 0});
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
        decision(owner, DecisionKind::DamageChoice, {1, 2}); return true;
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
            if (c.zone == Zone::Analysis && arcana::spell(catalog_.at(c.definition))) ++counts[c.owner];
        if (counts[0] == counts[1]) { finish(); return true; }
        chooser = counts[0] > counts[1] ? 0 : 1;
        for (const auto &[id, c] : s.cards)
            if (c.owner == chooser && c.zone == Zone::Analysis && arcana::spell(catalog_.at(c.definition))) options.push_back(id);
    } else if (effect.kind == EffectKind::ArcanaLovers) {
        chooser = 1 - owner;
        for (const auto &[id, c] : s.cards)
            if (c.owner == chooser && c.zone == Zone::Analysis && arcana::spell(catalog_.at(c.definition))) options.push_back(id);
    } else if (effect.kind == EffectKind::ArcanaTower) {
        optional = true;
        for (const auto &[id, c] : s.cards)
            if (c.owner != owner && c.zone == Zone::Analysis && catalog_.at(c.definition).type == CardType::Formation &&
                !immune(s, catalog_, id) && !sique::protectedCampfire(s, id) &&
                !catalog_.at(c.definition).opponentDestroyProtected &&
                std::find(catalog_.at(c.definition).traits.begin(), catalog_.at(c.definition).traits.end(),
                          "effect_indestructible") == catalog_.at(c.definition).traits.end()) options.push_back(id);
    } else {
        if (effect.kind == EffectKind::ArcanaWorld) {
            optional = true;
            if (f.clearRemaining < 0) f.clearRemaining = 0;
            if (f.clearRemaining >= 4) { shuffleArcanaDeck(s, owner); finish(); return true; }
        }
        std::set<std::string> selectedNames;
        if (effect.kind == EffectKind::ArcanaMagician)
            for (auto id : f.item.targetCards) selectedNames.insert(catalog_.at(s.cards.at(id).definition).name);
        for (auto id : s.players[owner].deck) {
            const auto &d = catalog_.at(s.cards.at(id).definition);
            bool eligible = effect.kind == EffectKind::ArcanaPriestess ? d.type == CardType::Seal :
                effect.kind == EffectKind::ArcanaEmperor ? !f.item.targetCards.empty() && arcana::spell(d) &&
                    d.rank == catalog_.at(s.cards.at(f.item.targetCards.front()).definition).rank + 3 :
                effect.kind == EffectKind::ArcanaMagician ? !selectedNames.count(d.name) :
                effect.kind == EffectKind::ArcanaWorld ?
                    (f.clearRemaining == 0 ? d.type == CardType::Analytic :
                     f.clearRemaining == 1 ? d.type == CardType::Word && d.id != "arcana-world" :
                     f.clearRemaining == 2 ? d.type == CardType::Talisman : d.type == CardType::Seal) : false;
            if (eligible) options.push_back(id);
        }
    }
    if (options.empty()) {
        if (effect.kind == EffectKind::ArcanaWorld) { ++f.clearRemaining; return true; }
        if (effect.kind == EffectKind::ArcanaMagician && !f.item.targetCards.empty()) {
            const auto chosen = detail::bounded(s, static_cast<std::uint32_t>(f.item.targetCards.size()));
            const auto cards = f.item.targetCards;
            for (std::size_t i = 0; i < cards.size(); ++i)
                if (i == chosen) deckToHand(s, owner, cards[i]); else arcana::erase(s, catalog_, cards[i]);
        }
        finish(); return true;
    }
    decision(chooser, DecisionKind::EffectChoice, std::move(options), optional);
    return true;
}
bool GameEngine::chooseArcanaEffect(PlayerId actor, CardId selected) {
    auto &s = state_;
    if (!catalog_.usesAlphaV252() || !s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    if (!arcanaKind(e.kind)) return false;
    const auto owner = f.item.owner;
    const auto finish = [&] { ++f.cursor; f.clearRemaining = -1; f.resolvedCount = 0; };
    if (e.kind == EffectKind::ArcanaDeath) {
        auto damage = Effect{EffectKind::Damage, e.amount, selected == 1 ? DamageType::Necrotic : DamageType::Radiant};
        EffectResolver::apply(s, catalog_, f.item, damage);
        if (s.effect) finish();
        return true;
    }
    if (e.kind == EffectKind::ArcanaMagician) {
        f.item.targetCards.push_back(selected);
        s.cards.at(selected).publiclyRevealed = true;
        s.events.push_back({"reveal", "魔术师：展示候选卡牌", -1, selected});
        if (++f.resolvedCount < 3) return true;
        const auto chosen = detail::bounded(s, static_cast<std::uint32_t>(f.item.targetCards.size()));
        const auto cards = f.item.targetCards;
        for (std::size_t i = 0; i < cards.size(); ++i)
            if (i == chosen) deckToHand(s, owner, cards[i]); else arcana::erase(s, catalog_, cards[i]);
        shuffleArcanaDeck(s, owner); finish(); return true;
    }
    if (e.kind == EffectKind::ArcanaPriestess || e.kind == EffectKind::ArcanaEmperor) {
        deckToHand(s, owner, selected); shuffleArcanaDeck(s, owner); finish(); return true;
    }
    if (e.kind == EffectKind::ArcanaWorld) {
        if (selected) deckToHand(s, owner, selected);
        ++f.clearRemaining; return true;
    }
    if (e.kind == EffectKind::ArcanaTower) {
        finish();
        if (selected) {
            f.item.targetCard = selected;
            f.item.targetCards = {selected};
            f.item.effects.insert(f.item.effects.begin() + static_cast<std::ptrdiff_t>(f.cursor),
                                 {EffectKind::Destroy, 0});
        }
        return true;
    }
    if (e.kind == EffectKind::ArcanaJustice || e.kind == EffectKind::ArcanaLovers) {
        if (selected) StateMaintenance::leave(s, selected, &catalog_);
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
                link->item.targetPlayer = static_cast<PlayerId>(selected - playerTarget(0));
            s.events.push_back({"retarget", "恶魔：改写对方法术目标", -1, selected});
            EffectResolver::apply(s, catalog_, f.item, {EffectKind::AddIndependent, 6, DamageType::Force, EffectRecipient::Owner});
        }
        if (s.effect) finish();
        return true;
    }
    (void)actor;
    return false;
}
} // namespace wizard
