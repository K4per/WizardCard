// Included inside Client. Effects consume filtered views and never register input hit areas.
void updatePresentation(float seconds) {
    if (app_.paused() || scene_ == Scene::Handoff)
        return;
    presentation_.update(seconds);
    visualClock_ += seconds;
}
static sf::Color faded(sf::Color color, float opacity) {
    color.a = static_cast<std::uint8_t>(255 * std::clamp(opacity, 0.f, 1.f));
    return color;
}
static sf::Vector2f center(Rect r) {
    return r.position + r.size / 2.f;
}
static sf::Vector2f curve(sf::Vector2f a, sf::Vector2f b, float t, float lift = 55) {
    const float eased = t * t * (3 - 2 * t);
    return a + (b - a) * eased + sf::Vector2f{0, -std::sin(t * 3.14159265f) * lift};
}
Rect playerHud(PlayerId p) const {
    return p == viewer_ ? Rect{{25, 642}, {215, 190}} : Rect{{1360, 127}, {215, 190}};
}
sf::Vector2f zoneAnchor(Zone zone, PlayerId p, CardId host = 0) const {
    const bool enemy = p != viewer_;
    if (zone == Zone::Hand)
        return enemy ? sf::Vector2f{1100, 92} : center(handArea);
    if (zone == Zone::Deck)
        return enemy ? sf::Vector2f{157, 201} : sf::Vector2f{1438, 762};
    if (zone == Zone::Attached || (zone == Zone::Analysis && host))
        for (const auto &h : cardHits_)
            if (h.second == host)
                return center(h.first) + (zone == Zone::Attached ? sf::Vector2f{0, 0}
                                                                 : sf::Vector2f{enemy ? -165.f : 165.f, 0});
    if (zone == Zone::Analysis || zone == Zone::Attached)
        return center(side(analysisArea, enemy));
    if (zone == Zone::Casting)
        return center(side(castingArea, enemy));
    if (zone == Zone::Words)
        return center(side(wordsArea, enemy));
    if (zone == Zone::Action)
        return center(side(actionZone, enemy));
    if (zone == Zone::Ash)
        return center(side(ashArea, enemy));
    return {800, 480};
}
void captureMotionOrigins(const GameView &previous) {
    std::set<std::uint64_t> alive;
    for (const auto &cue : presentation_.cues()) {
        alive.insert(cue.id);
        if (motionOrigins_.count(cue.id))
            continue;
        Rect origin{zoneAnchor(cue.from, cue.player, cue.host) - sf::Vector2f{48, 56}, {96, 112}};
        for (const auto &h : cardHits_)
            if (cue.source && h.second == cue.source) {
                bool known = false;
                for (const auto &c : previous.cards)
                    if (c.instance.id == cue.source)
                        known = true;
                if (known) {
                    origin = h.first;
                    break;
                }
            }
        motionOrigins_[cue.id] = origin;
    }
    for (auto i = motionOrigins_.begin(); i != motionOrigins_.end();)
        if (!alive.count(i->first))
            i = motionOrigins_.erase(i);
        else
            ++i;
}
sf::Vector2f cueDestination(const ui::MatchCue &cue) const {
    for (const auto &h : cardHits_)
        if (cue.source && h.second == cue.source)
            return center(h.first);
    return zoneAnchor(cue.to, cue.player, cue.host);
}
void ring(sf::RenderTarget &t, sf::Vector2f position, float radius, sf::Color color, float thickness = 2) {
    sf::CircleShape circle(radius, 48);
    circle.setOrigin({radius, radius});
    circle.setPosition(position);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineColor(color);
    circle.setOutlineThickness(thickness);
    t.draw(circle);
}
void sparks(sf::RenderTarget &t, sf::Vector2f position, float progress, sf::Color color, int count = 10) {
    for (int i = 0; i < count; ++i) {
        const float angle = i * 6.2831853f / count;
        const float distance = 12 + progress * (28 + i % 3 * 12);
        const auto p = position + sf::Vector2f{std::cos(angle), std::sin(angle)} * distance;
        box(t, {p, {3, 3}}, faded(color, (1 - progress) * .8f));
    }
}
void drawMatchGuide(sf::RenderTarget &t) {
    for (int n = 0; n < 5; ++n) {
        const bool current = static_cast<int>(view_.phase) == n;
        Rect r{{320 + n * 78.f, 73}, {72, 31}};
        box(t, r, current ? sf::Color{27, 69, 72} : panel, current ? mint : sf::Color{51, 70, 79});
        label(t, std::to_string(n + 1) + " " + phaseName(static_cast<Phase>(n)), r.position.x + 7,
              r.position.y + 7, 12, current ? ink : muted);
        if (current)
            box(t, {{r.position.x + 6, r.position.y + 28}, {60, 2}}, mint);
    }
    const bool own = view_.active == viewer_;
    label(t, own ? "本方回合" : "对手回合", 721, 73, 13, own ? mint : gold);
    std::string priority = view_.chain ? (view_.chain->mode == ChainMode::Resolving ? "连锁逆序结算"
                                          : view_.chain->priority == viewer_ ? "你拥有响应优先权"
                                                                             : "等待对手响应")
                           : view_.decision
                               ? (view_.decision->player == viewer_ ? "等待你的选择" : "等待对手选择")
                           : own ? "可进行当前阶段操作"
                                 : "等待对手行动";
    label(t, priority, 721, 93, 11, muted);
    button(t, motionButton_, app_.settings().reducedMotion ? "动画：精简" : "动画：完整");
}
void drawCardMotionState(sf::RenderTarget &t, const CardView &c, Rect r) {
    if (c.instance.spell == SpellState::Analyzing && r.size.y > 33) {
        const float progress = std::clamp(
            1.f - c.turnsToReady / static_cast<float>(std::max(1, c.definition.analysisTurns)), 0.f, 1.f);
        const float width = std::max(10.f, r.size.x - 40);
        const sf::Vector2f p{r.position.x + 34, r.position.y + r.size.y - 4};
        box(t, {p, {width, 2}}, {35, 52, 64});
        box(t, {p, {width * progress, 2}}, mint);
    }
    if (app_.settings().reducedMotion)
        return;
    if (c.instance.spell == SpellState::Ready &&
        c.instance.canceledTurn != view_.players[c.instance.owner].ownTurn) {
        const float opacity = .25f + .12f * std::sin(visualClock_ * 3);
        box(t, {r.position - sf::Vector2f{2, 2}, r.size + sf::Vector2f{4, 4}}, sf::Color::Transparent,
            faded(mint, opacity));
    }
}
void flyingCard(sf::RenderTarget &t, const ui::MatchCue &cue, sf::Vector2f position, float p) {
    const float opacity = std::min(1.f, (1 - p) * 5);
    const sf::Vector2f size{92, 108};
    const Rect r{position - size / 2.f, size};
    box(t, {r.position + sf::Vector2f{5, 7}, size}, faded({0, 0, 0}, opacity * .55f));
    if (!cue.card) {
        paint(t, "card_back_opponent", r, static_cast<std::uint8_t>(opacity * 255));
        return;
    }
    paint(t, "frame_" + typeKey(cue.card->definition.type) + "_hand", r,
          static_cast<std::uint8_t>(opacity * 255));
    illustration(t, *cue.card, {r.position + sf::Vector2f{6, 23}, {80, 61}},
                 static_cast<std::uint8_t>(opacity * 255));
    label(t, cue.card->definition.name, r.position.x + 8, r.position.y + 5, 11, faded(ink, opacity));
    label(t, kindName(cue.card->definition.type), r.position.x + 8, r.position.y + 88, 10,
          faded(muted, opacity));
}
void drawMatchEffects(sf::RenderTarget &t) {
    if (scene_ == Scene::Handoff)
        return;
    const bool reduced = app_.settings().reducedMotion;
    for (const auto &cue : presentation_.cues()) {
        if (!cue.visible())
            continue;
        const float p = cue.progress(), fade = std::min(1.f, (1 - p) * 4);
        const auto origin = motionOrigins_.find(cue.id);
        const auto start = origin == motionOrigins_.end() ? zoneAnchor(cue.from, cue.player, cue.host)
                                                          : center(origin->second);
        if (cue.kind == ui::CueKind::Move || cue.kind == ui::CueKind::Draw) {
            const auto end = cueDestination(cue);
            auto route = [&](float progress) {
                if (cue.used && cue.from == Zone::Hand && cue.to == Zone::Ash && cue.card) {
                    const auto middle = zoneAnchor(
                        cue.card->definition.type == CardType::Word ? Zone::Words : Zone::Action, cue.player);
                    return progress < .55f ? curve(start, middle, progress / .55f)
                                           : curve(middle, end, (progress - .55f) / .45f, 18);
                }
                return curve(start, end, progress);
            };
            for (int i = 1; i <= 6; ++i) {
                const auto tail = route(std::max(0.f, p - i * .035f));
                box(t, {tail - sf::Vector2f{2, 2}, {4, 4}}, faded(mint, (7 - i) * .055f * fade));
            }
            flyingCard(t, cue, route(p), p);
            ring(t, end, 8 + p * 17, faded(mint, .35f * fade), 1);
            continue;
        }
        if (cue.kind == ui::CueKind::Phase) {
            const float enter = std::min(1.f, p * 5);
            Rect r{{630, 438 - (reduced ? 0 : (1 - enter) * 12)}, {340, 42}};
            box(t, r, faded(panel, fade * .96f), faded(gold, fade));
            label(t, (cue.player == viewer_ ? "本方 · " : "对手 · ") + cue.text, r.position.x + 51,
                  r.position.y + 9, 18, faded(mint, fade));
            continue;
        }
        if (cue.kind == ui::CueKind::Damage || cue.kind == ui::CueKind::Heal ||
            cue.kind == ui::CueKind::Mana || cue.kind == ui::CueKind::Load) {
            const Rect hud = playerHud(cue.player);
            const bool damage = cue.kind == ui::CueKind::Damage;
            const bool life = damage || cue.kind == ui::CueKind::Heal;
            const auto color = damage                          ? sf::Color{255, 133, 114}
                               : cue.kind == ui::CueKind::Load ? gold
                                                               : mint;
            const auto impact = hud.position + sf::Vector2f{106, life                            ? 76.f
                                                                 : cue.kind == ui::CueKind::Mana ? 114.f
                                                                                                 : 146.f};
            if (damage && !reduced) {
                const float travel = std::min(1.f, p / .3f);
                const auto comet = curve(start, impact, travel, 75);
                if (p < .3f) {
                    for (int i = 0; i < 7; ++i) {
                        const auto point = curve(start, impact, std::max(0.f, travel - i * .04f), 75);
                        box(t, {point, {static_cast<float>(8 - i), static_cast<float>(8 - i)}},
                            faded(color, .8f - i * .09f));
                    }
                    ring(t, comet, 9, color);
                } else {
                    const float burst = (p - .3f) / .7f;
                    sparks(t, impact, burst, color, 14);
                    ring(t, impact, 18 + burst * 48, faded(color, (1 - burst) * .6f));
                    box(t, hud, faded(color, (1 - burst) * .13f), faded(color, (1 - burst) * .8f));
                }
            }
            if (!damage || reduced || p >= .3f) {
                const float rise = reduced ? 0 : p * (life ? 30 : 20);
                const std::string number = (cue.amount > 0 ? "+" : "") + std::to_string(cue.amount);
                label(t, number + (life ? "" : " " + cue.text), hud.position.x + (life ? 121 : 105),
                      impact.y - 21 - rise, life ? 30 : 13, faded(color, fade));
            }
            continue;
        }
        const auto position = cue.kind == ui::CueKind::Release || cue.kind == ui::CueKind::Response ? start
                              : cue.source ? cueDestination(cue)
                                           : start;
        const bool canceled = cue.kind == ui::CueKind::Cancel;
        const auto color = canceled                            ? sf::Color{237, 138, 120}
                           : cue.kind == ui::CueKind::Response ? sf::Color{175, 147, 224}
                                                               : mint;
        if (cue.kind == ui::CueKind::Response && !view_.chain && cue.card) {
            Rect summary{{25, 320}, {215, 135}};
            box(t, summary, faded(panel, .97f * fade), faded(color, fade));
            label(t, "连锁 · 响应已结算", 37, 332, 15, faded(color, fade));
            label(t, cue.card->definition.name, 37, 364, 14, faded(ink, fade));
            label(t, "↓ 响应目标", 37, 391, 12, faded(muted, fade));
            const auto *target = find(cue.target);
            label(t, target ? target->definition.name : "公开链节", 37, 417, 14, faded(mint, fade));
        }
        if (!reduced) {
            if (cue.kind == ui::CueKind::Response && cue.target) {
                for (const auto &hit : cardHits_)
                    if (hit.second == cue.target) {
                        const auto destination = center(hit.first);
                        sf::Vertex trace[2];
                        trace[0].position = position;
                        trace[1].position = destination;
                        trace[0].color = trace[1].color = faded(color, .45f * fade);
                        t.draw(trace, 2, sf::PrimitiveType::Lines);
                        ring(t, curve(position, destination, std::min(1.f, p * 2)), 7, faded(color, fade));
                        break;
                    }
            }
            ring(t, position, 18 + p * 37, faded(color, .75f * fade));
            sparks(t, position, p, color);
            if (canceled) {
                sf::Vertex cross[4];
                cross[0].position = position + sf::Vector2f{-23, -23};
                cross[1].position = position + sf::Vector2f{23, 23};
                cross[2].position = position + sf::Vector2f{-23, 23};
                cross[3].position = position + sf::Vector2f{23, -23};
                for (auto &v : cross)
                    v.color = faded(color, fade);
                t.draw(cross, 4, sf::PrimitiveType::Lines);
            }
        }
        const auto notice = sf::Vector2f{std::clamp(position.x - 61, 262.f, 1210.f),
                                         std::clamp(position.y - 39 - (reduced ? 0 : p * 14), 135.f, 830.f)};
        box(t, {notice, {122, 26}}, faded(panel, .95f * fade), faded(color, fade));
        label(t, cue.text, notice.x + 10, notice.y + 5, 13, faded(color, fade));
    }
}
