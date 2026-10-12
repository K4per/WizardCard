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
var _background: OptionButton
var _notice: Label
var _panel: PanelContainer
var _save_button: Button
const RESOLUTIONS: Array[Vector2i] = [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]

func _ready() -> void:
	_master = %Master
	_sound = %Sound
	_reduced = %Reduced
	_automatic = %Automatic
	_resolution = %Resolution
	_mode = %Mode
	_background = %Background
	_notice = %Notice
	_panel = %Panel
	_save_button = %Save
	(%Back as Button).pressed.connect(_return)
	_save_button.pressed.connect(save)
	_master.value_changed.connect(_volume_changed.bind(%MasterValue))
	_sound.value_changed.connect(_volume_changed.bind(%SoundValue))
	for caption: String in ["1280 × 720", "1600 × 1000", "1920 × 1080"]:
		_resolution.add_item(caption)
	for caption: String in ["窗口", "无边框", "全屏"]:
		_mode.add_item(caption)
	for caption: String in ["奥术（默认）", "石板", "余烬"]:
		_background.add_item(caption)
	resized.connect(_reflow)
	_reflow()

func _volume_changed(amount: float, caption: Label) -> void:
	caption.text = "%d%%" % int(amount)

func _reflow() -> void:
	_panel.custom_minimum_size.x = minf(1120, size.x - 120)

func configure(value: FrontendService) -> void:
	service = value
	_master.value = int(service.settings.get("masterVolume", 80))
	_sound.value = int(service.settings.get("soundVolume", 80))
	_reduced.button_pressed = bool(service.settings.get("reducedMotion", false))
	_automatic.button_pressed = bool(service.settings.get("automaticPhases", true))
	_mode.select(int(service.settings.get("windowMode", 0)))
	_background.select(maxi(0, ["arcane", "slate", "ember"].find(str(service.settings.get("fieldBackground", "arcane")))))
	var dimensions: Vector2i = Vector2i(int(service.settings.get("width", 1920)), int(service.settings.get("height", 1080)))
	var index: int = RESOLUTIONS.find(dimensions)
	if index < 0:
		_resolution.add_item("%d × %d" % [dimensions.x, dimensions.y])
		_resolution.set_item_metadata(_resolution.item_count - 1, dimensions)
		index = _resolution.item_count - 1
	_resolution.select(index)

func save() -> void:
	var dimensions: Vector2i = RESOLUTIONS[_resolution.selected] if _resolution.selected < RESOLUTIONS.size() else _resolution.get_item_metadata(_resolution.selected)
	var next: Dictionary = {"format": 1, "masterVolume": int(_master.value), "soundVolume": int(_sound.value), "windowMode": _mode.selected, "width": dimensions.x, "height": dimensions.y, "automaticPhases": _automatic.button_pressed, "reducedMotion": _reduced.button_pressed, "fieldBackground": ["arcane", "slate", "ember"][_background.selected]}
	if not service.apply_settings(next):
		_notice.text = service.error
		return
	UiFactory.reduced_motion = bool(next.get("reducedMotion", false))
	_notice.text = "已保存"
	FrontendDisplay.apply(next)

func _return() -> void:
	navigation_requested.emit(&"match" if service != null and service.match_active else &"menu")
