# WizardCard C Pixel v1 - Design Spec

## I. Project Information
Godot 2D full-frontend asset set, C clean fantasy pixel style selected by user. Production scope: STYLE-01..04, PAGE-01..12, CARD-01..07, BOARD-01..13, FX-01..07. Exact resource counts come from export manifest.

## II. Canvas Specification
1920x1080 primary, 1280x720 minimum, 1600x1000 alternate. Components support container reflow. Native icons64px, nine-slice128x96, four separate card display sizes. AI raster originals remain at returned resolution.

## III. Color Palette
The 32 explicit native colors and roles are locked in spec_lock.md. Dark navy low-contrast backgrounds, teal emphasis, bronze corners, cream readable areas; red danger, green legal, cyan selected, purple response. All states include geometry/text, not color alone.

## IV. Art Style Definition
Outlined, 2–3-tone top-left light, crisp stepped shapes. Native SVG source and PNG export; AI backgrounds/logo/back/detail body art generated separately, no false editable-layer claim.

## V. Asset List
Requirements-indexed manifest covers card bodies, compact templates, anonymous back, gem rarity, controls, board/HUD/chain/recipe/facedown interfaces, review pages, mask/sequence effects. Preserved 30 illustrations referenced by stable IDs.

## VI. Animation Specification
Reusable 64px masks,8 normal frames/4 reduced, movement curves and events. No rule prediction, cancellation doesn't imply success, facedown graphics cannot reveal identity.

## VII. Technical Constraints
All labels/numbers dynamic except WizardCard brand. Readability14px auxiliary/16px body minimum at720p. Scene review text is demonstration, not rules implementation. Latest horizontal host-to-analysis rows override prior vertical-lane mockup.

## VIII. Platform Export Notes
PNG nearest sampling with editable SVG for native assets. 9-slice margins and safe rects in manifest. First deliver resources, not Godot frontend integration; actual IME/DPI/input/network/privacy verification remains pending.
