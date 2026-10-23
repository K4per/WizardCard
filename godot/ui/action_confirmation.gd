class_name ActionConfirmation
extends PanelContainer

signal confirmed
signal canceled

var dialog_text: String = ""
var _shade: ColorRect
var _message: RichTextLabel

func _ready() -> void:
	_shade = get_parent().get_node("ConfirmShade") as ColorRect
	_message = %Message
	ArtBank.primary(%Confirm)
	%Confirm.pressed.connect(_confirm)
	%Cancel.pressed.connect(_cancel)
	hide()
	get_viewport().size_changed.connect(_layout)
	_layout()

func popup_centered() -> void:
	_message.text = dialog_text
	_shade.show()
	_shade.move_to_front()
	show()
	move_to_front()
	_layout()
	%Confirm.grab_focus()

func close_popup() -> void:
	if _shade != null:
		_shade.hide()
	super.hide()

func _layout() -> void:
	if not is_node_ready():
		return
	var viewport: Vector2 = get_viewport_rect().size
	var panel_width: float = minf(720.0, maxf(288.0, viewport.x - 32.0))
	size = Vector2(panel_width, minf(420.0, maxf(220.0, viewport.y - 40.0)))
	position = (viewport - size) * 0.5

func _confirm() -> void:
	close_popup()
	confirmed.emit()

func _cancel() -> void:
	close_popup()
	canceled.emit()

func _gui_input(event: InputEvent) -> void:
	if event is InputEventKey and (event as InputEventKey).pressed and not (event as InputEventKey).echo:
		var key: Key = (event as InputEventKey).keycode
		if key == KEY_ESCAPE:
			_cancel()
			accept_event()
		elif key == KEY_ENTER or key == KEY_KP_ENTER:
			_confirm()
			accept_event()
