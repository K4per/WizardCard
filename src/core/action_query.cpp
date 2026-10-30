#include "engine_internal.hpp"
#include "wizard/core/action_query.hpp"
#include <set>
#include <type_traits>
#include <charconv>
#include <limits>
namespace wizard {
namespace detail {
using SelectionPath = std::vector<ActionSelection>;
SelectionPath actionPath(const Command &command) {
    SelectionPath path{{ActionField::Operation, std::to_string(static_cast<unsigned>(commandKind(command)))}};
    auto value = [&](ActionField field, auto number) { path.push_back({field, std::to_string(number)}); };
    std::visit([&](const auto &cmd) {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, Choose>) {
            path.push_back({ActionField::Choice, choiceKey(cmd.option)});
            if (cmd.ringIndex >= 0) value(ActionField::Ring, cmd.ringIndex);
        } else if constexpr (std::is_same_v<T, AdvancePhase>) {
            value(ActionField::Choice, cmd.skipCast ? 1 : 0);
        } else if constexpr (!std::is_same_v<T, Advance> && !std::is_same_v<T, PassResponse> &&
                             !std::is_same_v<T, Surrender> && !std::is_same_v<T, Timeout>) {
            value(ActionField::Source, cmd.card);
            if constexpr (std::is_same_v<T, SetFormation>) {
                value(ActionField::Replacement, cmd.replace);
                for (auto material : cmd.materials) value(ActionField::Material, material);
            } else if constexpr (std::is_same_v<T, AttachSeal>) value(ActionField::Host, cmd.host);
            else if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, SetAmbush> || std::is_same_v<T, PreloadWord>) value(ActionField::Host, cmd.formation);
            if constexpr (std::is_same_v<T, Respond> || std::is_same_v<T, ActivateAbility>) {
                path.push_back({ActionField::Ability, cmd.ability});
                if constexpr (std::is_same_v<T, Respond>) if (cmd.link) value(ActionField::Link, cmd.link);
            }
            if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, PreloadWord> || std::is_same_v<T, PrepareCast> ||
                          std::is_same_v<T, PlayAction> || std::is_same_v<T, Respond> || std::is_same_v<T, FlipAmbush> || std::is_same_v<T, ActivateSpell>) {
                if (!cmd.targets.empty()) for (auto target : cmd.targets) value(ActionField::Target, target);
                else if (cmd.target) value(ActionField::Target, cmd.target);
            } else if constexpr (std::is_same_v<T, ActivateAbility>) if (cmd.target) value(ActionField::Target, cmd.target);
            if constexpr (std::is_same_v<T, PreloadWord> || std::is_same_v<T, PrepareCast> || std::is_same_v<T, Respond> ||
                          std::is_same_v<T, FlipAmbush> || std::is_same_v<T, ActivateSpell>) if (cmd.discard) value(ActionField::Discard, cmd.discard);
        }
    }, command);
    return path;
}
bool matches(const SelectionPath &path, const SelectionPath &selection) {
    if (selection.size() > path.size()) return false;
    for (std::size_t i = 0; i < selection.size(); ++i)
        if (path[i].field != selection[i].field || path[i].value != selection[i].value) return false;
    return true;
}
bool intentAllows(const ActionIntent *intent, ActionField field, std::uint64_t value) {
    if (!intent) return true;
    if (field == ActionField::Source && intent->source && value != intent->source) return false;
    for (const auto &entry : intent->selections)
        if (entry.field == field && entry.value != std::to_string(value)) return false;
    return true;
}
std::vector<CardId> intentPrefix(const ActionIntent *intent, ActionField field) {
    std::vector<CardId> out;
    if (intent) for (const auto &entry : intent->selections)
        if (entry.field == field) {
            std::uint64_t value = 0;
            if (entry.value.empty()) return {0};
            for (char c : entry.value) {
                if (c < '0' || c > '9' || value > 0xffffffffULL / 10) return {0};
                value = value * 10 + static_cast<unsigned>(c - '0');
                if (value > 0xffffffffULL) return {0};
            }
            out.push_back(static_cast<CardId>(value));
        }
    return out;
}
bool matchesIntent(const Command &command, const ActionIntent *intent) {
    if (!intent) return true;
    const auto path = actionPath(command);
    if (intent->source) {
        const auto source = std::find_if(path.begin(), path.end(), [](const auto &entry) { return entry.field == ActionField::Source; });
        if (source == path.end() || source->value != std::to_string(intent->source)) return false;
    }
    return matches(path, intent->selections);
}
}
ActionOptions GameEngine::queryActions(PlayerId viewer, const ActionIntent &intent, const std::function<bool(const Command &)> &filter) const {
    ActionOptions out;
    if (viewer < 0 || viewer > 1) { out.error = "invalid_viewer"; return out; }
    if (!intent.limit || intent.limit > 256 || intent.selections.size() > 32) { out.error = "invalid_query"; return out; }
    for (const auto &entry : intent.selections) {
        if (entry.field == ActionField::Ability) continue;
        if (entry.field == ActionField::Choice && entry.value.find(':') != std::string::npos) continue;
        std::uint64_t value{};
        const auto converted = std::from_chars(entry.value.data(), entry.value.data() + entry.value.size(), value);
        if (converted.ec != std::errc{} || converted.ptr != entry.value.data() + entry.value.size()) {
            out.error = "invalid_query"; return out;
        }
        if ((entry.field != ActionField::Link && value > std::numeric_limits<CardId>::max()) ||
            (entry.field == ActionField::Operation && value > static_cast<unsigned>(CommandKind::ActivateAbility))) {
            out.error = "invalid_query"; return out;
        }
    }
    // Operation/source menus ask only whether a completion exists, and stop on the
    // first witness. They do not expand every material/target combination.
    if (intent.selections.empty()) {
        for (unsigned kind = 0; kind <= static_cast<unsigned>(CommandKind::ActivateAbility); ++kind) {
            ActionIntent probe; probe.source = intent.source; probe.selections.push_back({ActionField::Operation, std::to_string(kind)});
            std::optional<LegalAction> witness;
            visitLegalActions(viewer, [&](const LegalAction &action) {
                if (static_cast<unsigned>(commandKind(action.command)) != kind || (filter && !filter(action.command))) return true;
                witness = action; return false;
            }, &probe);
            if (witness) out.options.push_back({std::to_string(kind), witness->label});
        }
        out.field = ActionField::Operation;
        const auto begin = std::min(intent.offset, out.options.size());
        const auto end = std::min(out.options.size(), begin + intent.limit);
        out.hasMore = end < out.options.size();
        out.options = std::vector<ActionOption>(out.options.begin() + begin, out.options.begin() + end);
        return out;
    }
    if (intent.selections.size() == 1 && intent.selections.front().field == ActionField::Operation) {
        const auto operation = intent.selections.front().value;
        for (const auto &[id, card] : state_.cards) {
            if (controllerOf(card) != viewer) continue;
            ActionIntent probe = intent; probe.selections.push_back({ActionField::Source, std::to_string(id)});
            std::optional<LegalAction> witness;
            visitLegalActions(viewer, [&](const LegalAction &action) {
                const auto path = detail::actionPath(action.command);
                if (!detail::matches(path, probe.selections) || (filter && !filter(action.command))) return true;
                witness = action; return false;
            }, &probe);
            if (witness) out.options.push_back({std::to_string(id), catalog().at(card.definition).name, id});
        }
        if (!out.options.empty()) {
            out.field = ActionField::Source;
            const auto begin = std::min(intent.offset, out.options.size());
            const auto end = std::min(out.options.size(), begin + intent.limit);
            out.hasMore = end < out.options.size();
            out.options = std::vector<ActionOption>(out.options.begin() + begin, out.options.begin() + end);
            return out;
        }
    }
    std::optional<LegalAction> first;
    visitLegalActions(viewer, [&](const LegalAction &action) {
        if (filter && !filter(action.command)) return true;
        first = action; return false;
    }, &intent);
    if (first) {
        const auto path = detail::actionPath(first->command);
        if (path.size() == intent.selections.size()) {
            out.complete = first->command; out.field = ActionField::Confirm; return out;
        }
        out.field = path[intent.selections.size()].field;
        std::vector<std::string> candidates;
        auto add = [&](auto value) { candidates.push_back(std::to_string(value)); };
        if (out.field == ActionField::Ability) {
            for (const auto &[id, card] : state_.cards) {
                if (!detail::intentAllows(&intent, ActionField::Source, id)) continue;
                candidates.push_back("__activate_spell");
                for (const auto &ability : catalog().at(card.definition).responses) candidates.push_back(ability.id);
                candidates.push_back("storm_search"); candidates.push_back("storm_heart_choice");
                candidates.push_back("storm_annihilation_destroy");
            }
        } else if (out.field == ActionField::Link && state_.chain) {
            for (const auto &link : state_.chain->links) add(link.id);
        } else if (out.field == ActionField::Ring) {
            for (const auto &[id, card] : state_.cards) if (card.owner == viewer && card.zone == Zone::Analysis &&
                catalog().at(card.definition).type == CardType::Formation)
                for (int ring = 0; ring < Rules::rings(state_, catalog(), id); ++ring) add(ring);
        } else if (out.field == ActionField::Choice) {
            if (state_.decision) {
                for (auto option : state_.decision->options) candidates.push_back(detail::choiceKey(option));
                candidates.push_back(detail::choiceKey({}));
            } else { add(0); add(1); }
        } else {
            add(0);
            for (const auto &[id, card] : state_.cards) add(id);
            if (out.field == ActionField::Target) { add(playerTarget(0)); add(playerTarget(1)); }
        }
        // Each candidate asks for one completion witness. Remaining targets or
        // materials are lazy suffixes, so pagination never expands their product.
        std::set<std::string> seen;
        std::size_t skipped = 0;
        for (const auto &value : candidates) {
            if (!seen.insert(value).second) continue;
            auto probe = intent; probe.selections.push_back({out.field, value});
            std::optional<LegalAction> witness;
            visitLegalActions(viewer, [&](const LegalAction &action) {
                if (filter && !filter(action.command)) return true;
                witness = action; return false;
            }, &probe);
            if (!witness) continue;
            if (skipped++ < intent.offset) continue;
            if (out.options.size() == intent.limit) { out.hasMore = true; break; }
            ActionOption option{value, witness->label};
            if (out.field == ActionField::Source || out.field == ActionField::Host || out.field == ActionField::Replacement ||
                out.field == ActionField::Material || out.field == ActionField::Target || out.field == ActionField::Discard) {
                const auto id = static_cast<CardId>(std::stoull(value));
                if (id == playerTarget(0) || id == playerTarget(1)) option.player = static_cast<PlayerId>(id - playerTarget(0));
                else if (state_.cards.count(id)) option.card = id;
            }
            if (out.field == ActionField::Choice && state_.decision) {
                const auto selected = std::get_if<Choose>(&witness->command);
                if (selected && selected->option.kind == ChoiceKind::Card) option.card = static_cast<CardId>(selected->option.value);
                if (selected && selected->option.kind == ChoiceKind::Player) option.player = static_cast<PlayerId>(selected->option.value);
            }
            out.options.push_back(std::move(option));
        }
    }
    if (!out.complete && out.options.empty()) out.error = "no_legal_options";
    return out;
}
ActionSummary GameEngine::actionSummary(PlayerId viewer) const {
    ActionSummary summary;
    if (viewer < 0 || viewer > 1 || state_.result != -1) return summary;
    // Safe fallback is known without materializing card action combinations.
    if (state_.decision && state_.decision->player == viewer && state_.decision->kind == DecisionKind::Response)
        summary.safeAdvance = PassResponse{state_.decision->id};
    else if (!state_.decision && state_.active == viewer && state_.phaseGate)
        summary.safeAdvance = AdvancePhase{state_.phaseGate};
    summary.actionable = summary.safeAdvance.has_value();
    visitLegalActions(viewer, [&](const LegalAction &action) {
        if (std::holds_alternative<Surrender>(action.command) || std::holds_alternative<PassResponse>(action.command)) return true;
        summary.actionable = true;
        if (!std::holds_alternative<AdvancePhase>(action.command) && !std::holds_alternative<Advance>(action.command)) {
            summary.hasNonPhaseAction = true; return false;
        }
        return true;
    });
    return summary;
}
} // namespace wizard
