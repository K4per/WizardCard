# Godot runtime art

The runtime manifest preserves all 212 card illustrations in `card/` and `cards/`. The 194 previously registered images keep their illustration IDs. The other 18 historical originals have IDs beginning with `illustration.historical.`, including 16 Qixing JPEGs. All 90 release cards have a registered illustration. The manifest also maps `branding.app.icon` to the original application icon and `c2d.v2.menu.logo` to the original main-menu banner with its 1040 × 543 crop. The remaining five entries are three Noto Sans CJK SC font weights and their two SIL license files.

The committed images include processed crops and transparent mattes. Use the SHA-256 of the runtime file when verifying a card. The source path records provenance and may point to a larger source atlas. To check the manifest, supported catalogues, image hashes, license files and any existing Godot import sidecars, run:

```sh
python tools/prepare_godot_frontend_assets.py --check
```

Without `--check` the tool can restore missing or altered files when the recorded source has the exact approved bytes. The committed file in Git supplies the approved bytes for processed images.

To inspect the actual compiled textures and art manifest in an extracted player package, run:

```sh
python tools/verify_godot_pck_art.py /path/to/extracted/WizardCard.pck
```
