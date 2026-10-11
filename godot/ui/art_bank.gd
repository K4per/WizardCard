class_name ArtBank
extends RefCounted

static var _entries: Dictionary[String, Dictionary] = {}
static var _textures: Dictionary[String, Texture2D] = {}
static var _illustration_order: Array[String] = []
const ILLUSTRATION_CACHE_LIMIT: int = 48

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
		if id.begins_with("illustration."):
			_illustration_order.erase(id)
			_illustration_order.append(id)
		return _textures[id]
	var entry: Dictionary = metadata(id)
	if entry.is_empty():
		push_error("Missing art ID: " + id)
		return null
	var resource: Texture2D = load(str(entry["file"])) as Texture2D
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
		while _illustration_order.size() > ILLUSTRATION_CACHE_LIMIT:
			_textures.erase(_illustration_order.pop_front())
	return resource

static func trim_illustrations() -> void:
	# Nodes retain the textures they still display; page changes release unused art.
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
	var result: Theme = load("res://ui/pixel_theme.tres") as Theme
	result = result.duplicate() as Theme
	result.set_stylebox(&"panel", &"PanelContainer", box("c2d.v2.ui.panel", 48))
	result.set_stylebox(&"panel", &"PopupMenu", box("c2d.v2.ui.panel", 26))
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		result.set_stylebox(StringName(state), &"Button", _outline() if state == "focus" else _button_box(state))
		result.set_stylebox(StringName(state), &"OptionButton", _outline() if state == "focus" else _field_box(state))
	result.set_stylebox(&"normal", &"LineEdit", _field_box("normal"))
	result.set_stylebox(&"focus", &"LineEdit", _outline())
	result.set_constant(&"arrow_margin", &"OptionButton", 24)
	result.set_constant(&"modulate_arrow", &"OptionButton", 1)
	# Give binary and navigation controls their own visual language instead of
	# inheriting the generic button surface.
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		var switch_style: StyleBox = _outline() if state == "focus" else StyleBoxEmpty.new()
		switch_style.set_content_margin_all(8)
		result.set_stylebox(StringName(state), &"CheckButton", switch_style)
		result.set_stylebox(StringName(state), &"TabBar", _field_box(state))
	result.set_icon(&"checked", &"CheckButton", texture("c2d.v2.ui.toggle-on"))
	result.set_icon(&"unchecked", &"CheckButton", texture("c2d.v2.ui.toggle-off"))
	result.set_icon(&"checked_disabled", &"CheckButton", texture("c2d.v2.ui.toggle-on"))
	result.set_icon(&"unchecked_disabled", &"CheckButton", texture("c2d.v2.ui.toggle-off"))
	result.set_icon(&"checked", &"CheckBox", texture("c2d.control.checked"))
	result.set_icon(&"unchecked", &"CheckBox", texture("c2d.control.unchecked"))
	for state: String in ["tab_selected", "tab_hovered", "tab_unselected", "tab_disabled", "tab_focus"]:
		var source: String = {"tab_selected": "pressed", "tab_hovered": "hover", "tab_unselected": "normal", "tab_disabled": "disabled", "tab_focus": "focus"}[state]
		result.set_stylebox(StringName(state), &"TabBar", _outline() if state == "tab_focus" else _field_box(source))
	result.set_stylebox(&"slider", &"HSlider", box("c2d.v2.ui.slider-track", 12))
	result.set_stylebox(&"grabber_area", &"HSlider", _progress(Color("167d93")))
	result.set_stylebox(&"grabber_area_highlight", &"HSlider", _progress(Color("32bdd0")))
	result.set_stylebox(&"focus", &"HSlider", _outline())
	for state: StringName in [&"grabber", &"grabber_highlight", &"grabber_disabled"]:
		result.set_icon(state, &"HSlider", texture("c2d.v2.ui.slider-knob"))
	for control: String in ["VScrollBar", "HScrollBar"]:
		result.set_stylebox(&"scroll", StringName(control), _track(Color("08131f"), 8))
		var scroll_handle: StyleBoxTexture = box("c2d.v2.ui.field", 4)
		result.set_stylebox(&"grabber", StringName(control), scroll_handle)
		result.set_stylebox(&"grabber_highlight", StringName(control), scroll_handle)
	result.set_stylebox(&"separator", &"HSeparator", _track(Color("a67c39"), 2))
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

static func _field_box(state: String) -> StyleBoxTexture:
	var style: StyleBoxTexture = box("c2d.v2.ui.field", 12)
	style.set_content_margin(SIDE_LEFT, 44)
	style.set_content_margin(SIDE_RIGHT, 44)
	if state == "hover" or state == "focus":
		style.modulate_color = Color(1.15, 1.2, 1.2)
	elif state == "pressed":
		style.modulate_color = Color(0.8, 0.9, 0.95)
	elif state == "disabled":
		style.modulate_color = Color(0.55, 0.6, 0.65)
	return style

static func _button_box(state: String) -> StyleBoxTexture:
	var style: StyleBoxTexture = box("c2d.v2.menu.button-" + ("normal" if state == "focus" else state), 12)
	for side: int in range(4):
		style.set_texture_margin(side, 0)
	style.set_content_margin(SIDE_LEFT, 32)
	style.set_content_margin(SIDE_RIGHT, 32)
	return style

static func _track(color: Color, thickness: float) -> StyleBoxFlat:
	var style: StyleBoxFlat = StyleBoxFlat.new()
	style.bg_color = color
	style.set_content_margin_all(thickness * 0.5)
	return style

static func primary(button: Button) -> void:
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		button.add_theme_stylebox_override(StringName(state), _outline() if state == "focus" else _button_box(state))
