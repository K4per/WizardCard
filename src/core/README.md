# Rule kernel

Public consumers include `wizard/core/view.hpp` (projection and decisions),
`wizard/core/action_query.hpp` (incremental action options), `catalog.hpp`,
`commands.hpp` or `config.hpp`. `core.hpp` remains an umbrella for offline tools.
AI, interaction, presentation and
sound do not include `GameState`. No JSON, filesystem, sockets or rendering APIs
belong in this library.

`GameEngine::submit` plans without mutation, applies on a copy, advances the
continuation state machine and commits atomically after checking invariants.
Candidate generation is lazy; query and commit share the same validation path.
Internal modules:

- `engine.cpp`: construction, scenario entry and transactional submission.
- `planning.cpp`, `action_query.cpp`: shared read-only planning and paged query.
- `commands.cpp`, `current_commands.cpp`: decisions, current commands and preparations.
- `chain.cpp`: response validation, priorities and link finalization.
- `phases.cpp`, `current_phases.cpp`: bounded stepping, phase continuation and cleanup.
- `resources.cpp`: load, capacity, fees, targets and deck validity.
- `resource_events.cpp`: individual load increases, rules damage and draws.
- `zones.cpp`: leaving hosts, attachment cleanup and terminal checks.
- `effects.cpp`, `current_effects.cpp`: generic primitives and typed current abilities.
- `invariants.cpp`: offline state diagnostics.
- `view.cpp`, `legal_actions.cpp`, `serialization.cpp`: privacy projection,
  streamed candidates and deterministic fingerprints.
- `engine_internal.hpp`: private helpers, never included by clients.

Tests: `wizard_tests` rules, chain, card_updates and alpha_cards cases. For pure
kernel builds configure `cmake -S . -B build/core-only -DWIZARD_CORE_ONLY=ON`,
then build that directory. This configuration does not discover JSON, Catch2,
TCP or Godot. Whole headless regression builds use the normal headless preset.

The production catalogue accepts only rules version 2.5.5 and mechanics revision
9. `CardCatalog::compileAbilities()` binds its declared traits to the static
capability registry during loading; scenarios use the same compiled definitions.
There is no production ruleset selector or older rules executor. Format-4 replay
compatibility is limited to its digest adapter and does not restore historical
rule behavior.

Current rule tests cover effect-induced overload, cumulative failed draws,
end-phase checks, atomic commands and card behavior. Pass the gross increase after
each load mutation, rather than comparing the net load before and after unrelated
departures. Rule damage consumes temporary life without spell resistance. The
effect interpreter checks life defeat before the next effect; the phase pump
checks the doubled capacity threshold only after end cleanup.
