# 对战布局 V5 美术生成

使用内置 imagegen，参考 Godot 1080p 实际渲染的配色与像素风格。生成图保留原始透明通道，运行时使用 AtlasTexture 取图，不修改原图。

## 01-floor-inlays.png

Use case: stylized-concept.
Asset type: transparent PNG floor-structure overlay atlas for a top-down 2D fantasy card battlefield in Godot.
Input image 1 is STYLE REFERENCE ONLY: navy-blue stone dungeon, restrained antique gold and teal pixel art. Do not reproduce its UI, cards, text or row layout.
Generate TWO isolated, full-width floor inlay structures stacked vertically on a truly transparent background:
TOP, occupying about x=4%-96%, y=5%-72%: ONE large empty rectangular analysis area, about 3.4:1 width to height. A single continuous recessed architectural outline made of worn gold-bronze inlay and dark stone edging, tiny arcane corner markings, subtle teal rune highlights, faint circular arcane engravings only near corners. Entire center is transparent and completely open for free placement of spell artwork. No subdivisions, no rows, no columns, no individual card slots, no horizontal internal separators.
BOTTOM, occupying about x=4%-96%, y=82%-96%: ONE much shallower casting strip, about 14:1 width to height. Fine recessed stone/gold edging and a very faint arcane circle engraved at its center, not a raised brick or pedestal.
Both structures look physically embedded flush into a dungeon floor, pure orthographic top view, precisely horizontal rectangle edges, delicate low-contrast pixel texture. Decorations remain close to edges. They must not look like thick floating UI panels. Transparent interiors, transparent gap between assets, no floor background, no characters, no cards, no text, no labels, no perspective, no shadows outside asset edges. Maintain crisp pixel art and a navy/aged gold/teal palette. Production sprite atlas with generous separation and every edge fully visible.

## 02-spell-art-frames.png

Use case: stylized-concept.
Asset type: two-state square artwork-only spell frame atlas with real transparent background, for a Godot 2D card battle.
Input image is style reference only: crisp pixel art, antique gold filigree, dark navy stone and subtle teal magic.
Produce exactly TWO equal square hollow frames side-by-side horizontally, isolated with generous transparent margin and gap. Left frame NORMAL, right frame SELECTED. Each square takes about 40% of image width. Both share identical silhouette and thickness.
The frame is a delicate antique gold border (only about 3% of square width) with tiny dark stone bevel and small arcane corner ornaments. Right frame has a slightly brighter gold border and fine soft teal outline to signal selection. Crisp square outer contour with only very small corner jewels. Entire large inner opening is completely transparent for displaying existing card illustrations. A dark narrow inner edge helps artwork read against a busy stone floor.
This is a square spell-art tile frame, not a full playing card. No title bars, no text boxes, no parchment, no cost circles, no numbers, no text, no artwork, no backing fill, no card silhouettes, no drop shadows, no illustrative background, no perspective. Minimal readable production game asset, matches supplied gold/blue fantasy dungeon pixel style. Keep transparent alpha everywhere except the borders.

