class_name MenuArtButton
extends Button

const DESIGN_SIZE: Vector2 = Vector2(560, 100)
static var _hit_polygon: PackedVector2Array = PackedVector2Array([
	Vector2(0, 50), Vector2(46, 0), Vector2(514, 0),
	Vector2(560, 50), Vector2(514, 100), Vector2(46, 100),
])

var _skin: TextureRect
var _state: String = ""

func _ready() -> void:
	_skin = get_node_or_null("Skin") as TextureRect
	if _skin == null:
		var template: Button = load("res://ui/menu_art_button.tscn").instantiate() as Button
		_skin = template.get_node("Skin") as TextureRect
		template.remove_child(_skin)
		add_child(_skin)
		template.free()
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		var empty: StyleBoxEmpty = StyleBoxEmpty.new()
		empty.set_content_margin(SIDE_LEFT, 72)
		empty.set_content_margin(SIDE_RIGHT, 72)
		empty.set_content_margin(SIDE_TOP, 12)
		empty.set_content_margin(SIDE_BOTTOM, 12)
		add_theme_stylebox_override(StringName(state), empty)
	add_theme_color_override(&"font_color", Color("fff0d0"))
	add_theme_color_override(&"font_pressed_color", Color("fff0d0"))
	add_theme_color_override(&"font_hover_color", Color("fff0d0"))
	add_theme_color_override(&"font_focus_color", Color("fff0d0"))
	add_theme_color_override(&"font_disabled_color", Color("b8ad98"))
	add_theme_font_override(&"font", Typography.MENU)
	add_theme_color_override(&"font_outline_color", Color("08131f"))
	add_theme_constant_override(&"outline_size", 2)
	draw.connect(_refresh_skin)
	pressed.connect(_animate_press)
	resized.connect(func() -> void: pivot_offset = size * 0.5)
	_refresh_skin()

func _refresh_skin() -> void:
	# A single skin avoids an opaque focus texture covering the pressed state.
	var next: String = "normal"
	var mode: DrawMode = get_draw_mode()
	if disabled:
		next = "disabled"
	elif mode == DRAW_PRESSED or mode == DRAW_HOVER_PRESSED:
		next = "pressed"
	elif has_focus():
		next = "focus"
	elif mode == DRAW_HOVER:
		next = "hover"
	if next != _state:
		_state = next
		_skin.texture = ArtBank.texture("c2d.v2.menu.button-" + next)

func _has_point(point: Vector2) -> bool:
	if size.x <= 0 or size.y <= 0:
		return false
	return Geometry2D.is_point_in_polygon(point * DESIGN_SIZE / size, _hit_polygon)

func _animate_press() -> void:
	if UiFactory.reduced_motion or disabled:
		return
	pivot_offset = size * 0.5
	var tween: Tween = create_tween()
	tween.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_property(self, "scale", Vector2(0.985, 0.985), 0.045)
	tween.tween_property(self, "scale", Vector2.ONE, 0.11)
