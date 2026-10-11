class_name MenuPage
extends Control

signal navigation_requested(page: StringName)

var _service: FrontendService
var _status: Label
var _column: VBoxContainer
var _logo: TextureRect
var _tools: HBoxContainer
var _buttons: Array[MenuArtButton] = []

func _ready() -> void:
	_logo = TextureRect.new()
	_logo.texture = ArtBank.texture("c2d.v2.menu.logo")
	_logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_logo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_logo)
	_column = VBoxContainer.new()
	add_child(_column)
	_add_button("单人游戏", _single_player, true)
	_add_button("多人游戏", navigation_requested.emit.bind(&"rooms"), true)
	_add_button("卡组构筑", navigation_requested.emit.bind(&"decks"), true)
	_add_button("游戏设置", navigation_requested.emit.bind(&"settings"), true)
	_add_button("退出游戏", get_tree().quit, true)
	_tools = HBoxContainer.new()
	_tools.alignment = BoxContainer.ALIGNMENT_CENTER
	_tools.add_theme_constant_override(&"separation", 24)
	add_child(_tools)
	for entry: Array in [["复盘记录", &"replay"], ["卡牌图鉴", &"library"]]:
		var link: Button = Button.new()
		link.text = str(entry[0])
		link.flat = true
		link.add_theme_font_size_override(&"font_size", 18)
		for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
			var empty: StyleBoxEmpty = StyleBoxEmpty.new()
			empty.set_content_margin(SIDE_LEFT, 8)
			empty.set_content_margin(SIDE_RIGHT, 8)
			empty.set_content_margin(SIDE_TOP, 2)
			empty.set_content_margin(SIDE_BOTTOM, 2)
			link.add_theme_stylebox_override(StringName(state), empty)
		var focus: StyleBoxFlat = StyleBoxFlat.new()
		focus.bg_color = Color.TRANSPARENT
		focus.border_color = Color("64cfe6")
		focus.set_border_width_all(1)
		link.add_theme_stylebox_override(&"focus", focus)
		link.pressed.connect(navigation_requested.emit.bind(entry[1]))
		_tools.add_child(link)
	_status = UiFactory.label("", 16, true)
	_status.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	add_child(_status)
	resized.connect(_reflow)
	_reflow()

func _reflow() -> void:
	var compact: bool = size.y < 850
	var full_hd: bool = size.y >= 1040
	var button_size: Vector2 = Vector2(448, 72) if compact else Vector2(560, 100 if full_hd else 88)
	var gap: int = 8 if compact else (12 if full_hd else 10)
	var menu_height: float = button_size.y * 5 + gap * 4
	_column.add_theme_constant_override(&"separation", gap)
	for button: MenuArtButton in _buttons:
		button.custom_minimum_size = button_size
		button.add_theme_font_size_override(&"font_size", 30 if compact else (40 if full_hd else 36))
	_column.position = Vector2((size.x - button_size.x) * 0.5, size.y - 52 - menu_height)
	_column.size = Vector2(button_size.x, menu_height)
	var logo_width: float = 540 if compact else (1040 if full_hd else 880)
	var logo_size: Vector2 = Vector2(logo_width, logo_width * _logo.texture.get_height() / _logo.texture.get_width())
	_logo.size = logo_size
	_logo.position = Vector2((size.x - logo_width) * 0.5, maxf(12, _column.position.y - logo_size.y + (40 if compact else 92)))
	_tools.position = Vector2((size.x - button_size.x) * 0.5, size.y - 40)
	_tools.size = Vector2(button_size.x, 28)
	_status.position = Vector2(24, 16)
	_status.size = Vector2(size.x - 48, 60)

func _add_button(title: String, callback: Callable, available: bool) -> void:
	var button: MenuArtButton = MenuArtButton.new()
	button.text = title
	button.disabled = not available
	if available:
		button.pressed.connect(callback)
	else:
		button.tooltip_text = "暂未开放"
	_buttons.append(button)
	_column.add_child(button)

func configure(service: FrontendService) -> void:
	_service = service
	_status.text = "" if service.error.is_empty() else service.error
	_status.visible = not service.error.is_empty()

func _single_player() -> void:
	_service.setup_mode = "ai"
	navigation_requested.emit(&"setup")
