class_name UiFactory
extends RefCounted

static func label(text: String, font_size: int = 18, wrap: bool = false) -> Label:
	var result: Label = Label.new()
	result.text = text
	result.add_theme_font_size_override(&"font_size", font_size)
	if wrap:
		result.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		result.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return result

static func button(text: String, callback: Callable, primary: bool = false) -> Button:
	var result: Button = Button.new()
	result.text = text
	result.custom_minimum_size.y = 48
	result.pressed.connect(callback)
	if primary:
		ArtBank.primary(result)
	return result

static func margin(parent: Node, amount: int = 24) -> MarginContainer:
	var result: MarginContainer = MarginContainer.new()
	for side: String in ["left", "top", "right", "bottom"]:
		result.add_theme_constant_override(StringName("margin_" + side), amount)
	parent.add_child(result)
	result.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	return result

static func heading(parent: Container, title: String, callback: Callable) -> HBoxContainer:
	var row: HBoxContainer = HBoxContainer.new()
	parent.add_child(row)
	row.add_child(button("‹ 返回", callback))
	var title_label: Label = label(title, 26)
	title_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(title_label)
	return row
