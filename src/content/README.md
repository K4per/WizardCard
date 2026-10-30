# Content, command codec and replay

`wizard_content` reads the explicit `assets/catalog.json` and each stable-ID file.
Manifest order is canonical. Duplicate IDs/files, traversal and mismatched IDs
fail the whole load. Legacy `cards.json` imports remain supported for fixtures.
Card field validation runs through `parseCatalog` after aggregation; no partial
catalog escapes. Unknown fields are currently ignored by the compatibility
parser, including card and nested effect/response objects; update the parser,
tests and `docs/card-design-guide.md` together when adding a field. Content and
file IO have no network or application dependency.

`wizard_codec` owns command encoding/decoding and integer range validation;
command kinds use explicit stable identifiers. `wizard_replay` owns local
sessions and accepted-command recordings. Format 5 records declare their
content, command, and digest schemas and validate each accepted command. Format
4 import is restricted to the current 2.5.5-hotfix.1 rules and uses a dedicated
legacy digest adapter; it does not restore historical rule execution.

`assets/catalog.json` and its listed files under `assets/cards/<type>/<id>.json`
are the sole formal content sources. Edit one listed card file; run
`python tools/catalog_source.py`, then `python tools/generate_card_catalog.py`.
Use `python tools/catalog_source.py --check` in CI. The aggregate is generated,
not a second editable source. Unlisted files do not enter the generated catalog;
the CMake asset-tree install must also avoid retaining unlisted source files.
The complete current field inventory is in `docs/card-schema.md`.

`catalog.json.format` versions the manifest shape, not individual card JSON.
`rulesVersion` identifies game semantics and `cardSetVersion` identifies the
reviewed pool. The production loader accepts only rules 2.5.5 with mechanics
revision 9. A definition or metadata change changes the canonical content hash
used by replay compatibility. Replay format versions independently from the
rules implementation.
Developer card packs use the independently versioned format documented in
`docs/card-pack.md`. Player deck exchange uses `wizardcard-deck` version 1;
its public shape and validation behavior are also documented there. Local deck
storage keeps its own outer format 1 and draft formats 1, 2 and 3.

Tests: content validation, manifest rejection, all command round trips, replay
tampering, application save failure and deterministic network replays.
