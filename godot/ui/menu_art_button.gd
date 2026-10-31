class_name MenuArtButton
extends Button

const DESIGN_SIZE: Vector2 = Vector2(560, 100)
static var _hit_polygon: PackedVector2Array = PackedVector2Array([
	Vector2(0, 50), Vector2(46, 0), Vector2(514, 0),
	Vector2(560, 50), Vector2(514, 100), Vector2(46, 100),
])

var _state: String = ""

func _ready() -> void:
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		add_theme_stylebox_override(StringName(state), _greybox_style(state))
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
	# Keep the state label for input/smoke diagnostics; the menu uses only flat
	# primitive styles and never resolves the retired button textures.
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
		queue_redraw()

func _has_point(point: Vector2) -> bool:
	return Rect2(Vector2.ZERO, size).has_point(point)

func _greybox_style(state: String) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = {
		"normal": DeckTheme.CONTROL,
		"hover": DeckTheme.HOVER,
		"pressed": DeckTheme.CONTROL,
		"disabled": DeckTheme.PANEL,
		"focus": Color.TRANSPARENT,
	}.get(state, DeckTheme.CONTROL)
	style.border_color = DeckTheme.ACCENT if state in ["pressed", "focus"] else DeckTheme.BORDER
	style.set_border_width_all(2 if state == "focus" else 1)
	style.set_corner_radius_all(3)
	style.set_content_margin(SIDE_LEFT, 72)
	style.set_content_margin(SIDE_RIGHT, 72)
	style.set_content_margin(SIDE_TOP, 6)
	style.set_content_margin(SIDE_BOTTOM, 6)
	return style

func _animate_press() -> void:
	if UiFactory.reduced_motion or disabled:
		return
	pivot_offset = size * 0.5
	var tween: Tween = create_tween()
	tween.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_property(self, "scale", Vector2(0.985, 0.985), 0.045)
	tween.tween_property(self, "scale", Vector2.ONE, 0.11)
