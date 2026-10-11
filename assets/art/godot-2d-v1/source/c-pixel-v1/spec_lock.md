# Execution Lock — WizardCard C Pixel v1

## authority
- User confirmed C clean pixel fantasy and requested one complete asset set against docs/art-resource-requirements-godot-2d.md.
- Latest layout: five horizontal analysis rows, each left formation anchor and right uninterrupted analysis lane; side deck ABOVE main deck at left; ash at lower right. Rows are repeatable, not rule capacity.
- User steering: card bodies MUST reference previous hand-drawn card template, not the C concept's arbitrary frame layout.

## canvas
- Platform: Godot 2D; no runtime integration in this asset delivery.
- Screen design 1920x1080; responsive checks 1280x720 and 1600x1000.
- Native icons 64x64; nine-slice panels 128x96; buttons 192x48; card templates hand 160x224, field 128x176, builder 224x304, detail 384x512; effects 64x64 frames, 8 normal / 4 reduced.
- AI card bodies portrait approximately 3:4; returned resolution recorded, never resize or recolor originals in scripts.
- Background AI wide 16:9 approximate; logical layout 1920x1080 via cover, no baked board geometry. AI logo and back returned dimensions recorded.
- AI-native PNGs preserve alpha; native SVG/PNG fully binary alpha.

## palette
- Native exact palette, AI visually matching (no claim of strict quantization):
- #08131f #101f2d #172f3d #214957 #2e606a #477b80 #7ba2a0 #b6d0c1
- #2c211d #493324 #715033 #9e7040 #c49855 #e6bd70 #f5db91 #fff0bc
- #524b3b #81765a #b6a37d #d8c6a0 #eddfbc #f6eed8 #ffffff #8995a2
- #246943 #3da965 #75d69a #318ba4 #64cfe6 #74528f #b183d9 #bd534d

## style
- C clean high quality 16-bit pixel fantasy: stepped dark outlines, 2–3 flat tones, teal/navy, bronze/gold corners, cream quiet reading surfaces, top-left light.
- No photorealism, 3D models, painterly blur, neon glow, old A UI reuse or baked card illustration/text.
- Native components editable SVG and reproducible generator; AI raster originals are raster sources, not falsely described as layered editable art.

## card-layout
- Name at top left, circular value top right; transparent large illustration aperture; below it type/speed left and rarity component right; broad effects area; circular value bottom left; identifier bottom right.
- Shared anonymous back for opponent hand and facedown analysis, no type leakage.
- Type: analysis, talisman, incantation, formation, rune. Main deck only non-formation; side deck only formation.
- Rarity: white simple crystal, green oval, cyan hexagon, purple diamond, GOLDEN YELLOW multifacet; component separate, no 5x30 baked composites.

## animation
- FX-01 movement curves: draw/place/flip/raise/return/discard, visual-only.
- FX-02..07: native reusable masks/8-frame sheets at 16fps and 4-frame reduced at 24fps; loops only analysis ongoing, no rules logic.
- Pixel alpha cannot smoothly fade inside sprites; apply opacity in engine; reduced mode short or instant, no camera movement.

## deliverables
- AI: menu/environment background, standalone title, anonymous back, five type detail frames, source/provenance prompts.
- Native: 5x4 card templates, separate rarity/type/speed/level/attributes/status icons, shared controls all five states, board layers and repeatable zones, interactions, all FX source and normal/reduced sheets.
- Twelve page groups and required error/decision/hidden states as composable HTML/SVG review screens with real existing card illustrations, not functional Godot pages.
- Stable IDs, source/export manifest, palette, anchors/slices, style/interaction/motion docs, preview gallery, ZIP and resource validation.
- Existing 30 illustration hashes preserved. No source code/rules changes or remote commits requested this turn.
