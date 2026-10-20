class_name MenuPage
extends Control

signal navigation_requested(page: StringName)

var _service: FrontendService
var _status: Label
var _column: VBoxContainer
var _logo: TextureRect
var _buttons: Array[Button] = []

func _ready() -> void:
	_logo = %Logo
	_logo.texture = ArtBank.texture("c2d.v2.menu.logo")
	_column = %Buttons
	_status = %Status
	_buttons.assign([%SinglePlayer, %Multiplayer, %Decks, %Library, %Replay, %Settings, %Quit])
	_buttons[0].pressed.connect(_single_player)
	_buttons[1].pressed.connect(navigation_requested.emit.bind(&"rooms"))
	_buttons[2].pressed.connect(navigation_requested.emit.bind(&"decks"))
	_buttons[3].pressed.connect(navigation_requested.emit.bind(&"library"))
	_buttons[4].pressed.connect(navigation_requested.emit.bind(&"replay"))
	_buttons[5].pressed.connect(navigation_requested.emit.bind(&"settings"))
	_buttons[6].pressed.connect(get_tree().quit)
	for index: int in range(_buttons.size()):
		var button: Button = _buttons[index]
		var previous: NodePath = button.get_path_to(_buttons[posmod(index - 1, _buttons.size())])
		var next: NodePath = button.get_path_to(_buttons[(index + 1) % _buttons.size()])
		button.focus_previous = previous
		button.focus_next = next
		button.focus_neighbor_top = previous
		button.focus_neighbor_bottom = next
	resized.connect(_reflow)
	_reflow()

func _reflow() -> void:
	if _column == null:
		return
	# Retain the v1.0 title-left / actions-right composition at any window ratio.
	var scale_factor: float = minf(size.x / 1920.0, size.y / 1080.0)
	var button_size: Vector2 = Vector2(560, 82) * scale_factor
	var gap: int = maxi(8, roundi(18 * scale_factor))
	_column.add_theme_constant_override(&"separation", gap)
	for button: Button in _buttons:
		button.custom_minimum_size = button_size
		Typography.override_size(button, &"font_size", roundi(30 * scale_factor))
		for state: StringName in [&"normal", &"hover", &"pressed", &"disabled", &"focus"]:
			var skin: StyleBox = button.get_theme_stylebox(state)
			skin.set_content_margin(SIDE_LEFT, 72 * scale_factor)
			skin.set_content_margin(SIDE_RIGHT, 72 * scale_factor)
			skin.set_content_margin(SIDE_TOP, 6 * scale_factor)
			skin.set_content_margin(SIDE_BOTTOM, 6 * scale_factor)
	var column_height: float = button_size.y * _buttons.size() + gap * (_buttons.size() - 1)
	_column.position = Vector2(size.x * 0.64 - button_size.x * 0.15, (size.y - column_height) * 0.5)
	_column.size = Vector2(button_size.x, column_height)
	_logo.position = Vector2(size.x * 0.055, size.y * 0.20)
	_logo.size = Vector2(size.x * 0.49, size.y * 0.60)

func configure(service: FrontendService) -> void:
	_service = service
	_status.text = "" if service.error.is_empty() else service.error
	_status.visible = not service.error.is_empty()

func _single_player() -> void:
	_service.setup_mode = "ai"
	navigation_requested.emit(&"setup")
