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
