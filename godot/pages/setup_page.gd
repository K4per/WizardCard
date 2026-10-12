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
	_mode = %Mode
	_difficulty = %Difficulty
	_decks.assign([%PlayerDeck, %OpponentDeck])
	_notice = %Notice
	_start = %Start
	(%Back as Button).pressed.connect(navigation_requested.emit.bind(&"menu"))
	(%BuildDeck as Button).pressed.connect(navigation_requested.emit.bind(&"decks"))
	_start.pressed.connect(start)
	for caption: String in ["挑战 AI", "本地双人换手", "新手教学"]:
		_mode.add_item(caption)
	for caption: String in ["入门", "标准", "进阶"]:
		_difficulty.add_item(caption)
	_difficulty.select(1)
	_mode.item_selected.connect(_mode_changed)
	_mode_changed(0)

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
