# Formal Card Definition Schema

This document describes the current formal card definition format used by
`assets/catalog.json` and its `assets/cards/<type>/<id>.json` entries. It is a
field inventory and validation contract, not a second source of card data.

## Sources and versions

- `catalog.json.format` is the manifest format. The current value is `1`.
- `catalog.json.metadata.rulesVersion` is `2.5.1` and identifies rule semantics.
- `catalog.json.metadata.cardSetVersion` is `2.5.1` and identifies the reviewed pool.
- `catalog.json.metadata.mechanicsRevision` is `4` and identifies parser/rule capability.
- `cards.json` is a generated compatibility aggregate. It is not an editing
  source; `python tools/catalog_source.py --check` detects drift.
- Individual card files currently have no embedded format-version field.
- The canonical content hash covers normalized metadata and card definitions;
  art and presentation resources are not part of that hash.
- Local `decks.json` has outer format `1`; draft formats `1`, `2`, and `3`
  are local persistence formats, not public deck interchange formats.
- Replay formats are currently selected by rules capability: `2` for legacy,
  `3` for Alpha v2, and `4` for Alpha v2.5.

## Card object

The card object is intentionally flat. The following are all current top-level
fields observed in the 78-card formal pool or supported by the parser:

| Group | Fields |
|---|---|
| Identity and presentation link | `id`, `number`, `illustrationId` |
| Card face | `name`, `rarity`, `text`, `flavor`, `tags` |
| Classification and timing | `type`, `level`, `rank`, `school`, `speed`, `duration`, `concentration`, `perpetual`, `immediate`, `responseOnly`, `concentrationUpkeep` |
| Formation and attachment rules | `body`, `capacity`, `income`, `rings`, `maxRank`, `baseEligible`, `baseAnalysisDiscount`, `quickRankOne`, `opponentDestroyProtected`, `recipe`, `traits`, `summonCondition` |
| Targets and costs | `target`, `targetCount`, `cost`, `castCost`, `dynamicCost`, `analysisTurns`, `burden`, `extraDiscard` |
| Attachment modifiers | `ringBonus`, `incomeBonus`, `refundSetting`, `refundCast`, `capacityBonus`, `rankBonus`, `castCostBonus`, `damageBonus` |
| Damage protection | `resistances`, `immunities` |
| Effects and triggers | `effects`, `onDraw`, `onPrepare`, `onMain`, `onCast`, `onEnd`, `onLeave`, `onMana` |
| Responses | `responses` |

Required identity/face fields are non-empty strings. Alpha v2 cards require a
unique six-digit `number` whose two-digit prefix matches the card type. Numeric
fields use the parser's integer safety range `0..1000`, except signed
`incomeBonus` (`-1000..1000`) and fields with more specific semantic checks.
IDs, numbers, effect/response IDs, definition references, and catalog paths are
unique where applicable. The parser reports the source path and field location
for validation failures and rejects the whole catalog rather than returning a
partial pool.

## Nested objects

Effect arrays (`effects` and trigger arrays) contain objects with:

```json
{"kind":"damage","amount":1,"damageType":"fire","recipient":"target"}
```

`kind` is required. `amount` defaults to zero for effects without a quantity;
`damageType` and `recipient` are validated enums and are interpreted only by
implemented rule effects.

Alpha v2.5.1 adds the validated `sique_*` effects for rune detachment,
filtered searches/recovery, the damage cap, alternate life payment, recycling,
optional draws, hand shuffling and negation. Their timing and the executable
`sique_detach_1/2/3`, `sique_cauldron_only`, `sique_requires_campfire` and
`sique_requires_cauldron` traits are documented in [sique-cards.md](sique-cards.md).
The series uses the existing fields and does not introduce an unvalidated
script or an arbitrary effect name.

Response entries contain a unique `id`, optional display `name`, `sources`,
`windows`, `target`, `effects`, `eventOwner`, `phases`, `requiresSource`, and
the declared response cost/target fields (`handCost`, `preloadedCost`,
`burden`, `extraDiscard`, `targetCount`). The `hand` source is recognized only
by legacy-compatible parsing and requires an explicit `handCost`; current
Alpha rules disallow hand responses. Sources, windows, targets, phases, and
nested effects are validated before a card is loaded.

Formation recipe entries contain `kind` and positive `amount`, with optional
`definition`, `school`, `readyOnly`, `distinctSchools`, and `fireDamage`.
Definition references must resolve to another catalog card; school and recipe
kind values are validated enums.

## Unknown fields

The compatibility parser currently ignores unknown fields on card, effect,
response, and recipe objects. This preserves loading of existing fixtures and
forward-compatible metadata, but means a misspelled field can be ignored.
Adding a rule field requires parser support, a rule regression, documentation,
and a content-hash/version review. Strict rejection of unknown fields requires
a new definition format version and migration review for existing formal and
fixture content.

