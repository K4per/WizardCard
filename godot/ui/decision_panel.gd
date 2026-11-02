class_name DecisionPanel
extends PanelContainer

signal confirmed(generation: String, revision: String, decision_id: String, selected: Array[String])
signal cancelled(generation: String, revision: String, decision_id: String)
signal card_chosen(card: CardPresentation)
signal collapse_requested()

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
var _title: Label
var _description: Label
var _choices: HBoxContainer
var _preview_column: VBoxContainer
var _actions: HBoxContainer
var _response_hint: Label
var _collapse: Button
@export var simple_style: bool = false
@export var content_padding: int = 20

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_padding = StyleBoxEmpty.new()
	add_theme_stylebox_override(&"panel", _padding)
	# Direct construction remains supported by the showcase and older callers.
	# Its structure still comes from the same editor-owned PackedScene.
	if not has_node("Column"):
		var structure: Control = load("res://ui/decision_panel.tscn").instantiate() as Control
		for child: Node in structure.get_children():
			_clear_structure_owner(child)
			structure.remove_child(child)
			add_child(child)
			_own_structure(child)
		structure.free()
	_column = %Column
	_title = %Title
	_counter = %Counter
	_description = %Description
	_candidate_scroll = %CandidateScroll
	_choices = %Choices
	_preview_column = %PreviewColumn
	_preview = %Preview
	_preview_text = %PreviewText
	_actions = %Actions
	_confirm = %Confirm
	_cancel = %Cancel
	_response_hint = %ResponseHint
	_collapse = %Collapse
	_collapse.pressed.connect(collapse_requested.emit)
	resized.connect(_layout_skin)
	_layout_skin()
	if request != null:
		configure(request)

func _clear_structure_owner(node: Node) -> void:
	node.owner = null
	for child: Node in node.get_children():
		_clear_structure_owner(child)

func _own_structure(node: Node) -> void:
	node.owner = self
	for child: Node in node.get_children():
		_own_structure(child)

func _layout_skin() -> void:
	if simple_style:
		for side: int in range(4):
			_padding.set_content_margin(side, 8 if request != null and request.docked else content_padding)
		queue_redraw()
		return
	_padding.set_content_margin(SIDE_LEFT, size.x * 0.07)
	_padding.set_content_margin(SIDE_RIGHT, size.x * 0.07)
	_padding.set_content_margin(SIDE_TOP, size.y * 0.18)
	_padding.set_content_margin(SIDE_BOTTOM, size.y * 0.11)
	queue_redraw()

func _draw() -> void:
	if simple_style:
		draw_rect(Rect2(Vector2.ZERO, size), Color("182631"), true)
		draw_rect(Rect2(Vector2.ZERO, size), Color("78919f"), false, 1)
		return
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
	_layout_skin()
	for child: Node in _choices.get_children():
		_choices.remove_child(child)
		child.queue_free()
	_title.text = value.title
	_counter.text = ""
	_description.text = value.description
	_description.visible = not value.docked and not value.description.is_empty()
	_preview_column.visible = not value.docked and not value.response_mode
	_actions.visible = not value.browse_mode
	_response_hint.visible = false
	_collapse.visible = value.docked
	_candidate_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_AUTO
	_candidate_scroll.vertical_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_candidate_scroll.custom_minimum_size.y = 124 if value.docked else 160
	_disconnect_actions()
	_confirm.visible = not value.browse_mode and (not value.response_mode or value.docked)
	_cancel.visible = value.may_cancel or not value.pass_id.is_empty()
	if value.response_mode and not value.docked:
		_build_response_choices(value)
		return
	_choices.add_theme_constant_override(&"separation", 12)
	for choice: DecisionChoice in value.choices:
		if choice.id == value.pass_id:
			continue
		var option: Button = Button.new()
		option.toggle_mode = not value.browse_mode
		option.text = ""
		option.clip_text = true
		option.tooltip_text = choice.label
		option.disabled = not choice.available
		option.custom_minimum_size = Vector2(116, 124) if value.docked else Vector2(132, 188)
		option.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		option.alignment = HORIZONTAL_ALIGNMENT_CENTER
		option.add_theme_font_size_override(&"font_size", 14)
		for state: String in ["normal", "hover", "pressed", "disabled", "focus"]:
			var choice_style: StyleBoxFlat = StyleBoxFlat.new()
			choice_style.bg_color = Color(0.03, 0.07, 0.12, 0.6)
			choice_style.border_color = Color("64cfe6") if state in ["pressed", "focus", "hover"] else Color("715033")
			choice_style.set_border_width_all(2 if state == "pressed" else 1)
			option.add_theme_stylebox_override(StringName(state), choice_style)
		if value.browse_mode:
			option.pressed.connect(card_chosen.emit.bind(choice.card))
		else:
			option.toggled.connect(_on_choice.bind(choice.id, value.generation, value.revision, value.decision_id))
		_choices.add_child(option)
		_options[choice.id] = option
		var caption: Label = UiFactory.label(choice.card.title if choice.card != null else choice.label, 14)
		caption.mouse_filter = Control.MOUSE_FILTER_IGNORE
		caption.position = Vector2(6, 98) if value.docked else Vector2(8, 157)
		caption.size = Vector2(104, 22) if value.docked else Vector2(116, 24)
		caption.clip_text = true
		caption.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		option.add_child(caption)
		_labels[choice.id] = caption
		if choice.card != null:
			var face: CardFace = CardFace.new()
			face.simple_style = simple_style
			face.mouse_filter = Control.MOUSE_FILTER_IGNORE
			option.add_child(face)
			face.position = Vector2(10, 6) if value.docked else Vector2(12, 8)
			face.size = Vector2(96, 90) if value.docked else Vector2(108, 146)
			face.configure(choice.card)
		else:
			caption.position = Vector2(8, 8)
			caption.size = option.custom_minimum_size - Vector2(16, 16)
			caption.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	if value.browse_mode and value.choices.is_empty():
		_choices.add_child(UiFactory.label("该区域当前没有卡牌。", 16, true))
	_preview.simple_style = simple_style
	if not value.choices.is_empty():
		_show_preview(value.choices[0])
	_confirm.pressed.connect(_on_confirm.bind(value.generation, value.revision, value.decision_id))
	if not simple_style:
		BattleArt.button(_confirm)
	_cancel.pressed.connect(_on_cancel.bind(value.generation, value.revision, value.decision_id))
	if not simple_style:
		BattleArt.button(_cancel)
	_cancel.visible = value.may_cancel or not value.pass_id.is_empty()
	_cancel.text = "放弃" if not value.pass_id.is_empty() else "返回"
	if not value.browse_mode:
		_refresh()

func _disconnect_actions() -> void:
	for button: Button in [_confirm, _cancel]:
		for connection: Dictionary in button.pressed.get_connections():
			var callback: Callable = connection["callable"]
			if callback.get_object() == self:
				button.pressed.disconnect(callback)

func _build_response_choices(value: DecisionPresentation) -> void:
	_choices.add_theme_constant_override(&"separation", 10)
	for choice: DecisionChoice in value.choices:
		var option: Button = Button.new()
		option.custom_minimum_size = Vector2(148, 212) if choice.card != null else Vector2(164, 96)
		option.disabled = not choice.available
		option.tooltip_text = choice.label
		option.pressed.connect(_on_response_choice.bind(choice.id, value.generation, value.revision, value.decision_id))
		if not simple_style:
			BattleArt.button(option)
		_choices.add_child(option)
		if choice.card != null:
			var face: CardFace = CardFace.new()
			face.simple_style = simple_style
			face.mouse_filter = Control.MOUSE_FILTER_IGNORE
			option.add_child(face)
			face.position = Vector2(14, 8)
			face.size = Vector2(120, 160)
			face.configure(choice.card)
			var caption: Label = UiFactory.label(choice.card.title, 13, true)
			caption.mouse_filter = Control.MOUSE_FILTER_IGNORE
			caption.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			caption.position = Vector2(8, 170)
			caption.size = Vector2(132, 34)
			option.add_child(caption)
		else:
			var pass_label: Label = UiFactory.label(choice.label, 17, true)
			pass_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
			pass_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
			pass_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
			pass_label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
			option.add_child(pass_label)

func _on_response_choice(id: String, generation: String, revision: String, decision_id: String) -> void:
	if not _current(generation, revision, decision_id):
		return
	var selected: Array[String] = [id]
	confirmed.emit(generation, revision, decision_id, selected)

func _current(generation: String, revision: String, decision_id: String) -> bool:
	return request != null and request.generation == generation and request.revision == revision and request.decision_id == decision_id

func _on_choice(enabled: bool, id: String, generation: String, revision: String, decision_id: String) -> void:
	if not _current(generation, revision, decision_id):
		return
	var allowed: bool = false
	for choice: DecisionChoice in request.choices:
		if not _options.has(choice.id):
			continue
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
		if not _options.has(choice.id):
			continue
		var option: Button = _options[choice.id]
		option.set_pressed_no_signal(_selected.has(choice.id))
		_labels[choice.id].text = choice.card.title if choice.card != null else choice.label
	_confirm.disabled = not request.has_exact_context() or _selected.size() < request.minimum or _selected.size() > request.maximum
	_confirm.text = "确定"
	_counter.text = "" if request.maximum == 1 else "已选 %d / %d" % [_selected.size(), request.maximum]

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
	if not _current(generation, revision, decision_id):
		return
	if not request.pass_id.is_empty():
		var selected: Array[String] = [request.pass_id]
		confirmed.emit(generation, revision, decision_id, selected)
		return
	if not request.may_cancel:
		return
	cancelled.emit(request.generation, request.revision, request.decision_id)
