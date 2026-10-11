class_name DeckUi
extends RefCounted

static func compact_button(control: Button) -> void:
	control.custom_minimum_size = Vector2.ZERO
	control.add_theme_font_size_override(&"font_size", 16)
	for state: String in ["normal", "hover", "pressed", "disabled"]:
		var skin: StyleBoxTexture = ArtBank.box("c2d.v2.menu.button-" + state, 6)
		for side: int in range(4):
			skin.set_texture_margin(side, 0)
		skin.set_content_margin(SIDE_LEFT, 16)
		skin.set_content_margin(SIDE_RIGHT, 16)
		control.add_theme_stylebox_override(StringName(state), skin)

static func compact_field(control: Control, font_size: int = 16) -> void:
	if control is OptionButton:
		(control as OptionButton).fit_to_longest_item = false
		(control as OptionButton).clip_text = true
	control.add_theme_font_size_override(&"font_size", font_size)
	for state: StringName in [&"normal", &"hover", &"pressed", &"disabled"]:
		var skin: StyleBoxTexture = ArtBank.box("c2d.v2.ui.field", 4)
		skin.set_content_margin(SIDE_LEFT, 34)
		skin.set_content_margin(SIDE_RIGHT, 34)
		control.add_theme_stylebox_override(state, skin)
	if control is OptionButton:
		control.add_theme_constant_override(&"arrow_margin", 10)

static func place(control: Control, rect: Rect2) -> void:
	control.position = rect.position
	control.size = rect.size

static func total(cards: Dictionary) -> int:
	var count: int = 0
	for value: Variant in cards.values():
		count += int(value)
	return count
