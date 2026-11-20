class_name CardActionMenu
extends Control

signal effect_preview_requested()

var _effect: Button

func _ready() -> void:
	_effect = UiFactory.button("效果选择示例", effect_preview_requested.emit)
	_effect.tooltip_text = "展示候选卡牌选择控件"
	add_child(_effect)
	_effect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
