class_name SetupPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var _mode: OptionButton
var _difficulty: OptionButton
var _decks: Array[OptionButton] = []
var _notice: Label
var _start: Button

func _ready() -> void:
	var column: VBoxContainer = VBoxContainer.new()
	UiFactory.margin(self, 32).add_child(column)
	UiFactory.heading(column, "开始对局", navigation_requested.emit.bind(&"menu"))
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	column.add_child(scroll)
	var center: CenterContainer = CenterContainer.new()
	center.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	center.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.add_child(center)
	var panel: PanelContainer = PanelContainer.new()
	panel.custom_minimum_size.x = 720
	center.add_child(panel)
	var options: VBoxContainer = VBoxContainer.new()
	options.add_theme_constant_override(&"separation", 10)
	panel.add_child(options)
	options.add_child(UiFactory.label("选择玩法与卡组", 30))
	_mode = _option(options, "对局模式", ["挑战 AI", "本地双人换手", "新手教学"])
	_mode.item_selected.connect(_mode_changed)
	_difficulty = _option(options, "AI 难度", ["入门", "标准", "进阶"])
	_difficulty.select(1)
	for title: String in ["玩家一 · 卡组", "玩家二 / AI · 卡组"]:
		_decks.append(_option(options, title, []))
	_notice = UiFactory.label("使用合法的 40 张主卡组和 10 张副卡组。换手时会遮挡双方私有信息。", 18, true)
	options.add_child(_notice)
	_start = UiFactory.button("开始对局", start, true)
	options.add_child(_start)
	options.add_child(UiFactory.button("前往卡组构筑", navigation_requested.emit.bind(&"decks")))
	_mode_changed(0)

func _option(parent: VBoxContainer, title: String, items: Array[String]) -> OptionButton:
	parent.add_child(UiFactory.label(title, 18))
	var option: OptionButton = OptionButton.new()
	option.custom_minimum_size.y = 52
	for item: String in items:
		option.add_item(item)
	parent.add_child(option)
	return option

func configure(value: FrontendService) -> void:
	service = value
	_mode.select(1 if service.setup_mode == "hotseat" else 0)
	_mode_changed(_mode.selected)
	if not service.refresh_library():
		_notice.text = service.error
	for option: OptionButton in _decks:
		option.clear()
		for deck: Dictionary in service.playable:
			option.add_item(str(deck.get("name", deck.get("id", ""))))
			option.set_item_metadata(option.item_count - 1, str(deck.get("id", "")))
	_start.disabled = service.playable.is_empty() and _mode.selected != 2
	if _start.disabled:
		_notice.text = "还没有可用于对局的合法卡组，请先完成卡组构筑。"

func _mode_changed(index: int) -> void:
	_difficulty.disabled = index != 0
	for deck: OptionButton in _decks:
		deck.disabled = index == 2
	if service != null:
		_start.disabled = service.playable.is_empty() and index != 2

func start() -> void:
	var modes: Array[String] = ["ai", "hotseat", "tutorial"]
	var options: Dictionary = {"mode": modes[_mode.selected], "difficulty": _difficulty.selected, "seed": randi()}
	if _mode.selected != 2:
		if _decks[0].selected < 0 or _decks[1].selected < 0:
			_notice.text = "请先选择双方卡组。"
			return
		options["playerDeck"] = _decks[0].get_item_metadata(_decks[0].selected)
		options["opponentDeck"] = _decks[1].get_item_metadata(_decks[1].selected)
	_start.disabled = true
	if not service.start_match(options):
		_notice.text = service.error
		_start.disabled = false
		return
	navigation_requested.emit(&"match")
