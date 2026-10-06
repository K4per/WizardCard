# Execution Lock — Pixel Fantasy v2

## status
- version: pixel_fantasy_v2
- stage: revised_card_prototypes
- source: user revision to pixel_fantasy_v1
- user_requirements: all four illustrations without backgrounds; simplify barbs; balance as a designed six-pointed hexagram like the Star of David; symmetrical ring design
- integration: false

## canvas
- format: RGBA PNG, real transparent exterior
- balance: 1024x768 requested, centered isolated symbol
- barbs: 1024x768 requested, centered isolated symbol
- ring: 1024x768 requested, centered isolated symbol
- fireball: retain original 4:3 composition and fireball, remove entire environment
- actual_dimensions: measure output; keep original generated pixels
- safe_margin: entire subject enclosed with transparent margin on all sides

## palette
- name: Arcane Brass direction palette
- colors: #101320 #191D2B #242B3D #354057 #44352B #6D5035 #9B7448 #C49A5A #E0BE7C #F2DBA0 #174A4B #247475 #3CA6A0 #72D7C5 #B6F0DD #68778E #A4B4CB #DCE6EB #703244 #B74C60 #ED8A78 #B54E2A #ED8E3E #FFD181
- strict_quantization: not claimed for AI-generated prototypes

## style
- modern pixel art, stepped outlines and discrete shading
- no antialiased vector look, no blurred bloom
- flat front elevation for balance and ring, symmetric shape and ornaments
- symmetric emissive lighting for balance and ring; this supersedes v1 directional light
- no scene, floor, pedestal, shadow, scenery, vignette, solid color or checkerboard backdrop
- no text, numbers, card frame, watermark or brand mark
- transparent negative spaces and empty centers

## assets
- balance: two interlocked equilateral triangles forming one regular six-pointed star, fine brass bands, six equal teal accents, empty central hexagon, sixfold balanced decoration
- barbs: one compact silver barbed spell-word/sound glyph, at most three simple sweeping angular strokes; remove orb, room and particle cloud
- ring: one complete circular arcane rune, exact front view, fourfold symmetric repeated ornaments and mirrored glyph arrangement, empty center, no asymmetrical flow arrows or orbiting props
- fireball: large orange flame core and diagonal tail from v1, isolated cutout with no background or floor reflection

## verification
- inspect all four actual PNG files visually
- measure dimensions, alpha extrema, transparent-pixel fraction and corner alpha
- verify empty background, complete silhouette and intended pattern
- preserve v1 and save revised files separately
- preserve prompts, provenance and metadata; no claim of runtime integration
