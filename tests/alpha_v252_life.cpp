#include "wizard/application.hpp"
#include "wizard/network.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
namespace {
CardCatalog lifeCatalog() {
    auto cat = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    cat.mechanicsRevision = 5;
    return cat;
}
}
TEST_CASE("Alpha v2.5.2 temporary life stacks sources and absorbs by expiry", "[alpha-v252][temporary-life]") {
    auto cat = lifeCatalog(); GameState s;
    s.players[0].ownTurn = 2; s.globalTurn = 4; s.phase = Phase::Main;
    Trigger a; a.owner = 0; a.source = 10;
    Trigger b = a; b.source = 20;
    EffectResolver::apply(s, cat, a, {EffectKind::AddTemporaryLife, 6});
    ResourceRules::grantTemporaryLife(s, 0, b.source, 5, 4);
    REQUIRE(s.players[0].temporaryLife == 11);
    REQUIRE(s.temporaryLives.size() == 2);
    CHECK(s.temporaryLives.front().acquiredTurn == 4);
    CHECK(s.temporaryLives.front().acquiredPhase == Phase::Main);
    ResourceRules::damage(s, 0, 8, "overload");
    CHECK(s.players[0].life == startingLife);
    REQUIRE(s.players[0].temporaryLife == 3);
    REQUIRE(s.temporaryLives.size() == 1);
    CHECK(s.temporaryLives.front().source == 20);
    s.players[0].ownTurn = 3;
    ResourceRules::expireTemporaryLife(s, 0);
    CHECK(s.players[0].temporaryLife == 3);
    ResourceRules::damage(s, 0, 5, "exhaustion");
    CHECK(s.players[0].life == startingLife - 2);
    CHECK(s.players[0].temporaryLife == 0);
    CHECK(s.temporaryLives.empty());
}
TEST_CASE("Alpha v2.5.2 life clearing and expiry preserve other sources and owners", "[alpha-v252][temporary-life]") {
    GameState s; s.players[0].ownTurn = 2;
    ResourceRules::grantTemporaryLife(s, 0, 10, 6, 3);
    ResourceRules::grantTemporaryLife(s, 0, 20, 5, 4);
    ResourceRules::grantTemporaryLife(s, 1, 10, 7, 3);
    ResourceRules::clearTemporaryLife(s, 0, 10);
    CHECK(s.players[0].temporaryLife == 5);
    CHECK(s.players[1].temporaryLife == 7);
    s.players[0].ownTurn = 3; ResourceRules::expireTemporaryLife(s, 0);
    CHECK(s.players[0].temporaryLife == 5);
    s.players[0].ownTurn = 4; ResourceRules::expireTemporaryLife(s, 0);
    CHECK(s.players[0].temporaryLife == 0);
    ResourceRules::clearTemporaryLife(s, 1);
    CHECK(s.players[1].temporaryLife == 0);
    CHECK(s.temporaryLives.empty());
}
TEST_CASE("Alpha v2.5.2 temporary life does not change load clearing healing or death", "[alpha-v252][temporary-life]") {
    auto cat = lifeCatalog(); GameState s; Trigger t; t.owner = 0;
    ResourceRules::grantTemporaryLife(s, 0, 10, 6, 3);
    s.temporary.push_back({s.nextLoad++, 0, 10, 2, 3});
    EffectResolver::apply(s, cat, t, {EffectKind::ClearAllTemporary, 0});
    REQUIRE(s.players[0].temporaryLife == 6);
    CHECK(s.temporary.empty());
    EffectResolver::apply(s, cat, t, {EffectKind::Heal, 20});
    CHECK(s.players[0].life == maximumLife);
    CHECK(s.players[0].temporaryLife == 6);
    ResourceRules::damage(s, 0, maximumLife + 6, "overload");
    StateMaintenance::check(s, cat);
    CHECK(s.players[0].life == 0);
    CHECK(s.result == 1);
}
TEST_CASE("Alpha v2.5.2 temporary life survives views codec and canonical state", "[alpha-v252][temporary-life]") {
    auto cat = lifeCatalog(); GameState s;
    s.players[0].temporaryLife = 4;
    ResourceRules::grantTemporaryLife(s, 1, 20, 2, 3);
    auto engine = GameEngine::scenario(cat, s);
    REQUIRE(engine.state().temporaryLives.size() == 2);
    CHECK(engine.state().players[0].temporaryLife == 4);
    for (int viewer : {0, 1}) {
        const auto view = engine.viewFor(viewer);
        auto copy = net::decodeView(net::encodeView(view), cat);
        REQUIRE(copy.temporaryLives.size() == 2);
        CHECK(copy.temporaryLives.front().amount == view.temporaryLives.front().amount);
        CHECK(copy.temporaryLives.front().expiryOwnTurn == view.temporaryLives.front().expiryOwnTurn);
        CHECK(copy.temporaryLives.front().acquiredTurn == view.temporaryLives.front().acquiredTurn);
        CHECK(copy.players[0].temporaryLife == 4);
    }
    CHECK(engine.canonicalState().find("H") != std::string::npos);
    CHECK(Rules::invariants(engine.state(), cat).empty());
    cat.mechanicsRevision = 4; GameState legacy; Trigger t; t.owner = 0;
    EffectResolver::apply(legacy, cat, t, {EffectKind::AddTemporaryLife, 6});
    EffectResolver::apply(legacy, cat, t, {EffectKind::AddTemporaryLife, 5});
    CHECK(legacy.players[0].temporaryLife == 6);
    CHECK(legacy.temporaryLives.empty());
}
