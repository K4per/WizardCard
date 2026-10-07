class_name WizardZoneView
extends PanelContainer

signal dropped(data: Dictionary, destination: int, target: String)
var destination: int = 0
var target: String = "0"
var accepts_cards: bool = true

func _can_drop_data(_position: Vector2, data: Variant) -> bool:
	return accepts_cards and data is Dictionary and bool((data as Dictionary).get("wizard_card", false))

func _drop_data(_position: Vector2, data: Variant) -> void:
	dropped.emit(data as Dictionary, destination, target)
