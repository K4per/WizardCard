class_name DecisionPanel
extends PanelContainer

signal confirmed(generation: String, revision: String, decision_id: String, selected: Array[String])
signal cancelled(generation: String, revision: String, decision_id: String)

var request: DecisionPresentation
var _selected: Array[String] = []
var _column: VBoxContainer
var _confirm: Button
var _options: Dictionary[String, Button] = {}

func _ready() -> void:
	_column = VBoxContainer.new()
	add_child(_column)
	if request != null:
		configure(request)

func configure(value: DecisionPresentation) -> void:
	request = value
	_selected.clear()
	_options.clear()
	if not is_node_ready():
		return
	for child: Node in _column.get_children():
		_column.remove_child(child)
		child.queue_free()
	_column.add_child(UiFactory.label(value.title, 24))
	_column.add_child(UiFactory.label(value.description, 18, true))
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.custom_minimum_size.y = 100
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_column.add_child(scroll)
	var choices: VBoxContainer = VBoxContainer.new()
	choices.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(choices)
	for choice: DecisionChoice in value.choices:
		var option: Button = Button.new()
		option.toggle_mode = true
		option.text = choice.label
		option.clip_text = true
		option.tooltip_text = choice.label
		option.disabled = not choice.available
		option.custom_minimum_size.y = 44
		option.toggled.connect(_on_choice.bind(choice.id, value.generation, value.revision, value.decision_id))
		choices.add_child(option)
		_options[choice.id] = option
	var actions: HBoxContainer = HBoxContainer.new()
	_column.add_child(actions)
	_confirm = UiFactory.button("确认选择", _on_confirm.bind(value.generation, value.revision, value.decision_id), true)
	actions.add_child(_confirm)
	var cancel: Button = UiFactory.button("取消", _on_cancel.bind(value.generation, value.revision, value.decision_id))
	cancel.disabled = not value.may_cancel
	actions.add_child(cancel)
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
		option.text = ("✓ " if _selected.has(choice.id) else "□ ") + choice.label
	_confirm.disabled = not request.has_exact_context() or _selected.size() < request.minimum or _selected.size() > request.maximum
	_confirm.text = "确认选择（%d）" % _selected.size()

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
