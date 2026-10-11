class_name DeckDialog
extends Control

signal chosen(index: int)

var _panel: PanelContainer
var _title: Label
var _body: RichTextLabel
var _actions: HBoxContainer

func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var shade: ColorRect = ColorRect.new()
	shade.color = Color(0.01, 0.03, 0.06, 0.82)
	add_child(shade)
	shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_panel = PanelContainer.new()
	_panel.add_theme_stylebox_override(&"panel", ArtBank.box("c2d.v2.ui.panel", 40))
	add_child(_panel)
	var column: VBoxContainer = VBoxContainer.new()
	_panel.add_child(column)
	_title = UiFactory.label("", 26)
	column.add_child(_title)
	_body = RichTextLabel.new()
	_body.bbcode_enabled = false
	_body.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(_body)
	_actions = HBoxContainer.new()
	column.add_child(_actions)
	resized.connect(_reflow)
	hide()

func present(title: String, body: String, actions: Array[String]) -> void:
	_title.text = title
	_body.text = body
	for child: Node in _actions.get_children():
		_actions.remove_child(child)
		child.queue_free()
	for index: int in range(actions.size()):
		var button: Button = UiFactory.button(actions[index], _choose.bind(index))
		DeckUi.compact_button(button)
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.custom_minimum_size.y = 44
		_actions.add_child(button)
	show()
	_reflow()
	(_actions.get_child(actions.size() - 1) as Button).grab_focus()

func _reflow() -> void:
	if _panel != null:
		var extent: Vector2 = Vector2(minf(820, size.x - 80), minf(380, size.y - 80))
		DeckUi.place(_panel, Rect2((size - extent) * 0.5, extent))

func _choose(index: int) -> void:
	hide()
	chosen.emit(index)
