# WizardCard art production v1

## baseline
- Date: 2026-10-04
- Rules: 0.2.2; client: 0.4.1-dev; card set: 0.1.0
- Scope: CARD-01..09, BOARD-01..08, OP-01..10 plus nine missing illustrations
- Existing balance/fireball/barbs/ring PNGs preserved byte-for-byte
- No per-turn action allowance; action region is a resolution area
- Details read-only at 1360,330,215,354; execution lives in floating action bar

## canvas
- Illustrations: request 1024x768 RGBA, actual dimensions measured, transparent isolated subject
- UI source: layered SVG and deterministic pixel drawing source
- UI raster: integer pixels, binary alpha, shared palette
- Board and flow sheets: 1600x1000 logical canvas
- Hand card: 101x118; short card: 125x44; field portrait: 90x112; detail: 215x354
- Icons: 32x32 native plus 64x64 nearest-neighbor export
- Card back: 101x118 and 39x41 opponent version

## palette
- #0A1119 #101923 #122029 #192D38 #263E49 #364E59
- #44352B #6D5035 #9B7448 #C49A5A #E0BE7C #F2DBA0
- #174A4B #247475 #3CA6A0 #72D7C5 #B6F0DD
- #68778E #A4B4CB #DCE6EB #E7E5D5
- #703244 #B74C60 #ED8A78 #B54E2A #ED8E3E #FFD181
- #AD91D8 #629ED2 #78BE98
- UI exact palette; generated illustrations are pixel-style, no strict quantization claim

## art style
- Pixel fantasy: dark slate, narrow aged-brass frames, restrained teal rune inlays
- Hard edged clusters, no background on card illustrations, no typography in art
- UI ornaments in corners, low contrast centers, all gameplay text drawn separately
- Symmetrical formations; simplest readable silhouette per card
- No scene, floor, scenery, cast shadow, vignette, painted checkerboard in card art

## illustration subjects
- recall: open silver-brass grimoire with three returning luminous knowledge pages, no healing symbols
- spark: small upright three-pronged orange spark, no spherical fireball or long tail
- mend: coral heart-shaped life core with one fissure stitched by gold/teal threads
- unravel: disassembling empty brass geometric formation, central void, no living victim
- disrupt: dense bronze burden weight disturbed by two jagged violet incoming ripples
- clarity: calm silver/teal central glyph releasing two dark knotted fragments, no healing crystal
- reservoir: symmetrical upright chalice-like open magical containment frame, ample empty interior
- conduit: symmetrical three converging teal energy channels feeding one small central crystal
- ward: open crescent ritual boundary with three upright aggressive orange flames, no shield outline

## validation
- Nine new illustrations checked for subject, transparency, intact silhouette, file validity
- UI rasters checked for binary alpha and palette adherence; labels excluded from runtime textures
- Flow sheets use real card definitions, dynamic text examples, never runtime background
- Preview includes all five types, initial/complex board, six decision kinds, all thirteen requested flows
- UI assets and design completion tracked separately from game implementation and human acceptance
