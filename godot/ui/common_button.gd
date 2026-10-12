class_name CommonButton
extends Button

@export var primary: bool = false
@export var compact: bool = false

func _ready() -> void:
	UiFactory.bind_button(self)
	if primary:
		ArtBank.primary(self)
	if compact:
		DeckUi.compact_button(self)
