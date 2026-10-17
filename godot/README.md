# Godot Alpha v2.5.0 frontend

This directory contains the native bridge and Godot 4.7.2 frontend for WizardCard
2.5.0. C++ owns rules, legal actions, decisions, privacy, clocks and replay;
GDScript displays viewer projections and submits intents.

The frontend includes menu, library, settings, deck construction, setup,
hotseat, AI, tutorial, LAN rooms, matches, replay and a display-only field
showcase. The official library reads the 45-card native catalogue. Historical
illustrated previews in `data/card-previews-v2.json` are display-only and cannot
enter playable decks.

## Toolchain and execution

The pinned engine is `4.7.2.stable.official.ed1daf0bf`; godot-cpp is pinned to
`507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`. See `toolchain-lock.json`.
Configure with `WIZARD_BUILD_CLIENT=OFF`, `WIZARD_BUILD_GODOT=ON` and
`GODOT_BIN=<pinned console executable>`. Use separate build directories for
`GODOTCPP_TARGET=template_debug` and `template_release`.

```powershell
cmake --build build/godot-debug --config Debug --parallel 4
ctest --test-dir build/godot-debug -C Debug --output-on-failure
<Godot console executable> --path godot -- --assets <assets directory> --user-data <isolated directory>
```

`tools/run_godot_frontend.ps1` is another development entry point. Native
content is loaded from the filesystem; `tools/package_godot.py` exports the
frontend and copies content and dependencies into a distribution directory.
Default Godot persistence is isolated under `user://alpha-v2-frontend`;
`--user-data` overrides it for native persistence tests.

## Editable scenes

Main page skeletons and shared controls are `.tscn` files. Headers, state bars,
menus, details and decision shells can be selected in the scene tree and
adjusted in the Inspector. Card lists, ring counts, legal actions and decision
candidates are filled from projections at runtime.

Open `pages/match_page.tscn` to edit the full-screen match shell. Its field is
`ui/field_canvas.tscn`; child Controls define analysis, casting, hand and pile
bounds. `FieldLayout` supplies their geometry by default. Enable
`Presentation > Scene Region Overrides` to author the child Control anchors
directly. To edit the field itself, open the shared scene or enable editable
children on its instance.

`ui/default_field_layout.tres` exposes formation row spacing, slot width ratio,
analysis insets, hand fan and field/pile positions. Changing these
parameters redraws the canvas and affects match and replay rendering. Use Make
Unique and save a separate resource for a scene-specific layout.

`ui/simple_battle_theme.tres` owns battle fonts, panel styles and buttons. Match
and replay scenes reference it directly; runtime initialization does not
replace it. `pages/field_greybox.tscn` is the simple layout prototype.

`FieldCanvas` has a tool preview using local silhouettes and exported ring/hand
counts. It does not initialize a native session, read player files, generate
input hitboxes or submit commands in the editor.

## Match interaction

Each player has five permanent vertical body slots. A formation occupies
consecutive slots equal to its projected body: body 2 spans two and body 4 spans
four. Its ring cells use the entire span; remaining slots stay empty. Opponent
slots, piles and casting cards are centrally
symmetric. The two casting tracks are compact and placed at the center.

The field fills the viewport. Own player status sits in the lower-left corner;
opponent status stays at the top. Turn, acting player and phase appear directly
above phase switching, to the right of own casting.
During Main it offers Cast or End; End skips Cast while retaining required
responses and end-of-turn decisions. Every choice comes from legal actions.

Hover displays non-modal card details. Clicking pins a card and shows one short
button per operation. Placement highlights projected legal formations; clicking
a formation submits that exact action. Return decisions highlight formations
and empty rings directly. Targets and materials are selected on the field.
Submission preserves exact string generation/revision IDs.
Available-card highlights come from `snapshot.actions[].source`: setting,
attachment and activation use gold; covering uses bright blue. A legal formation
setting action also lights the side deck. Both highlights can coexist. Attached
runes occupy inspectable small squares beside their host. Card costs at the
upper right use native effective values; analytic spells show setting/casting
costs, while talismans show only the casting cost and cover for free.
GDScript does not derive legality or pay costs. Unknown cards discard identity
before rendering, details and drag previews.

Escape cancels the current transient interaction first and opens the match menu
when idle. Settings, phase actions, replay and leaving are in that menu.
Side deck, ash and rule decisions share a bottom horizontal card tray replacing
the hand. Response choices use explicit confirm and pass buttons. Collapsing a
tray restores the hand while its rule decision remains pending; the hand button
reopens it. Mandatory decisions cannot be skipped. A compact hand toggle and
right click control the fan; rotated card hit tests use inverse transforms. Hand
cards remain inside the canvas at supported viewport sizes. Fan end cards have
matching bounds, hover lifts and straightens the card, and opponent backs use
the same size and style. Facedown cards show no name on the field; projected
identity controls whether their full details can be inspected.

Matches use simple color panels, borders and text around the shared compact card
face. Battlefield cards use the same illustrated card treatment as deck
construction; hidden or facedown projections render the anonymous cover.
Runtime card illustrations, including the Storm series and the small hexagram,
are stored together under `art/card/`. Collection trays for the side deck and
ash zone use the same illustrated face and open the same card detail panel.
Settings persist `arcane`, `slate` or `ember` and apply the same background
identifier in match and replay.

Both players privately choose a level-one formation from their own side deck
before simultaneous reveal. Local hotseat, AI and tutorial have no clock. LAN
clocks are authoritative: 300 seconds, +60 at own turn start, capped at 300.
Menus and settings do not pause a LAN match; disconnect freeze and reconnect
are managed by the host session. Card setting-condition text comes from the
native recipe through `settingConditions`.

Deck format 3 stores 40 main cards and 10 formation side cards without a fixed
initial choice. Legacy drafts remain readable and repairable; incomplete
drafts can be saved. Replay format 4 checks version and content hash and
explicitly rejects 2.0.0-rc.1 recordings.

## Verification

`ctest -R godot_` checks import, cold extension discovery, bridge boundaries,
full deterministic CLI/bridge replay, frontend, match, field and deck flows.
Extension users run serially to avoid Windows DLL hot-reload collisions.

Frontend checks cover 1280x720, 1600x1000 and 1920x1080, projected 0/1/7/12 ring
counts, all five formation lanes, a twenty-card hand, mouse/right-click/Escape,
drag intents, decision locks, hidden identities, settings and deck drafts.
Match checks exercise private opening, real viewport clicks and confirmation,
Escape/reselection/menu, replay, AI privacy, tutorial and rooms.
Card selection checks cover grouped operations, field targets, return rings and
stale contexts. Tray checks cover all three sizes, field visibility, private
collections, collapse without submission, and explicit response pass.

For actual rendered screenshots, run a smoke script without `--headless` and
append `-- --assets <directory> --user-data <isolated directory> --capture
--output <capture directory>`. Headless success alone is not visual acceptance.
The standalone field script accepts `--output` without `--capture`.

Cold imports use `--import --frame-delay 1000` so deferred extension docs finish
before exit, matching the upstream race in Godot #111048. This is a test harness
workaround rather than a change to the engine. Human DPI/IME and physical
multi-device LAN acceptance are recorded separately from automated results.

Art hashes and licenses remain in `art/assets.json`, `assets/art/` and the root
third-party notices. Typography uses unmodified OFL Noto Sans CJK SC fonts.
