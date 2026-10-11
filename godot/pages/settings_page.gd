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
var _panel: PanelContainer
var _save_button: Button
const RESOLUTIONS: Array[Vector2i] = [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	UiFactory.heading(column, "游戏设置", _return)
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	column.add_child(scroll)
	var center: CenterContainer = CenterContainer.new()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.add_child(center)
	_panel = PanelContainer.new()
	_panel.custom_minimum_size.x = 1120
	center.add_child(_panel)
	var options: VBoxContainer = VBoxContainer.new()
	_panel.add_child(options)
	options.add_theme_constant_override(&"separation", 24)
	var sections: HBoxContainer = HBoxContainer.new()
	sections.add_theme_constant_override(&"separation", 48)
	options.add_child(sections)
	var audio: VBoxContainer = VBoxContainer.new()
	audio.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sections.add_child(audio)
	var display: VBoxContainer = VBoxContainer.new()
	display.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	sections.add_child(display)
	audio.add_child(UiFactory.label("声音", 26))
	_master = _slider(audio, "总音量")
	_sound = _slider(audio, "音效音量")
	display.add_child(UiFactory.label("显示", 26))
	_resolution = _dropdown(display, "窗口分辨率", ["1280 × 720", "1600 × 1000", "1920 × 1080"])
	_mode = _dropdown(display, "窗口模式", ["窗口", "无边框", "全屏"])
	options.add_child(HSeparator.new())
	var toggles: HBoxContainer = HBoxContainer.new()
	options.add_child(toggles)
	_reduced = CheckButton.new()
	_reduced.text = "精简动画"
	_reduced.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_reduced.custom_minimum_size.y = 60
	toggles.add_child(_reduced)
	_automatic = CheckButton.new()
	_automatic.text = "自动通过无操作阶段"
	_automatic.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_automatic.custom_minimum_size.y = 60
	toggles.add_child(_automatic)
	var footer: HBoxContainer = HBoxContainer.new()
	options.add_child(footer)
	_notice = UiFactory.label("修改后，点击保存并应用。", 16, true)
	_notice.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	footer.add_child(_notice)
	_save_button = UiFactory.button("保存并应用", save, true)
	_save_button.custom_minimum_size = Vector2(280, 64)
	footer.add_child(_save_button)
	resized.connect(_reflow)
	_reflow()

func _reflow() -> void:
	_panel.custom_minimum_size.x = minf(1120, size.x - 120)

func _slider(parent: Container, title: String) -> HSlider:
	var group: VBoxContainer = VBoxContainer.new()
	group.add_theme_constant_override(&"separation", 8)
	parent.add_child(group)
	var row: HBoxContainer = HBoxContainer.new()
	group.add_child(row)
	var caption: Label = UiFactory.label(title)
	caption.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(caption)
	var slider: HSlider = HSlider.new()
	slider.max_value = 100
	slider.step = 1
	slider.custom_minimum_size.y = 56
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	group.add_child(slider)
	var value: Label = UiFactory.label("100%", 18)
	value.custom_minimum_size.x = 64
	value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(value)
	slider.value_changed.connect(func(amount: float) -> void: value.text = "%d%%" % int(amount))
	return slider

func _dropdown(parent: Container, title: String, items: Array[String]) -> OptionButton:
	var row: VBoxContainer = VBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	parent.add_child(row)
	var caption: Label = UiFactory.label(title)
	row.add_child(caption)
	var dropdown: OptionButton = OptionButton.new()
	dropdown.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	dropdown.custom_minimum_size.y = 56
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
	var dimensions: Vector2i = Vector2i(int(service.settings.get("width", 1920)), int(service.settings.get("height", 1080)))
	var index: int = RESOLUTIONS.find(dimensions)
	if index < 0:
		_resolution.add_item("%d × %d" % [dimensions.x, dimensions.y])
		_resolution.set_item_metadata(_resolution.item_count - 1, dimensions)
		index = _resolution.item_count - 1
	_resolution.select(index)

func save() -> void:
	var dimensions: Vector2i = RESOLUTIONS[_resolution.selected] if _resolution.selected < RESOLUTIONS.size() else _resolution.get_item_metadata(_resolution.selected)
	var next: Dictionary = {"format": 1, "masterVolume": int(_master.value), "soundVolume": int(_sound.value), "windowMode": _mode.selected, "width": dimensions.x, "height": dimensions.y, "automaticPhases": _automatic.button_pressed, "reducedMotion": _reduced.button_pressed}
	if not service.apply_settings(next):
		_notice.text = service.error
		return
	_notice.text = "已保存"
	FrontendDisplay.apply(next)

func _return() -> void:
	navigation_requested.emit(&"match" if service != null and service.match_active else &"menu")
