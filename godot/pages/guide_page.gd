class_name GuidePage
extends Control

signal navigation_requested(page: StringName)

signal lesson_requested(id: String)

var service: FrontendService
var tabs: TabContainer
var _library: LibraryPage
var _course_list: VBoxContainer
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
	heading.add_child(UiFactory.label(get_node("/root/Text").t("tutorial.guide.title", {"version": ProjectSettings.get_setting("application/config/version")}), 28, true))
	tabs = TabContainer.new()
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(tabs)
	var lessons: VBoxContainer = VBoxContainer.new()
	lessons.name = "新手教程"
	lessons.add_theme_constant_override(&"separation", 12)
	tabs.add_child(lessons)
	_course_list = VBoxContainer.new()
	lessons.add_child(_course_list)
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
	_rules = JSON.parse_string(FileAccess.get_file_as_string("res://data/rulebook_2_6_4.json")) as Dictionary
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
	for child: Node in _course_list.get_children():
		_course_list.remove_child(child)
		child.queue_free()
	var progress := GuideProgress.read(service.user_directory)
	for lesson: Dictionary in service.tutorial_lessons:
		var id := str(lesson.id)
		var key := "completed" if progress.completed.has(id) else ("skipped" if progress.skipped.has(id) else "available")
		var catalog := get_node("/root/Text")
		var label: String = catalog.t("tutorial.guide.entry", {"order": lesson.order, "title": lesson.title, "status": catalog.t("tutorial.guide." + key)})
		var button := UiFactory.button(label, lesson_requested.emit.bind(id))
		_course_list.add_child(button)
	_course_list.visible = not service.tutorial_lessons.is_empty()
	_library.configure(service)

func _read_rule(index: int) -> void:
	var chapters: Array = _rules.get("chapters", [])
	if index >= 0 and index < chapters.size():
		_rule_text.text = "规则版本 %s\n\n" % _rules["version"] + str(chapters[index]["title"]) + "\n\n" + str(chapters[index]["text"])
		_rule_text.scroll_to_line(0)
