class_name DeckDialog
extends Control

signal chosen(index: int)

var _panel: PanelContainer
var _title: Label
var _body: RichTextLabel
var _actions: HBoxContainer

func _ready() -> void:
	theme = DeckTheme.create()
	_panel = %Panel
	_title = %Title
	_body = %Body
	_actions = %Actions
	_panel.add_theme_stylebox_override(&"panel", DeckTheme.box(DeckTheme.PANEL, DeckTheme.BORDER, 20))
	resized.connect(_reflow)
	hide()

func present(title: String, body: String, actions: Array[String]) -> void:
	_title.text = title
	_body.text = body
	for child: Node in _actions.get_children():
		_actions.remove_child(child)
		child.queue_free()
	for index: int in range(actions.size()):
		var button: Button = Button.new()
		button.text = actions[index]
		button.pressed.connect(_choose.bind(index))
		DeckTheme.button(button)
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.custom_minimum_size.y = 36
		_actions.add_child(button)
	show()
	_reflow()
	if not actions.is_empty():
		(_actions.get_child(actions.size() - 1) as Button).grab_focus()

func _reflow() -> void:
	if _panel != null:
		var extent: Vector2 = Vector2(minf(620, maxf(0.0, size.x - 48)), minf(280, maxf(0.0, size.y - 48)))
		_panel.position = (size - extent) * 0.5
		_panel.size = extent

func _choose(index: int) -> void:
	hide()
	chosen.emit(index)

func _unhandled_key_input(event: InputEvent) -> void:
	if visible and event.is_action_pressed(&"ui_cancel") and _actions.get_child_count() > 0:
		_choose(_actions.get_child_count() - 1)
		get_viewport().set_input_as_handled()
