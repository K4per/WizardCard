extends ColorRect

signal action_requested(action: StringName)

const TextCatalog = preload("res://app/text.gd")
@onready var _text: TextCatalog = get_node("/root/Text") as TextCatalog
var _primary: StringName = &"next"

func _ready() -> void:
	%Retry.text = _text.t("ui.lesson.retry")
	%Menu.text = _text.t("tutorial.general.g03.menu")
	%Next.pressed.connect(func() -> void: action_requested.emit(_primary))
	%Retry.pressed.connect(action_requested.emit.bind(&"retry"))
	%Menu.pressed.connect(action_requested.emit.bind(&"menu"))

func present(message: String = "", completed: bool = true, editor: bool = false) -> void:
	_primary = &"deck" if editor else &"next"
	%Title.text = _text.t("tutorial.general.g03" if completed else "tutorial.general.g05")
	%Message.text = message
	%Message.visible = not message.is_empty()
	%Next.text = _text.t("ui.lesson.deck" if _primary == &"deck" else "tutorial.general.g03.next")
	%Next.visible = completed
	show()
	if completed:
		%Next.grab_focus()
	else:
		%Retry.grab_focus()
