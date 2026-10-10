class_name MenuPage
extends Control

signal navigation_requested(page: StringName)

var _service: FrontendService
var _status: Label

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self, 32)
	var centered: CenterContainer = CenterContainer.new()
	margin.add_child(centered)
	var column: VBoxContainer = VBoxContainer.new()
	column.custom_minimum_size.x = 500
	centered.add_child(column)
	var logo: TextureRect = TextureRect.new()
	logo.texture = ArtBank.texture("c2d.brand.logo")
	logo.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	logo.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	logo.custom_minimum_size = Vector2(500, 170)
	column.add_child(logo)
	var subtitle: Label = UiFactory.label("ALPHA v2 · 前端开发预览", 18)
	subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	column.add_child(subtitle)
	column.add_child(UiFactory.button("场地预览", navigation_requested.emit.bind(&"field"), true))
	column.add_child(UiFactory.button("卡牌图鉴", navigation_requested.emit.bind(&"library")))
	column.add_child(UiFactory.button("游戏设置", navigation_requested.emit.bind(&"settings")))
	column.add_child(UiFactory.button("退出", get_tree().quit))
	_status = UiFactory.label("", 16, true)
	_status.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	column.add_child(_status)

func configure(service: FrontendService) -> void:
	_service = service
	_status.text = "场地与控件展示 · 完整对局将在规则接入后开放" if service.error.is_empty() else service.error
