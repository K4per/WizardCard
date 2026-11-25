#include "wizard/lesson_script.hpp"
#include "wizard/command_codec.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace wizard::app {
namespace detail {
struct CommandPattern {
    CommandKind kind;
    Json fields;
};
struct Context {
    std::optional<PlayerId> active;
    std::optional<std::uint64_t> turn;
    std::optional<Phase> phase;
};
struct Transition {
    std::optional<CommandPattern> command;
    std::string event;
    std::optional<int> gameOver;
    bool onContinue{};
    PlayerId actor{-1};
    Context context;
    std::size_t next{};
};
struct Step {
    std::string id, hint, blockedHint, anchor;
    Json params, highlights;
    std::vector<CommandPattern> allow;
    std::vector<Transition> until;
};
struct OpponentAction {
    std::size_t step;
    CommandPattern command;
    Context context;
    bool once;
};
struct LessonProgram {
    std::vector<Step> steps;
    std::vector<OpponentAction> opponent;
    std::optional<Transition> completion;
    bool learnerWinsOpening{};
};
} // namespace detail

namespace {
using namespace detail;
constexpr std::size_t finish = std::numeric_limits<std::size_t>::max();

[[noreturn]] void reject(const std::string &message) {
    throw std::invalid_argument(message);
}

void fields(const Json &object, std::initializer_list<std::string_view> keys) {
    if (!object.is_object()) reject("expected object");
    for (const auto &item : object.items())
        if (std::find(keys.begin(), keys.end(), item.key()) == keys.end())
            reject("unknown field " + item.key());
}

const Json &list(const Json &object, const char *key) {
    static const Json empty = Json::array();
    if (!object.contains(key)) return empty;
    const auto &result = object.at(key);
    if (!result.is_array()) reject(std::string(key) + " must be an array");
    return result;
}

std::uint64_t integer(const Json &value, std::uint64_t minimum, std::uint64_t maximum,
                      std::string_view field) {
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0))
        reject(std::string(field) + " must be an integer");
    const auto number = value.get<std::uint64_t>();
    if (number < minimum || number > maximum) reject(std::string(field) + " out of range");
    return number;
}

bool name(std::string_view value, std::string_view punctuation = {}) {
    const auto letter = [](char c) { return c >= 'a' && c <= 'z'; };
    return !value.empty() && letter(value.front()) && std::all_of(value.begin(), value.end(), [&](char c) {
        return letter(c) || (c >= '0' && c <= '9') || punctuation.find(c) != std::string_view::npos;
    });
}

void requireCard(const CardCatalog &cards, const std::string &id) {
    if (!cards.cards.count(id)) reject("unknown card ID " + id);
}

PlayerId player(const Json &value, bool either = false) {
    if (value == "learner") return 0;
    if (value == "opponent") return 1;
    if (either && value == "either") return -1;
    reject("expected learner or opponent");
}

void requireText(const TextCatalog &text, const CardCatalog &cards, const std::string &key,
                 const Json &params = Json::object()) {
    const auto ref = decodeTextRef({{"key", key}, {"args", params}});
    std::string error;
    text.render(ref, {}, [&](const std::string &id) {
        return cards.cards.count(id) ? id : std::string{};
    }, [&](const std::string &message) {
        if (error.empty()) error = message;
    });
    if (!error.empty()) reject(error);
}

constexpr std::array zones{"deck", "hand", "action", "analysis", "words", "casting", "ash",
                           "attached", "resolving", "side_deck", "exile"};
bool zone(std::string_view value) {
    return std::find(zones.begin(), zones.end(), value) != zones.end();
}
void requireHighlight(const std::string &value, const CardCatalog &cards) {
    const std::string_view id(value);
    const auto colon = id.find(':');
    if (colon == std::string_view::npos) reject("invalid highlight " + value);
    const auto kind = id.substr(0, colon), path = id.substr(colon + 1);
    const auto dot = path.find('.');
    if (kind == "ui" && name(path, "_-")) return;
    if (dot != std::string_view::npos) {
        const auto group = path.substr(0, dot), target = path.substr(dot + 1);
        if (kind == "zone" && (group == "learner" || group == "opponent") &&
            (zone(target) || target == "status" || target == "formation")) return;
        if (kind == "card" && zone(group)) {
            requireCard(cards, std::string(target));
            return;
        }
    }
    reject("invalid highlight " + value);
}

struct PatternSchema {
    CommandKind kind;
    std::vector<std::string_view> fields;
};

// These names describe selectors, not transport fields such as decision IDs.
const std::map<std::string_view, PatternSchema> commands{
    {"SetFormation", {CommandKind::SetFormation, {"card", "replace", "materials"}}},
    {"RemoveFormation", {CommandKind::RemoveFormation, {"card"}}},
    {"StartAnalysis", {CommandKind::StartAnalysis, {"card", "formation", "target", "targets", "ringIndex"}}},
    {"PrepareCast", {CommandKind::PrepareCast, {"card", "target", "targets", "discard"}}},
    {"PlayAction", {CommandKind::PlayAction, {"card", "target", "targets"}}},
    {"PreloadWord", {CommandKind::PreloadWord, {"card", "formation", "target", "targets", "discard", "ringIndex", "release"}}},
    {"AttachSeal", {CommandKind::AttachSeal, {"card", "host"}}},
    {"RemoveSeal", {CommandKind::RemoveSeal, {"card"}}},
    {"Abandon", {CommandKind::Abandon, {"card"}}},
    {"Advance", {CommandKind::Advance, {}}},
    {"Choose", {CommandKind::Choose, {"option", "ringIndex"}}},
    {"Surrender", {CommandKind::Surrender, {}}},
    {"Respond", {CommandKind::Respond, {"card", "ability", "target", "targets", "discard"}}},
    {"PassResponse", {CommandKind::PassResponse, {}}},
    {"AdvancePhase", {CommandKind::AdvancePhase, {"skipCast"}}},
    {"SetAmbush", {CommandKind::SetAmbush, {"card", "formation", "ringIndex"}}},
    {"FlipAmbush", {CommandKind::FlipAmbush, {"card", "target", "targets", "discard"}}},
    {"ActivateSpell", {CommandKind::ActivateSpell, {"card", "target", "targets", "discard"}}},
    {"SelectInitialFormation", {CommandKind::SelectInitialFormation, {"card"}}},
    {"ActivateAbility", {CommandKind::ActivateAbility, {"card", "target", "ability"}}},
    {"SubmitOpeningGesture", {CommandKind::SubmitOpeningGesture, {"gesture"}}}
};

constexpr std::array gestures{"rock", "paper", "scissors"};
void requireTarget(const CardCatalog &cards, const Json &value) {
    if (value == "learner" || value == "opponent" || value == "self" || value == "none") return;
    requireCard(cards, value.get<std::string>());
}
void requireOption(const CardCatalog &cards, const Json &value) {
    if (value == "pass") return;
    fields(value, {"card", "player", "mode", "load", "payment", "link", "trigger"});
    if (value.size() != 1) reject("option requires one selector");
    const auto field = value.begin().key();
    const auto &item = value.begin().value();
    if (field == "card") requireCard(cards, item.get<std::string>());
    else if (field == "player") {
        if (item != "self") (void)player(item);
    } else (void)integer(item, 1, std::numeric_limits<std::uint64_t>::max(), field);
}

CommandPattern commandPattern(const Json &input, const CardCatalog &cards) {
    const auto word = input.is_string() ? input.get<std::string>() : input.at("command").get<std::string>();
    const auto schema = commands.find(word);
    if (schema == commands.end()) reject("unknown command " + word);
    CommandPattern pattern{schema->second.kind, Json::object()};
    if (input.is_string()) return pattern;
    for (const auto &[key, value] : input.items()) {
        if (key == "command") continue;
        const auto &allowed = schema->second.fields;
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
            reject("invalid selector " + key + " for " + word);
        if (key == "ability") {
            if (!value.is_string() || value.get<std::string>().empty()) reject("expected ability ID");
        } else if (key == "ringIndex") {
            (void)integer(value, 0, 199, key);
        } else if (key == "skipCast" || key == "release") {
            if (!value.is_boolean()) reject(key + " must be boolean");
        } else if (key == "gesture") {
            const auto gesture = value.get<std::string>();
            if (std::find(gestures.begin(), gestures.end(), gesture) == gestures.end()) reject("invalid gesture");
        } else if (key == "option") {
            requireOption(cards, value);
        } else if (key == "target") {
            requireTarget(cards, value);
        } else if (key == "targets" || key == "materials") {
            if (!value.is_array()) reject(key + " must be an array");
            for (const auto &item : value) {
                if (key == "targets") requireTarget(cards, item);
                else requireCard(cards, item.get<std::string>());
            }
        } else {
            requireCard(cards, value.get<std::string>());
        }
        pattern.fields[key] = value;
    }
    return pattern;
}

Context context(const Json &input) {
    Context result;
    if (input.contains("active")) result.active = player(input.at("active"));
    if (input.contains("turn")) result.turn = integer(input.at("turn"), 0, 1000000, "turn");
    if (input.contains("phase")) {
        const auto phase = input.at("phase").get<std::string>();
        constexpr std::array phases{"Draw", "Prepare", "Main", "Cast", "End"};
        const auto match = std::find(phases.begin(), phases.end(), phase);
        if (match == phases.end()) reject("invalid phase " + phase);
        result.phase = static_cast<Phase>(match - phases.begin());
    }
    return result;
}

Transition transition(const Json &input, const CardCatalog &cards) {
    fields(input, {"command", "event", "gameOver", "continue", "actor", "active", "turn", "phase", "next"});
    Transition result;
    if (input.contains("command")) result.command = commandPattern(input.at("command"), cards);
    if (input.contains("event")) {
        result.event = input.at("event").get<std::string>();
        if (result.event == "ChainResolved") result.event = "chain_closed";
        else if (result.event != "ResponseOpened" && !name(result.event, "_")) reject("invalid event identifier");
    }
    if (input.contains("gameOver")) {
        const auto &value = input.at("gameOver");
        if (value == true) result.gameOver = -1;
        else if (value == "draw") result.gameOver = 2;
        else result.gameOver = player(value);
    }
    if (input.contains("continue")) {
        if (input.at("continue") != true || input.size() != (input.contains("next") ? 2u : 1u))
            reject("continue cannot be combined with command or event conditions");
        result.onContinue = true;
    }
    if (!result.command && result.event.empty() && !result.gameOver && !result.onContinue)
        reject("expected command, event, gameOver or continue");
    result.actor = input.contains("actor") ? player(input.at("actor"), true) : (result.command ? 0 : -1);
    result.context = context(input);
    return result;
}

std::size_t stepIndex(const LessonProgram &program, const std::string &id) {
    const auto it = std::find_if(program.steps.begin(), program.steps.end(), [&](const Step &step) {
        return step.id == id;
    });
    if (it == program.steps.end()) reject("unknown step " + id);
    return static_cast<std::size_t>(it - program.steps.begin());
}

std::shared_ptr<const LessonProgram> parseProgram(const Json &document, const CardCatalog &cards,
                                                  const TextCatalog &text) {
    auto program = std::make_shared<LessonProgram>();
    const auto &steps = list(document, "steps");
    if (steps.empty()) reject("lesson needs at least one step");
    std::set<std::string> stepIds;
    for (const auto &input : steps) {
        fields(input, {"id", "hint", "params", "blockedHint", "highlight", "hintAnchor", "allow", "continue", "until"});
        Step step;
        step.id = input.at("id").get<std::string>();
        if (!name(step.id, "-_") || !stepIds.insert(step.id).second) reject("invalid or duplicate step ID " + step.id);
        step.hint = input.at("hint").get<std::string>();
        step.params = input.value("params", Json::object());
        requireText(text, cards, step.hint, step.params);
        if (input.contains("blockedHint")) {
            step.blockedHint = input.at("blockedHint").get<std::string>();
            requireText(text, cards, step.blockedHint);
        }
        step.anchor = input.value("hintAnchor", std::string("auto"));
        constexpr std::array anchors{"auto", "top", "bottom", "left", "right"};
        if (std::find(anchors.begin(), anchors.end(), step.anchor) == anchors.end()) reject("invalid hint anchor");
        step.highlights = list(input, "highlight");
        std::set<std::string> highlights;
        for (const auto &highlight : step.highlights) {
            const auto id = highlight.get<std::string>();
            requireHighlight(id, cards);
            if (!highlights.insert(id).second) reject("duplicate highlight " + id);
        }
        for (const auto &allowed : list(input, "allow")) step.allow.push_back(commandPattern(allowed, cards));
        program->steps.push_back(std::move(step));
    }
    for (std::size_t index = 0; index < steps.size(); ++index) {
        const auto &input = steps.at(index);
        auto &step = program->steps.at(index);
        auto add = [&](const Json &rule) {
            auto result = transition(rule, cards);
            if (rule.contains("next")) {
                const auto id = rule.at("next").get<std::string>();
                result.next = id == "$complete" ? finish : stepIndex(*program, id);
            } else {
                result.next = index + 1;
            }
            step.until.push_back(std::move(result));
        };
        if (input.contains("until")) {
            const auto &until = input.at("until");
            if (until.is_array()) {
                for (const auto &rule : until) add(rule);
            } else {
                add(until);
            }
        }
        if (input.value("continue", false) && std::none_of(step.until.begin(), step.until.end(), [](const Transition &rule) {
            return rule.onContinue;
        })) add({{"continue", true}});
        if (step.until.empty()) reject("step " + step.id + " needs a transition");
    }

    for (const auto &input : list(document, "opponent")) {
        fields(input, {"step", "do", "turn", "active", "phase", "once"});
        const auto index = stepIndex(*program, input.at("step").get<std::string>());
        const auto command = commandPattern(input.at("do"), cards);
        if (command.kind == CommandKind::Surrender) reject("opponent cannot surrender");
        program->opponent.push_back({index, command, context(input), input.value("once", true)});
    }
    if (document.contains("complete")) {
        const auto &input = document.at("complete");
        program->completion = transition(input, cards);
        if (program->completion->onContinue || input.contains("next"))
            reject("completion must follow an accepted command, event or match result");
    }
    if (document.contains("openingOpponent")) {
        if (document.at("openingOpponent") != "learnerWins" || !document.at("scenario").value("opening", false))
            reject("openingOpponent requires learnerWins and a real opening");
        program->learnerWinsOpening = true;
    }
    return program;
}

bool definitionIs(const GameView &view, CardId id, const Json &definition) {
    if (!id) return false;
    const auto card = std::find_if(view.cards.begin(), view.cards.end(), [&](const CardView &candidate) {
        return candidate.instance.id == id;
    });
    return card != view.cards.end() && !card->hidden && definition == card->definition.id;
}

bool targetIs(const GameView &view, PlayerId actor, CardId id, const Json &expected) {
    if (expected == "none") return id == 0;
    if (expected == "learner") return id == playerTarget(0);
    if (expected == "opponent") return id == playerTarget(1);
    if (expected == "self") return id == playerTarget(actor);
    return definitionIs(view, id, expected);
}

bool optionIs(const GameView &view, PlayerId actor, const ChoiceValue &option, const Json &expected) {
    if (expected == "pass") return option.kind == ChoiceKind::Pass;
    if (expected.contains("card"))
        return option.kind == ChoiceKind::Card && definitionIs(view, static_cast<CardId>(option.value), expected.at("card"));
    if (expected.contains("player"))
        return option.kind == ChoiceKind::Player && option.value == static_cast<std::uint64_t>(
            expected.at("player") == "self" ? actor : player(expected.at("player")));
    const std::array<std::pair<const char *, ChoiceKind>, 5> kinds{{
        {"mode", ChoiceKind::Mode}, {"load", ChoiceKind::Load}, {"payment", ChoiceKind::Payment},
        {"link", ChoiceKind::Link}, {"trigger", ChoiceKind::Trigger}}};
    return std::any_of(kinds.begin(), kinds.end(), [&](const auto &item) {
        return option.kind == item.second && expected.contains(item.first) && expected.at(item.first) == option.value;
    });
}

bool matches(const CommandPattern &pattern, const GameView &view, PlayerId actor, const Command &command) {
    if (pattern.kind != commandKind(command)) return false;
    if (pattern.fields.empty()) return true;
    const auto encoded = encodeCommand(command);
    for (const auto &[key, expected] : pattern.fields.items()) {
        if (key == "option") {
            if (!optionIs(view, actor, std::get<Choose>(command).option, expected)) return false;
        } else if (key == "ringIndex" || key == "gesture" || key == "ability" || key == "release" || key == "skipCast") {
            if (key == "gesture") {
                const auto gesture = std::get<SubmitOpeningGesture>(command).gesture;
                if (gesture < 0 || gesture >= static_cast<int>(gestures.size()) || expected != gestures[gesture]) return false;
            } else if (key == "skipCast") {
                if (expected != std::get<AdvancePhase>(command).skipCast) return false;
            } else if (!encoded.contains(key) || expected != encoded.at(key)) return false;
        } else if (key == "targets" || key == "materials") {
            const auto values = encoded.value(key, std::vector<CardId>{});
            if (values.size() != expected.size()) return false;
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (key == "targets" ? !targetIs(view, actor, values[i], expected.at(i)) :
                    !definitionIs(view, values[i], expected.at(i))) return false;
            }
        } else {
            const auto id = encoded.value(key, CardId{});
            if (key == "target" ? !targetIs(view, actor, id, expected) : !definitionIs(view, id, expected))
                return false;
        }
    }
    return true;
}

bool matches(const Context &context, const GameView &view) {
    return (!context.active || *context.active == view.active) &&
        (!context.phase || *context.phase == view.phase) &&
        (!context.turn || *context.turn == view.globalTurn);
}

bool matches(const Transition &rule, PlayerId actor, const Command &command, const GameView &before,
             const GameView &after, const std::vector<GameEvent> &events) {
    if (rule.onContinue || (rule.actor != -1 && actor != rule.actor) ||
        !matches(rule.context, rule.command ? before : after) ||
        (rule.command && !matches(*rule.command, before, actor, command))) return false;
    if (rule.gameOver && (before.result != -1 || after.result == -1 ||
        (*rule.gameOver != -1 && after.result != *rule.gameOver))) return false;
    if (rule.event.empty()) return true;
    if (rule.event == "ResponseOpened") {
        // Decisions are private to their player. The actor who opened their
        // opponent's response can still observe the public building chain.
        if (!after.chain || after.chain->mode != ChainMode::Building) return false;
        if (after.decision && after.decision->kind == DecisionKind::Response)
            return !before.decision || before.decision->id != after.decision->id;
        return !before.chain || before.chain->mode != ChainMode::Building ||
            before.chain->id != after.chain->id || before.chain->priority != after.chain->priority ||
            before.chain->links.size() != after.chain->links.size();
    }
    return std::any_of(events.begin(), events.end(), [&](const GameEvent &event) { return event.kind == rule.event; });
}

bool forced(const GameView &view, const Command &command) {
    if (!view.decision || view.decision->player != view.viewer || view.decision->kind == DecisionKind::Response)
        return false;
    if (const auto *choice = std::get_if<Choose>(&command)) return choice->decision == view.decision->id;
    const auto *prepare = std::get_if<PrepareCast>(&command);
    return prepare && view.decision->kind == DecisionKind::EffectPrepare && prepare->decision == view.decision->id;
}

bool defaultAdvanced(const Command &command) {
    return std::holds_alternative<AdvancePhase>(command) || std::holds_alternative<Advance>(command);
}
} // namespace

LessonDefinition::LessonDefinition(TutorialScenario scenarioValue, int orderValue, std::string title,
                                   std::shared_ptr<const detail::LessonProgram> program)
    : scenario(std::move(scenarioValue)), order(orderValue), titleKey(std::move(title)), program_(std::move(program)) {}

LessonCatalog LessonCatalog::load(const std::filesystem::path &directory, const CardCatalog &cards,
                                 const TextCatalog &text, const TextCatalog::Diagnostic &diagnostic) {
    LessonCatalog catalog;
    const auto log = [&](const std::string &message) {
        if (diagnostic) diagnostic(message);
        else std::clog << message << '\n';
    };
    std::error_code error;
    if (directory.empty() || !std::filesystem::exists(directory, error)) {
        if (error) log("tutorial lesson directory " + directory.u8string() + ": " + error.message());
        return catalog;
    }
    std::vector<std::filesystem::path> paths;
    for (auto it = std::filesystem::directory_iterator(directory, error); !error && it != std::filesystem::directory_iterator();
         it.increment(error)) {
        if (it->path().extension() == ".json") paths.push_back(it->path());
    }
    if (error) log("tutorial lesson directory " + directory.u8string() + ": " + error.message());
    std::sort(paths.begin(), paths.end());
    std::map<int, std::size_t> orders;
    for (const auto &path : paths) {
        try {
            auto scenario = TutorialScenario::load(directory, path.stem().u8string(), cards);
            const auto &document = scenario.document();
            fields(document, {"id", "order", "titleKey", "cardRevision", "scenario", "steps", "opponent", "complete", "openingOpponent"});
            const auto order = static_cast<int>(integer(document.at("order"), 1, std::numeric_limits<int>::max(), "order"));
            auto title = document.at("titleKey").get<std::string>();
            requireText(text, cards, title);
            auto program = parseProgram(document, cards, text);
            catalog.lessons_.push_back(LessonDefinition(std::move(scenario), order, std::move(title), std::move(program)));
            ++orders[order];
        } catch (const std::exception &error) {
            log("tutorial lesson " + path.filename().u8string() + ": " + error.what());
        }
    }
    auto &lessons = catalog.lessons_;
    lessons.erase(std::remove_if(lessons.begin(), lessons.end(), [&](const LessonDefinition &lesson) {
        if (orders.at(lesson.order) == 1) return false;
        log("tutorial lesson " + lesson.scenario.id() + ".json: duplicate order " + std::to_string(lesson.order));
        return true;
    }), lessons.end());
    std::sort(lessons.begin(), lessons.end(), [](const LessonDefinition &a, const LessonDefinition &b) {
        return a.order < b.order;
    });
    return catalog;
}

const LessonDefinition *LessonCatalog::find(std::string_view id) const {
    const auto it = std::find_if(lessons_.begin(), lessons_.end(), [&](const LessonDefinition &lesson) {
        return lesson.scenario.id() == id;
    });
    return it == lessons_.end() ? nullptr : &*it;
}

LessonScript::LessonScript(const LessonDefinition &definition)
    : lessonId_(definition.scenario.id()), program_(definition.program_), opponentUsed_(program_->opponent.size()) {}

bool LessonScript::allows(const GameView &before, const Command &command) const {
    if (std::holds_alternative<Surrender>(command) || defaultAdvanced(command) || forced(before, command)) return true;
    if (complete_ || terminal_) return false;
    const auto &allow = program_->steps.at(step_).allow;
    return std::any_of(allow.begin(), allow.end(), [&](const CommandPattern &pattern) {
        return matches(pattern, before, before.viewer, command);
    });
}

void LessonScript::advance(std::size_t next) {
    if (next == finish || (next == program_->steps.size() && !program_->completion)) complete_ = true;
    else if (next == program_->steps.size()) awaitingCompletion_ = true;
    else step_ = next;
}

void LessonScript::observe(PlayerId actor, const Command &command, const GameView &before, const GameView &after,
                           const std::vector<GameEvent> &events) {
    if (complete_ || terminal_) return;
    terminal_ = after.result != -1;
    if (std::holds_alternative<Surrender>(command) || std::holds_alternative<Timeout>(command)) return;
    if (actor == 0 && program_->learnerWinsOpening)
        if (const auto *gesture = std::get_if<SubmitOpeningGesture>(&command)) learnerGesture_ = *gesture;
    if (actor == 1) {
        for (std::size_t index = 0; index < program_->opponent.size(); ++index) {
            const auto &action = program_->opponent[index];
            if (action.step == step_ && !opponentUsed_[index] && matches(action.context, before) &&
                matches(action.command, before, actor, command)) {
                opponentUsed_[index] = action.once;
                break;
            }
        }
    }
    const auto hit = [&](const Transition &rule) { return matches(rule, actor, command, before, after, events); };
    if (program_->completion && hit(*program_->completion)) {
        complete_ = true;
        return;
    }
    if (awaitingCompletion_) return;
    const auto &rules = program_->steps.at(step_).until;
    const auto rule = std::find_if(rules.begin(), rules.end(), hit);
    if (rule != rules.end()) advance(rule->next);
}

bool LessonScript::hasContinue() const {
    if (complete_ || terminal_ || awaitingCompletion_) return false;
    const auto &rules = program_->steps.at(step_).until;
    return std::any_of(rules.begin(), rules.end(), [](const Transition &rule) { return rule.onContinue; });
}

bool LessonScript::canContinue(const GameView &view) const {
    return view.result == -1 && hasContinue();
}

bool LessonScript::continueLesson(const GameView &view) {
    if (!canContinue(view)) return false;
    const auto &rules = program_->steps.at(step_).until;
    const auto rule = std::find_if(rules.begin(), rules.end(), [](const Transition &rule) { return rule.onContinue; });
    advance(rule->next);
    return true;
}

std::optional<ai::Selection> LessonScript::opponent(const GameView &view, const ai::CandidateSource &source) const {
    if (complete_ || terminal_ || hasContinue() || view.viewer != 1 || view.result != -1 || !source) return {};
    const bool scriptedGesture = program_->learnerWinsOpening && view.openingStage == OpeningStage::Gestures;
    if (scriptedGesture && (!learnerGesture_ || learnerGesture_->round != view.openingRound || !view.gestureConfirmed[0]))
        return {};
    const auto &plan = program_->opponent;
    if (scriptedGesture) {
        std::optional<ai::Selection> selection;
        const auto desired = (learnerGesture_->gesture + 2) % 3;
        source([&](const LegalAction &action) {
            const auto *gesture = std::get_if<SubmitOpeningGesture>(&action.command);
            if (!gesture || gesture->gesture != desired) return true;
            selection = ai::Selection{action.command, 0, 1};
            return false;
        });
        return selection;
    }

    std::vector<std::size_t> wanted;
    for (std::size_t index = 0; index < plan.size(); ++index) {
        const auto &action = plan[index];
        if (action.step == step_ && !opponentUsed_[index] && matches(action.context, view)) wanted.push_back(index);
    }
    std::optional<ai::Selection> selection;
    std::size_t best = wanted.size() + 2;
    source([&](const LegalAction &action) {
        const auto &command = action.command;
        const auto match = std::find_if(wanted.begin(), wanted.end(), [&](std::size_t index) {
            return matches(plan[index].command, view, 1, command);
        });
        std::size_t rank = best;
        if (match != wanted.end()) rank = static_cast<std::size_t>(match - wanted.begin());
        else if (defaultAdvanced(command) || std::holds_alternative<PassResponse>(command) ||
                 std::holds_alternative<SelectInitialFormation>(command) ||
                 std::holds_alternative<SubmitOpeningGesture>(command)) rank = wanted.size();
        else if (forced(view, command)) rank = wanted.size() + 1;
        if (rank < best) {
            best = rank;
            selection = ai::Selection{command, 0, 1};
        }
        return best != 0;
    });
    return selection;
}

Json LessonScript::snapshot() const {
    const auto &step = program_->steps.at(step_);
    return {{"lessonId", lessonId_}, {"step", step.id}, {"hintKey", step.hint}, {"params", step.params},
        {"highlights", step.highlights}, {"blockedHint", step.blockedHint}, {"hintAnchor", step.anchor},
        {"canContinue", hasContinue()}, {"complete", complete_}};
}

const std::string &LessonScript::blockedHint() const { return program_->steps.at(step_).blockedHint; }
} // namespace wizard::app
