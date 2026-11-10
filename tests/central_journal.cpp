#include "wizard/central_journal.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>

using namespace wizard;
using namespace wizard::central;

TEST_CASE("central journal preserves different room lifetimes that reuse a public code", "[central][journal]") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    const auto path = std::filesystem::path(WIZARD_SOURCE_DIR) / "build/journal-code-reuse";
    std::filesystem::remove_all(path);
    JournalStore store(path, content);
    JournalWrite write;
    write.room = "ABCDEFGH"; write.serial = 1;
    write.bytes = std::make_shared<const std::string>("{\"kind\":\"first\"}\n");
    write.summary = {{"phase", "waiting"}, {"storageId", "ABCDEFGH-instance-a-1"}};
    REQUIRE(store.write(write).error.empty());
    write.bytes = std::make_shared<const std::string>("{\"kind\":\"second\"}\n");
    write.summary["storageId"] = "ABCDEFGH-instance-b-1";
    REQUIRE(store.write(write).error.empty());
    REQUIRE(std::filesystem::is_regular_file(path / "ABCDEFGH-instance-a-1/journal.ndjson"));
    REQUIRE(std::filesystem::is_regular_file(path / "ABCDEFGH-instance-b-1/journal.ndjson"));
    REQUIRE(store.abortUnfinished() == 2);
    std::filesystem::remove_all(path);
}
namespace {
Content formal() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260"); }
struct Directory {
    std::filesystem::path path = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "central-journal" / net::token();
    ~Directory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
}
TEST_CASE("incremental recording captures authority timeouts and rejected commands add nothing", "[central][storage]") {
    auto content = formal();
    auto now = std::chrono::steady_clock::time_point{};
    MatchSession match(content, 42);
    match.enableClock(false, [&] { return now; });
    REQUIRE(match.submit(0, SubmitOpeningGesture{1, 0}).accepted);
    REQUIRE(match.submit(1, SubmitOpeningGesture{1, 2}).accepted);
    auto initial = match.recordingSince({});
    REQUIRE(initial.commands.size() == 2);
    REQUIRE_FALSE(match.submit(0, AdvancePhase{}).accepted);
    REQUIRE(match.recordingSince(initial.next).commands.empty());
    now += std::chrono::minutes(6);
    match.tick();
    auto delta = match.recordingSince(initial.next);
    REQUIRE(delta.commands.size() == 1);
    REQUIRE(delta.commands.at(0).at("command").at("type") == 19);
    REQUIRE_FALSE(delta.events.empty());
    REQUIRE(match.recordingSince(delta.next).commands.empty());
    REQUIRE(match.recordingSince(delta.next).events.empty());
    auto record = match.recordingHeader();
    record["commands"] = initial.commands;
    for (auto &command : delta.commands) record["commands"].push_back(command);
    record["events"] = initial.events;
    for (auto &event : delta.events) record["events"].push_back(event);
    REQUIRE(replay(content, record).digest() == match.engine().digest());
}
TEST_CASE("journal retry truncates an incomplete append at its acknowledged offset", "[central][storage]") {
    Directory directory;
    JournalStore store(directory.path, formal());
    JournalWrite first;
    first.room = "ABCDEFG2"; first.serial = 1; first.offset = 0;
    first.bytes = std::make_shared<const std::string>("{\"kind\":\"start\"}\n");
    first.summary = {{"phase", "playing"}, {"revision", "0"}};
    const auto one = store.write(first);
    REQUIRE(one.error.empty());
    {
        std::ofstream partial(directory.path / first.room / "journal.ndjson", std::ios::app);
        partial << "{\"kind\":\"partial";
    }
    auto next = first; next.serial = 2; next.offset = first.bytes->size();
    next.bytes = std::make_shared<const std::string>("{\"kind\":\"delta\",\"revision\":\"1\"}\n");
    next.summary["revision"] = "1";
    REQUIRE(store.write(next).error.empty());
    std::ifstream file(directory.path / first.room / "journal.ndjson");
    const std::string bytes((std::istreambuf_iterator<char>(file)), {});
    REQUIRE(bytes == *first.bytes + *next.bytes);
    REQUIRE(readJson(directory.path / first.room / "metadata.json").at("journalOffset") == first.bytes->size() + next.bytes->size());
}
TEST_CASE("journal restart aborts unfinished rooms and retains only complete JSON lines", "[central][storage]") {
    Directory directory;
    JournalStore store(directory.path, formal());
    JournalWrite write;
    write.room = "ABCDEFG3"; write.serial = 1;
    write.bytes = std::make_shared<const std::string>("{\"kind\":\"start\"}\n");
    write.summary = {{"phase", "playing"}, {"revision", "3"}};
    REQUIRE(store.write(write).error.empty());
    {
        std::ofstream file(directory.path / write.room / "journal.ndjson", std::ios::app);
        file << "{\"kind\":\"delta\",\"revision\":\"4\"}\n{\"unfinished\":";
    }
    const auto recovered = store.abortUnfinished();
    REQUIRE(recovered == 1);
    const auto metadata = readJson(directory.path / write.room / "metadata.json");
    REQUIRE(metadata.at("phase") == "aborted");
    REQUIRE(metadata.at("abortReason") == "server_restart");
    REQUIRE(metadata.at("revision") == "4");
    std::ifstream file(directory.path / write.room / "journal.ndjson");
    const std::string bytes((std::istreambuf_iterator<char>(file)), {});
    REQUIRE(bytes.back() == '\n');
    REQUIRE(bytes.find("unfinished") == std::string::npos);
}
TEST_CASE("journal final replay is validated while aborted private recordings remain administrative", "[central][storage]") {
    Directory directory;
    auto content = formal();
    JournalStore store(directory.path, content);
    auto match = std::make_shared<MatchSession>(content, 42);
    REQUIRE(match->submit(1, Surrender{}).accepted);
    JournalWrite write;
    write.room = "ABCDEFG4"; write.serial = 1; write.bytes = std::make_shared<const std::string>("{\"kind\":\"terminal\"}\n");
    write.summary = {{"phase", "completed"}, {"revision", "1"}}; write.finished = match;
    const auto result = store.write(write);
    REQUIRE(result.error.empty());
    REQUIRE(result.replay);
    REQUIRE(replay(content, Json::parse(*result.replay)).digest() == match->engine().digest());
    REQUIRE(std::filesystem::exists(directory.path / write.room / "replay.json"));
    write.room = "ABCDEFG5"; write.summary["phase"] = "aborted";
    const auto aborted = store.write(write);
    REQUIRE(aborted.error.empty());
    REQUIRE_FALSE(aborted.replay);
    REQUIRE_FALSE(std::filesystem::exists(directory.path / write.room / "replay.json"));
    REQUIRE(store.abortUnfinished() == 0);
}
