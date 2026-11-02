#pragma once
#include "wizard/core/view.hpp"
#include "wizard/core/action_query.hpp"

namespace wizard {
// Cursors are local to one viewer. A private event never advances the other
// viewer's cursor; epoch changes invalidate cursors after a debug rewind.
struct EventBatch {
    std::uint64_t epoch{}, base{}, next{};
    bool reset{};
    std::vector<GameEvent> events;
};
// Local and authoritative network adapters share this projection-only interface.
// Accepted, non-pending mutations advance revision; pending/rejected ones do not.
// accepted && pending is the retained transport-queued result, not authority acceptance.
// tick must run independently of page navigation, animation and pause controls.
class Session {
  public:
    virtual ~Session() = default;
    virtual GameView viewFor(PlayerId viewer) const = 0;
    virtual MatchClockView clockView() const = 0;
    virtual CommandResult submit(PlayerId actor, const Command &) = 0;
    virtual std::uint64_t revision() const = 0;
    virtual bool started() const = 0;
    virtual bool blocked() const = 0;
    virtual void tick() = 0;
    virtual EventBatch eventsSince(PlayerId viewer, std::uint64_t epoch,
                                   std::uint64_t cursor) const = 0;
    virtual bool technicalPaused() const = 0;
    virtual ActionOptions queryActions(PlayerId viewer, std::uint64_t generation,
                                       std::uint64_t revision, const ActionIntent &) = 0;
};
} // namespace wizard
