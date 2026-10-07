class_name WizardSkin
extends RefCounted

const TITLE: int = 28
const SECTION: int = 20
const BODY: int = 18
const HELPER: int = 14
const CARD_STATE: int = 12
const GAP: int = 8

var directory: String = ""
var manifest: Dictionary = {}
var entries: Dictionary = {}
var palette: Dictionary = {}
var artwork: Dictionary = {}
var textures: Dictionary = {}
var errors: PackedStringArray = []
var font: FontFile = FontFile.new()
var theme: Theme = Theme.new()

func initialize(assets: String) -> void:
	directory = assets
	manifest = read_json(assets.path_join("art/ui/a-gilded-v2/manifest.json"))
	palette = manifest.get("palette", {}) as Dictionary
	for raw: Variant in manifest.get("assets", []) as Array:
		var entry: Dictionary = raw as Dictionary
		entries[str(entry["id"])] = entry
	var nonmodel: Dictionary = read_json(assets.path_join("art/nonmodel/a-gilded-v15/manifest.json"))
	for raw: Variant in nonmodel.get("assets", []) as Array:
		var entry: Dictionary = raw as Dictionary
		entries[str(entry["id"])] = entry
	artwork = read_json(assets.path_join("art/runtime.json")).get("cards", {}) as Dictionary
	if font.load_dynamic_font(assets.path_join("fonts/NotoSansCJKsc-Regular.otf")) != OK:
		errors.append("中文字体未加载")
	font.antialiasing = TextServer.FONT_ANTIALIASING_GRAY
	theme.default_font = font
	theme.default_font_size = BODY
	for kind: String in ["Label", "Button", "OptionButton", "CheckBox", "LineEdit", "SpinBox", "RichTextLabel"]:
		theme.set_color("font_color", kind, color("text"))
		theme.set_color("font_hover_color", kind, color("teal0"))
		theme.set_color("font_pressed_color", kind, color("teal0"))
		theme.set_color("font_disabled_color", kind, color("muted"))
	theme.set_color("default_color", "RichTextLabel", color("text"))
	for kind: String in ["Button", "OptionButton"]:
		for state: String in ["normal", "hover", "pressed", "disabled"]:
			var fill: Color = color("cream") if state == "normal" else color("panel")
			var edge: Color = color("teal1") if state in ["hover", "pressed"] else color("bronze1")
			var button_box: StyleBoxFlat = flat(fill, edge)
			button_box.content_margin_top = 4
			button_box.content_margin_bottom = 4
			theme.set_stylebox(state, kind, button_box)
		var focus: StyleBoxFlat = flat(Color.TRANSPARENT, color("teal1"))
		focus.set_border_width_all(2)
		theme.set_stylebox("focus", kind, focus)
	for role: String in ["PrimaryButton", "DangerButton"]:
		theme.set_type_variation(role, "Button")
		for state: String in ["normal", "hover", "pressed", "disabled"]:
			theme.set_stylebox(state, role, style("button_" + ("primary_" if role == "PrimaryButton" else "danger_") + state, 36, 8))
			theme.set_color("font_color", role, color("cream"))
			theme.set_color("font_hover_color", role, color("lightgold"))
			theme.set_color("font_pressed_color", role, color("cream"))
	for kind: String in ["LineEdit", "SpinBox"]:
		theme.set_color("font_placeholder_color", kind, Color("#765A43"))
		theme.set_stylebox("normal", kind, flat(color("cream"), color("bronze1")))
		theme.set_stylebox("focus", kind, flat(color("cream"), color("teal1")))
	theme.set_stylebox("panel", "PanelContainer", style("tooltip", 12, 12))
	theme.set_stylebox("panel", "PopupMenu", flat(color("panel"), color("bronze1")))
	theme.set_color("font_color", "PopupMenu", color("text"))
	theme.set_type_variation("MatchButton", "Button")
	for state: String in ["normal", "hover", "pressed", "disabled"]:
		theme.set_stylebox(state, "MatchButton", style("nm_hud_phase", 12, 4))
		theme.set_color("font_" + ("color" if state == "normal" else state + "_color"), "MatchButton", color("cream") if state != "disabled" else color("gray"))

static func read_json(path: String) -> Dictionary:
	var file: FileAccess = FileAccess.open(path, FileAccess.READ)
	if file == null:
		return {}
	var data: Variant = JSON.parse_string(file.get_as_text())
	return data as Dictionary if data is Dictionary else {}

func color(key: String) -> Color:
	return Color(str(palette.get(key, "#322324")))

func image(path: String) -> Texture2D:
	if textures.has(path):
		return textures[path] as Texture2D
	var raster: Image = Image.load_from_file(path)
	if raster == null or raster.is_empty():
		errors.append("素材未加载：" + path)
		return null
	raster.generate_mipmaps()
	var texture: ImageTexture = ImageTexture.create_from_image(raster)
	textures[path] = texture
	return texture

func texture(id: String) -> Texture2D:
	var entry: Dictionary = entries.get(id, {}) as Dictionary
	return image(directory.path_join("art").path_join(str(entry["file"]))) if entry.has("file") else null

func illustration(id: String) -> Texture2D:
	return image(directory.path_join("art").path_join(str(artwork[id]))) if artwork.has(id) else texture("missing_art")

func style(id: String, horizontal: float = 12, vertical: float = 10) -> StyleBox:
	var entry: Dictionary = entries.get(id, {}) as Dictionary
	if not id.begins_with("button_") and not id.begins_with("nm_hud_"):
		var simple: StyleBoxFlat = flat(color("panel"), color("bronze1"))
		for side: int in range(4):
			simple.set_content_margin(side, horizontal if side in [SIDE_LEFT, SIDE_RIGHT] else vertical)
		return simple
	if entry.get("stretch", "") != "nine_slice" and not entry.has("insets"):
		return flat(color("panel"), color("bronze1"))
	var box: StyleBoxTexture = StyleBoxTexture.new()
	box.texture = texture(id)
	var insets: Array = entry.get("insets", [0, 0, 0, 0]) as Array
	for side: int in range(4):
		box.set_texture_margin(side, float(insets[side]))
		box.set_content_margin(side, horizontal if side in [SIDE_LEFT, SIDE_RIGHT] else vertical)
	return box

static func flat(fill: Color, border: Color) -> StyleBoxFlat:
	var box: StyleBoxFlat = StyleBoxFlat.new()
	box.bg_color = fill
	box.border_color = border
	box.set_border_width_all(1)
	box.content_margin_left = 12
	box.content_margin_right = 12
	box.content_margin_top = 8
	box.content_margin_bottom = 8
	return box
