extends RefCounted

const ATLAS: Texture2D = preload("res://art/fx/damage-border.png")
const CELL := Vector2(64, 30)
const FRAMES := 18
# Row order is shared with tools/bake_damage_border.mjs.
const TYPES := [
	"acid", "cold", "fire", "lightning", "poison", "thunder", "radiant",
	"necrotic", "force", "psychic", "piercing", "bludgeoning", "slashing",
	"overload", "exhaustion", "mana_recovery", "life_recovery"
]
const COLORS := [
	Color("#80e642"), Color("#6bcfff"), Color("#ff772f"), Color("#b36eff"),
	Color("#92a38b"), Color("#8458bd"), Color("#ffda62"), Color("#686879"),
	Color("#ff3345"), Color("#f18cba"), Color("#e2edf5"), Color("#e9ebef"), Color("#f0f2fa"),
	Color("#ff7240"), Color("#727582"), Color("#8067df"), Color("#4eb877")
]

static func paint(surface: CanvasItem, region: Rect2, kind: String, progress: float) -> void:
	if progress < 0.0 or progress >= 1.0 or minf(region.size.x, region.size.y) <= 6.0:
		return
	var row: int = TYPES.find(kind)
	if row < 0:
		surface.draw_rect(region.grow(-2), Color(Color("e89191"), 0.7 * (1.0 - progress)), false, 4)
		return
	var opacity: float = (0.65 + 0.35 * smoothstep(0.0, 0.09, progress)) * (1.0 - smoothstep(0.65, 1.0, progress))
	var short_side: float = minf(region.size.x, region.size.y)
	var thickness: float = minf(short_side * 0.17, 34)
	var inset: float = minf(3.0, short_side * 0.025)
	var frame: Rect2 = region.grow(-inset)
	if kind == "thunder":
		# Reserve the displacement inside the original bounds; only the decoration moves.
		var shake: float = inset * (1.0 - progress) * 0.75
		frame.position += Vector2(sin(progress * 69), sin(progress * 91)) * shake
	var tint: Color = COLORS[row]
	var wash_width: float = minf(thickness * 0.4, 8.0)
	surface.draw_rect(frame.grow(-wash_width * 0.5), Color(tint, opacity * 0.17), false, wash_width)
	var rim_width: float = 2.0
	var rim_alpha: float = 0.85
	if kind == "force":
		var pulse: float = pow(maxf(0.0, cos((progress - 0.08) * TAU * 2.25)), 2.0)
		rim_width = minf(thickness, 4.0 + 5.0 * pulse)
		rim_alpha = 0.64 + 0.36 * pulse
	surface.draw_rect(frame.grow(-rim_width * 0.5), Color(tint, opacity * rim_alpha), false, rim_width)
	var corners: PackedVector2Array = PackedVector2Array([
		frame.position, Vector2(frame.end.x, frame.position.y), frame.end, Vector2(frame.position.x, frame.end.y)
	])
	# Rising bubbles, faces, smoke and hammers stay upright on all four edges.
	var upright: bool = kind in ["acid", "poison", "bludgeoning", "necrotic"]
	var modulation := PackedColorArray([Color(Color.WHITE, opacity)])
	var texture_size: Vector2 = ATLAS.get_size()
	for edge: int in range(4):
		var start: Vector2 = corners[edge]
		var span: Vector2 = corners[(edge + 1) % 4] - start
		var tangent: Vector2 = span.normalized()
		var inward: Vector2 = Vector2(-tangent.y, tangent.x)
		var count: int = maxi(1, floori(span.length() / maxf(2.0, thickness * 3.1)))
		var stride: float = span.length() / count
		var extent: float = minf(thickness, stride * CELL.y / CELL.x)
		var tile_width: float = extent * CELL.x / CELL.y
		for index: int in range(count):
			var center: Vector2 = start + tangent * ((index + 0.5) * stride) + inward * (extent * 0.5)
			var delay: float = float((index * 5 + edge * 3) % 7) * 0.012
			var tick: int = clampi(floori((progress - delay) / (1.0 - delay) * FRAMES), 0, FRAMES - 1)
			var horizontal: Vector2 = tangent * tile_width * 0.5
			var vertical: Vector2 = -inward * extent * 0.5
			var source := Rect2(Vector2(tick, row) * CELL, CELL)
			if upright:
				if edge % 2 == 0:
					horizontal = Vector2.RIGHT * tile_width * 0.5
					vertical = Vector2.DOWN * extent * 0.5
				else:
					# Crop the transparent side margins to fit an upright motif into the narrow band.
					source.position.x += 10
					source.size.x -= 20
					horizontal = Vector2.RIGHT * extent * 0.5
					vertical = Vector2.DOWN * extent * (CELL.y / source.size.x) * 0.5
			if index % 2 == 1:
				horizontal = -horizontal
			var points := PackedVector2Array([
				center - horizontal - vertical, center + horizontal - vertical,
				center + horizontal + vertical, center - horizontal + vertical
			])
			source = source.grow(-0.5)
			var uv := Rect2(source.position / texture_size, source.size / texture_size)
			var uvs := PackedVector2Array([
				uv.position, Vector2(uv.end.x, uv.position.y), uv.end, Vector2(uv.position.x, uv.end.y)
			])
			surface.draw_polygon(points, modulation, uvs, ATLAS)
