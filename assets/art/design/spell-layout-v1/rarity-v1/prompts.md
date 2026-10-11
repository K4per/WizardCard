# 稀有度差分提示词

工具：内置 image_gen。参考：用户提供的古金酒红像素卡框。用户允许更改镶嵌宝石形状。

## 普通 / common

```text
Use case: precise-object-edit. Edit the provided card frame itself, do not redesign or recreate it. Change ONLY the inlaid gemstone interiors at EVERY existing gem location, including the large horizontal hexagonal rarity gem at right of middle metadata strip and every small jewel around header, illustration corners, sides, lower circular socket, bottom center and footer. Preserve each gem's existing shape, size, location and faceted pixel shading. Replace emerald gemstone coloration with TARGET below. Keep every non-gem detail invariant: exact card silhouette, burgundy leather, antique gold ornament and bezels, parchment texture, all borders, illustration aperture dimensions, blank text areas, top-right and bottom-left empty circular sockets, bottom-right ID plaque, alignment and pixel-art style. No added adornments, glow, rays, text, digits, symbols, illustrations, background, perspective or changes in gold metal color. Original transparent regions MUST remain real alpha transparency including outside card, large illustration window and both circular centers. Same portrait framing and resolution as input. One whole frame, not a sheet or comparison. TARGET: neutral WHITE / clear diamond, white highlights and neutral gray facets, no green or yellow tint
```

## 罕见 / uncommon

```text
Precise-object-edit of provided fantasy pixel-art card frame. User now allows GEM SHAPE changes to communicate rarity. Change ONLY the jewels and the main middle-right gemstone's immediate gold setting needed to fit its new shape. Preserve rest of frame: same exact burgundy leather and antique gold filigree, same empty parchment header, illustration window, type/speed row, effects box, empty top-right and bottom-left circles, bottom-right ID blank. Keep main gemstone within existing middle-row allocation, never intrude into illustration or rules panel. All smaller jewels recolor to same rarity but retain locations/sizes. No extra decorative motifs, words, text, numbers, illustrations, particles or background. Crisp square pixel art. Outside card, large illustration aperture, and both empty circular centers must remain true transparent alpha. One portrait whole frame same resolution and alignment as input. Target: GREEN emerald: MAIN gemstone at right of middle strip becomes a horizontal OVAL faceted green gem, smooth stepped oval silhouette; small accent gems all green
```

## 稀有 / rare

```text
Use case: precise-object-edit. Edit the provided card frame itself, do not redesign or recreate it. Change ONLY the inlaid gemstone interiors at EVERY existing gem location, including the large horizontal hexagonal rarity gem at right of middle metadata strip and every small jewel around header, illustration corners, sides, lower circular socket, bottom center and footer. Preserve each gem's existing shape, size, location and faceted pixel shading. Replace emerald gemstone coloration with TARGET below. Keep every non-gem detail invariant: exact card silhouette, burgundy leather, antique gold ornament and bezels, parchment texture, all borders, illustration aperture dimensions, blank text areas, top-right and bottom-left empty circular sockets, bottom-right ID plaque, alignment and pixel-art style. No added adornments, glow, rays, text, digits, symbols, illustrations, background, perspective or changes in gold metal color. Original transparent regions MUST remain real alpha transparency including outside card, large illustration window and both circular centers. Same portrait framing and resolution as input. One whole frame, not a sheet or comparison. TARGET: bright CYAN / aquamarine blue, ice blue highlights and deep teal-blue facets, clearly blue-cyan rather than emerald green
```

## 史诗 / epic

```text
Precise-object-edit of provided fantasy pixel-art card frame. User now allows GEM SHAPE changes to communicate rarity. Change ONLY the jewels and the main middle-right gemstone's immediate gold setting needed to fit its new shape. Preserve rest of frame: same exact burgundy leather and antique gold filigree, same empty parchment header, illustration window, type/speed row, effects box, empty top-right and bottom-left circles, bottom-right ID blank. Keep main gemstone within existing middle-row allocation, never intrude into illustration or rules panel. All smaller jewels recolor to same rarity but retain locations/sizes. No extra decorative motifs, words, text, numbers, illustrations, particles or background. Crisp square pixel art. Outside card, large illustration aperture, and both empty circular centers must remain true transparent alpha. One portrait whole frame same resolution and alignment as input. Target: PURPLE amethyst: MAIN gemstone at right of middle strip becomes a striking broad FOUR-POINT DIAMOND / rhombus, with distinct top, bottom, left, right vertices and layered sharp facets; small accent gems all purple
```

## 传说 / legendary

```text
Precise-object-edit of provided fantasy pixel-art card frame. User now allows GEM SHAPE changes to communicate rarity. Change ONLY the jewels and the main middle-right gemstone's immediate gold setting needed to fit its new shape. Preserve rest of frame: same exact burgundy leather and antique gold filigree, same empty parchment header, illustration window, type/speed row, effects box, empty top-right and bottom-left circles, bottom-right ID blank. Keep main gemstone within existing middle-row allocation, never intrude into illustration or rules panel. All smaller jewels recolor to same rarity but retain locations/sizes. No extra decorative motifs, words, text, numbers, illustrations, particles or background. Crisp square pixel art. Outside card, large illustration aperture, and both empty circular centers must remain true transparent alpha. One portrait whole frame same resolution and alignment as input. Target: GOLDEN YELLOW: MAIN gemstone at right of middle strip becomes a bright yellow OCTAGONAL SUN-CUT crystal, eight clearly defined stepped sides and many triangular radial facets, subtle double antique-gold bezel tightly surrounding gem; small accent gems golden yellow. No glow or flares
```

