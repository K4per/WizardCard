#include "wizard/central.hpp"

namespace wizard::central {
Json hello(const Content &content) {
    auto result = net::hello(content);
    result["protocol"] = 5;
    return result;
}
std::string compatibilityError(const Content &content, const Json &message) {
    const auto expected = hello(content);
    if (!message.is_object() || message.value("type", std::string{}) != "Hello")
        return "请先完成中央服务器握手";
    for (const auto *key : {"protocol", "program", "rules", "mechanics", "cards", "hash"})
        if (!message.contains(key) || message.at(key) != expected.at(key))
            return "程序、协议、规则或卡池版本不一致";
    return {};
}
Json encodeDeck(const PlayerDeck &deck) {
    return {{"baseFormation", deck.baseFormation}, {"cards", deck.cards}, {"sideDeck", deck.sideDeck}};
}
PlayerDeck decodeDeck(const Json &message, const CardCatalog &catalog) {
    PlayerDeck deck{message.value("baseFormation", std::string{}),
                    message.at("cards").get<std::vector<std::string>>(),
                    message.at("sideDeck").get<std::vector<std::string>>()};
    const auto problems = Rules::deckErrors(catalog, deck);
    if (!problems.empty()) throw std::invalid_argument(problems.front());
    return deck;
}
Json error(const std::string &code, const std::string &message) {
    return {{"type", "Error"}, {"code", code}, {"error", message}};
}
} // namespace wizard::central
