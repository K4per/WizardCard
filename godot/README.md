# Godot Alpha v2.6.2 frontend

This directory contains the native bridge and Godot 4.7.2 frontend for WizardCard
2.6.2. C++ owns rules, legal actions, decisions, privacy, clocks and replay;
GDScript displays viewer projections and submits intents.

The frontend includes menu, library, settings, deck construction, setup,
hotseat, AI, tutorial, LAN rooms, matches, replay and a display-only field
showcase. The official library reads the 92-card v2.6 native catalogue. Historical
illustrated previews in `data/card-previews-v2.json` are display-only and cannot
enter playable decks.

## Toolchain and execution

The pinned engine is `4.7.2.stable.official.ed1daf0bf`; godot-cpp is pinned to
`507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`. See `toolchain-lock.json`.
Configure with `WIZARD_BUILD_GODOT=ON` and
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
Windows Godot persistence is isolated under `user://alpha-v2-frontend`;
on Linux it is `$XDG_DATA_HOME/WizardCard`, falling back to
`$HOME/.local/share/WizardCard`. `--user-data` overrides either default.

Linux development uses Ninja and accepts the official engine or an Arch build
with the same `4.7.2`/`ed1daf0bf` version and commit:

```sh
cmake --preset linux-godot-debug -DGODOT_BIN="$(command -v godot)"
cmake --build --preset linux-godot-debug --parallel 4
ctest --preset linux-godot-debug
tools/run_godot_frontend.sh --editor-path godot \
  --assets assets/release-v260 --user-data build/linux-local-data
# Pass native display options after --:
tools/run_godot_frontend.sh --editor-path godot -- --display-driver wayland
```

The launcher imports before starting from a temporary working directory. Linux
bridges use the system C++ runtime (`GODOTCPP_USE_STATIC_CPP=OFF`), with separate
`linux-godot-release`/`template_release` output for exports. Build official release
archives on Ubuntu 24.04; Arch builds are for development and local acceptance.
Put matching official `linux_{debug,release}.x86_64` templates in
`.cache/godot-templates/templates/`, then export `Linux Desktop`. This preset
supports both X11 and native Wayland and writes `WizardCard.x86_64` plus an
external `WizardCard.pck`; `res://bin/libwizard_godot_bridge.release.so` stays
beside the export. See `docs/releases/2.6.0-hotfix.1-linux.md` for packaging.

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

All business pages use the shared greybox theme from `DeckTheme` through
`ArtBank.make_theme()`. It supplies panel styles, buttons, input fields, focus
and disabled states; existing slider and toggle icons remain. The
`ui/simple_battle_theme.tres` scene resource remains an editor fallback, while
the application supplies the shared theme at runtime.
`pages/field_greybox.tscn` is the simple layout prototype.

`FieldCanvas` has a tool preview using local silhouettes and exported ring/hand
counts. It does not initialize a native session, read player files, generate
input hitboxes or submit commands in the editor.

## Menu, settings and deck exchange

The main menu keeps the banner and seven equal primary
buttons in a single right-hand column: single player, multiplayer, deck
construction, card library, replay, settings and quit. Keyboard and controller
focus follow the same order and wrap at either end. The menu keeps its banner
and layout while background and button art are removed for the v2.6 greybox.

Settings offer 1280x720, 1600x900, 1440x900, 1600x1000, 1920x1080 and 2560x1440, preserve
saved custom sizes and support free resizing down to 1280x720. Font sizes are
90%, 100%, 110% and 120%, with 100% as the default. Save and apply updates the
current UI and persists `fontScale`; old settings without the field use 100%.
`GlobalTypography` scales inherited themes, explicit font sizes, popups, rich
text and controls created later. Custom card drawing fits text within the card;
full rules remain scrollable in details.

Deck construction has a player-facing Import action; the editor has Export.
Exchange files use the readable `wizardcard-deck` version 1 JSON format with a
name and sorted stable card ID/count entries in `main` and `side`. Import
validates the complete deck before saving: unknown cards, invalid sections,
counts, duplicates and unsupported versions produce explicit errors without
replacing the active draft. This public format is separate from the local
format 3 draft described below.

Adding, removing, rejecting and saving cards immediately updates counts and
result text, with short visual feedback. Card detail panels display `number`
while decks continue to persist stable `id` values. Card-pool rows adapt to
available height to keep cards useful in smaller windows.

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

Short projection-based borders show entering/leaving cards, zone and state
changes, damage and healing. They never move real cards or their hit regions,
delay commands or replay steps, or infer rule outcomes. Privacy changes clear
old outlines immediately. Reduced motion disables transient feedback while
state and result text update immediately.

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

Frontend and typography checks cover 1280x720, 1600x900, 1440x900, 1600x1000,
1920x1080 and 2560x1440. Typography exercises all four font sizes on the menu,
settings, deck editor, library, field and replay pages. Frontend checks cover
projected 0/1/7/12 ring
counts, all five formation lanes, a twenty-card hand, mouse/right-click/Escape,
drag intents, decision locks, hidden identities, settings and deck drafts.
Match checks exercise private opening, real viewport clicks and confirmation,
Escape/reselection/menu, replay, AI privacy, tutorial and rooms.
Card selection checks cover grouped operations, field targets, return rings and
stale contexts. Tray checks cover field visibility, private
collections, collapse without submission, and explicit response pass.

Additional standalone checks are `tests/deck_exchange_smoke.gd` for public deck
round trips and invalid imports, `tests/feedback_smoke.gd` for immediate state,
privacy and reduced-motion behavior, and `tests/network_frontend_smoke.gd` for
three loopback create/join/start/leave cycles. Native settings tests cover font
validation and persistence. Run these scripts serially after rebuilding and
importing the current extension.

`tests/art_residency_smoke.gd` checks the complete official illustration mapping,
rejects fallback textures, and verifies that more than 48 same-page illustrations
stay resident. Run without `--headless` and pass `--output <directory>` to capture
all official cards in both library and deck styles. `--assets` and `--user-data`
also accept a clean extracted release directory for package verification.

For actual rendered screenshots, run a smoke script without `--headless` and
append `-- --assets <directory> --user-data <isolated directory> --capture
--output <capture directory>`. Headless success alone is not visual acceptance.
The standalone field script accepts `--output` without `--capture`.
`tests/typography_smoke.gd` instead accepts `--capture-output <directory>` and
captures each checked page at 1280x720 with 120% text; its other size/font
combinations use geometry assertions.

Cold imports use `--import --frame-delay 1000` so deferred extension docs finish
before exit, matching the upstream race in Godot #111048. This is a test harness
workaround rather than a change to the engine. Human DPI/IME and physical
multi-device LAN acceptance are recorded separately from automated results.

Art hashes and licenses remain in `art/assets.json`, `assets/art/` and the root
third-party notices. Typography uses unmodified OFL Noto Sans CJK SC fonts.

## 便携版公网回归

`diagnostics/central_verification.tscn` 使用正常页面、服务和 TLS，供发布验收通过用户参数 `--verify-central` 显式启动。它不会替换产品的默认场景。

使用 `python tools/verify_windows_release.py --archive dist/WizardCard-2.6.2-windows-x64.zip --evidence build/v262/archive-checks --server-host aq.rainplay.cn --server-port 23513 --trusted-ca deploy/production/aq.rainplay.cn.crt` 验证最终归档。该工具从ZIP解压后直接运行两份便携版程序，核对清单与启动、开局、连续操作、主动重连、正常终局和两端复盘一致性。公网选项应仅指向已授权的测试或部署服务。
