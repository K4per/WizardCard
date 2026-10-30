class_name MatchPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var canvas: FieldCanvas
var _notice: Label
var _phase: Label
var _action_title: Label
var _action_list: VBoxContainer
var _action_button: Button
var _action_popup: PanelContainer
var _detail_shade: ColorRect
var _detail: CardDetail
var _tutorial: Label
var _tutorial_button: Button
var _privacy_layer: ColorRect
var _privacy_title: Label
var _feedback: MatchFeedback
var _cue: Label
var _cue_tween: Tween
var _decision: DecisionPanel
var _shade: ColorRect
var _selected: CardPresentation
var _pending: Dictionary = {}
var _pending_viewer: int = -1
var _global_popup_open: bool = false
var _auto_busy: bool = false
var _quitting: bool = false
var _network_waiting: bool = false
var _confirm: ActionConfirmation
var _leave_dialog: ConfirmationDialog
var _pending_generation: String = ""
var _pending_revision: String = ""
var _collection: PanelContainer
var _collection_title: Label
var _collection_grid: HBoxContainer
var _collection_scroll: ScrollContainer
var _collection_kind: String = ""
var _leaving_network: bool = false
var _force_leave: Button
var _force_dialog: ConfirmationDialog
var _reconnect: Button
var _end_phase: Button
var _phase_selector: PhaseSelector
var _response_mode: OptionButton
var _hand_button: Button
var _status_own: PlayerStatusBar
var _status_opponent: PlayerStatusBar
var _logs_panel: PanelContainer
var _logs_text: RichTextLabel
var _log_seen: Dictionary[String, bool] = {}
var _debug_panel: PanelContainer
var _debug_shade: ColorRect
var _debug_result: RichTextLabel
var _debug_card: LineEdit
var _debug_amount: SpinBox
var _debug_player: OptionButton
var _debug_operation: OptionButton
var _debug_load_kind: OptionButton
var _debug_life_kind: OptionButton
var _debug_infinite_life: CheckBox
var _debug_infinite_mana: CheckBox
var _debug_infinite_load: CheckBox
var _debug_rewind: Button
var _clock_received: int = 0
var _clock_poll_elapsed: float = 0.0
var _hovering: bool = false
var _selection_id: String = ""
var _match_menu: PanelContainer
var _menu_shade: ColorRect
var _tray_active: bool = false
var _tray_suspended: bool = false
var _hand_before_tray: bool = false
var _tray_action_groups: Dictionary[String, Array] = {}
var _selection: ActionSelectionController = ActionSelectionController.new()
var _network_recovery: NetworkRecoveryController = NetworkRecoveryController.new()
var _query_is_decision: bool = false
var _query_confirmed: bool = false
var _query_title: String = ""
var _contextual_query: bool = false
var _target_field: String = ""
var _target_picks: Dictionary[String, Array] = {}
var _target_generation: String = ""
var _target_revision: String = ""
var _target_decision_id: String = ""

func _ready() -> void:
	canvas = %FieldCanvas
	_notice = %Notice
	_phase = %Phase
	_action_button = %MenuPhaseActions
	_reconnect = %MenuReconnect
	_force_leave = %MenuForceLeave
	_match_menu = %MatchMenu
	_menu_shade = %MenuShade
	_end_phase = %EndPhase
	_phase_selector = PhaseSelector.new()
	add_child(_phase_selector)
	_phase_selector.action_chosen.connect(_choose_phase_action)
	_response_mode = OptionButton.new()
	_response_mode.name = "ResponseMode"
	_response_mode.add_theme_font_size_override(&"font_size", 13)
	_response_mode.add_item("全时点", ResponseTiming.Mode.ALL)
	_response_mode.add_item("自动时点", ResponseTiming.Mode.AUTO)
	_response_mode.add_item("忽略时点", ResponseTiming.Mode.IGNORE)
	_response_mode.select(ResponseTiming.Mode.AUTO)
	_response_mode.tooltip_text = "全时点：每个可放弃响应的时点都询问。\n自动时点：有合法响应时询问，否则自动放弃。\n忽略时点：自动放弃可放弃的响应，直到切换模式。\n强制效果选择仍须完成。"
	_response_mode.item_selected.connect(_response_mode_changed)
	add_child(_response_mode)
	_hand_button = %HandToggle
	_status_own = %OwnStatus
	_status_opponent = %OpponentStatus
	_tutorial = %Tutorial
	_tutorial_button = %TutorialContinue
	_cue = %Cue
	_detail = %CardDetail
	_detail.battle_skin = true
	_detail.vertical_battle_layout = true
	%MenuSettings.pressed.connect(_open_settings)
	%MenuSaveReplay.pressed.connect(_save_replay)
	%MenuViewReplay.pressed.connect(_open_replay)
	%MenuLeave.pressed.connect(request_leave)
	%Resume.pressed.connect(_close_match_menu)
	_action_button.pressed.connect(_toggle_action_popup)
	_reconnect.pressed.connect(_reconnect_network)
	_force_leave.pressed.connect(_request_force_leave)
	_end_phase.pressed.connect(_end_current_phase)
	_hand_button.pressed.connect(_toggle_hand)
	%ClearSelection.pressed.connect(_clear_card)
	_tutorial_button.pressed.connect(_continue_tutorial)
	canvas.card_hovered.connect(_hover_card)
	canvas.resized.connect(_position_selected_actions)
	canvas.card_selected.connect(_select_card)
	canvas.selection_cleared.connect(_clear_card)
	canvas.hand_toggle_requested.connect(_toggle_hand)
	canvas.placement_requested.connect(_placement_requested)
	canvas.ring_selected.connect(_ring_selected)
	canvas.zone_clicked.connect(_zone_clicked)
	canvas.layout_changed.connect(_position_selected_actions)
	canvas.layout_changed.connect(_position_phase_button)
	_detail_shade = %DetailShade
	_detail.closed.connect(_clear_card)
	_detail.hide()
	_action_popup = %ActionPopup
	_action_title = %ActionTitle
	_action_list = %ActionList
	_action_popup.hide()
	_feedback = MatchFeedback.new()
	add_child(_feedback)
	_build_dialogs()
	_build_privacy()
	_build_decision()
	_collection = _build_collection()
	_collection.hide()
	_build_logs()
	_build_debug_console()
	resized.connect(_layout_panels)
	get_viewport().size_changed.connect(_layout_panels)
	_layout_panels()

func _build_collection() -> PanelContainer:
	_collection_title = _decision._title
	_collection_scroll = _decision._candidate_scroll
	_collection_grid = _decision._choices
	_decision.card_chosen.connect(_collection_card)
	_decision.collapse_requested.connect(_collapse_tray)
	return _decision

func _layout_panels() -> void:
	if _decision == null:
		return
	# MatchPage can be laid out before the app assigns the viewport size. Use the
	# viewport as a fallback so right-anchored detail never receives negative
	# offsets during that first pass.
	var available: Vector2 = size
	if available.x < 1.0 or available.y < 1.0:
		available = get_viewport_rect().size
	available.x = maxf(available.x, 320.0)
	available.y = maxf(available.y, 240.0)
	PlayerStatusBar.place_corners(_status_own, _status_opponent, available)
	# A full twenty-card fan must leave both corner HUDs readable too.
	canvas.hand_side_clearance = maxf(_status_own.size.x, _status_opponent.size.x) + 52.0
	var tray_height: float = minf(252.0, available.y - 96.0)
	_decision.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
	_decision.offset_left = 12.0
	_decision.offset_right = -12.0
	_decision.offset_top = -tray_height - 8.0
	_decision.offset_bottom = -8.0
	var detail_width: float = clampf(available.x * 0.31, 320.0, 520.0)
	detail_width = minf(detail_width, available.x - 24.0)
	_detail.offset_left = -detail_width - 12
	_detail.offset_right = -12
	_detail.offset_top = 12
	_detail.offset_bottom = -12
	_action_popup.custom_minimum_size.x = minf(220, available.x * 0.8)
	if _logs_panel != null:
		var log_width: float = minf(400.0, maxf(280.0, available.x - 24.0))
		var log_height: float = minf(296.0, available.y - _status_own.size.y - 116.0)
		_logs_panel.size = Vector2(log_width, log_height)
		_logs_panel.position = Vector2(12.0, maxf(96.0, _status_own.position.y - _logs_panel.size.y - 12.0))
	if _debug_panel != null:
		_debug_panel.size = Vector2(minf(600.0, maxf(300.0, available.x - 32.0)), minf(660.0, maxf(320.0, available.y - 32.0)))
		_debug_panel.position = (available - _debug_panel.size) * 0.5
	_position_selected_actions()

func _position_selected_actions() -> void:
	if _action_popup != null and _action_popup.visible:
		_position_action_popup(_selected != null)

func _position_phase_button() -> void:
	if _end_phase == null or canvas == null:
		return
	var casting: Rect2 = canvas._own_casting_region
	if not casting.has_area():
		return
	var left: float = canvas.position.x + casting.end.x + 8
	var width: float = maxf(82, minf(156, size.x - left - 12))
	_end_phase.position = Vector2(minf(left, size.x - width - 12), canvas.position.y + casting.position.y)
	_end_phase.size = Vector2(width, maxf(34, casting.size.y))
	_phase.size = Vector2(width, 88)
	_phase.position = Vector2(_end_phase.position.x, maxf(8, _end_phase.position.y - _phase.size.y - 8))
	_response_mode.size = Vector2(width, 32)
	_response_mode.position = Vector2(_phase.position.x, maxf(8, _phase.position.y - _response_mode.size.y - 6))

func _current_response_mode() -> int:
	return service.response_modes[service.match_viewer] if service != null and service.match_viewer >= 0 and service.match_viewer < service.response_modes.size() else ResponseTiming.Mode.AUTO

func _response_mode_changed(index: int) -> void:
	if service == null or service.match_viewer < 0 or service.match_viewer >= service.response_modes.size():
		return
	service.response_modes[service.match_viewer] = index
	var raw: Variant = service.match_snapshot.get("decision")
	if raw is Dictionary and int((raw as Dictionary).get("kind", -1)) == 0:
		_clear_action_targets()
		_action_popup.hide()
		_detail.hide()
		if not _pending.is_empty():
			_confirm.close_popup()
			_cancel_pending()
	_render_decision()
	_schedule_automatic()

func _build_dialogs() -> void:
	_confirm = %ConfirmAction
	_confirm.confirmed.connect(_commit_pending)
	_confirm.canceled.connect(_cancel_pending)
	_leave_dialog = %ConfirmLeave
	_leave_dialog.confirmed.connect(_leave)
	_force_dialog = %ConfirmForceLeave
	_force_dialog.confirmed.connect(_force_leave_network)

func _build_privacy() -> void:
	_privacy_layer = %PrivacyLayer
	_privacy_title = %PrivacyTitle
	%Reveal.pressed.connect(_reveal)
	_privacy_layer.hide()
	_response_mode.show()
	_response_mode.select(_current_response_mode())

func _build_decision() -> void:
	_shade = %DecisionShade
	_shade.hide()
	_decision = %DecisionPanel
	_decision.confirmed.connect(_decision_confirmed)
	_decision.cancelled.connect(_decision_cancelled)
	_decision.hide()

func _build_logs() -> void:
	var toggle: Button = %LogsToggle
	toggle.pressed.connect(_toggle_logs)
	_logs_panel = PanelContainer.new()
	_logs_panel.name = "MatchLogs"
	_logs_panel.visible = false
	_logs_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_logs_panel.z_index = 12
	add_child(_logs_panel)
	var column: VBoxContainer = VBoxContainer.new()
	column.add_theme_constant_override(&"separation", 6)
	_logs_panel.add_child(column)
	var header: HBoxContainer = HBoxContainer.new()
	column.add_child(header)
	var title: Label = UiFactory.label("对局日志", 16, true)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var close: Button = UiFactory.button("×", _toggle_logs)
	close.tooltip_text = "收起日志"
	close.custom_minimum_size = Vector2(36, 36)
	header.add_child(close)
	_logs_text = RichTextLabel.new()
	_logs_text.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_logs_text.fit_content = false
	_logs_text.scroll_following = true
	_logs_text.bbcode_enabled = false
	_logs_text.custom_minimum_size = Vector2(280, 190)
	_logs_text.add_theme_font_size_override(&"normal_font_size", 14)
	column.add_child(_logs_text)

func _build_debug_console() -> void:
	%MenuDebug.pressed.connect(_open_debug_console)
	_debug_shade = ColorRect.new()
	_debug_shade.name = "DebugShade"
	_debug_shade.color = Color(0.015, 0.025, 0.04, 0.82)
	_debug_shade.mouse_filter = Control.MOUSE_FILTER_STOP
	_debug_shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_debug_shade.hide()
	_debug_shade.z_index = 20
	add_child(_debug_shade)
	_debug_panel = PanelContainer.new()
	_debug_panel.name = "DebugConsole"
	_debug_panel.visible = false
	_debug_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_debug_panel.z_index = 21
	add_child(_debug_panel)
	var outer: VBoxContainer = VBoxContainer.new()
	outer.add_theme_constant_override(&"separation", 8)
	_debug_panel.add_child(outer)
	var heading: HBoxContainer = HBoxContainer.new()
	outer.add_child(heading)
	var title: Label = UiFactory.label("单人调试控制台", 20, true)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	heading.add_child(title)
	var close: Button = UiFactory.button("关闭", _close_debug_console)
	close.custom_minimum_size = Vector2(76, 42)
	heading.add_child(close)
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	outer.add_child(scroll)
	var content: VBoxContainer = VBoxContainer.new()
	content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	content.add_theme_constant_override(&"separation", 8)
	scroll.add_child(content)
	_debug_player = OptionButton.new()
	_debug_player.add_item("玩家一")
	_debug_player.add_item("玩家二")
	_debug_player.set_item_metadata(0, 0)
	_debug_player.set_item_metadata(1, 1)
	_debug_player.select(0)
	_add_debug_row(content, "操作玩家", _debug_player)
	_debug_operation = OptionButton.new()
	for entry: Array in [["从虚空加入卡牌", "add_card"], ["获得魔素", "gain_mana"], ["失去魔素", "lose_mana"], ["获得生命", "gain_life"], ["失去生命", "lose_life"], ["获得临时生命", "gain_temporary_life"], ["失去临时生命", "lose_temporary_life"], ["增加荷载", "gain_load"], ["清除荷载", "clear_load"]]:
		_debug_operation.add_item(str(entry[0]))
		_debug_operation.set_item_metadata(_debug_operation.item_count - 1, str(entry[1]))
	_debug_operation.item_selected.connect(_debug_operation_changed)
	_add_debug_row(content, "操作", _debug_operation)
	_debug_card = LineEdit.new()
	_debug_card.placeholder_text = "卡牌 ID/名称；荷载按来源区分时填来源 ID"
	_debug_card.custom_minimum_size.y = 42
	_add_debug_row(content, "卡牌 / 来源", _debug_card)
	var amount_row: HBoxContainer = HBoxContainer.new()
	_debug_amount = SpinBox.new()
	_debug_amount.min_value = 1
	_debug_amount.max_value = 9999
	_debug_amount.value = 1
	_debug_amount.custom_minimum_size = Vector2(140, 42)
	amount_row.add_child(_debug_amount)
	_debug_amount.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_add_debug_row(content, "数量", amount_row)
	_debug_load_kind = OptionButton.new()
	for entry: Array in [["临时荷载", "temporary"], ["独立荷载", "independent"], ["绑定荷载", "bound"]]:
		_debug_load_kind.add_item(str(entry[0]))
		_debug_load_kind.set_item_metadata(_debug_load_kind.item_count - 1, str(entry[1]))
	_add_debug_row(content, "荷载来源类型", _debug_load_kind)
	var run: Button = UiFactory.button("执行调试操作", _execute_debug_operation, true)
	run.custom_minimum_size.y = 50
	content.add_child(run)
	var toggles: HBoxContainer = HBoxContainer.new()
	toggles.add_theme_constant_override(&"separation", 8)
	content.add_child(toggles)
	_debug_infinite_life = CheckBox.new()
	_debug_infinite_life.text = "无限生命"
	_debug_infinite_life.toggled.connect(_set_debug_infinite.bind("life"))
	toggles.add_child(_debug_infinite_life)
	_debug_infinite_mana = CheckBox.new()
	_debug_infinite_mana.text = "无限魔素"
	_debug_infinite_mana.toggled.connect(_set_debug_infinite.bind("mana"))
	toggles.add_child(_debug_infinite_mana)
	_debug_infinite_load = CheckBox.new()
	_debug_infinite_load.text = "无限荷载上限"
	_debug_infinite_load.toggled.connect(_set_debug_infinite.bind("load"))
	toggles.add_child(_debug_infinite_load)
	_debug_rewind = UiFactory.button("回到上一回合", _rewind_debug, false)
	content.add_child(_debug_rewind)
	_debug_result = RichTextLabel.new()
	_debug_result.custom_minimum_size = Vector2(0, 56)
	_debug_result.fit_content = true
	_debug_result.bbcode_enabled = false
	_debug_result.text = "等待调试命令。"
	outer.add_child(_debug_result)
	_debug_operation_changed(0)

func _add_debug_row(parent: VBoxContainer, caption: String, control: Control) -> void:
	var row: HBoxContainer = HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 10)
	parent.add_child(row)
	var label: Label = UiFactory.label(caption, 15, true)
	label.custom_minimum_size.x = 112
	row.add_child(label)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(control)

func _open_debug_console() -> void:
	if service == null or service.match_mode != "ai":
		return
	_close_match_menu()
	_debug_shade.show()
	_debug_panel.show()
	_layout_panels()
	_debug_panel.move_to_front()
	_debug_card.grab_focus()
	_refresh_debug_controls()

func _close_debug_console() -> void:
	_debug_panel.hide()
	_debug_shade.hide()
	canvas.grab_focus()

func _debug_operation_changed(_index: int) -> void:
	if _debug_operation == null:
		return
	var operation: String = str(_debug_operation.get_item_metadata(_debug_operation.selected))
	_debug_card.editable = operation in ["add_card", "gain_load", "clear_load"]
	_debug_card.placeholder_text = "卡牌 ID 或名称" if operation == "add_card" else "绑定荷载来源卡牌 ID" if operation in ["gain_load", "clear_load"] else "该操作不需要卡牌或来源"
	_debug_load_kind.visible = operation in ["gain_load", "clear_load"]

func _selected_debug_player() -> int:
	return int(_debug_player.get_item_metadata(_debug_player.selected))

func _execute_debug_operation() -> void:
	if service == null or service.match_mode != "ai":
		return
	var operation: String = str(_debug_operation.get_item_metadata(_debug_operation.selected))
	var command: Dictionary = {"player": _selected_debug_player(), "amount": int(_debug_amount.value)}
	var amount: int = int(_debug_amount.value)
	command["operation"] = operation
	if operation in ["lose_mana", "lose_life"]:
		command["amount"] = amount
	elif operation in ["gain_mana", "gain_life", "gain_temporary_life", "gain_load"]:
		command["amount"] = amount
	if operation == "add_card":
		command["cardId"] = _debug_card.text.strip_edges()
	if operation in ["gain_load", "clear_load"]:
		command["loadKind"] = str(_debug_load_kind.get_item_metadata(_debug_load_kind.selected))
		command["sourceId"] = _debug_card.text.strip_edges()
	_submit_debug(command)

func _set_debug_infinite(enabled: bool, resource: String) -> void:
	if service == null or service.match_mode != "ai":
		return
	_submit_debug({"operation": "set_infinite_" + resource, "player": _selected_debug_player(), "enabled": enabled})

func _rewind_debug() -> void:
	_submit_debug({"operation": "rewind_previous_turn"})

func _submit_debug(command: Dictionary) -> void:
	var result: Dictionary = service.match_request(&"debug_command", [command])
	if not bool(result.get("ok", false)):
		_debug_result.text = "失败：" + str(result.get("error", service.error))
		return
	var data: Dictionary = result.get("data", {}) if result.get("data", {}) is Dictionary else {}
	_debug_result.text = str(data.get("message", "调试操作已执行。"))
	if not data.is_empty():
		_debug_result.text += "\n" + JSON.stringify(data)
	var updated: Dictionary = service.snapshot()
	if not updated.is_empty():
		_refresh()
		_refresh_debug_controls()

func _refresh_debug_controls() -> void:
	if _debug_panel == null or service == null:
		return
	var debug: Dictionary = service.match_snapshot.get("debug", {})
	var debug_players: Array = debug.get("players", [])
	var debug_player: Dictionary = debug_players[_selected_debug_player()] if debug_players.size() > _selected_debug_player() else {}
	_debug_infinite_life.set_pressed_no_signal(bool(debug_player.get("infiniteLife", false)))
	_debug_infinite_mana.set_pressed_no_signal(bool(debug_player.get("infiniteMana", false)))
	_debug_infinite_load.set_pressed_no_signal(bool(debug_player.get("infiniteLoad", false)))
	var available: bool = bool(debug.get("canRewind", false))
	_debug_rewind.disabled = not available
	_debug_rewind.tooltip_text = "回到上一回合开始时的完整状态。" if available else str(debug.get("reason", "没有可用的上一回合快照。"))

func _toggle_logs() -> void:
	if _logs_panel == null:
		return
	_logs_panel.visible = not _logs_panel.visible
	if _logs_panel.visible:
		_logs_panel.move_to_front()
		_layout_panels()

func _events_changed(update: Dictionary) -> void:
	if _logs_text == null:
		return
	var reset_log: bool = bool(update.get("reset", false)) or bool(update.get("truncated", false))
	if reset_log:
		_logs_text.clear()
	var events: Array = service.event_log.entries if bool(update.get("truncated", false)) else update.get("events", [])
	for event: Dictionary in events:
		_logs_text.append_text(str(event.get("text", "对局状态已更新")) + "\n")

func _match_changed(_snapshot: Dictionary) -> void:
	_network_waiting = false
	_refresh()

func _clock_changed(_clock: Dictionary) -> void:
	_clock_received = service.clock_received
	_update_clock()

func configure(value: FrontendService) -> void:
	service = value
	_selection.service = service
	_selection.changed.connect(_query_changed)
	_selection.completed.connect(_query_completed)
	_selection.failed.connect(_query_failed)
	service.match_changed.connect(_match_changed)
	service.availability_changed.connect(_availability_changed)
	service.clock_changed.connect(_clock_changed)
	service.events_changed.connect(_events_changed)
	service.session_polled.connect(_selection.poll)
	_events_changed({"reset": true, "events": service.event_log.entries})
	%MenuSettings.text = "设置（继续计时）" if service.match_mode == "network" else "暂停 / 设置"
	%MenuDebug.visible = service.match_mode == "ai"
	_feedback.service = service
	if not service.match_active:
		_notice.text = "没有进行中的对局。"
		return
	if service.match_mode != "network":
		var resumed: Dictionary = service.match_request(&"set_paused", [false])
		if not bool(resumed.get("ok", false)):
			_notice.text = service.error
	if service.match_mode == "hotseat":
		var initial: Dictionary = service.snapshot()
		if not initial.is_empty() and int(initial.get("result", -1)) == -1:
			_cover(int(initial.get("actingPlayer", 0)))
			return
	_refresh()

func _refresh() -> void:
	if service == null or not service.match_active or _pending_viewer >= 0:
		return
	var preserved_id: String = _selection_id if not _selection_id.is_empty() else (_selected.instance_id if _selected != null else (canvas.selected_id if canvas != null else ""))
	var snapshot: Dictionary = service.snapshot()
	if snapshot.is_empty():
		_notice.text = service.error
		return
	_clear_cue()
	if not _pending.is_empty() and (str(snapshot.get("generation", "")) != _pending_generation or str(snapshot.get("revision", "")) != _pending_revision):
		_cancel_pending()
		_confirm.close_popup()
	_restore_tray_hand()
	_tray_action_groups.clear()
	_phase_selector.hide()
	_clear_action_targets()
	if service.match_mode == "hotseat" and int(snapshot.get("actingPlayer", service.match_viewer)) != service.match_viewer and int(snapshot.get("result", -1)) == -1:
		_cover(int(snapshot.get("actingPlayer", 0)))
		return
	var hand_collapsed: bool = canvas.hand_collapsed
	canvas.configure(MatchProjection.field(snapshot))
	canvas.set_background_theme(str(service.settings.get("fieldBackground", "arcane")))
	canvas.hand_collapsed = hand_collapsed
	canvas.focus_mode = Control.FOCUS_ALL
	canvas.set_privacy(false)
	_privacy_layer.hide()
	_response_mode.show()
	_response_mode.select(_current_response_mode())
	var keep_selection: CardPresentation = null
	if not preserved_id.is_empty():
		keep_selection = MatchProjection.card(snapshot, preserved_id)
	_selected = keep_selection
	canvas.selected_id = preserved_id if keep_selection != null else ""
	if keep_selection == null:
		_detail.hide()
	else:
		_detail.configure(keep_selection)
		_detail.show()
	_detail_shade.hide()
	_collection.hide()
	_action_popup.hide()
	_global_popup_open = false
	var actor: int = int(snapshot.get("activePlayer", 0))
	var phase: String = str(snapshot.get("phaseName", ""))
	var players: Array = snapshot.get("players", [])
	var own_turn: String = str((players[actor] as Dictionary).get("ownTurn", "0")) if actor >= 0 and actor < players.size() else "0"
	_phase.text = "第 %s 个己方回合\n玩家 %d · %s" % [own_turn, actor + 1, phase]
	_clock_received = service.clock_received
	_update_status()
	_hand_button.text = "▴ 手牌" if canvas.hand_collapsed else "▾ 手牌"
	if int(snapshot.get("result", -1)) != -1:
		canvas.stop_feedback()
		_phase.text = "对局结束 · " + ("平局" if int(snapshot["result"]) > 1 else "玩家 %d 获胜" % [int(snapshot["result"]) + 1])
		_notice.text = "对局已结束，可保存并查看复盘。"
	_tutorial.visible = snapshot.get("tutorial") is Dictionary
	_tutorial_button.visible = _tutorial.visible
	if _tutorial.visible:
		var lesson: Dictionary = snapshot["tutorial"]
		_tutorial.text = str(lesson.get("title", "教学")) + "\n" + str(lesson.get("hint", ""))
		_tutorial_button.disabled = not bool(lesson.get("canContinue", false))
	_render_actions()
	_render_decision()
	_render_opening()
	_schedule_automatic()

func _render_actions() -> void:
	var snapshot: Dictionary = service.match_snapshot
	var summary: Dictionary = snapshot.get("actionSummary", {})
	var safe: Variant = summary.get("safeAdvance")
	_end_phase.disabled = not safe is Dictionary or int(safe.get("type", -1)) != KernelContract.CommandKind.AdvancePhase or _privacy_layer.visible or _network_waiting or not _pending.is_empty() or bool(snapshot.get("paused", false))
	_action_button.text = "阶段操作"
	if bool(snapshot.get("technicalPaused", false)):
		_notice.text = "规则执行故障：对局已技术暂停，时钟已冻结。请导出诊断或退出。"
		%MenuSaveReplay.text = "导出复盘与诊断"
		_action_popup.hide()
		return
	if _selected != null:
		_show_action_list(_selected.instance_id, _selected.title + " · 操作", true)
	elif _global_popup_open:
		_show_action_list("0", "阶段操作", false)
	else:
		_action_popup.hide()

func _end_current_phase() -> void:
	if _end_phase.disabled:
		return
	_query_is_decision = true
	_query_title = "结束当前阶段"
	_contextual_query = false
	_selection.begin("0", KernelContract.CommandKind.AdvancePhase)

func _choose_phase_action(action: Dictionary) -> void:
	if action.has("command"):
		_apply_result(service.match_request(&"submit_command", [service.match_viewer, action["command"], str(service.match_snapshot.get("generation", "")), str(service.match_snapshot.get("revision", ""))]))

func _update_status() -> void:
	if service == null or service.match_snapshot.is_empty():
		return
	var snapshot: Dictionary = service.match_snapshot
	var players: Array = snapshot.get("players", [])
	if players.size() != 2:
		return
	var clock: Dictionary = snapshot.get("clock", {})
	var remaining: Array = clock.get("remainingMs", [0, 0])
	var running_players: Array = clock.get("runningPlayers", [])
	var bars: Array[PlayerStatusBar] = [_status_own, _status_opponent]
	var loads: Array = snapshot.get("loads", [])
	var cards: Array = snapshot.get("cards", [])
	var debug: Dictionary = snapshot.get("debug", {})
	var debug_players: Array = debug.get("players", [])
	for index: int in range(2):
		var player: int = service.match_viewer if index == 0 else 1 - service.match_viewer
		var values: Dictionary = players[player]
		var clock_text: String = ""
		var clock_warning: bool = false
		if bool(clock.get("enabled", false)) and remaining.size() > player:
			var milliseconds: int = int(remaining[player])
			var player_running: bool = bool(running_players[player]) if running_players.size() > player else int(clock.get("runningPlayer", -1)) == player
			if bool(clock.get("running", false)) and player_running:
				milliseconds -= Time.get_ticks_msec() - _clock_received
			var seconds: int = maxi(0, ceili(float(milliseconds) / 1000.0))
			clock_text = "%02d:%02d" % [seconds / 60, seconds % 60]
			clock_warning = seconds <= 30
		var player_debug: Dictionary = debug_players[player] if debug_players.size() > player else {}
		bars[index].set_status("我方" if index == 0 else "对手", player, values, loads, cards, player_debug, clock_text, clock_warning)

func _process(_delta: float) -> void:
	if service != null and service.match_active:
		_update_clock()

func _update_clock() -> void:
	var clock: Dictionary = service.match_snapshot.get("clock", {})
	var remaining: Array = clock.get("remainingMs", [0, 0])
	var running: Array = clock.get("runningPlayers", [false, false])
	var bars: Array[PlayerStatusBar] = [_status_own, _status_opponent]
	for index: int in range(2):
		var player: int = service.match_viewer if index == 0 else 1 - service.match_viewer
		var milliseconds: int = int(remaining[player]) if remaining.size() > player else 0
		if bool(clock.get("running", false)) and running.size() > player and bool(running[player]):
			milliseconds -= Time.get_ticks_msec() - _clock_received
		var seconds: int = maxi(0, ceili(float(milliseconds) / 1000.0))
		bars[index].set_clock("%02d:%02d" % [seconds / 60, seconds % 60] if bool(clock.get("enabled", false)) else "", seconds <= 30)


func _hover_card(card: CardPresentation) -> void:
	if _selected != null or _selection.active or _privacy_layer.visible or _decision.visible or _collection.visible or _match_menu.visible:
		return
	_hovering = card != null
	if card == null:
		_detail.hide()
		return
	_detail.configure(card)
	_detail.show()
	_detail.move_to_front()
	_layout_panels()

func _render_opening() -> void:
	var snapshot: Dictionary = service.match_snapshot
	if not bool(snapshot.get("opening", false)) or int(snapshot.get("result", -1)) != -1 or bool(snapshot.get("aiTurn", false)):
		return
	var confirmed: Array = snapshot.get("initialFormationConfirmed", [false, false])
	if confirmed.size() > service.match_viewer and bool(confirmed[service.match_viewer]):
		_notice.text = "初始阵法已确认，等待对手确认。"
		return
	_close_match_menu()
	_phase.text = "开局 · 选择等级 1 初始阵法"
	_query_is_decision = true
	_query_title = "选择初始阵法"
	_contextual_query = false
	_selection.begin("0", KernelContract.CommandKind.SelectInitialFormation)

func _show_action_list(source: String, title: String, contextual: bool) -> void:
	if _network_waiting or bool(service.match_snapshot.get("paused", false)):
		_action_popup.hide()
		return
	_query_is_decision = false
	_query_confirmed = false
	_query_title = title
	_contextual_query = contextual
	_selection.begin(source)

func _action_visible(action: Dictionary) -> bool:
	var command: Dictionary = action.get("command", {})
	var type: int = int(command.get("type", -1))
	# Casting is a committed phase. Returning a spell to hand here is
	# deliberately unavailable; the card can still be abandoned in its other
	# legal phases and through response decisions.
	return not (type == 8 and int(service.match_snapshot.get("phase", -1)) == 3)

func _position_action_popup(contextual: bool) -> void:
	if not _action_popup.visible:
		return
	var available: Vector2 = size
	if available.x < 1.0 or available.y < 1.0:
		available = get_viewport_rect().size
	var header_height: float = 0.0 if contextual else 30.0
	var desired_height: float = clampf(header_height + _action_list.get_combined_minimum_size().y, 44, maxf(44, minf(500, available.y - 32)))
	_action_popup.size = Vector2(maxf(160, minf(220, available.x - 24)), desired_height)
	var point: Vector2
	var max_right: float = available.x - _action_popup.size.x - 8
	if contextual and _detail.visible:
		max_right = minf(max_right, size.x - _detail.size.x - _action_popup.size.x - 20)
	if contextual and _selected != null:
		var bounds: Rect2 = canvas.card_bounds(_selected)
		if not bounds.has_area():
			for row: Dictionary in service.match_snapshot.get("cards", []):
				if str(row.get("id", "0")) != _selected.instance_id:
					continue
				var kind: String = "side_deck" if int(row.get("zone", -1)) == 9 else "ash"
				for hit: Dictionary in canvas._zone_hits:
					if str(hit.get("kind", "")) == kind and int(hit.get("owner", -1)) == int(row.get("owner", -2)):
						bounds = hit["rect"]
						break
				break
		point = canvas.global_position - global_position + Vector2(bounds.get_center().x - _action_popup.size.x * 0.5, bounds.position.y - _action_popup.size.y - 8)
		if point.y < 8:
			point.y = bounds.end.y + 8
	else:
		point = _action_button.global_position - global_position + Vector2(_action_button.size.x - _action_popup.size.x, _action_button.size.y + 4)
	_action_popup.position = Vector2(clampf(point.x, 8, maxf(8, max_right)), clampf(point.y, 8, maxf(8, available.y - _action_popup.size.y - 8)))

func _query_changed(options: ActionOptions) -> void:
	if not options.matches(service.match_snapshot, service.match_viewer):
		return
	canvas.clear_action_targets()
	if _query_title == "结束当前阶段" and options.field == KernelContract.ActionField.Choice:
		var phases: Array[Dictionary] = []
		for option: Dictionary in options.options:
			phases.append({"label": str(option.get("label", "结束阶段")), "command": {"type": KernelContract.CommandKind.AdvancePhase, "gate": str(service.match_snapshot.get("phaseGate", "0")), "skipCast": str(option["value"]) == "1"}})
		if phases.size() > 1:
			_phase_selector.open_for(_end_phase, phases)
		elif phases.size() == 1:
			_choose_phase_action(phases[0])
		return
	if options.field == KernelContract.ActionField.Operation and not _query_is_decision:
		for child: Node in _action_list.get_children():
			_action_list.remove_child(child)
			child.queue_free()
		for option: Dictionary in options.options:
			var operation: int = int(option.get("value", -1))
			if not _contextual_query and operation not in [KernelContract.CommandKind.Advance, KernelContract.CommandKind.AdvancePhase, KernelContract.CommandKind.PassResponse, KernelContract.CommandKind.Choose, KernelContract.CommandKind.Surrender]:
				continue
			if operation == KernelContract.CommandKind.Abandon and int(service.match_snapshot.get("phase", -1)) == 3:
				continue
			var label: String = MatchProjection.action_label(service.match_snapshot, {"source": str(_selection.intent.get("source", "0")), "command": {"type": operation}})
			if operation == KernelContract.CommandKind.ActivateAbility:
				label = str(option.get("label", "发动能力"))
			var button: Button = UiFactory.button(label, _selection.choose.bind(str(option["value"])))
			button.custom_minimum_size = Vector2(196, 44)
			_action_list.add_child(button)
		if _action_list.get_child_count() == 0:
			_action_list.add_child(UiFactory.label("暂无合法操作。", 14, true))
		_action_title.text = _query_title
		_action_title.visible = not _contextual_query
		_action_popup.reset_size()
		_action_popup.show()
		_action_popup.move_to_front()
		_position_action_popup(_contextual_query)
		return
	var current_decision: Variant = service.match_snapshot.get("decision")
	if current_decision is Dictionary and int(current_decision.get("kind", -1)) == KernelContract.DecisionKind.ReturnSpell:
		if options.field == KernelContract.ActionField.Choice and options.options.size() == 1 and not options.has_more:
			_selection.choose(str(options.options[0]["value"]))
			return
		if options.field == KernelContract.ActionField.Ring:
			var host: String = ""
			for entry: Dictionary in _selection.intent.get("selections", []):
				if int(entry.get("field", -1)) == KernelContract.ActionField.Choice:
					var value: PackedStringArray = str(entry.get("value", "")).split(":")
					if value.size() == 2 and int(value[0]) == KernelContract.ChoiceKind.Card:
						host = value[1]
			if not host.is_empty():
				var rings: PackedInt32Array = []
				for option: Dictionary in options.options:
					rings.append(int(option["value"]))
				var ring_targets: Dictionary[String, PackedInt32Array] = {host: rings}
				canvas.set_action_targets([], ring_targets)
				_decision.hide()
				_restore_tray_hand()
				_action_popup.hide()
				_detail.hide()
				_notice.text = "选择高亮环位完成回位。"
				return
	if options.options.size() == 1 and not options.has_more and options.field in [KernelContract.ActionField.Host, KernelContract.ActionField.Ability, KernelContract.ActionField.Replacement]:
		_selection.choose(str(options.options[0]["value"]))
		return
	var request: DecisionPresentation = DecisionPresentation.new()
	request.generation = options.generation
	request.revision = options.revision
	request.decision_id = options.decision_id
	var titles: Array[String] = ["选择操作", "选择卡牌", "选择宿主", "选择替换阵法", "选择材料", "选择目标", "选择支付卡牌", "选择连锁目标", "选择能力", "选择环位", "完成规则选择", "确认"]
	request.title = titles[options.field]
	request.description = "选择选项后确认；费用仅在完整命令确认后支付。"
	request.docked = true
	request.response_mode = _query_is_decision and options.field == KernelContract.ActionField.Operation
	request.may_cancel = not _query_is_decision
	var card_ids: Array[String] = []
	for option: Dictionary in options.options:
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = str(option["value"])
		choice.label = str(option.get("label", "选择"))
		var card_id: String = str(option.get("card", "0"))
		choice.card = MatchProjection.card(service.match_snapshot, card_id)
		if int(option.get("player", -1)) >= 0:
			choice.label = "玩家 %d" % [int(option["player"]) + 1]
		if card_id != "0":
			card_ids.append(card_id)
		if options.field == KernelContract.ActionField.Operation and int(choice.id) == KernelContract.CommandKind.Surrender:
			continue
		if options.field == KernelContract.ActionField.Operation and int(choice.id) == KernelContract.CommandKind.PassResponse:
			request.pass_id = choice.id
		request.choices.append(choice)
	if int(_selection.intent.get("offset", 0)) > 0:
		var previous: DecisionChoice = DecisionChoice.new()
		previous.id = "@previous"
		previous.label = "上一页"
		request.choices.append(previous)
	if options.has_more:
		var more: DecisionChoice = DecisionChoice.new()
		more.id = "@next"
		more.label = "下一页"
		request.choices.append(more)
	canvas.set_action_targets(card_ids)
	_decision.configure(request)
	_decision.show()
	_action_popup.hide()
	_detail.hide()
	_detail_shade.hide()
	_show_tray()
	_layout_panels()

func _query_completed(command: Dictionary, intent: Dictionary) -> void:
	var generation: String = _selection.generation
	var revision: String = _selection.revision
	var selected: Dictionary = service.match_request(&"select_intent", [service.match_viewer, intent, generation, revision])
	if not bool(selected.get("ok", false)):
		_query_failed(service.error)
		return
	if bool(selected.get("data", {}).get("pending", false)):
		_selection.refresh()
		return
	_decision.hide()
	_restore_tray_hand()
	_pending_generation = generation
	_pending_revision = revision
	if _query_is_decision or _query_confirmed:
		_selection.cancel()
		_apply_result(service.match_request(&"confirm_action", [generation, revision]))
	else:
		_pending = {"command": command}
		_confirm.dialog_text = MatchProjection.action_detail(service.match_snapshot, {"command": command, "label": _query_title, "source": str(command.get("card", "0"))}) + "\n\n确认执行？"
		_confirm.popup_centered()

func _query_failed(message: String) -> void:
	_notice.text = message
	_action_popup.hide()

func _pick_action_target(id: String) -> void:
	if _selection.active and not _selection.current():
		_clear_action_targets()
		return
	if _selection.current() and _selection.options != null:
		for option: Dictionary in _selection.options.options:
			if str(option.get("card", "0")) == id:
				_query_confirmed = true
				_selection.choose(str(option["value"]))
				return

func _ring_selected(_host_id: String, ring_index: int) -> void:
	if _selection.current() and _selection.options != null and _selection.options.field == KernelContract.ActionField.Ring:
		_query_confirmed = true
		_selection.choose(str(ring_index))

func _clear_action_targets() -> void:
	_selection.cancel()
	_target_decision_id = ""
	if canvas != null:
		canvas.clear_action_targets()

func _toggle_action_popup() -> void:
	_close_match_menu()
	if _selected != null:
		_clear_card()
	_global_popup_open = not _global_popup_open
	_render_actions()

func _zone_clicked(kind: String, owner: int) -> void:
	if _privacy_layer.visible or _decision.visible:
		return
	if kind == "side_deck":
		if owner == service.match_viewer:
			_open_collection("side_deck", owner)
		else:
			_notice.text = "对手副卡组内容不公开。"
	elif kind == "ash":
		_open_collection("ash", owner)
	elif kind == "main_deck":
		_notice.text = "主牌库内容保持未知。"

func _open_collection(kind: String, owner: int) -> void:
	if kind == "side_deck" and owner != service.match_viewer:
		return
	# Collections and mandatory choices share one tray. A collapsed rule choice
	# still owns that tray: replacing its request would discard Confirm/Pass.
	if service.match_snapshot.get("decision") is Dictionary or bool(service.match_snapshot.get("opening", false)):
		_notice.text = "请先完成当前响应或选择。"
		return
	_clear_card()
	_collection_kind = kind
	var request: DecisionPresentation = DecisionPresentation.new()
	request.title = ("我方" if owner == service.match_viewer else "对手") + (" · 副卡组" if kind == "side_deck" else " · 灰烬区")
	request.browse_mode = true
	request.docked = true
	var wanted_zone: int = 9 if kind == "side_deck" else 6
	var cards: Array[CardPresentation] = []
	for row: Dictionary in service.match_snapshot.get("cards", []):
		if int(row.get("zone", -1)) == wanted_zone and int(row.get("owner", -1)) == owner:
			cards.append(CardPresentation.from_projection(row))
	for card: CardPresentation in cards:
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = card.instance_id
		choice.label = card.title
		choice.card = card
		request.choices.append(choice)
	_decision.configure(request)
	_show_tray()
	_layout_panels()

func _collection_card(card: CardPresentation) -> void:
	_close_collection()
	_select_card(card)

func _close_collection() -> void:
	_collection.hide()
	_collection_kind = ""
	_restore_tray_hand()
	_detail_shade.hide()

func _show_tray() -> void:
	if not _tray_active:
		_hand_before_tray = canvas.hand_collapsed
	_tray_active = true
	_tray_suspended = false
	canvas.hand_collapsed = true
	canvas.queue_redraw()
	_shade.hide()
	_decision.show()
	_decision.move_to_front()
	_hand_button.text = "手牌"

func _restore_tray_hand() -> void:
	if _tray_active:
		canvas.hand_collapsed = _hand_before_tray
		canvas.queue_redraw()
	_tray_active = false
	_tray_suspended = false

func _collapse_tray() -> void:
	_decision.hide()
	_shade.hide()
	_tray_active = false
	_tray_suspended = true
	canvas.hand_collapsed = false
	canvas.queue_redraw()
	_hand_button.text = "展开选择"

func _render_decision() -> void:
	var snapshot: Dictionary = service.match_snapshot
	var decision: Variant = snapshot.get("decision")
	if not decision is Dictionary or int(snapshot.get("result", -1)) != -1 or int(decision.get("player", -1)) != service.match_viewer or bool(snapshot.get("technicalPaused", false)) or bool(snapshot.get("aiTurn", false)):
		_decision.hide()
		_shade.hide()
		return
	if not ResponseTiming.pass_action(snapshot, _current_response_mode()).is_empty():
		_decision.hide()
		_restore_tray_hand()
		return
	_query_is_decision = true
	_query_title = "规则选择"
	_contextual_query = false
	var kind: int = int(decision.get("kind", -1))
	_selection.begin("0", -1 if kind == KernelContract.DecisionKind.Response else KernelContract.CommandKind.Choose)

func _select_card(card: CardPresentation) -> void:
	if _privacy_layer.visible:
		return
	if _selection.active and _selection.options != null and _selection.options.field != KernelContract.ActionField.Operation:
		_pick_action_target(card.instance_id)
		return
	if _decision.visible:
		return
	_selected = card
	_selection_id = card.instance_id
	_global_popup_open = false
	canvas.selected_id = card.instance_id
	canvas.queue_redraw()
	_detail.configure(card)
	_detail_shade.hide()
	_detail.show()
	_detail.move_to_front()
	_layout_panels()
	_render_actions()

func _clear_card() -> void:
	if not _target_decision_id.is_empty():
		return
	_clear_action_targets()
	_selected = null
	_selection_id = ""
	_detail.hide()
	_detail_shade.hide()
	canvas.selected_id = ""
	canvas.queue_redraw()
	if canvas.visible and canvas.focus_mode != Control.FOCUS_NONE:
		canvas.grab_focus()
	if service != null and not service.match_snapshot.is_empty():
		_render_actions()

func dismiss_cancel() -> bool:
	# Keep Esc local to transient match UI even if the root receives it first.
	if _phase_selector.visible:
		_phase_selector.hide()
		return true
	if _decision.visible and _decision.request != null and _decision.request.browse_mode:
		_close_collection()
		return true
	if _decision.visible and not _query_is_decision and _selection.active:
		_decision_cancelled("", "", "")
		return true
	if _decision.visible or _privacy_layer.visible:
		return true
	if _selection.active:
		if not _target_decision_id.is_empty():
			return true
		if _selected != null:
			_global_popup_open = false
			_clear_card()
		else:
			_selection.cancel()
			_action_popup.hide()
			_global_popup_open = false
		return true
	if _match_menu.visible:
		_close_match_menu()
		return true
	if _confirm.visible:
		_confirm.close_popup()
		_cancel_pending()
		return true
	if _collection.visible:
		_close_collection()
		return true
	if _detail.visible or _action_popup.visible or _global_popup_open or _selected != null:
		_global_popup_open = false
		_clear_card()
		return true
	_open_match_menu()
	return true

func _open_match_menu() -> void:
	_menu_shade.show()
	_menu_shade.move_to_front()
	_match_menu.show()
	_match_menu.move_to_front()
	%Resume.grab_focus()

func _close_match_menu() -> void:
	_match_menu.hide()
	_menu_shade.hide()
	if canvas != null and canvas.focus_mode != Control.FOCUS_NONE:
		canvas.grab_focus()

func _toggle_hand() -> void:
	if _privacy_layer.visible:
		return
	if _decision.visible:
		_collapse_tray()
		return
	if _tray_suspended:
		_show_tray()
		return
	canvas.set_hand_collapsed(not canvas.hand_collapsed)
	_hand_button.text = "▴ 手牌" if canvas.hand_collapsed else "▾ 手牌"

func _placement_requested(card_id: String, _host_id: String, _ring_index: int) -> void:
	var card: CardPresentation = MatchProjection.card(service.match_snapshot, card_id)
	if card != null:
		_select_card(card)

func _choose_action(action: Dictionary) -> void:
	var command: Dictionary = action.get("command", {})
	_query_is_decision = false
	_query_title = str(action.get("label", "确认操作"))
	_selection.begin(str(command.get("card", "0")), int(command.get("type", -1)))

func _cancel_pending() -> void:
	_pending.clear()
	service.match_request(&"cancel_action")

func _commit_pending() -> void:
	if _pending.is_empty():
		return
	_pending.clear()
	var result: Dictionary = service.match_request(&"confirm_action", [_pending_generation, _pending_revision])
	_apply_result(result)

func _decision_confirmed(generation: String, revision: String, decision_id: String, selected: Array[String]) -> void:
	if selected.size() != 1 or _network_waiting or _privacy_layer.visible or not _selection.current() or _selection.options == null:
		return
	if generation != _selection.generation or revision != _selection.revision or decision_id != _selection.options.decision_id:
		return
	_query_confirmed = true
	_selection.choose(selected[0])

func _decision_cancelled(_generation: String, _revision: String, _decision_id: String) -> void:
	if _query_is_decision:
		return
	_selection.cancel()
	_decision.hide()
	_restore_tray_hand()
	service.match_request(&"cancel_action")
	_render_actions()

func _apply_result(result: Dictionary) -> void:
	_clear_action_targets()
	_decision.hide()
	_shade.hide()
	_restore_tray_hand()
	if not bool(result.get("ok", false)):
		_notice.text = service.error
		_refresh()
		return
	var data: Dictionary = result.get("data", {})
	if bool(data.get("pending", false)):
		_network_waiting = true
		_network_recovery.submitted()
		_notice.text = "操作已发送，等待房主确认。"
		_render_actions()
		return
	if not bool(data.get("replaySaved", true)):
		_notice.text = "复盘自动保存失败：" + str(data.get("saveError", "请手动保存"))
	else:
		_notice.text = "操作已执行。"
	if service.match_mode == "hotseat":
		_cover(-1)
	_refresh()
	# The authoritative projection is installed before any decorative feedback.
	_feedback.present(data)
	if service.match_mode != "hotseat":
		_show_cue(data)

func _show_cue(data: Dictionary) -> void:
	_clear_cue()
	if bool(service.settings.get("reducedMotion", false)):
		return
	var cues: Array = data.get("cues", [])
	if cues.is_empty():
		return
	var latest: Dictionary = cues.back()
	var message: String = str(latest.get("text", ""))
	if message.is_empty():
		return
	_cue.text = message
	_cue.modulate.a = 1.0
	_cue.show()
	_cue_tween = create_tween()
	_cue_tween.tween_interval(minf(1.5, maxf(0.2, float(latest.get("duration", 0.7)))))
	_cue_tween.tween_property(_cue, "modulate:a", 0.0, 0.45)
	_cue_tween.tween_callback(_cue.hide)

func _clear_cue() -> void:
	if _cue_tween != null and _cue_tween.is_valid():
		_cue_tween.kill()
	_cue_tween = null
	if is_instance_valid(_cue):
		_cue.hide()
		_cue.modulate.a = 1.0

func _exit_tree() -> void:
	if service != null and service.session_polled.is_connected(_selection.poll):
		service.session_polled.disconnect(_selection.poll)
	_clear_cue()
	_clear_action_targets()
	if not _pending.is_empty():
		_cancel_pending()

func _cover(next_viewer: int) -> void:
	_logs_panel.hide()
	_logs_text.clear()
	_pending_viewer = next_viewer
	_clear_cue()
	_clear_action_targets()
	_clear_card()
	canvas.set_privacy(true)
	_privacy_layer.show()
	_privacy_layer.move_to_front()
	_privacy_title.text = "请将设备交给玩家 %d" % [next_viewer + 1] if next_viewer >= 0 else "正在确认下一位玩家"
	_decision.hide()
	_shade.hide()
	_response_mode.hide()

func _reveal() -> void:
	if _pending_viewer < 0:
		return
	service.match_viewer = _pending_viewer
	_pending_viewer = -1
	_refresh()

func _schedule_automatic() -> void:
	if _auto_busy or _pending_viewer >= 0 or _privacy_layer.visible or _decision.visible or _tray_suspended or _network_waiting or not _pending.is_empty():
		return
	if not AutomaticActionController.request(service.match_snapshot, _current_response_mode()).is_empty():
		_auto_busy = true
		_automatic_step.call_deferred(str(service.match_snapshot.get("generation", "")), str(service.match_snapshot.get("revision", "")))

func _automatic_step(generation: String, revision: String) -> void:
	await get_tree().create_timer(0.1).timeout
	_auto_busy = false
	if not is_inside_tree() or _privacy_layer.visible or _network_waiting or not _pending.is_empty() or str(service.match_snapshot.get("generation", "")) != generation or str(service.match_snapshot.get("revision", "")) != revision:
		return
	var request: Dictionary = AutomaticActionController.request(service.match_snapshot, _current_response_mode())
	if not request.is_empty():
		_apply_result(service.match_request(request["method"], request["arguments"]))

func _continue_tutorial() -> void:
	var snapshot: Dictionary = service.match_snapshot
	var result: Dictionary = service.match_request(&"continue_tutorial", [str(snapshot.get("generation", "")), str(snapshot.get("revision", ""))])
	if not bool(result.get("ok", false)):
		_notice.text = service.error
	_refresh()

func _open_settings() -> void:
	_close_match_menu()
	if service.match_mode != "network":
		var response: Dictionary = service.match_request(&"set_paused", [true])
		if not bool(response.get("ok", false)):
			_notice.text = service.error
			return
	navigation_requested.emit(&"settings")

func _open_replay() -> void:
	_close_match_menu()
	if int(service.match_snapshot.get("result", -1)) == -1:
		_notice.text = "完整复盘在对局结束后开放；现在可以先保存记录。"
		return
	if not service.save_replay():
		_notice.text = service.error
		return
	if service.match_mode != "network":
		var response: Dictionary = service.match_request(&"set_paused", [true])
		if not bool(response.get("ok", false)):
			_notice.text = service.error
			return
	navigation_requested.emit(&"replay")

func _save_replay() -> void:
	if bool(service.match_snapshot.get("technicalPaused", false)):
		var diagnostic: Dictionary = service.match_request(&"save_diagnostic")
		if not bool(diagnostic.get("ok", false)):
			_notice.text = service.error
			return
	_notice.text = "复盘已保存：" + service.last_replay_path if service.save_replay() else service.error

func request_leave(quit_application: bool = false) -> void:
	_quitting = quit_application
	_leave_dialog.dialog_text = "投降并等待终局复盘保存后离开房间？" if service.match_mode == "network" else "保存终局复盘，并结束当前对局？"
	_leave_dialog.popup_centered()

func _leave() -> void:
	if service.match_mode == "network":
		_leaving_network = true
		var response: Dictionary = service.network_request({"operation": "leave", "force": false})
		if not bool(response.get("ok", false)):
			_notice.text = service.error
			_force_leave.show()
			return
	elif not service.leave_match():
		_notice.text = "保存终局记录失败，对局仍保留：" + service.error
		return
	_finish_leave()

func _finish_leave() -> void:
	if _quitting:
		get_tree().quit()
	else:
		navigation_requested.emit(&"menu")

func _request_force_leave() -> void:
	_force_dialog.popup_centered()

func _force_leave_network() -> void:
	if bool(service.network_request({"operation": "leave", "force": true}).get("ok", false)):
		_finish_leave()
	else:
		_notice.text = service.error

func _reconnect_network() -> void:
	service.network_request({"operation": "reconnect"})
	_notice.text = service.error if not service.error.is_empty() else "正在重新连接…"

func network_updated(state: Dictionary) -> void:
	if service == null or service.match_mode != "network":
		return
	_network_recovery.update(state)
	_network_waiting = _network_recovery.waiting
	if not bool(state.get("exists", state.has("room"))):
		if _leaving_network:
			_finish_leave()
		else:
			_notice.text = str(state.get("notice", "房间已关闭"))
		return
	if _network_recovery.blocked:
		_clear_action_targets()
		_clear_cue()
		canvas.stop_feedback()
		canvas._cancel_pointer_state()
		_confirm.close_popup()
		_cancel_pending()
		_notice.text = _network_recovery.notice
	_reconnect.visible = _network_recovery.blocked and not bool(state.get("host", false))
	_force_leave.visible = _network_recovery.blocked or _leaving_network

func _unhandled_key_input(event: InputEvent) -> void:
	if not event.is_action_pressed(&"ui_cancel"):
		return
	if dismiss_cancel():
		get_viewport().set_input_as_handled()

func _availability_changed(snapshot: Dictionary) -> void:
	if canvas == null or canvas.field == null or _privacy_layer.visible:
		return
	canvas.field.legal_cards.clear()
	for id: String in snapshot.get("availableCards", {}):
		canvas.field.legal_cards[id] = int(snapshot["availableCards"][id])
	canvas.field.can_set_formation = bool(snapshot.get("canSetFormation", false))
	canvas.queue_redraw()
