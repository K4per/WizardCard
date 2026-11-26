extends ColorRect

signal action_requested(action: StringName)

const TextCatalog = preload("res://app/text.gd")
@onready var _text: TextCatalog = get_node("/root/Text") as TextCatalog
var _primary: StringName = &"next"

func _ready() -> void:
	%Retry.text = _text.t("ui.lesson.retry")
	%Menu.text = _text.t("ui.lesson.menu")
	%Next.pressed.connect(func() -> void: action_requested.emit(_primary))
	%Retry.pressed.connect(action_requested.emit.bind(&"retry"))
	%Menu.pressed.connect(action_requested.emit.bind(&"menu"))

func present(order: int, message: String = "", completed: bool = true) -> void:
	_primary = &"deck" if order == 8 else &"next"
	%Title.text = _text.t("ui.lesson.complete" if completed else "ui.lesson.incomplete", {"order": order})
	%Message.text = message
	%Message.visible = not message.is_empty()
	%Next.text = _text.t("ui.lesson.deck" if _primary == &"deck" else "ui.lesson.next")
	%Next.visible = completed
	show()
	if completed:
		%Next.grab_focus()
	else:
		%Retry.grab_focus()
