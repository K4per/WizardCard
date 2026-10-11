# Godot 2D Alpha v2 integration

This directory contains the native bridge and the runnable 2D frontend for
`2.0.0-rc.1`: menu, the 46-card library, persisted settings, deck
construction/editing, local match flows, replay and a field showcase. The
showcase remains a presentation fixture, while match pages use the native
session service. Approved C pixel art and preserved illustrations are copied
without editing into `art/`, with hashes and geometry in `art/assets.json`.
Old 2.5D layouts are not imported.

Pinned toolchain: Godot **4.7.2**, matching export templates, godot-cpp
**10.0.0-stable**, commit `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`.
See `toolchain-lock.json`. Headless/native tests need neither Godot nor SFML.

Configure with `-DWIZARD_BUILD_CLIENT=OFF -DWIZARD_BUILD_GODOT=ON` and
`-DGODOT_BIN=<pinned-editor>`. Use `GODOTCPP_TARGET=template_debug` and
`template_release` in separate build directories. Build `wizard_godot_bridge`
and `wizard_cli`, then run `ctest -R godot_ --output-on-failure`.
`FETCHCONTENT_SOURCE_DIR_GODOT_CPP` may point to a verified local checkout.

`WizardBridge` is a RefCounted handle. Snapshots contain only the viewer's
projection, legal actions, decisions and visible events. Session generations,
revisions, decision/chain/link IDs cross the boundary as exact strings.
Selection never pays; confirmation checks generation/revision again. Offline
replay verification is explicitly separate from player snapshots. GDScript
does not implement authoritative rules.

The isolated harness tests native loading, malformed DTOs, IDs, stale/duplicate
callbacks, release and full CLI/Godot command-sequence replay. Human IME/DPI,
visual/audio and physical LAN acceptance belong to the release-candidate gate.

## Frontend modules

- `app/`: page navigation and a thin native library/settings service. It does
  not start matches or interpret rules. Missing native/content failures are
  visible, without inventing substitute card data.
- `models/`: typed presentation resources. Hidden projection cards are stripped
  of identity before textures, tooltips, details and drag previews see them.
- `pages/`: independent menu, library, settings, deck list/editor, match,
  replay, rooms and field scenes. The official library reads the native
  46-card `assets/cards.json` catalogue. `data/card-previews-v2.json` contains
  32 display-only design previews and cannot enter native deck drafts.
- `ui/`: shared pixel theme, art cache, card drawing/detail, decision panel and
  field canvas. Cards and rings on the battlefield are drawn by one CanvasItem;
  there are no per-card buttons, permanent log panels or boxed empty zones.
  Details appear on click; material decisions are modal and lock field input.
  Formations start at the left of one large analysis boundary per side, with
  ordered ring positions and no visible grid. Wheel browsing keeps additional
  formations reachable. Hand cards overlap in a fan with lowered edges, extend
  on selection, and retract with right click or Escape. With no card selected,
  right click collapses or expands the whole hand; a footer button does the same.
- `fixtures/`: labeled display-only arrangements, separate from real sessions.
  Ring/slot counts are presentation inputs, not legality calculated in GDScript.

Decision signals carry exact string generation/revision/decision IDs; callbacks
from an older displayed decision are ignored. Clicking or dragging emits an
intent, without consuming a material, paying or mutating the projected board.
Actual match submission, response/return choices, network polling, FX and sound
integration follow the rule/session gates in dev.4–dev.7.

## Run and verify

Deck construction opens through the enabled main-menu entry. Its list/editor
preserve the v1.1 toolbar, three columns and pagination, with **40 main + 10
formation side cards**, separate tabs/counts, a six-column built grid (30 per
page), and a 3×3 pool with native search/combined filters. Cards use the original
full AI frames. New archive/panel/slot/badge/drop art is in `art/deck-v2/`, with
originals, prompts, 1080p exports and actual screenshots under
`assets/art/godot-2d-v2/deck-builder-1080p-v1/` at the repository root.

Right clicks and committed opposite-column drops adjust draft counts; Escape,
right click, focus loss and invalid drops leave counts unchanged. Native draft
format 2 stores main/side cards and initial formation independently. Opening a
legacy draft migrates only the in-memory edit until explicitly saved. Incomplete
drafts remain savable; failed writes preserve all edits. Unsaved leaving offers
save/discard/continue. Construction validation is separate from old match
legality: format-2 drafts cannot start legacy matches, and display-only 2.0
preview cards cannot enter drafts. `tests/deck_frontend_smoke.gd` verifies these
flows with a fresh isolated `--user-data`, `--assets` and `--output` directory;
add `--capture` for actual rendered screenshots.

The 2.0 illustrated preview catalogue is `data/card-previews-v2.json`, marked
`2.0.0-rc.1-preview` so it cannot be mistaken for the formal card-set version.
Source design
documents are preserved in `assets/card-designs/v2/` at the repository root;
`tools/import_card_previews_v2.py` regenerates presentation data from those
documents. Values, schools, summon conditions and effect text are shown in
details and searched in Chinese. Slash-separated cost notation remains as
supplied, and unspecified rarities/formation speeds remain blank. Sixteen cards
reuse old illustrations (including the renamed mend/hexagram/pentagram motifs);
sixteen new transparent PNGs and their ImageGen prompts/provenance live in
`assets/art/godot-2d-v2/card-illustrations-v2-20261011/`. Art is copied unchanged
to `art/illustrations-v2/`. This step imports art and catalogue display only;
authoritative 2.0 effects remain a separate rule-engine migration.

The settings page and shared controls now use refined C-style ImageGen art in
`art/ui-v2/`: gold-and-cyan panel borders, input frames, jewel sliders, and
on/off switches. Ordinary buttons reuse the generated menu skins. Original
images, 1080p exports, slicing metadata, prompts, and actual screenshots are in
`assets/art/godot-2d-v2/ui-components-1080p-v1/` at the repository root. Settings
behavior still uses the native persistence service.

Typography uses open-source Noto Sans CJK SC: real Black 900 menu lettering
(40px at 1080p), Bold 700 headings/actions/card names, and Regular body text.
Shared weights are defined in `ui/typography.gd` and `ui/pixel_theme.tres`.
Unmodified fonts and OFL notices are bundled under `art/fonts/`; pinned upstream
URLs, copyright and hashes live in `assets/art/fonts/noto-sans-cjk-sc/` at the
repository root. Menu glyph coverage and text/outline bounds are checked at
all three frontend sizes.

The field showcase uses overhead dungeon battle art in `art/battle-v3/` and
`art/battle-v5/`: large unified analysis inlays, shallow casting strips, framed
art-only spell thumbnails, HUD/load bars, a full card beside one information
box, context controls and a card-effect choice skin. V5 source images and
ImageGen prompts are in `assets/art/godot-2d-v2/battle-production-v5/` at the
repository root. Rotated hand cards use their actual transformed rectangles
for pointer input; the selected card is raised and drawn last. The analysis
floor extends beneath the hand to the bottom of the field. Casting strips use
9% of field height and artwork tiles are at most 80px at 1080p.
Analysis card positions use the floor border's inner safe area. Larger resource
icons and 20–26px Bold numbers, plus outlined Bold field labels, improve contrast
against the dungeon floor.
Enter through the menu's field preview link. Effect choices remain a labeled
showcase; real session command submission is unchanged. Integration evidence is
in `docs/reports/battle-layout-v7-integration.md` (repository root).

After building the debug bridge, run the pinned editor with `--path godot` from
the repository root, or use `tools/run_godot_frontend.ps1`. `--assets <directory>`
selects the official content directory; `--user-data <directory>` isolates native
persistence. Development defaults to `user://alpha-v2-frontend`, separate from
the old client's data. Content is still read by native filesystem code; packing
it into a release distribution is a later export task.

`python tools/prepare_godot_frontend_assets.py --check` verifies committed art
without the uncommitted original collection. `ctest -R godot_` now also imports
the actual frontend and runs `tests/frontend_smoke.gd`: three sizes, 0/1/7/12
rings, five formations, a twenty-card fan, whole-hand collapse/expand, real
viewport mouse/right-click/Esc/drag input, hidden details,
handover, modal selection, exact/stale IDs, Chinese search, long text and settings
save/reopen. Tests use a build-local user directory. To capture actual rendered
images, run that script with a graphics display and `--capture --output <folder>`;
headless success alone is not visual acceptance.

Cold headless discovery uses `--import --frame-delay 1000` and is verified in a
new directory on every `godot_cold_import` run. Immediate import-and-exit crashed
locally only on first extension discovery; delaying editor shutdown passed.
This matches the upstream documentation-generation race reported in
[Godot #111048](https://github.com/godotengine/godot/issues/111048). This is a
harness workaround, not a patch to the pinned engine or a retry that hides errors.
