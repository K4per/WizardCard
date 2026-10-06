// Deck editor presentation. Definitions are read-only; edits affect the player's ID/count list only.
std::array<std::string, 2> selectedDeckIds_{"default", "default"};
app::DeckDraft editingDeck_;
Json savedDeck_;
app::CardQuery deckQuery_;
sf::String searchText_, deckNameText_;
enum class DeckFocus { None, Name, Search };
DeckFocus deckFocus_{DeckFocus::None};
bool deckSelectAll_{}, deckLeaveQuits_{}, logoUnavailable_{};
bool showDeckProblems_{};
int deckProblemOffset_{};
std::size_t importPreset_{}, deckListOffset_{}, poolOffset_{}, builtOffset_{};
int detailOffset_{};
std::string inspectedDefinition_, deleteDeckId_;
std::vector<std::string> poolRows_, builtRows_;

static std::string stringUtf8(const sf::String &s) {
    const auto bytes = s.toUtf8();
    return {bytes.begin(), bytes.end()};
}
std::string shortText(const std::string &s, std::size_t limit) const {
    auto u = utf8(s);
    return u.getSize() > limit ? stringUtf8(u.substring(0, limit)) + "…" : s;
}
const app::DeckPreset selectedChoice(int p) const {
    auto choices = app_.playableDecks();
    auto it = std::find_if(choices.begin(), choices.end(),
                           [&](const app::DeckPreset &d) { return d.id == selectedDeckIds_[p]; });
    return it == choices.end() ? choices.front() : *it;
}
void nextChoice(int p) {
    const auto choices = app_.playableDecks();
    auto it = std::find_if(choices.begin(), choices.end(),
                           [&](const app::DeckPreset &d) { return d.id == selectedDeckIds_[p]; });
    if (it == choices.end() || ++it == choices.end())
        it = choices.begin();
    selectedDeckIds_[p] = it->id;
}
bool drawTitleLogo(sf::RenderTarget &t) {
    if (logoUnavailable_)
        return false;
    try {
        const auto &texture = resources_.texture(assets_ / "art" / "branding" / "wizardcard-logo-v1.png");
        sf::Sprite logo(texture);
        const auto size = texture.getSize();
        const float scale = std::min(770.f / size.x, 280.f / size.y);
        logo.setScale({scale, scale});
        logo.setPosition({470 - size.x * scale / 2, 450 - size.y * scale / 2});
        t.draw(logo);
        return true;
    } catch (const std::exception &) {
        logoUnavailable_ = true;
        return false;
    }
}
bool deckDirty() const {
    return savedDeck_.is_null() || savedDeck_ != app::encodeDraft(editingDeck_);
}
void editDeck(app::DeckDraft d, bool saved) {
    editingDeck_ = std::move(d);
    showDeckProblems_ = false;
    deckProblemOffset_ = 0;
    savedDeck_ = saved ? app::encodeDraft(editingDeck_) : Json{};
    deckNameText_ = utf8(editingDeck_.name);
    searchText_.clear();
    deckQuery_ = {};
    poolOffset_ = builtOffset_ = 0;
    detailOffset_ = 0;
    inspectedDefinition_.clear();
    deckFocus_ = DeckFocus::None;
    deckLeaveQuits_ = false;
    status_ = app_.decks().writable() ? "" : app_.decks().notice();
    app_.navigate(app::Page::DeckEditor);
}
void importDeck() {
    const auto &preset = app_.presets().at(importPreset_);
    auto d = app_.decks().create(preset.name + "副本");
    d.baseFormation = preset.deck.baseFormation;
    for (const auto &id : preset.deck.cards)
        ++d.cards[id];
    editDeck(std::move(d), false);
}
bool saveDeck() {
    std::string error;
    if (!app_.decks().save(editingDeck_, error)) {
        status_ = error;
        cue(ui::SoundId::Reject);
        return false;
    }
    savedDeck_ = app::encodeDraft(editingDeck_);
    cue(ui::SoundId::Confirm);
    status_ = app_.decks().problems(editingDeck_).empty() ? "卡组已保存，可在开局配置中选用。"
                                                          : "草稿已保存；完成校验前不能进入对战。";
    return true;
}
void finishDeckLeave() {
    deckFocus_ = DeckFocus::None;
    app_.navigate(app::Page::Decks);
    if (deckLeaveQuits_)
        app_.requestLeave(true);
}
void requestDeckLeave(bool quit = false) {
    deckLeaveQuits_ = quit;
    deckFocus_ = DeckFocus::None;
    if (deckDirty())
        app_.navigate(app::Page::ConfirmDeckDiscard);
    else
        finishDeckLeave();
}
int namedCopies(const std::string &id) const {
    int result = 0;
    const auto it = content_.catalog.cards.find(id);
    if (it == content_.catalog.cards.end())
        return 0;
    for (const auto &[other, count] : editingDeck_.cards) {
        const auto definition = content_.catalog.cards.find(other);
        if (definition != content_.catalog.cards.end() && definition->second.name == it->second.name)
            result += count;
    }
    return result;
}
bool canAdd(const std::string &id) const {
    return content_.catalog.cards.count(id) && editingDeck_.size() < 30 && namedCopies(id) < 3;
}
void adjustDeck(const std::string &id, int delta) {
    if (delta > 0 && !canAdd(id)) {
        status_ = "主卡组最多30张，同名最多3张。";
        cue(ui::SoundId::Reject);
        return;
    }
    auto it = editingDeck_.cards.find(id);
    if (delta < 0) {
        if (it == editingDeck_.cards.end())
            return;
        if (--it->second == 0)
            editingDeck_.cards.erase(it);
    } else
        ++editingDeck_.cards[id];
    inspectedDefinition_ = id;
    detailOffset_ = 0;
    cue(ui::SoundId::Place);
    status_.clear();
}
void deckTextInput(char32_t code) {
    if (app_.page() != app::Page::DeckEditor || deckFocus_ == DeckFocus::None)
        return;
    if (code != 8 && (code < 32 || code == 127 || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)))
        return;
    auto &field = deckFocus_ == DeckFocus::Name ? deckNameText_ : searchText_;
    if (deckSelectAll_) {
        field.clear();
        deckSelectAll_ = false;
    }
    if (code == 8) {
        if (!field.isEmpty())
            field.erase(field.getSize() - 1);
    } else if (field.getSize() < (deckFocus_ == DeckFocus::Name ? 40u : 60u))
        field += sf::String(code);
    if (deckFocus_ == DeckFocus::Name)
        editingDeck_.name = stringUtf8(field);
    else {
        deckQuery_.name = stringUtf8(field);
        poolOffset_ = 0;
    }
}
bool deckKey(const sf::Event::KeyPressed &e) {
    if (app_.page() != app::Page::DeckEditor || deckFocus_ == DeckFocus::None)
        return false;
    if (e.control && e.code == sf::Keyboard::Key::A) {
        deckSelectAll_ = true;
        return true;
    }
    if (e.control && e.code == sf::Keyboard::Key::V) {
        for (auto code : sf::Clipboard::getString())
            deckTextInput(code);
        return true;
    }
    if (e.code == sf::Keyboard::Key::Enter || e.code == sf::Keyboard::Key::Tab) {
        deckFocus_ = DeckFocus::None;
        return true;
    }
    return false;
}
template <class T> void cycleFilter(std::optional<T> &current, const std::vector<T> &values) {
    if (!current) {
        if (!values.empty())
            current = values.front();
        return;
    }
    auto it = std::find(values.begin(), values.end(), *current);
    if (it == values.end() || ++it == values.end())
        current.reset();
    else
        current = *it;
}
void changeFilter(int n) {
    if (n == 0)
        cycleFilter(deckQuery_.type,
                    std::vector<CardType>{CardType::Action, CardType::Analytic, CardType::Word,
                                          CardType::Formation, CardType::Seal});
    if (n == 1 || n == 5) {
        std::set<std::string> values;
        for (const auto &[id, d] : content_.catalog.cards) {
            (void)id;
            if (n == 1)
                values.insert(d.rarity);
            else {
                auto tags = app::cardTags(d);
                values.insert(tags.begin(), tags.end());
            }
        }
        auto current = n == 1 ? deckQuery_.rarity : (deckQuery_.tags.empty() ? "" : deckQuery_.tags.front());
        auto it = values.find(current);
        const std::string next = current.empty() ? (values.empty() ? "" : *values.begin())
                                                 : (it == values.end() || ++it == values.end() ? "" : *it);
        if (n == 1)
            deckQuery_.rarity = next;
        else
            deckQuery_.tags = next.empty() ? std::vector<std::string>{} : std::vector<std::string>{next};
    }
    if (n >= 2 && n <= 4) {
        std::set<int> values;
        for (const auto &[id, d] : content_.catalog.cards) {
            (void)id;
            if (n == 3 && d.type != CardType::Analytic)
                continue;
            if (n == 4 && !app::hasSpellRank(d.type))
                continue;
            values.insert(n == 2 ? d.cost : n == 3 ? d.castCost : d.rank);
        }
        auto &value = n == 2 ? deckQuery_.cost : n == 3 ? deckQuery_.castCost : deckQuery_.rank;
        cycleFilter(value, std::vector<int>{values.begin(), values.end()});
    }
    poolOffset_ = 0;
}
std::string rarityName(const std::string &rarity) const {
    return app::rarityLabel(rarity);
}
#include "deck_layout.inc"
void clickDecks(sf::Vector2f p) {
    const auto page = app_.page();
    if (page == app::Page::ConfirmDeckDelete) {
        if (hit(p, confirmNo_)) {
            app_.navigate(app::Page::Decks);
            status_.clear();
        }
        if (hit(p, confirmYes_)) {
            std::string error;
            if (app_.decks().erase(deleteDeckId_, error)) {
                app_.navigate(app::Page::Decks);
                status_ = "卡组已删除。";
            } else
                status_ = error;
        }
        return;
    }
    if (page == app::Page::ConfirmDeckDiscard) {
        if (hit(p, deckDiscardCancel_)) {
            app_.navigate(app::Page::DeckEditor);
            deckLeaveQuits_ = false;
        }
        if (hit(p, deckDiscardNo_))
            finishDeckLeave();
        if (hit(p, deckDiscardSave_) && saveDeck())
            finishDeckLeave();
        return;
    }
    if (page == app::Page::Decks) {
        if (hit(p, deckListBack_))
            app_.navigate(app::Page::Menu);
        if (hit(p, deckNew_))
            editDeck(app_.decks().create(), false);
        if (hit(p, deckImportChoice_))
            importPreset_ = (importPreset_ + 1) % app_.presets().size();
        if (hit(p, deckImport_))
            importDeck();
        if (hit(p, deckListPrev_) && deckListOffset_ >= 6)
            deckListOffset_ -= 6;
        if (hit(p, deckListNext_) && deckListOffset_ + 6 < app_.decks().drafts().size())
            deckListOffset_ += 6;
        for (int n = 0; n < 6 && deckListOffset_ + n < app_.decks().drafts().size(); ++n) {
            const auto d = app_.decks().drafts()[deckListOffset_ + n];
            if (hit(p, deckListRow(n))) {
                editDeck(d, true);
                return;
            }
            if (hit(p, deckDeleteRow(n))) {
                deleteDeckId_ = d.id;
                status_.clear();
                app_.navigate(app::Page::ConfirmDeckDelete);
                return;
            }
        }
        return;
    }
    if (page != app::Page::DeckEditor)
        return;
    if (showDeckProblems_) {
        if (hit(p, {{1000, 717}, {180, 46}}))
            showDeckProblems_ = false;
        return;
    }
    if (hit(p, deckIssues_)) {
        showDeckProblems_ = true;
        deckProblemOffset_ = 0;
        return;
    }
    deckFocus_ = hit(p, deckName_)     ? DeckFocus::Name
                 : hit(p, deckSearch_) ? DeckFocus::Search
                                       : DeckFocus::None;
    deckSelectAll_ = false;
    if (hit(p, deckBase_)) {
        auto ids = bases();
        auto it = std::find(ids.begin(), ids.end(), editingDeck_.baseFormation);
        editingDeck_.baseFormation = it == ids.end() || ++it == ids.end() ? ids.front() : *it;
    }
    if (hit(p, deckClear_)) {
        deckQuery_ = {};
        searchText_.clear();
        poolOffset_ = 0;
    }
    for (int n = 0; n < 6; ++n)
        if (hit(p, filterButton(n)))
            changeFilter(n);
    if (hit(p, deckSave_))
        saveDeck();
    if (hit(p, deckEditorBack_)) {
        requestDeckLeave();
        return;
    }
    if (hit(p, deckPoolPrev_) && poolOffset_ >= 9)
        poolOffset_ -= 9;
    if (hit(p, deckPoolNext_) && poolOffset_ + 9 < poolRows_.size())
        poolOffset_ += 9;
    if (builtRows_.size() > 30) {
        if (hit(p, deckCardsPrev_) && builtOffset_ >= 30)
            builtOffset_ -= 30;
        if (hit(p, deckCardsNext_) && builtOffset_ + 30 < builtRows_.size())
            builtOffset_ += 30;
    }
    if (const auto h = deckHitAt(p))
        inspectDeckCard(h->id);
}
void scrollDecks(sf::Vector2f p, int delta) {
    if (deckPressed_)
        return;
    if (app_.page() == app::Page::DeckEditor) {
        if (showDeckProblems_) {
            deckProblemOffset_ += delta;
            return;
        }
        if (hit(p, {{0, 112}, {350, 831}}))
            detailOffset_ += delta;
        if (hit(p, poolDrop_)) {
            if (delta > 0 && poolOffset_ + 9 < poolRows_.size())
                poolOffset_ += 9;
            if (delta < 0 && poolOffset_ >= 9)
                poolOffset_ -= 9;
        }
        if (hit(p, deckDrop_)) {
            if (delta > 0 && builtOffset_ + 30 < builtRows_.size())
                builtOffset_ += 30;
            if (delta < 0 && builtOffset_ >= 30)
                builtOffset_ -= 30;
        }
    }
}
void exerciseDecks(sf::RenderWindow &window) {
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
    auto center = [](Rect r) { return r.position + r.size / 2.f; };
    auto rightTap = [&](Rect r) {
        draw(window);
        rightClick(center(r));
        draw(window);
    };
    auto drag = [&](Rect from, Rect to) {
        draw(window);
        press(center(from));
        movePointer(center(to));
        draw(window);
        releasePointer(center(to));
        draw(window);
    };
    auto replaceText = [&](Rect r, const std::string &value) {
        tap(r);
        sf::Event::KeyPressed select{};
        select.code = sf::Keyboard::Key::A;
        select.control = true;
        deckKey(select);
        for (auto code : utf8(value))
            deckTextInput(code);
        draw(window);
    };
    auto rowFor = [&](const std::string &id) {
        draw(window);
        auto it = std::find(builtRows_.begin(), builtRows_.end(), id);
        require(it != builtRows_.end(), "built card absent");
        auto index = static_cast<std::size_t>(it - builtRows_.begin());
        while (index >= builtOffset_ + 30)
            tap(deckCardsNext_);
        while (index < builtOffset_)
            tap(deckCardsPrev_);
        return static_cast<int>(index - builtOffset_);
    };
    auto listRowFor = [&](const std::string &id) {
        const auto &list = app_.decks().drafts();
        auto it = std::find_if(list.begin(), list.end(), [&](const app::DeckDraft &d) { return d.id == id; });
        require(it != list.end(), "saved deck absent");
        const auto index = static_cast<std::size_t>(it - list.begin());
        while (index >= deckListOffset_ + 6)
            tap(deckListNext_);
        while (index < deckListOffset_)
            tap(deckListPrev_);
        return static_cast<int>(index - deckListOffset_);
    };
    require(app_.page() == app::Page::Menu && !app_.hasMatch(), "deck smoke requires menu");
    tap(menuRow(2));
    tap(deckNew_);
    const auto emptyId = editingDeck_.id;
    require(editingDeck_.size() == 0, "new draft was not empty");
    tap(deckPoolNext_);
    require(poolOffset_ == 9, "pool next page failed");
    tap(deckPoolNext_);
    rightTap(poolRow(0));
    require(editingDeck_.size() == 1, "adding from a later pool page failed");
    rightTap(builtRow(0));
    require(editingDeck_.size() == 0, "last copy was not removed");
    tap(deckPoolPrev_);
    tap(deckPoolPrev_);
    require(poolOffset_ == 0, "pool previous page failed");
    // Cross-column transfers mutate exactly one copy; all cancellation paths leave the draft intact.
    draw(window);
    const auto first = poolRows_[poolOffset_];
    drag(poolRow(0), builtRow(0));
    require(editingDeck_.size() == 1 && editingDeck_.cards.at(first) == 1, "pool-to-deck drag failed");
    drag(builtRow(0), poolRow(0));
    require(editingDeck_.size() == 0, "deck-to-pool drag failed");
    const auto beforeCancel = app::encodeDraft(editingDeck_);
    drag(poolRow(0), {{20, 500}, {60, 60}});
    require(app::encodeDraft(editingDeck_) == beforeCancel, "invalid drop changed counts");
    press(center(poolRow(0)));
    movePointer(center(builtRow(0)));
    escaped();
    releasePointer(center(builtRow(0)));
    require(app_.page() == app::Page::DeckEditor && app::encodeDraft(editingDeck_) == beforeCancel,
            "Esc drag cancellation failed");
    press(center(poolRow(0)));
    movePointer(center(builtRow(0)));
    resetPointer();
    releasePointer(center(builtRow(0)));
    require(app::encodeDraft(editingDeck_) == beforeCancel, "focus cancellation changed draft");
    press(center(poolRow(0)));
    movePointer(center(builtRow(0)));
    rightClick(center(builtRow(0)));
    releasePointer(center(builtRow(0)));
    require(app::encodeDraft(editingDeck_) == beforeCancel, "right-button drag cancellation changed draft");
    rightTap(poolRow(0));
    rightTap(poolRow(0));
    rightTap(poolRow(0));
    rightTap(poolRow(0));
    require(editingDeck_.cards.at(first) == 3 && editingDeck_.size() == 3,
            "right-click exceeded same-name cap");
    drag(poolRow(0), builtRow(0));
    require(editingDeck_.size() == 3, "drag exceeded same-name cap");
    rightTap(builtRow(0));
    rightTap(builtRow(0));
    rightTap(builtRow(0));
    require(editingDeck_.size() == 0, "right-click failed to remove each copy");
    tap(deckSave_);
    tap(deckEditorBack_);
    require(app_.decks().find(emptyId), "empty draft did not save");
    tap(deckDeleteRow(listRowFor(emptyId)));
    tap(confirmNo_);
    require(app_.decks().find(emptyId), "delete cancellation lost draft");
    tap(deckDeleteRow(listRowFor(emptyId)));
    tap(confirmYes_);
    require(!app_.decks().find(emptyId), "empty draft delete failed");
    importPreset_ = 0;
    tap(deckImport_);
    const auto id = editingDeck_.id;
    draw(window);
    require(builtRows_.size() == 30 && deckHits_.size() == 39, "all 30 copies were not visible");
    require(builtOffset_ == 0, "legal deck unexpectedly paged");
    replaceText(deckName_, "构筑验收");
    replaceText(deckSearch_, "火球");
    tap(filterButton(0));
    tap(filterButton(0));
    for (int n = 0; n < 20 && deckQuery_.rarity != "rare"; ++n)
        tap(filterButton(1));
    for (int n = 0; n < 20 && deckQuery_.cost != 3; ++n)
        tap(filterButton(2));
    for (int n = 0; n < 20 && deckQuery_.castCost != 1; ++n)
        tap(filterButton(3));
    for (int n = 0; n < 20 && deckQuery_.rank != 2; ++n)
        tap(filterButton(4));
    for (int n = 0; n < 50 && (deckQuery_.tags.empty() || deckQuery_.tags.front() != "伤害"); ++n)
        tap(filterButton(5));
    draw(window);
    require(poolRows_ == std::vector<std::string>{"fireball"}, "combined filters failed");
    tap(poolRow(0));
    require(inspectedDefinition_ == "fireball", "card details did not open");
    const auto otherRow = rowFor("spark");
    movePointer(builtRow(otherRow).position + sf::Vector2f{20, 20});
    movePointer({28, 350});
    require(inspectedDefinition_ == "fireball", "hover or sidebar movement replaced clicked details");
    tap(builtRow(otherRow));
    require(inspectedDefinition_ == "spark", "click did not replace persistent details");
    tap(poolRow(0));
    rightTap(poolRow(0));
    require(editingDeck_.cards.at("fireball") == 3 && editingDeck_.size() == 30, "copy or size cap failed");
    rightTap(builtRow(rowFor("fireball")));
    tap(deckSave_);
    require(editingDeck_.size() == 29 && !deckDirty(), "incomplete draft failed to save");
    tap(deckIssues_);
    require(showDeckProblems_, "validation problems did not open");
    tap({{1000, 717}, {180, 46}});
    require(!showDeckProblems_, "validation problems did not close");
    for (const auto &d : app_.playableDecks())
        require(d.id != "user/" + id, "illegal draft entered match choices");
    app::DeckLibrary loaded(content_.catalog, app_.userDirectory());
    require(loaded.find(id) && loaded.find(id)->cards.at("fireball") == 2, "restart lost draft counts");
    tap(deckEditorBack_);
    tap(deckListRow(listRowFor(id)));
    replaceText(deckSearch_, "逆三角");
    draw(window);
    rightTap(poolRow(0));
    require(editingDeck_.size() == 30 && app_.decks().problems(editingDeck_).empty(), "draft repair failed");
    const auto originalBase=editingDeck_.baseFormation;
    tap(deckBase_);
    require(editingDeck_.baseFormation!=originalBase,"editor base formation control missing");
    const auto savedBase=editingDeck_.baseFormation;
    tap(deckEditorBack_);
    require(app_.page() == app::Page::ConfirmDeckDiscard, "unsaved changes were not confirmed");
    tap(deckDiscardCancel_);
    require(editingDeck_.cards.at("conduit") == 3, "cancel discarded modifications");
    requestDeckLeave(true);
    escaped();
    require(!app_.quitRequested() && app_.page() == app::Page::DeckEditor, "OS-close cancellation failed");
    tap(deckSave_);
    replaceText(deckName_, "未保存名称");
    tap(deckEditorBack_);
    tap(deckDiscardNo_);
    tap(deckListRow(listRowFor(id)));
    require(editingDeck_.name == "构筑验收", "discard wrote changed name");
    replaceText(deckName_, "构筑验收卡组");
    tap(deckEditorBack_);
    tap(deckDiscardSave_);
    require(app_.page() == app::Page::Decks && app_.decks().find(id)->name == "构筑验收卡组",
            "save and leave failed");
    tap(deckListBack_);
    tap(menuRow(0));
    for (std::size_t n = 0; n <= app_.playableDecks().size() && selectedDeckIds_[0] != "user/" + id; ++n)
        tap(deckRow(0));
    require(selectedChoice(0).id == "user/" + id, "user deck absent from setup");
    const auto setupBase=selectedChoice(0).deck.baseFormation;
    tap(baseRow(0));
    require(selectedChoice(0).deck.baseFormation==setupBase && setupBase==savedBase,"setup overrides the saved editor base");
    tap(setupStart_);
    require(app_.configuration().players[0].baseFormation==savedBase,"match ignored the saved editor base");
    app::DeckLibrary baseReload(content_.catalog,app_.userDirectory());
    require(baseReload.find(id)->baseFormation==savedBase,"editor base did not persist across restart");
    require(app_.hasMatch() &&
                app_.configuration().players[0].cards == app_.decks().find(id)->playerDeck().cards,
            "match did not use saved user deck");
    require(app_.configuration().players[0].cards != app_.configuration().players[1].cards,
            "independent player configuration lost");
    require(replay(content_, app_.match().recording()).digest() == app_.match().engine().digest(),
            "user deck replay mismatch");
    tap(settingsButton);
    tap(leaveSettings_);
    tap(confirmYes_);
    tap(menuRow(2));
    tap(deckDeleteRow(listRowFor(id)));
    tap(confirmYes_);
    app::DeckLibrary deleted(content_.catalog, app_.userDirectory());
    require(!deleted.find(id), "delete did not persist");
    tap(deckListBack_);
    std::cout << "Deck UI smoke passed: fullscreen pages, 30 visible copies, cross-column drags, "
                 "right-click, cancellations, new/copy/name, combined filters, details, copy limit, draft "
                 "save/reload, repair, unsaved confirmation, independent user-deck match/replay and delete\n";
}
