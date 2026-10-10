#pragma once
#include "wizard/core/view.hpp"

namespace wizard {
// Local and authoritative network adapters share this projection-only interface.
// Accepted, non-pending mutations advance revision; pending/rejected ones do not.
// accepted && pending is the retained transport-queued result, not authority acceptance.
// tick must run independently of page navigation, animation and pause controls.
class Session {
  public:
    virtual ~Session() = default;
    virtual GameView viewFor(PlayerId viewer) const = 0;
    virtual CommandResult submit(PlayerId actor, const Command &) = 0;
    virtual std::uint64_t revision() const = 0;
    virtual bool started() const = 0;
    virtual bool blocked() const = 0;
    virtual void tick() = 0;
};
} // namespace wizard
