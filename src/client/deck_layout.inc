// Full-viewport deck pages. Input hit regions match the card thumbnails drawn below.
const Rect deckNew_{{32, 108}, {170, 44}}, deckImportChoice_{{222, 108}, {400, 44}},
    deckImport_{{642, 108}, {200, 44}}, deckListBack_{{1350, 950}, {230, 38}};
const Rect deckName_{{400, 22}, {640, 44}}, deckBase_{{400, 73}, {640, 38}},
    deckSearch_{{1160, 158}, {418, 38}}, deckClear_{{1490, 117}, {88, 30}};
const Rect deckSave_{{20, 950}, {220, 38}}, deckEditorBack_{{1350, 950}, {230, 38}},
    deckIssues_{{380, 950}, {740, 38}};
const Rect deckPoolPrev_{{1160, 887}, {105, 40}}, deckPoolNext_{{1470, 887}, {105, 40}},
    deckCardsPrev_{{885, 125}, {105, 38}}, deckCardsNext_{{1005, 125}, {105, 38}};
const Rect deckListPrev_{{32, 874}, {170, 42}}, deckListNext_{{1400, 874}, {170, 42}};
const Rect deckDiscardSave_{{425, 575}, {230, 48}}, deckDiscardNo_{{680, 575}, {230, 48}},
    deckDiscardCancel_{{935, 575}, {230, 48}};
const Rect deckDrop_{{368, 175}, {760, 755}}, poolDrop_{{1150, 358}, {440, 525}};
static Rect deckListRow(int n) {
    return {{32, 200 + n * 107.f}, {1340, 86}};
}
static Rect deckDeleteRow(int n) {
    return {{1400, 200 + n * 107.f}, {170, 86}};
}
static Rect poolRow(int n) {
    return {{1160 + (n % 3) * 140.f, 370 + (n / 3) * 166.f}, {130, 154}};
}
static Rect builtRow(int n) {
    return {{370 + (n % 6) * 127.f, 180 + (n / 6) * 151.f}, {118, 140}};
}
static Rect filterButton(int n) {
    return {{1160 + (n % 2) * 212.f, 208 + (n / 2) * 48.f}, {200, 40}};
}
struct DeckHit {
    Rect rect;
    std::string id;
    bool pool;
};
std::vector<DeckHit> deckHits_;
std::optional<DeckHit> deckPressed_;
bool deckDragging_{};
bool isDeckPage() const {
    const auto p = app_.page();
    return p == app::Page::Decks || p == app::Page::DeckEditor || p == app::Page::ConfirmDeckDelete ||
           p == app::Page::ConfirmDeckDiscard;
}
void resetDeckPointer() {
    deckPressed_.reset();
    deckDragging_ = false;
}
std::optional<DeckHit> deckHitAt(sf::Vector2f p) const {
    if (app_.page() != app::Page::DeckEditor || showDeckProblems_)
        return {};
    for (const auto &h : deckHits_)
        if (hit(p, h.rect))
            return h;
    return {};
}
void inspectDeckCard(const std::string &id) {
    if (inspectedDefinition_ != id) {
        inspectedDefinition_ = id;
        detailOffset_ = 0;
        cue(ui::SoundId::Select);
    }
}
void pressDeck(sf::Vector2f p) {
    deckPressed_ = deckHitAt(p);
    if (deckPressed_) {
        deckFocus_ = DeckFocus::None;
        inspectDeckCard(deckPressed_->id);
    }
}
void moveDeckPointer(sf::Vector2f p) {
    const auto delta = p - pressPoint_;
    if (mouseDown_ && deckPressed_ && delta.x * delta.x + delta.y * delta.y >= 64)
        deckDragging_ = true;
}
void rightClick(sf::Vector2f p) {
    const bool cancelGesture = mouseDown_;
    if (cancelGesture || interaction_.selected() || interaction_.pending())
        cue(ui::SoundId::Cancel);
    resetPointer();
    pointer_ = p;
    if (app_.page() == app::Page::DeckEditor) {
        if (cancelGesture)
            return;
        if (const auto h = deckHitAt(p)) {
            deckFocus_ = DeckFocus::None;
            adjustDeck(h->id, h->pool ? 1 : -1);
        }
    } else
        interaction_.cancel();
}
void releaseDeck(sf::Vector2f p) {
    moveDeckPointer(p);
    const auto source = deckPressed_;
    const bool dragged = deckDragging_;
    const auto delta = p - pressPoint_;
    resetPointer();
    if (!dragged) {
        if (delta.x * delta.x + delta.y * delta.y < 64)
            click(p);
        return;
    }
    if (source && hit(p, source->pool ? deckDrop_ : poolDrop_))
        adjustDeck(source->id, source->pool ? 1 : -1);
    else if (source)
        cue(ui::SoundId::Reject);
}
void deckCanvas(sf::RenderTarget &t) {
    // Fill physical margins as well as the logical canvas, including widescreen letterboxing.
    const auto view = t.getView();
    t.setView(t.getDefaultView());
    box(t, {{0, 0}, sf::Vector2f(t.getSize())}, {10, 19, 28});
    t.setView(view);
    box(t, {{0, 0}, {1600, 112}}, {17, 29, 39});
    box(t, {{0, 943}, {1600, 57}}, {17, 29, 39});
    box(t, {{0, 111}, {1600, 1}}, gold);
    box(t, {{0, 943}, {1600, 1}}, gold);
}
void deckThumbnail(sf::RenderTarget &t, const std::string &id, Rect r, bool pool, bool ghost = false) {
    const auto it = content_.catalog.cards.find(id);
    if (it == content_.catalog.cards.end()) {
        box(t, r, panel, gold);
        wrapped(t, resources_, "失效卡牌\n" + shortText(id, 20), r.position + sf::Vector2f{8, 14}, 12, gold,
                8);
        label(t, "右键移除", r.position.x + 8, r.position.y + r.size.y - 24, 11, muted);
        return;
    }
    const auto &d = it->second;
    paint(t, "frame_" + typeKey(d.type) + "_portrait", r);
    if (id == inspectedDefinition_ || ghost)
        box(t, r, sf::Color::Transparent, mint);
    const auto x = r.position.x, y = r.position.y;
    label(t, shortText(d.name, 9), x + 8, y + 5, 11, ink);
    CardView preview;
    preview.definition = d;
    illustration(t, preview, {{x + 7, y + 27}, {r.size.x - 14, r.size.y - 72}});
    label(t, kindName(d.type), x + 8, y + r.size.y - 41, 10, accent(d.type));
    std::string cost = "费用 " + std::to_string(d.cost);
    if (d.type == CardType::Analytic)
        cost += " / 施法 " + std::to_string(d.castCost);
    label(t, cost, x + 8, y + r.size.y - 24, 9, ink);
    if (app::hasSpellRank(d.type)) {
        box(t, {{x + r.size.x - 29, y + 29}, {22, 22}}, panel, gold);
        label(t, std::to_string(d.rank), x + r.size.x - 23, y + 31, 11, gold);
    }
    if (pool) {
        box(t, {{x + r.size.x - 39, y + r.size.y - 42}, {32, 16}}, panel);
        label(t, std::to_string(namedCopies(id)) + "/3", x + r.size.x - 35, y + r.size.y - 42, 10, mint);
        if (!canAdd(id))
            box(t, r, {10, 19, 28, 65});
    }
}
void deckDetails(sf::RenderTarget &t) {
    auto it = content_.catalog.cards.find(inspectedDefinition_);
    if (it == content_.catalog.cards.end()) {
        label(t, inspectedDefinition_.empty() ? "卡牌详情" : "失效卡牌", 22, 132, 24, gold);
        wrapped(t, resources_,
                inspectedDefinition_.empty()
                    ? "点击卡牌选中并保留详情。\n右键：卡池加入，卡组移除。\n拖动卡牌至另一栏增减一张。"
                    : "卡池已无此ID：" + inspectedDefinition_ + "。从主卡组移除可修复卡组。",
                {22, 202}, 17, muted, 18);
        return;
    }
    const auto &d = it->second;
    label(t, d.name, 22, 129, 23, gold);
    std::string category = kindName(d.type) + " · " + rarityName(d.rarity);
    if (app::hasSpellRank(d.type))
        category += " · 位阶 " + std::to_string(d.rank);
    label(t, category, 22, 165, 16, accent(d.type));
    CardView preview;
    preview.definition = d;
    illustration(t, preview, {{22, 201}, {300, 218}});
    label(t, app::primaryCostLabel(d.type) + " " + std::to_string(d.cost), 22, 435, 16, ink);
    if (d.type == CardType::Analytic)
        label(t, "施法费用 " + std::to_string(d.castCost) + " · 准备施法时支付", 22, 464, 14, mint);
    std::vector<std::string> lines;
    auto add = [&](const std::string &s) {
        auto u = utf8(s);
        for (std::size_t n = 0; n < u.getSize(); n += 20)
            lines.push_back(stringUtf8(u.substring(n, 20)));
        lines.emplace_back();
    };
    if (app::hasSpellRank(d.type)) {
        add(schoolName(d.school) + " · 咒文速度 " + std::to_string(d.speed));
        if (d.type == CardType::Analytic) add(d.analysisTurns ? "解析时间 " + std::to_string(d.analysisTurns) + " 回合" : "解析时间：立即");
    }
    if (d.type == CardType::Formation) {
        add("阵体 " + std::to_string(d.body) + " / 荷载上限 " + std::to_string(d.capacity));
        add("环位数 " + std::to_string(d.rings) + " / 魔素收入 " + std::to_string(d.income) +
            " / 承载位阶上限 " + std::to_string(d.maxRank));
        if (d.baseEligible) {
            add("可选作基础阵法（不计入30张主卡组）");
            add("开局容量：阵法 " + std::to_string(d.capacity) + " + 基础 " +
                std::to_string(content_.catalog.baseLoadCapacity) + " = " +
                std::to_string(d.capacity + content_.catalog.baseLoadCapacity));
        }
    }
    std::string tags = "标签：";
    for (const auto &tag : app::cardTags(d))
        tags += tag + " ";
    add(tags);
    add(d.text);
    if (!d.flavor.empty())
        add("背景说明：" + d.flavor);
    detailOffset_ = std::clamp(detailOffset_, 0, std::max(0, static_cast<int>(lines.size()) - 14));
    for (int n = 0; n < 14 && n + detailOffset_ < static_cast<int>(lines.size()); ++n)
        label(t, lines[n + detailOffset_], 22, 505 + n * 24.f, 15, ink);
    label(t, lines.size() > 14 ? "滚轮查看完整说明" : "点选详情持续保留", 22, 852, 12, muted);
    label(t, "右键：卡池加入 / 卡组移除", 22, 884, 14, mint);
    label(t, "拖到另一栏增减 · Esc 取消拖动", 22, 910, 13, muted);
}
void drawDecks(sf::RenderTarget &t) {
    deckCanvas(t);
    deckHits_.clear();
    const auto page = app_.page();
    if (page == app::Page::ConfirmDeckDelete || page == app::Page::ConfirmDeckDiscard) {
        frame(t, {{390, 310}, {820, 355}});
        label(t, page == app::Page::ConfirmDeckDelete ? "删除卡组？" : "尚有未保存的修改", 425, 345, 30,
              gold);
        const auto *d = app_.decks().find(deleteDeckId_);
        wrapped(t, resources_,
                page == app::Page::ConfirmDeckDelete
                    ? "确认删除“" + (d ? d->name : deleteDeckId_) + "”？此操作会移除本地卡组。"
                    : "保存草稿后离开，或放弃本次修改；保存失败时编辑内容仍保留。",
                {425, 413}, 18, ink, 38);
        if (page == app::Page::ConfirmDeckDelete) {
            button(t, confirmNo_, "取消");
            button(t, confirmYes_, "确认删除", true);
        } else {
            button(t, deckDiscardSave_, "保存并离开", true);
            button(t, deckDiscardNo_, "放弃修改");
            button(t, deckDiscardCancel_, "继续编辑");
        }
        if (!status_.empty())
            wrapped(t, resources_, status_, {425, 688}, 14, muted, 48);
        return;
    }
    if (page == app::Page::Decks) {
        label(t, "卡组构筑", 32, 25, 32, gold);
        label(t, "管理本地卡组 · 草稿可保存，合法卡组可用于对战", 32, 73, 16, muted);
        button(t, deckNew_, "新建卡组", true);
        button(t, deckImportChoice_, "预设：" + app_.presets().at(importPreset_).name + "  ›");
        button(t, deckImport_, "从预设复制");
        const auto &drafts = app_.decks().drafts();
        deckListOffset_ = std::min(deckListOffset_, drafts.empty() ? 0 : ((drafts.size() - 1) / 6) * 6);
        for (int n = 0; n < 6 && deckListOffset_ + n < drafts.size(); ++n) {
            const auto &d = drafts[deckListOffset_ + n];
            const auto issues = app_.decks().problems(d);
            const auto r = deckListRow(n);
            box(t, r, hit(pointer_, r) ? sf::Color{27, 51, 58} : panel, issues.empty() ? mint : gold);
            auto cover = content_.catalog.cards.find(d.baseFormation);
            if (cover != content_.catalog.cards.end()) {
                CardView c;
                c.definition = cover->second;
                illustration(t, c, {r.position + sf::Vector2f{8, 8}, {91, 70}});
            }
            label(t, shortText(d.name.empty() ? "未命名卡组" : d.name, 40), 146, r.position.y + 9, 23, ink);
            label(t,
                  std::to_string(d.size()) + " / 30 张 · " +
                      (issues.empty() ? "合法 · 点击编辑" : shortText(issues.front(), 48)),
                  146, r.position.y + 49, 16, issues.empty() ? mint : muted);
            button(t, deckDeleteRow(n), "删除卡组");
        }
        if (drafts.empty())
            wrapped(t, resources_, "还没有自建卡组。新建空白草稿，或复制预设开始构筑。", {32, 244}, 22, muted,
                    55);
        button(t, deckListPrev_, "上一页", false, deckListOffset_ > 0);
        button(t, deckListNext_, "下一页", false, deckListOffset_ + 6 < drafts.size());
        label(t, std::to_string(drafts.size()) + " 个自建卡组", 690, 880, 17, muted);
        button(t, deckListBack_, "返回主菜单");
        auto notice = status_.empty() ? app_.decks().notice() : status_;
        if (!notice.empty())
            label(t, shortText(notice, 80), 32, 957, 14, muted);
        return;
    }
    box(t, {{350, 112}, {1, 831}}, gold);
    box(t, {{1139, 112}, {1, 831}}, gold);
    label(t, "编辑卡组", 22, 27, 30, gold);
    label(t, deckDirty() ? "尚未保存" : "已保存", 24, 76, 15, deckDirty() ? muted : mint);
    box(t, deckName_, panel, deckFocus_ == DeckFocus::Name ? mint : gold);
    label(t, shortText(editingDeck_.name, 34) + (deckFocus_ == DeckFocus::Name ? "|" : ""), 412, 32, 18, ink);
    const auto base = content_.catalog.cards.find(editingDeck_.baseFormation);
    const auto capacity = base == content_.catalog.cards.end() ? std::string{} :
        " · 容量 " + std::to_string(base->second.capacity) + "+" +
        std::to_string(content_.catalog.baseLoadCapacity) + "=" +
        std::to_string(base->second.capacity + content_.catalog.baseLoadCapacity);
    button(t, deckBase_,
           "基础阵法：" +
               (base == content_.catalog.cards.end() ? editingDeck_.baseFormation : base->second.name) +
               capacity + "  ›");
    poolRows_ = app::queryCards(content_.catalog, deckQuery_);
    builtRows_.clear();
    for (const auto &[id, count] : editingDeck_.cards)
        builtRows_.insert(builtRows_.end(), std::max(0, count), id);
    poolOffset_ = std::min(poolOffset_, poolRows_.empty() ? 0 : ((poolRows_.size() - 1) / 9) * 9);
    builtOffset_ = std::min(builtOffset_, builtRows_.empty() ? 0 : ((builtRows_.size() - 1) / 30) * 30);
    label(t, "主卡组 · " + std::to_string(editingDeck_.size()) + " / 30 张", 370, 128, 24, mint);
    if (builtRows_.size() > 30) {
        button(t, deckCardsPrev_, "上一页", false, builtOffset_ > 0);
        button(t, deckCardsNext_, "下一页", false, builtOffset_ + 30 < builtRows_.size());
    }
    for (int n = 0; n < 30; ++n) {
        const auto r = builtRow(n);
        if (builtOffset_ + n < builtRows_.size()) {
            const auto &id = builtRows_[builtOffset_ + n];
            deckThumbnail(t, id, r, false);
            deckHits_.push_back({r, id, false});
        } else {
            box(t, r, {14, 24, 34}, {32, 48, 58});
            label(t, std::to_string(builtOffset_ + n + 1), r.position.x + 50, r.position.y + 58, 15,
                  {57, 75, 84});
        }
    }
    label(t, "卡池 · " + std::to_string(poolRows_.size()) + " 种", 1160, 119, 22, mint);
    button(t, deckClear_, "重置");
    box(t, deckSearch_, panel, deckFocus_ == DeckFocus::Search ? mint : gold);
    label(t,
          searchText_.isEmpty() && deckFocus_ != DeckFocus::Search
              ? "搜索卡名（部分匹配）"
              : shortText(stringUtf8(searchText_), 22) + (deckFocus_ == DeckFocus::Search ? "|" : ""),
          1170, 167, 16, searchText_.isEmpty() ? muted : ink);
    const std::array<std::string, 6> filters{
        "类型：" + (deckQuery_.type ? kindName(*deckQuery_.type) : "全部"),
        "稀有度：" + (deckQuery_.rarity.empty() ? "全部" : rarityName(deckQuery_.rarity)),
        "费用：" + (deckQuery_.cost ? std::to_string(*deckQuery_.cost) : "全部"),
        "施法费用：" + (deckQuery_.castCost ? std::to_string(*deckQuery_.castCost) : "全部"),
        "位阶：" + (deckQuery_.rank ? std::to_string(*deckQuery_.rank) : "全部"),
        "标签：" + (deckQuery_.tags.empty() ? "全部" : shortText(deckQuery_.tags.front(), 5))};
    for (int n = 0; n < 6; ++n)
        button(t, filterButton(n), filters[n]);
    label(t, "右键加入 · 拖入主卡组", 1160, 350, 13, muted);
    for (int n = 0; n < 9 && poolOffset_ + n < poolRows_.size(); ++n) {
        const auto &id = poolRows_[poolOffset_ + n];
        const auto r = poolRow(n);
        deckThumbnail(t, id, r, true);
        deckHits_.push_back({r, id, true});
    }
    if (poolRows_.empty())
        wrapped(t, resources_, "无匹配卡牌。\n请调整搜索或筛选。", {1170, 411}, 18, muted, 19);
    button(t, deckPoolPrev_, "上一页", false, poolOffset_ > 0);
    button(t, deckPoolNext_, "下一页", false, poolOffset_ + 9 < poolRows_.size());
    label(t,
          std::to_string(poolOffset_ / 9 + 1) + " / " +
              std::to_string(std::max<std::size_t>(1, (poolRows_.size() + 8) / 9)),
          1300, 896, 16, muted);
    const auto issues = app_.decks().problems(editingDeck_);
    label(t, issues.empty() ? "合法卡组，可用于对战" : shortText(issues.front(), 38) + "（点击查看）", 390,
          958, 16, issues.empty() ? mint : gold);
    button(t, deckSave_, deckDirty() ? "保存卡组 / 草稿 *" : "已保存 · 再次保存", true,
           app_.decks().writable());
    button(t, deckEditorBack_, "返回卡组列表");
    deckDetails(t);
    if (showDeckProblems_) {
        frame(t, {{410, 300}, {800, 490}});
        label(t, "卡组校验 · " + std::to_string(issues.size()) + " 项问题", 440, 330, 26, gold);
        deckProblemOffset_ =
            std::clamp(deckProblemOffset_, 0, std::max(0, static_cast<int>(issues.size()) - 7));
        for (int n = 0; n < 7 && n + deckProblemOffset_ < static_cast<int>(issues.size()); ++n)
            label(t, shortText(issues[n + deckProblemOffset_], 38), 440, 395 + n * 39.f, 18, ink);
        label(t, issues.empty() ? "当前卡组合法" : "滚轮查看更多问题", 440, 719, 15, muted);
        button(t, {{1000, 717}, {180, 46}}, "返回编辑");
    }
    if (!status_.empty())
        label(t, shortText(status_, 108), 22, 986, 10, muted);
    if (deckDragging_ && deckPressed_) {
        const auto &source = *deckPressed_;
        const auto target = source.pool ? deckDrop_ : poolDrop_;
        const bool valid = hit(pointer_, target) && (!source.pool || canAdd(source.id));
        box(t, target, {40, 150, 130, 22}, mint);
        Rect ghost{pointer_ + sf::Vector2f{14, 14}, {118, 140}};
        ghost.position.x = std::clamp(ghost.position.x, 4.f, 1478.f);
        ghost.position.y = std::clamp(ghost.position.y, 4.f, 856.f);
        deckThumbnail(t, source.id, ghost, false, true);
        box(t, {{ghost.position.x, ghost.position.y + 141}, {118, 24}}, panel, valid ? mint : gold);
        label(t, valid ? (source.pool ? "加入一张" : "移除一张") : "松开返回", ghost.position.x + 9,
              ghost.position.y + 144, 12, valid ? mint : muted);
    }
}
