# Content, command codec and replay

`wizard_content` reads the explicit `assets/catalog.json` and each stable-ID file.
Manifest order is canonical. Duplicate IDs/files, traversal and mismatched IDs
fail the whole load. Legacy `cards.json` imports remain supported for fixtures.
Card field validation runs through `parseCatalog` after aggregation; no partial
catalog escapes. Content and file IO have no network or application dependency.

`wizard_codec` owns command encoding/decoding and integer range validation;
variant order is the existing protocol contract. `wizard_replay` owns local
sessions and accepted-command recordings. Replays validate content versions,
hashes and every command's digest. Existing format 2 records remain compatible
during the engineering phase; switching rules is a separate versioned change.

Edit one file under `assets/cards/<type>/<id>.json`; run
`python tools/catalog_source.py`, then `python tools/generate_card_catalog.py`.
Use `python tools/catalog_source.py --check` in CI. The aggregate is generated,
not a second editable source. Unlisted files do not enter the shipped catalog.

Tests: content validation, manifest rejection, all command round trips, replay
tampering, application save failure and deterministic network replays.
