class_name SettingsPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _master: HSlider
var _sound: HSlider
var _reduced: CheckButton
var _automatic: CheckButton
var _resolution: OptionButton
var _mode: OptionButton
var _notice: Label
const RESOLUTIONS: Array[Vector2i] = [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	UiFactory.heading(column, "游戏设置", navigation_requested.emit.bind(&"menu"))
	var center: CenterContainer = CenterContainer.new()
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(center)
	var panel: PanelContainer = PanelContainer.new()
	panel.custom_minimum_size.x = 600
	center.add_child(panel)
	var options: VBoxContainer = VBoxContainer.new()
	panel.add_child(options)
	options.add_child(UiFactory.label("声音与显示", 24))
	_master = _slider(options, "总音量")
	_sound = _slider(options, "音效音量")
	_resolution = _dropdown(options, "窗口分辨率", ["1280 × 720", "1600 × 1000", "1920 × 1080"])
	_mode = _dropdown(options, "窗口模式", ["窗口", "无边框", "全屏"])
	_reduced = CheckButton.new()
	_reduced.text = "精简动画"
	options.add_child(_reduced)
	_automatic = CheckButton.new()
	_automatic.text = "自动通过无操作阶段"
	options.add_child(_automatic)
	options.add_child(UiFactory.button("保存并应用", save, true))
	_notice = UiFactory.label("", 16, true)
	options.add_child(_notice)

func _slider(parent: Container, title: String) -> HSlider:
	var row: HBoxContainer = HBoxContainer.new()
	parent.add_child(row)
	var caption: Label = UiFactory.label(title)
	caption.custom_minimum_size.x = 140
	row.add_child(caption)
	var slider: HSlider = HSlider.new()
	slider.max_value = 100
	slider.step = 1
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(slider)
	return slider

func _dropdown(parent: Container, title: String, items: Array[String]) -> OptionButton:
	var row: HBoxContainer = HBoxContainer.new()
	parent.add_child(row)
	var caption: Label = UiFactory.label(title)
	caption.custom_minimum_size.x = 140
	row.add_child(caption)
	var dropdown: OptionButton = OptionButton.new()
	dropdown.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	for item: String in items:
		dropdown.add_item(item)
	row.add_child(dropdown)
	return dropdown

func configure(value: FrontendService) -> void:
	service = value
	_master.value = int(service.settings.get("masterVolume", 80))
	_sound.value = int(service.settings.get("soundVolume", 80))
	_reduced.button_pressed = bool(service.settings.get("reducedMotion", false))
	_automatic.button_pressed = bool(service.settings.get("automaticPhases", true))
	_mode.select(int(service.settings.get("windowMode", 0)))
	var dimensions: Vector2i = Vector2i(int(service.settings.get("width", 1600)), int(service.settings.get("height", 1000)))
	var index: int = RESOLUTIONS.find(dimensions)
	_resolution.select(index if index >= 0 else 1)

func save() -> void:
	var dimensions: Vector2i = RESOLUTIONS[_resolution.selected]
	var next: Dictionary = {"format": 1, "masterVolume": int(_master.value), "soundVolume": int(_sound.value), "windowMode": _mode.selected, "width": dimensions.x, "height": dimensions.y, "automaticPhases": _automatic.button_pressed, "reducedMotion": _reduced.button_pressed}
	if not service.apply_settings(next):
		_notice.text = service.error
		return
	_notice.text = "已保存"
	FrontendDisplay.apply(next)
