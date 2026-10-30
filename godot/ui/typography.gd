class_name Typography
extends RefCounted

# Keep the shipped Chinese fonts in the UI while retaining a safe fallback for
# platforms where the importer cannot provide a dynamic font resource.
static var BODY: Font = _bundled_or_fallback("res://art/fonts/NotoSansCJKsc-Regular.otf")
static var HEADING: Font = _bundled_or_fallback("res://art/fonts/NotoSansCJKsc-Bold.otf")
static var MENU: Font = _bundled_or_fallback("res://art/fonts/NotoSansCJKsc-Black.otf")
const SCALES: Array[int] = [90, 100, 110, 120]
const DEFAULT_SCALE: int = 100

static var scale_percent: int = 100

static func _bundled_or_fallback(path: String) -> Font:
	var bundled: Font = load(path) as Font
	return bundled if bundled != null else ThemeDB.fallback_font

static func override_size(control: Control, item: StringName, base_size: int) -> void:
	var base: Dictionary = control.get_meta(&"typography_base_sizes", {})
	var applied: Dictionary = control.get_meta(&"typography_applied_sizes", {})
	base[item] = base_size
	applied[item] = scaled_size(base_size)
	control.set_meta(&"typography_base_sizes", base)
	control.set_meta(&"typography_applied_sizes", applied)
	control.add_theme_font_size_override(item, int(applied[item]))

static func set_scale(percent: int) -> void:
	var tree: SceneTree = Engine.get_main_loop() as SceneTree
	(tree.root.get_node("GlobalTypography") as Node).call(&"set_scale", percent)

static func refresh() -> void:
	var tree: SceneTree = Engine.get_main_loop() as SceneTree
	(tree.root.get_node("GlobalTypography") as Node).call(&"refresh")

static func scaled_size(base_size: int) -> int:
	return maxi(1, roundi(base_size * scale_percent / 100.0))

static func fitted_size(base_size: int, height: float, font: Font = BODY) -> int:
	var result: int = scaled_size(base_size)
	while result > 5 and font.get_height(result) > height:
		result -= 1
	return result
