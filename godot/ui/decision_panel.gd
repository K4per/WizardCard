class_name DecisionPanel
extends PanelContainer

signal confirmed(generation: String, revision: String, decision_id: String, selected: Array[String])
signal cancelled(generation: String, revision: String, decision_id: String)

var request: DecisionPresentation
var _selected: Array[String] = []
var _column: VBoxContainer
var _confirm: Button
var _options: Dictionary[String, Button] = {}
var _labels: Dictionary[String, Label] = {}
var _preview: CardFace
var _preview_text: RichTextLabel
var _counter: Label
var _cancel: Button
var _padding: StyleBoxEmpty
var _candidate_scroll: ScrollContainer

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_padding = StyleBoxEmpty.new()
	add_theme_stylebox_override(&"panel", _padding)
	_column = VBoxContainer.new()
	_column.add_theme_constant_override(&"separation", 8)
	add_child(_column)
	resized.connect(_layout_skin)
	_layout_skin()
	if request != null:
		configure(request)

func _layout_skin() -> void:
	_padding.set_content_margin(SIDE_LEFT, size.x * 0.07)
	_padding.set_content_margin(SIDE_RIGHT, size.x * 0.07)
	_padding.set_content_margin(SIDE_TOP, size.y * 0.18)
	_padding.set_content_margin(SIDE_BOTTOM, size.y * 0.11)
	queue_redraw()

func _draw() -> void:
	draw_texture_rect(BattleArt.texture("effect_popup_complete"), Rect2(Vector2.ZERO, size), false)
	# Replace the baked footer controls with an empty footer segment, so mandatory
	# decisions can actually hide cancellation and live buttons own their states.
	var footer: Rect2 = Rect2(size * Vector2(0.045, 0.79), size * Vector2(0.91, 0.18))
	draw_rect(footer, Color("08131f"))
	draw_line(footer.position, footer.position + Vector2(footer.size.x, 0), Color("715033"), 1)

func configure(value: DecisionPresentation) -> void:
	request = value
	_selected.clear()
	_options.clear()
	_labels.clear()
	if not is_node_ready():
		return
	for child: Node in _column.get_children():
		_column.remove_child(child)
		child.queue_free()
	var heading: HBoxContainer = HBoxContainer.new()
	_column.add_child(heading)
	var title: Label = UiFactory.label(value.title, 22)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	heading.add_child(title)
	_counter = UiFactory.label("", 16)
	heading.add_child(_counter)
	_column.add_child(UiFactory.label(value.description, 16, true))
	var content: HBoxContainer = HBoxContainer.new()
	content.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content.add_theme_constant_override(&"separation", 32)
	_column.add_child(content)
	var scroll: ScrollContainer = ScrollContainer.new()
	_candidate_scroll = scroll
	scroll.custom_minimum_size.y = 160
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	content.add_child(scroll)
	var choices: HBoxContainer = HBoxContainer.new()
	choices.add_theme_constant_override(&"separation", 12)
	scroll.add_child(choices)
	for choice: DecisionChoice in value.choices:
		var option: Button = Button.new()
		option.toggle_mode = true
		option.text = ""
		option.clip_text = true
		option.tooltip_text = choice.label
		option.disabled = not choice.available
		option.custom_minimum_size = Vector2(132, 188)
		option.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		option.alignment = HORIZONTAL_ALIGNMENT_CENTER
		option.add_theme_font_size_override(&"font_size", 14)
		for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
			var choice_style: StyleBoxFlat = StyleBoxFlat.new()
			choice_style.bg_color = Color(0.03, 0.07, 0.12, 0.6)
			choice_style.border_color = Color("64cfe6") if state in ["pressed", "focus", "hover"] else Color("715033")
			choice_style.set_border_width_all(2 if state == "pressed" else 1)
			option.add_theme_stylebox_override(StringName(state), choice_style)
		option.toggled.connect(_on_choice.bind(choice.id, value.generation, value.revision, value.decision_id))
		choices.add_child(option)
		_options[choice.id] = option
		var caption: Label = UiFactory.label(choice.label, 14)
		caption.mouse_filter = Control.MOUSE_FILTER_IGNORE
		caption.position = Vector2(8, 157)
		caption.size = Vector2(116, 24)
		caption.clip_text = true
		caption.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		option.add_child(caption)
		_labels[choice.id] = caption
		if choice.card != null:
			var face: CardFace = CardFace.new()
			face.mouse_filter = Control.MOUSE_FILTER_IGNORE
			option.add_child(face)
			face.position = Vector2(12, 8)
			face.size = Vector2(108, 146)
			face.configure(choice.card)
		else:
			caption.position = Vector2(10, 16)
			caption.size = Vector2(112, 154)
			caption.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var preview_column: VBoxContainer = VBoxContainer.new()
	preview_column.custom_minimum_size.x = 180
	content.add_child(preview_column)
	_preview = CardFace.new()
	_preview.custom_minimum_size = Vector2(180, 190 if get_viewport_rect().size.y >= 900 else 140)
	preview_column.add_child(_preview)
	_preview_text = RichTextLabel.new()
	_preview_text.bbcode_enabled = false
	_preview_text.custom_minimum_size.y = 42
	_preview_text.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_preview_text.add_theme_font_size_override(&"normal_font_size", 14)
	preview_column.add_child(_preview_text)
	if not value.choices.is_empty():
		_show_preview(value.choices[0])
	var actions: HBoxContainer = HBoxContainer.new()
	_column.add_child(actions)
	_confirm = UiFactory.button("确认选择", _on_confirm.bind(value.generation, value.revision, value.decision_id), true)
	BattleArt.button(_confirm)
	var spacer: Control = Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	actions.add_child(spacer)
	actions.add_child(_confirm)
	_cancel = UiFactory.button("返回", _on_cancel.bind(value.generation, value.revision, value.decision_id))
	BattleArt.button(_cancel)
	_cancel.visible = value.may_cancel
	actions.add_child(_cancel)
	_refresh()

func _current(generation: String, revision: String, decision_id: String) -> bool:
	return request != null and request.generation == generation and request.revision == revision and request.decision_id == decision_id

func _on_choice(enabled: bool, id: String, generation: String, revision: String, decision_id: String) -> void:
	if not _current(generation, revision, decision_id):
		return
	var allowed: bool = false
	for choice: DecisionChoice in request.choices:
		if choice.id == id and choice.available:
			allowed = true
			_show_preview(choice)
			break
	if not allowed:
		return
	if enabled and not _selected.has(id):
		if request.maximum == 1:
			_selected.clear()
		_selected.append(id)
	elif not enabled:
		_selected.erase(id)
	_refresh()

func _refresh() -> void:
	for choice: DecisionChoice in request.choices:
		var option: Button = _options[choice.id]
		option.set_pressed_no_signal(_selected.has(choice.id))
		_labels[choice.id].text = ("✓ " if _selected.has(choice.id) else "") + choice.label
	_confirm.disabled = not request.has_exact_context() or _selected.size() < request.minimum or _selected.size() > request.maximum
	_confirm.text = "确认选择（%d）" % _selected.size()
	_counter.text = "已选 %d / %d" % [_selected.size(), request.maximum]

func _show_preview(choice: DecisionChoice) -> void:
	_preview.visible = choice.card != null
	if choice.card != null:
		_preview.configure(choice.card)
		_preview_text.text = choice.label + "\n\n" + ("身份未公开" if choice.card.hidden else choice.card.detail_text())
	else:
		_preview_text.text = choice.label

func _gui_input(event: InputEvent) -> void:
	if not event is InputEventMouseButton or _candidate_scroll == null:
		return
	var mouse: InputEventMouseButton = event
	if not mouse.pressed or mouse.button_index != MOUSE_BUTTON_LEFT:
		return
	var point: Vector2 = mouse.position / size
	if point.y < 0.40 or point.y > 0.70:
		return
	if point.x < 0.09:
		_candidate_scroll.scroll_horizontal -= int(_candidate_scroll.size.x * 0.8)
		accept_event()
	elif point.x > 0.735 and point.x < 0.79:
		_candidate_scroll.scroll_horizontal += int(_candidate_scroll.size.x * 0.8)
		accept_event()

func _on_confirm(generation: String, revision: String, decision_id: String) -> void:
	if not _current(generation, revision, decision_id) or _confirm.disabled:
		return
	# Parent submits this intent; a selected material is never consumed here.
	_confirm.disabled = true
	confirmed.emit(request.generation, request.revision, request.decision_id, _selected.duplicate())

func _on_cancel(generation: String, revision: String, decision_id: String) -> void:
	if not _current(generation, revision, decision_id) or not request.may_cancel:
		return
	cancelled.emit(request.generation, request.revision, request.decision_id)
