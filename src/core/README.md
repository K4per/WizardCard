# Rule kernel

Public consumers include `wizard/core/view.hpp` (projection, actions and decisions),
`catalog.hpp`, `commands.hpp` or `config.hpp`. `core.hpp` remains a compatibility
umbrella for the old client and offline tools. AI, interaction, presentation and
sound do not include `GameState`. No JSON, filesystem, sockets or rendering APIs
belong in this library.

`GameEngine::submit` validates against a copy and commits atomically. Candidate
generation calls the same command validation. Internal modules:

- `engine.cpp`: construction, scenario entry and transactional submission.
- `commands.cpp`: ordinary commands and preparations.
- `chain.cpp`: response validation, priorities and link finalization.
- `phases.cpp`: phase advancement, queued effects and decisions.
- `resources.cpp`: load, capacity, fees, targets and deck validity.
- `resource_events.cpp`: individual load increases, rules damage and draws.
- `zones.cpp`: leaving hosts, attachment cleanup and terminal checks.
- `effects.cpp`: finite effect interpretation and confirmed events.
- `invariants.cpp`: offline state diagnostics.
- `view.cpp`, `legal_actions.cpp`, `serialization.cpp`: privacy projection,
  validated candidates and deterministic fingerprints.
- `engine_internal.hpp`: private helpers, never included by clients.

Tests: `wizard_tests` rules, chain, card_updates and alpha_cards cases. For pure
kernel builds configure `cmake -S . -B build/core-only -DWIZARD_CORE_ONLY=ON`,
then build that directory. This configuration does not discover JSON, Catch2,
TCP, Godot or SFML. Whole headless regression builds use the normal headless preset.

Alpha v2 resource scenarios live in `tests/alpha_v2_resources.cpp`. The temporary
`CardCatalog::alphaV2Draft` switch is set by explicit offline fixtures only;
the JSON loader cannot enable it and the normal match constructor rejects it.
It currently covers effect-induced overload, cumulative failed draws and final
end-phase overload checks. Payments, deck construction and zones still require
the rest of dev.3. Do not expose this partial rule mode in sessions or player UI.
Pass the gross increase after each load mutation, rather than comparing the net
load before and after unrelated departures. Rule damage consumes temporary life
without spell resistance. The effect interpreter checks life defeat before the
next effect; the phase pump checks the doubled capacity threshold only after
end cleanup. Legacy catalogues retain their original rules and fingerprints.
