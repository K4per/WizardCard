class_name PhaseSelector
extends PopupPanel

signal action_chosen(action: Dictionary)

var _choices: VBoxContainer

func _ready() -> void:
	_choices = VBoxContainer.new()
	_choices.add_theme_constant_override(&"separation", 4)
	add_child(_choices)

func open_for(button: Button, actions: Array[Dictionary]) -> void:
	for child: Node in _choices.get_children():
		_choices.remove_child(child)
		child.queue_free()
	for action: Dictionary in actions:
		var command: Dictionary = action.get("command", {})
		var label: String = "切换到结束阶段" if bool(command.get("skipCast", false)) else "切换到施法阶段"
		var choice: Button = UiFactory.button(label, _choose.bind(action))
		choice.custom_minimum_size = Vector2(184, 40)
		_choices.add_child(choice)
	var extent: Vector2i = Vector2i(200, actions.size() * 44 + 16)
	var screen: Rect2i = Rect2i(button.get_viewport().get_visible_rect())
	var point: Vector2i = Vector2i(button.get_global_rect().position) - Vector2i(extent.x - int(button.size.x), extent.y + 8)
	point.x = clampi(point.x, 8, maxi(8, screen.size.x - extent.x - 8))
	point.y = clampi(point.y, 8, maxi(8, screen.size.y - extent.y - 8))
	popup(Rect2i(point, extent))

func _choose(action: Dictionary) -> void:
	hide()
	action_chosen.emit(action)
