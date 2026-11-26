class_name CardActionMenu
extends Control

signal effect_preview_requested()

var _effect: Button

func _ready() -> void:
	_effect = UiFactory.button(GameText.label("ui.showcase.decision_example"), effect_preview_requested.emit)
	_effect.tooltip_text = GameText.label("ui.card_action.showcase_choice")
	add_child(_effect)
	_effect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
