class_name CardActionMenu
extends Control

signal detail_requested()
signal effect_preview_requested()

var _detail: Button
var _effect: Button
var _skin: TextureRect

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_skin = TextureRect.new()
	_skin.texture = BattleArt.texture("action_two_normal")
	_skin.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_skin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_skin)
	_skin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_detail = UiFactory.button("查看详情", detail_requested.emit)
	_effect = UiFactory.button("效果选择示例", effect_preview_requested.emit)
	_effect.tooltip_text = "展示候选卡牌选择控件"
	for button: Button in [_detail, _effect]:
		button.custom_minimum_size = Vector2.ZERO
		for state: String in ["normal", "hover", "pressed", "disabled"]:
			button.add_theme_stylebox_override(StringName(state), StyleBoxEmpty.new())
		button.add_theme_font_size_override(&"font_size", 14)
		add_child(button)
	resized.connect(_layout)
	_layout()

func _layout() -> void:
	_detail.position = size * Vector2(0.055, 0.13)
	_detail.size = size * Vector2(0.425, 0.59)
	_effect.position = size * Vector2(0.52, 0.13)
	_effect.size = size * Vector2(0.425, 0.59)
