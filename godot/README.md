# Godot 2D Alpha v2 integration

This directory currently contains the restored native bridge and verification
scripts. Player scenes are implemented in the subsequent frontend stages; old
2.5D layouts and the A art family are not imported. The approved C pixel assets
remain in `assets/art/godot-2d-v1` and original card illustrations are reused.

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

Cold headless discovery uses `--import --frame-delay 1000` and is verified in a
new directory on every `godot_cold_import` run. Immediate import-and-exit crashed
locally only on first extension discovery; delaying editor shutdown passed.
This matches the upstream documentation-generation race reported in
[Godot #111048](https://github.com/godotengine/godot/issues/111048). This is a
harness workaround, not a patch to the pinned engine or a retry that hides errors.
