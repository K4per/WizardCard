extends ColorRect

signal action_requested(action: StringName)

const TextCatalog = preload("res://app/text.gd")
@onready var _text: TextCatalog = get_node("/root/Text") as TextCatalog
@onready var _panel: PanelContainer = %Panel
var _primary: StringName = &"next"

func _ready() -> void:
	# Packed parent scenes can override the inherited anchors with position layout.
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	%Retry.text = _text.t("ui.lesson.retry")
	%Menu.text = _text.t("tutorial.general.g03.menu")
	%Next.pressed.connect(func() -> void: action_requested.emit(_primary))
	%Retry.pressed.connect(action_requested.emit.bind(&"retry"))
	%Menu.pressed.connect(action_requested.emit.bind(&"menu"))
	_panel.add_theme_stylebox_override(&"panel", DeckTheme.box(DeckTheme.PANEL, DeckTheme.ACCENT, 28))
	%Title.add_theme_font_override(&"font", Typography.HEADING)
	resized.connect(_layout)
	_panel.minimum_size_changed.connect(_center)
	_layout()

func _layout() -> void:
	var scale_factor: float = clampf(minf(size.x / 1600.0, size.y / 900.0), 0.85, 1.35)
	Typography.override_size(%Title, &"font_size", roundi(38 * scale_factor))
	Typography.override_size(%Message, &"font_size", roundi(25 * scale_factor))
	for button: Button in [%Next, %Retry, %Menu]:
		Typography.override_size(button, &"font_size", roundi(24 * scale_factor))
	_panel.custom_minimum_size = Vector2(minf(size.x * 0.54, 1200), size.y * 0.38)
	# Prepare the wrap width before the first Container sort, including while hidden.
	%Message.size.x = maxf(1.0, _panel.custom_minimum_size.x - _panel.get_theme_stylebox(&"panel").get_minimum_size().x)
	_center()

func _center() -> void:
	_panel.set_offsets_preset(Control.PRESET_CENTER, Control.PRESET_MODE_MINSIZE)

func present(message: String = "", completed: bool = true, editor: bool = false) -> void:
	_primary = &"deck" if editor else &"next"
	%Title.text = _text.t("tutorial.general.g03" if completed else "tutorial.general.g05")
	%Message.text = message
	%Message.visible = not message.is_empty()
	%Next.text = _text.t("ui.lesson.deck" if _primary == &"deck" else "tutorial.general.g03.next")
	%Next.visible = completed
	show()
	var primary: Button = %Next if completed else %Retry
	var secondary: Button = %Retry if completed else %Next
	secondary.remove_theme_stylebox_override(&"normal")
	secondary.remove_theme_stylebox_override(&"hover")
	ArtBank.primary(primary)
	primary.grab_focus()
	_center()
