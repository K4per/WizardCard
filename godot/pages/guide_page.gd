class_name GuidePage
extends Control

signal navigation_requested(page: StringName)

const TITLES: Array[String] = ["认识状态与区域", "猜拳开局与付费阵法", "解析法术：火球术", "言灵法术：火花", "咒符法术：护盾与连锁"]
var service: FrontendService
var tabs: TabContainer
var chapter_buttons: Array[Button] = []
var _library: LibraryPage
var _notice: Label
var _rule_text: RichTextLabel
var _rule_chapters: OptionButton
var _rules: Dictionary

func _ready() -> void:
	var margin: MarginContainer = MarginContainer.new()
	add_child(margin)
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side: String in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override(StringName("margin_" + side), 24)
	var column: VBoxContainer = VBoxContainer.new()
	margin.add_child(column)
	var heading: HBoxContainer = HBoxContainer.new()
	column.add_child(heading)
	heading.add_child(UiFactory.button("‹ 返回主菜单", navigation_requested.emit.bind(&"menu")))
	heading.add_child(UiFactory.label("巫师指南 · Alpha 2.6.3", 28, true))
	tabs = TabContainer.new()
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(tabs)
	var lessons: VBoxContainer = VBoxContainer.new()
	lessons.name = "新手教程"
	lessons.add_theme_constant_override(&"separation", 12)
	tabs.add_child(lessons)
	lessons.add_child(UiFactory.label("五章均可直接进入；重试从章首开始。完成标记保存在本机。", 18, true))
	for index: int in range(5):
		var button: Button = UiFactory.button("第%d章 · %s" % [index + 1, TITLES[index]], start_chapter.bind(index + 1))
		button.custom_minimum_size.y = 64
		chapter_buttons.append(button)
		lessons.add_child(button)
	_notice = UiFactory.label("", 16, true)
	lessons.add_child(_notice)
	var rules_page: VBoxContainer = VBoxContainer.new()
	rules_page.name = "规则书"
	tabs.add_child(rules_page)
	_rule_chapters = OptionButton.new()
	rules_page.add_child(_rule_chapters)
	_rule_text = RichTextLabel.new()
	_rule_text.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_rule_text.add_theme_font_size_override(&"normal_font_size", 20)
	_rule_text.selection_enabled = true
	rules_page.add_child(_rule_text)
	_rules = JSON.parse_string(FileAccess.get_file_as_string("res://data/rulebook_2_6_3.json")) as Dictionary
	for chapter: Dictionary in _rules.get("chapters", []):
		_rule_chapters.add_item(str(chapter["title"]))
	_rule_chapters.item_selected.connect(_read_rule)
	_read_rule(0)
	_library = load("res://pages/library_page.tscn").instantiate() as LibraryPage
	_library.name = "卡牌图鉴"
	tabs.add_child(_library)
	_library.get_node("Margin/Column/Heading/Back").hide()

func configure(value: FrontendService) -> void:
	service = value
	_library.configure(service)
	var completed: Array = GuideProgress.completed(service.user_directory)
	for index: int in range(5):
		chapter_buttons[index].text = "第%d章 · %s%s" % [index + 1, TITLES[index], "　✓ 已完成" if completed.has(index + 1) else ""]

func _read_rule(index: int) -> void:
	var chapters: Array = _rules.get("chapters", [])
	if index >= 0 and index < chapters.size():
		_rule_text.text = "规则版本 2.6.3\n\n" + str(chapters[index]["title"]) + "\n\n" + str(chapters[index]["text"])
		_rule_text.scroll_to_line(0)

func start_chapter(chapter: int) -> void:
	if not service.start_match({"mode": "tutorial", "chapter": chapter}):
		_notice.text = service.error
		return
	navigation_requested.emit(&"match")
