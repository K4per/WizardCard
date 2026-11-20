class_name MenuArtButton
extends Button

func _ready() -> void:
	for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
		add_theme_stylebox_override(StringName(state), _greybox_style(state))
	add_theme_color_override(&"font_color", Color("fff0d0"))
	add_theme_color_override(&"font_pressed_color", Color("fff0d0"))
	add_theme_color_override(&"font_hover_color", Color("fff0d0"))
	add_theme_color_override(&"font_focus_color", Color("fff0d0"))
	add_theme_color_override(&"font_disabled_color", Color("b8ad98"))
	add_theme_font_override(&"font", Typography.MENU)

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
	style.set_corner_radius_all(0)
	style.set_content_margin(SIDE_LEFT, 72)
	style.set_content_margin(SIDE_RIGHT, 72)
	style.set_content_margin(SIDE_TOP, 6)
	style.set_content_margin(SIDE_BOTTOM, 6)
	return style
