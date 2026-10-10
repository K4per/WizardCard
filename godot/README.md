# Godot 2D Alpha v2 integration

This directory contains the native bridge and the first runnable 2D frontend:
menu, current official card library, persisted settings and a field showcase.
The showcase is explicitly not a playable match. Approved C pixel art and all
30 original illustrations are copied without editing into `art/`, with hashes
and geometry in `art/assets.json`. Old 2.5D layouts are not imported.

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
- `pages/`: independent menu, library, settings and field scenes. Card migration
  proposals are not used as the official library.
- `ui/`: shared pixel theme, art cache, card drawing/detail, decision panel and
  field canvas. Cards and rings on the battlefield are drawn by one CanvasItem;
  there are no per-card buttons, permanent log panels or boxed empty zones.
  Details appear on click; material decisions are modal and lock field input.
  Many formations are browsed by row, instead of shrinking all cards to fit.
- `fixtures/`: labeled display-only arrangements, separate from real sessions.
  Ring/slot counts are presentation inputs, not legality calculated in GDScript.

Decision signals carry exact string generation/revision/decision IDs; callbacks
from an older displayed decision are ignored. Clicking or dragging emits an
intent, without consuming a material, paying or mutating the projected board.
Actual match submission, response/return choices, network polling, FX and sound
integration follow the rule/session gates in dev.4–dev.7.

## Run and verify

After building the debug bridge, run the pinned editor with `--path godot` from
the repository root, or use `tools/run_godot_frontend.ps1`. `--assets <directory>`
selects the official content directory; `--user-data <directory>` isolates native
persistence. Development defaults to `user://alpha-v2-frontend`, separate from
the old client's data. Content is still read by native filesystem code; packing
it into a release distribution is a later export task.

`python tools/prepare_godot_frontend_assets.py --check` verifies committed art
without the uncommitted original collection. `ctest -R godot_` now also imports
the actual frontend and runs `tests/frontend_smoke.gd`: three sizes, 0/1/7/12
rings, five formations, real viewport mouse/Esc/drag input, hidden details,
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
