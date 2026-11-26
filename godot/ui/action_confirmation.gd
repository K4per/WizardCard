class_name ActionConfirmation
extends PanelContainer

signal confirmed
signal canceled

var dialog_text: String = ""
var context: String = ""
var response_question: bool = false
var _bottom_docked: bool = false
var _dragging: bool = false
var _drag_offset: Vector2
var _pressed_context: String = ""
@onready var _shade: ColorRect = get_parent().get_node("ConfirmShade") as ColorRect
@onready var _heading: Label = %Heading
@onready var _message: RichTextLabel = %Message

func _ready() -> void:
	ArtBank.primary(%Confirm)
	for action: Button in [%Confirm, %Cancel]:
		action.button_down.connect(_arm_button)
	%Confirm.pressed.connect(_finish.bind(true))
	%Cancel.pressed.connect(_finish.bind(false))
	_heading.mouse_filter = Control.MOUSE_FILTER_STOP
	_heading.mouse_default_cursor_shape = Control.CURSOR_MOVE
	_heading.gui_input.connect(_drag_title)
	get_parent_control().resized.connect(_layout)
	hide()

func popup_centered(identity: String = "action", overload_warning: String = "") -> void:
	_present(identity, "确认操作", "确认", "取消", false, overload_warning)

func popup_response(identity: String, title: String, accept_label: String, cancel_label: String) -> void:
	_present(identity, title, accept_label, cancel_label, true, "")

func _present(identity: String, title: String, accept_label: String, cancel_label: String, response: bool, warning: String) -> void:
	var keep_position: bool = visible and context == identity
	context = identity
	response_question = response
	if not keep_position:
		_bottom_docked = response
	_heading.text = title
	_message.text = dialog_text
	%Confirm.text = accept_label
	%Cancel.text = cancel_label
	%OverloadWarning.text = warning
	%OverloadWarning.visible = not warning.is_empty()
	_shade.visible = not response
	if not response:
		_shade.move_to_front()
	show()
	move_to_front()
	_layout()
	if not keep_position:
		if not response:
			position = (get_parent_control().size - size) * 0.5
		%Confirm.grab_focus()

func close_popup() -> void:
	_dragging = false
	context = ""
	response_question = false
	_bottom_docked = false
	_shade.hide()
	hide()

func _layout() -> void:
	if not is_node_ready():
		return
	var available: Vector2 = get_parent_control().size
	size = Vector2(minf(560.0, available.x), minf(284.0, available.y))
	if _bottom_docked:
		position = Vector2((available.x - size.x) * 0.5, available.y - size.y - 8.0)
	position = position.clamp(Vector2.ZERO, (available - size).max(Vector2.ZERO))

func _drag_title(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		_dragging = event.pressed
		_drag_offset = event.global_position - global_position
		accept_event()
	elif _dragging and event is InputEventMouseMotion:
		_bottom_docked = false
		global_position = event.global_position - _drag_offset
		_layout()
		accept_event()

func _arm_button() -> void:
	_pressed_context = context

func _finish(accept: bool) -> void:
	if not visible or (not _pressed_context.is_empty() and _pressed_context != context):
		_pressed_context = ""
		return
	_pressed_context = ""
	close_popup()
	if accept:
		confirmed.emit()
	else:
		canceled.emit()

func _input(event: InputEvent) -> void:
	if not visible or not event is InputEventKey or not event.pressed or event.echo:
		return
	if event.keycode in [KEY_ESCAPE, KEY_ENTER, KEY_KP_ENTER]:
		get_viewport().set_input_as_handled()
		_pressed_context = ""
		_finish(event.keycode != KEY_ESCAPE and get_viewport().gui_get_focus_owner() != %Cancel)
