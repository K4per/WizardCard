class_name DeckDialog
extends Control

signal chosen(index: int)

var _panel: PanelContainer
var _title: Label
var _body: RichTextLabel
var _actions: HBoxContainer

func _ready() -> void:
	_panel = %Panel
	_title = %Title
	_body = %Body
	_actions = %Actions
	_panel.add_theme_stylebox_override(&"panel", ArtBank.box("c2d.v2.ui.panel", 40))
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
