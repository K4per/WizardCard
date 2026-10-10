# Godot frontend art

This directory is the committed runtime subset of the existing C pixel fantasy
collection. `assets.json` maps stable IDs to full `res://art/...` paths. Each
entry records its unchanged original file, SHA-256, and applicable geometry.
`authoringSource` records the SVG or raster source separately from the exported
PNG copied into this directory. The local source collection is not required to
run the game or to verify these files in CI.

The collection contains 242 files: native controls and icons, 20 native card
templates, one anonymous compact back, board layers and interaction overlays,
the C environment and logo, all 30 preserved card illustrations, and the Noto
Sans CJK SC font with its SIL Open Font License. Large AI detail card bodies and
animation sheets are excluded from this foundation subset.

## Stable IDs

| Purpose | ID |
| --- | --- |
| Chinese font | `font.cjk` |
| Font license | `font.cjk.license` |
| Card illustration | `illustration.<card-id>` |
| Native card frame | `c2d.card.<field/hand/builder/detail>.<analysis/talisman/incantation/formation/rune>` |
| Anonymous back | `c2d.card.back.compact` |
| Missing illustration | `c2d.card.missing-art` |
| Rarity | `c2d.rarity.<common/uncommon/rare/epic/legendary>` |
| Environment | `c2d.background.realm` |
| Logo | `c2d.brand.logo` |
| Base board layers | `c2d.board.background`, `c2d.board.main-surface`, `c2d.board.edge-overlay` |
| Ring / formation anchor | `c2d.zone.slot`, `c2d.zone.formation-anchor` |
| Primary button | `c2d.button.primary.<normal/hover/pressed/focus/disabled>` |
| Selection / target | `c2d.overlay.selected`, `c2d.overlay.target` |
| Legal action / recipe material | `c2d.overlay.legal`, `c2d.overlay.cost-candidate`, `c2d.overlay.cost-selected` |

Use the native frame for the target card size and its recorded `artRect` and
text geometry. Render card illustrations behind the transparent frame aperture.
Native components use nearest sampling; original illustrations use linear
sampling. Panels expose `nineSlice` margins in left, top, right, bottom order.
The environment and logo are unmodified AI raster art, visually matching the
C collection; exact 32-color claims apply only to the native assets.

For battle scenes, use the environment, cards, ring markings and small resource
indicators as the main composition. The availability of zone panels is not a
requirement to draw permanent panels around every region. Full card text and
logs belong in optional detail surfaces. Card types, card values and ring counts
must come from the current session projection; the art does not establish rules.
Both opponent hand cards and anonymous facedown cards use the same back.

## Reproducible synchronization

From the repository root:

```powershell
python tools/prepare_godot_frontend_assets.py
python tools/prepare_godot_frontend_assets.py --check
```

The first command requires the original collection under
`assets/art/godot-2d-v1`, verifies source hashes, and copies bytes without image
processing. It then writes the deterministic runtime manifest. The second
command requires only committed runtime files. It checks hashes, stable-ID and
file uniqueness, path confinement, PNG dimensions, nine-slice geometry, required
foundation IDs, and all 30 illustration entries. Neither command edits original
art or deletes files.

Original collection manifest:
`assets/art/godot-2d-v1/manifests/c-pixel-v1/manifest.json`.
Illustration provenance:
`assets/art/godot-2d-v1/manifests/c-pixel-v1/preserved-illustrations.json`.
The font license is copied verbatim into `fonts/OFL.txt`; art origin and license
statements are carried from the original collection rather than reinterpreted.
