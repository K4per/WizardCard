#pragma once
#include "wizard/replay.hpp"

namespace wizard::app {
struct DeckDraft {
    std::string id, name, baseFormation{"balance"};
    std::map<std::string, int> cards;
    int size() const;
    PlayerDeck playerDeck() const;
};
Json encodeDraft(const DeckDraft &);
DeckDraft decodeDraft(const Json &);
std::vector<std::string> deckProblems(const CardCatalog &, const DeckDraft &);
struct CardQuery {
    std::string name, rarity;
    std::optional<CardType> type;
    std::optional<int> cost, castCost, rank;
    std::vector<std::string> tags;
};
std::vector<std::string> cardTags(const CardDefinition &);
bool hasSpellRank(CardType);
std::string rarityLabel(const std::string &);
std::string cardTypeLabel(CardType);
std::string primaryCostLabel(CardType);
std::vector<std::string> queryCards(const CardCatalog &, const CardQuery &);

// Keeps missing content IDs so a content upgrade never silently rewrites a player's deck.
class DeckLibrary {
  public:
    DeckLibrary(CardCatalog, std::filesystem::path userDirectory);
    const std::vector<DeckDraft> &drafts() const {
        return drafts_;
    }
    const std::string &notice() const {
        return notice_;
    }
    bool writable() const {
        return writable_;
    }
    DeckDraft create(const std::string &name = "新卡组") const;
    const DeckDraft *find(const std::string &id) const;
    bool save(const DeckDraft &, std::string &error);
    bool erase(const std::string &id, std::string &error);
    std::vector<std::string> problems(const DeckDraft &d) const {
        return deckProblems(catalog_, d);
    }

  private:
    CardCatalog catalog_;
    std::filesystem::path path_;
    std::vector<DeckDraft> drafts_;
    std::string notice_;
    bool writable_{true};
    void persist(const std::vector<DeckDraft> &) const;
};
} // namespace wizard::app
