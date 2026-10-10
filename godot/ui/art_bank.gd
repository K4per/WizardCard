class_name ArtBank
extends RefCounted

static var _entries: Dictionary[String, Dictionary] = {}
static var _textures: Dictionary[String, Texture2D] = {}

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
		push_error("Missing art ID: " + id)
		return null
	var resource: Texture2D = load(str(entry["file"])) as Texture2D
	_textures[id] = resource
	return resource

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
	result.set_stylebox(&"panel", &"PanelContainer", box("c2d.panel.surface", 24))
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		result.set_stylebox(StringName(state), &"Button", box("c2d.button.secondary." + state))
		result.set_stylebox(StringName(state), &"OptionButton", box("c2d.control.dropdown." + state))
	result.set_stylebox(&"normal", &"LineEdit", box("c2d.control.input.normal"))
	result.set_stylebox(&"focus", &"LineEdit", box("c2d.control.input.focus"))
	return result

static func primary(button: Button) -> void:
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		button.add_theme_stylebox_override(StringName(state), box("c2d.button.primary." + state))
