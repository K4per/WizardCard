class_name SettingsPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _master: HSlider
var _sound: HSlider
var _reduced: CheckButton
var _automatic: CheckButton
var _response_sound: CheckButton
var _resolution: OptionButton
var _font_scale: OptionButton
var _mode: OptionButton
var _background: OptionButton
var _notice: Label
var _panel: PanelContainer
var _save_button: Button
const RESOLUTIONS: Array[Vector2i] = [
	Vector2i(1280, 720),
	Vector2i(1600, 900),
	Vector2i(1440, 900),
	Vector2i(1600, 1000),
	Vector2i(1920, 1080),
	Vector2i(2560, 1440),
]

func _ready() -> void:
	_master = %Master
	_sound = %Sound
	_reduced = %Reduced
	_automatic = %Automatic
	_response_sound = %ResponseSound
	_resolution = %Resolution
	_font_scale = %FontScale
	_mode = %Mode
	_background = %Background
	_notice = %Notice
	_panel = %Panel
	_save_button = %Save
	(%Back as Button).pressed.connect(_return)
	_save_button.pressed.connect(save)
	_master.value_changed.connect(_volume_changed.bind(%MasterValue))
	_sound.value_changed.connect(_volume_changed.bind(%SoundValue))
	for dimensions: Vector2i in RESOLUTIONS:
		var caption: String = "%d × %d" % [dimensions.x, dimensions.y]
		_resolution.add_item(caption)
	for caption: String in ["窗口", "无边框", "全屏"]:
		_mode.add_item(caption)
	for caption: String in ["奥术（默认）", "石板", "余烬"]:
		_background.add_item(caption)
	for scale: int in Typography.SCALES:
		_font_scale.add_item("%d%%" % scale)
	_font_scale.tooltip_text = "默认 100%。保存后同步调整所有界面、提示、弹窗和卡面文字，重启后保留。"
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
	_font_scale.select(Typography.SCALES.find(int(service.settings.get("fontScale", Typography.DEFAULT_SCALE))))
	_reduced.button_pressed = bool(service.settings.get("reducedMotion", false))
	_response_sound.button_pressed = bool(service.settings.get("responseTimingSound", false))
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
	var next: Dictionary = {"format": 1, "masterVolume": int(_master.value), "soundVolume": int(_sound.value), "windowMode": _mode.selected, "width": dimensions.x, "height": dimensions.y, "automaticPhases": _automatic.button_pressed, "reducedMotion": _reduced.button_pressed, "responseTimingSound": _response_sound.button_pressed, "fieldBackground": ["arcane", "slate", "ember"][_background.selected], "fontScale": Typography.SCALES[_font_scale.selected]}
	if not service.apply_settings(next):
		_notice.text = service.error
		return
	UiFactory.reduced_motion = bool(next.get("reducedMotion", false))
	Typography.set_scale(int(next.get("fontScale", Typography.DEFAULT_SCALE)))
	_notice.text = "已保存"
	FrontendDisplay.apply(next)

func _return() -> void:
	navigation_requested.emit(&"match" if service != null and service.match_active else &"menu")
