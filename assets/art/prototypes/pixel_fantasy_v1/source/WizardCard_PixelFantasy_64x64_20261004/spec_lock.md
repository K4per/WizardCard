# Execution Lock

## status
- version: pixel_fantasy_v1
- stage: concept_prototype
- date: 2026-10-04
- user_requirements: pixel art; fantasy UI; attempt generation from docs
- detailed_parameters: draft production defaults, not individually user-confirmed
- runtime_integration: false

## canvas
- base_size: 64x64 (future individual icons only)
- format: PNG
- match_ui_concept: 1600x1000 requested; opaque
- fantasy_ui_kit: 1536x1024 requested; transparent
- balance: 1024x768 requested; opaque
- fireball: 1024x768 requested; opaque
- barbs: 1024x768 requested; opaque
- ring: 1024x768 requested; opaque
- actual_dimensions: measure output; preserve original

## palette
- name: Arcane Brass 24
- #101320
- #191D2B
- #242B3D
- #354057
- #44352B
- #6D5035
- #9B7448
- #C49A5A
- #E0BE7C
- #F2DBA0
- #174A4B
- #247475
- #3CA6A0
- #72D7C5
- #B6F0DD
- #68778E
- #A4B4CB
- #DCE6EB
- #703244
- #B74C60
- #ED8A78
- #B54E2A
- #ED8E3E
- #FFD181
- strict_palette: target; report measured violations, retain prototype status on failure

## per_sprite_budget
- max_colors: 24

## style
- sub_style: modern pixel; outlined; stepped cel shading
- outline_color: #101320
- light_direction: top-left, plus emissive magic
- materials: aged brass; dark slate; carved runes
- pixel_grid_target: visible square pixel clusters; 256x192 equivalent for card art
- shading: 3–5 discrete tones per material
- animation: none

## assets
- ui: [match_ui_concept, fantasy_ui_kit]
- items: [balance, fireball, barbs, ring]
- characters: []
- tiles: []
- effects: []
- backgrounds: []

## forbidden
- Smooth gradients, photographic shading, antialiased vector appearance
- Text, numbers, rarity frames, logos or watermark in card illustrations
- Vertical cards replacing existing horizontal strips
- Fixed slot count for casting zone
- Load == capacity shown as overload
- Destruction or theft of a spell in barbs
- Claiming concept boards are validated sprite atlases or integrated assets

## verification
- Measure dimensions, alpha, colors, exact-palette match
- Visually inspect subject, style and composition
- Save originals and prompts
- Explicitly record unmet production requirements
