class_name ArtBank
extends RefCounted

static var _entries: Dictionary[String, Dictionary] = {}
static var _textures: Dictionary[String, Texture2D] = {}
static var _illustration_order: Array[String] = []
static var _missing_logged: Dictionary[String, bool] = {}
static var _fallback: Texture2D
# Custom painters retain drawing commands rather than Texture2D resources.
# Keep every illustration used by the current page alive until navigation;
# evicting one while its drawing command remains can leave an empty art window.

static func load_manifest() -> void:
	if not _entries.is_empty():
		return
	var source: String = FileAccess.get_file_as_string("res://art/assets.json")
	var document: Variant = JSON.parse_string(source)
	if not document is Dictionary:
		push_error("Missing or invalid frontend art manifest")
		return
	var entries: Array = (document as Dictionary).get("assets", [])
	for value: Variant in entries:
		if value is Dictionary:
			var entry: Dictionary = value
			_entries[str(entry.get("id", ""))] = entry

static func metadata(id: String) -> Dictionary:
	load_manifest()
	return _entries.get(id, {})

static func texture(id: String) -> Texture2D:
	if _textures.has(id):
		return _textures[id]
	var entry: Dictionary = metadata(id)
	if entry.is_empty():
		return _fallback_texture(id + " -> <not in res://art/assets.json>")
	var resource: Texture2D = load(str(entry["file"])) as Texture2D
	if resource == null:
		return _fallback_texture(id + " -> " + str(entry.get("file", "")))
	if entry.has("region"):
		var bounds: Array = entry["region"]
		var atlas: AtlasTexture = AtlasTexture.new()
		atlas.atlas = resource
		atlas.region = Rect2(float(bounds[0]), float(bounds[1]), float(bounds[2]), float(bounds[3]))
		atlas.filter_clip = true
		resource = atlas
	_textures[id] = resource
	if id.begins_with("illustration."):
		_illustration_order.append(id)
	return resource

static func _fallback_texture(id: String) -> Texture2D:
	if not _missing_logged.has(id):
		_missing_logged[id] = true
		# A warning records the exact ID/path while keeping fixture-only cards from
		# turning every redraw into a hard engine error.  Formal catalogue checks
		# still fail when an entry is absent because they inspect the manifest.
		push_warning("Missing art resource: " + id)
	if _fallback != null:
		return _fallback
	var image: Image = Image.create(128, 128, false, Image.FORMAT_RGBA8)
	image.fill(Color("352b48"))
	for y: int in range(0, 128, 8):
		for x: int in range(0, 128, 8):
			if (int(x / 8) + int(y / 8)) % 2 == 0:
				image.fill_rect(Rect2i(x, y, 8, 8), Color("4d3c62"))
	# A conspicuous broken-image cross distinguishes this from published art.
	for pixel: int in range(20, 108):
		image.fill_rect(Rect2i(pixel - 2, pixel - 2, 5, 5), Color("ed8c9a"))
		image.fill_rect(Rect2i(125 - pixel, pixel - 2, 5, 5), Color("ed8c9a"))
	_fallback = ImageTexture.create_from_image(image)
	return _fallback

static func trim_illustrations() -> void:
	# Called after the old page leaves the tree, before the new page draws.
	for id: String in _illustration_order:
		_textures.erase(id)
	_illustration_order.clear()

static func box(id: String, padding: float = 16.0) -> StyleBoxTexture:
	var style: StyleBoxTexture = StyleBoxTexture.new()
	style.texture = texture(id)
	var margins: Array = metadata(id).get("nineSlice", [12, 12, 12, 12])
	for side: int in range(4):
		style.set_texture_margin(side, float(margins[side]))
		style.set_content_margin(side, padding)
	return style

static func make_theme() -> Theme:
	var result: Theme = DeckTheme.create()
	# Retain recognizable accessible toggle and slider affordances inside the
	# shared flat control hierarchy used by the battle and deck pages.
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		var switch_style: StyleBox = _outline() if state == "focus" else StyleBoxEmpty.new()
		switch_style.set_content_margin_all(8)
		result.set_stylebox(StringName(state), &"CheckButton", switch_style)
	result.set_icon(&"checked", &"CheckButton", texture("c2d.v2.ui.toggle-on"))
	result.set_icon(&"unchecked", &"CheckButton", texture("c2d.v2.ui.toggle-off"))
	result.set_icon(&"checked_disabled", &"CheckButton", texture("c2d.v2.ui.toggle-on"))
	result.set_icon(&"unchecked_disabled", &"CheckButton", texture("c2d.v2.ui.toggle-off"))
	result.set_icon(&"checked", &"CheckBox", texture("c2d.control.checked"))
	result.set_icon(&"unchecked", &"CheckBox", texture("c2d.control.unchecked"))
	for state: StringName in [&"tab_selected", &"tab_hovered", &"tab_unselected", &"tab_disabled", &"tab_focus"]:
		result.set_stylebox(state, &"TabBar", DeckTheme.box(DeckTheme.HOVER if state == &"tab_hovered" else DeckTheme.CONTROL, DeckTheme.ACCENT if state in [&"tab_selected", &"tab_focus"] else DeckTheme.BORDER, 8))
	result.set_stylebox(&"slider", &"HSlider", _track(DeckTheme.BACKGROUND, 4))
	result.set_stylebox(&"grabber_area", &"HSlider", _progress(DeckTheme.BORDER))
	result.set_stylebox(&"grabber_area_highlight", &"HSlider", _progress(DeckTheme.ACCENT))
	result.set_stylebox(&"focus", &"HSlider", _outline())
	for state: StringName in [&"grabber", &"grabber_highlight", &"grabber_disabled"]:
		result.set_icon(state, &"HSlider", texture("c2d.v2.ui.slider-knob"))
	result.set_stylebox(&"separator", &"HSeparator", _track(DeckTheme.BORDER, 1))
	return result

static func _outline() -> StyleBoxFlat:
	var style: StyleBoxFlat = StyleBoxFlat.new()
	style.bg_color = Color.TRANSPARENT
	style.border_color = Color("75ecf5")
	style.set_border_width_all(2)
	style.set_content_margin_all(8)
	return style

static func _progress(color: Color) -> StyleBoxLine:
	var style: StyleBoxLine = StyleBoxLine.new()
	style.color = color
	style.thickness = 4
	style.grow_begin = -24
	style.grow_end = -8
	return style

static func _track(color: Color, thickness: float) -> StyleBoxFlat:
	var style: StyleBoxFlat = StyleBoxFlat.new()
	style.bg_color = color
	style.set_content_margin_all(thickness * 0.5)
	return style

static func primary(button: Button) -> void:
	button.add_theme_stylebox_override(&"normal", DeckTheme.box(DeckTheme.CONTROL, DeckTheme.ACCENT, 8))
	button.add_theme_stylebox_override(&"hover", DeckTheme.box(DeckTheme.HOVER, DeckTheme.ACCENT, 8))
