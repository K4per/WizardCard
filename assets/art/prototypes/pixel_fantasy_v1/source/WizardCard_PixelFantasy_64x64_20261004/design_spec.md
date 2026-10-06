# WizardCard Pixel Fantasy v1 - Design Spec

> Human-readable design narrative. Machine-readable contract: spec_lock.md.

## I. Project Information
- Project Name: 巫师牌像素奇幻试制 v1
- Canvas Size: per-asset specifications below
- Asset Count: 6 images, 2 UI boards and 4 card illustrations
- Art Style: Modern Pixel, outlined, stepped cel shading, fantasy brass and runes
- Target Platform: existing C++ / SFML client; this batch is artwork only
- Color Palette: Arcane Brass 24
- Created Date: 2026-10-04
- User requirements: pixel art and fantasy UI. Detailed values are draft production assumptions, not individually user-confirmed.

## II. Canvas Specification
- match_ui_concept: request 1600x1000 opaque, current horizontal-card layout.
- fantasy_ui_kit: request 1536x1024 transparent, exploratory component board, not a validated atlas.
- balance, fireball, barbs, ring: request 1024x768 opaque each; equivalent pixel clusters around 256x192.
- Future icon base size: 64x64; validate at 24–32 logical pixels later.
- Actual dimensions must be measured; preserve generated originals without hidden resampling.

## III. Color Palette
Arcane Brass 24:
#101320 #191D2B #242B3D #354057 #44352B #6D5035 #9B7448 #C49A5A #E0BE7C #F2DBA0 #174A4B #247475 #3CA6A0 #72D7C5 #B6F0DD #68778E #A4B4CB #DCE6EB #703244 #B74C60 #ED8A78 #B54E2A #ED8E3E #FFD181
Dark slate, aged brass, teal magic, silver words, coral life and orange fire. Exact palette compliance is a generation target; measure violations and retain prototype status.

## IV. Art Style Definition
Outlined modern pixel art, 3–5 stepped tones per material, sparse ordered texture, light from top-left with emissive magic cores. Brass corner fittings and restrained rune details on dark slate. Low-contrast content interiors. No smooth glass, photographic paint or blurred bloom.

## V. Asset List
### Characters
None.
### Tiles
None.
### Items
balance: stable symmetric multi-ring formation.
fireball: large concentrated directional fireball.
barbs: silver sharp spell-words interrupt a preparing spell; do not destroy or steal it.
ring: single clear circular rune with circulating energy, no host-specific object.
### UI
match_ui_concept: retain two player areas, five public columns and horizontal strips, bottom hand and right action area.
fantasy_ui_kit: panel, card strips, button states, four resource silhouettes; no rules or text baked into pieces.
### Effects
None.
### Backgrounds
No standalone background in this batch.

## VI. Animation Specification
Static; no animation frames.

## VII. Technical Constraints
- Card art has no card name, numbers, rules, costs or rarity.
- Mana uses crystal; load uses a burden weight; capacity uses an open supporting frame; life uses a heart core.
- Load equal to capacity is safe. Casting area has no fixed slot count.
- Original 196x52 field strips and 143x57 hand strips remain the layout basis; illustrations belong in future detail views.
- Text in mockup is illustrative and remains dynamic in production.
- Verify PNG dimensions, alpha, color count and exact-palette match. Unmet constraints remain pending; do not call these production-ready sprites.

## VIII. Platform Export Notes
Save original PNGs, prompts, metadata and validation report. Future SFML integration needs nearest-neighbor sampling, actual atlas coordinates, nine-slice margins, card-ID mapping and runtime privacy checks. No client changes in this batch.
Source: built-in OpenAI imagegen; no external stock assets. Do not invent a third-party license or exclusive-rights claim.
