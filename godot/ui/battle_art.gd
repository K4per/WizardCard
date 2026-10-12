@tool
class_name BattleArt
extends RefCounted

static var _regions: Dictionary = {}
static var _sheets: Dictionary[String, Texture2D] = {}
static var _textures: Dictionary[String, Texture2D] = {}

static func texture(id: String) -> Texture2D:
	if _textures.has(id):
		return _textures[id]
	if _regions.is_empty():
		for manifest: String in ["res://art/battle-v3/regions.json", "res://art/battle-v5/regions.json"]:
			var data: Variant = JSON.parse_string(FileAccess.get_file_as_string(manifest).trim_prefix("\ufeff"))
			if data is Dictionary:
				_regions.merge(data)
	if not _regions.has(id):
		push_error("Missing battle art: " + id)
		return null
	var entry: Dictionary = _regions[id]
	var path: String = str(entry["file"])
	if not _sheets.has(path):
		_sheets[path] = load(path) as Texture2D
	var pixels: Array = entry["region"]
	var atlas: AtlasTexture = AtlasTexture.new()
	atlas.atlas = _sheets[path]
	atlas.region = Rect2(float(pixels[0]), float(pixels[1]), float(pixels[2]), float(pixels[3]))
	atlas.filter_clip = true
	_textures[id] = atlas
	return atlas

static func button(control: Button) -> void:
	for state: String in ["normal", "hover", "pressed", "disabled"]:
		var image: Texture2D = texture("button_" + state)
		var style: StyleBoxTexture = StyleBoxTexture.new()
		style.texture = image
		for side: int in range(4):
			# These generated buttons contain complete ornaments: scale the whole
			# skin instead of treating large source corners as destination pixels.
			style.set_texture_margin(side, 0)
			style.set_content_margin(side, 12)
		control.add_theme_stylebox_override(StringName(state), style)
	var focus: StyleBoxFlat = StyleBoxFlat.new()
	focus.bg_color = Color.TRANSPARENT
	focus.border_color = Color("64cfe6")
	focus.set_border_width_all(2)
	control.add_theme_stylebox_override(&"focus", focus)
	control.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST

static func fitted(bounds: Rect2, image: Texture2D) -> Rect2:
	var factor: float = minf(bounds.size.x / image.get_width(), bounds.size.y / image.get_height())
	var dimensions: Vector2 = image.get_size() * factor
	return Rect2(bounds.get_center() - dimensions * 0.5, dimensions)
