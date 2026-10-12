class_name UiFactory
extends RefCounted

static var reduced_motion: bool = false

static func label(text: String, font_size: int = 18, wrap: bool = false) -> Label:
	var result: Label = Label.new()
	result.text = text
	result.add_theme_font_size_override(&"font_size", font_size)
	if font_size >= 22:
		result.add_theme_font_override(&"font", Typography.HEADING)
	if wrap:
		result.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		result.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	return result

static func button(text: String, callback: Callable, primary: bool = false) -> Button:
	var result: Button = load("res://ui/common_button.tscn").instantiate() as Button
	result.text = text
	result.custom_minimum_size.y = 52
	result.pressed.connect(callback)
	if primary:
		ArtBank.primary(result)
	return result

static func bind_button(control: Button, callback: Callable = Callable()) -> void:
	if callback.is_valid() and not control.pressed.is_connected(callback):
		control.pressed.connect(callback)
	var pressed: Callable = _button_pressed.bind(control)
	if control.pressed.is_connected(pressed):
		return
	control.pressed.connect(pressed)
	control.mouse_entered.connect(_button_hover.bind(control, true))
	control.mouse_exited.connect(_button_hover.bind(control, false))
	control.resized.connect(_button_resized.bind(control))

static func _button_hover(button: Button, hovered: bool) -> void:
	if not is_instance_valid(button):
		return
	var target: Color = Color(1.08, 1.04, 0.94, 1.0) if hovered and not button.disabled else Color.WHITE
	if reduced_motion:
		button.self_modulate = target
		return
	var tween: Tween = button.create_tween()
	tween.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_property(button, "self_modulate", target, 0.12)

static func _button_resized(button: Button) -> void:
	button.pivot_offset = button.size * 0.5

static func _button_pressed(button: Button) -> void:
	if reduced_motion or button.disabled:
		return
	button.pivot_offset = button.size * 0.5
	var tween: Tween = button.create_tween()
	tween.set_trans(Tween.TRANS_QUAD).set_ease(Tween.EASE_OUT)
	tween.tween_property(button, "scale", Vector2(0.985, 0.985), 0.045)
	tween.tween_property(button, "scale", Vector2.ONE, 0.11)

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
	var title_label: Label = label(title, 28)
	title_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(title_label)
	return row
