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
var _decision: DecisionPanel
var _shade: ColorRect
var _selected: CardPresentation
var _pending: Dictionary = {}
var _pending_viewer: int = -1
var _global_popup_open: bool = false
var _auto_busy: bool = false
var _quitting: bool = false
var _network_waiting: bool = false
var _confirm: ConfirmationDialog
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
var _hand_button: Button
var _status_own: Label
var _status_opponent: Label
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
var _target_actions: Array[Dictionary] = []
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

func _build_decision() -> void:
	_shade = %DecisionShade
	_shade.hide()
	_decision = %DecisionPanel
	_decision.confirmed.connect(_decision_confirmed)
	_decision.hide()

func configure(value: FrontendService) -> void:
	service = value
	%MenuSettings.text = "设置（继续计时）" if service.match_mode == "network" else "暂停 / 设置"
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
	var actor: int = int(snapshot.get("actingPlayer", 0))
	var phase: String = str(snapshot.get("phaseName", ""))
	_phase.text = "第 %s 回合\n玩家 %d · %s" % [str(snapshot.get("globalTurn", "0")), actor + 1, phase]
	_clock_received = Time.get_ticks_msec()
	_update_status()
	_hand_button.text = "▴ 手牌" if canvas.hand_collapsed else "▾ 手牌"
	if int(snapshot.get("result", -1)) != -1:
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
	var actions: Array = snapshot.get("actions", [])
	_end_phase.disabled = true
	for action: Dictionary in actions:
		if int((action.get("command", {}) as Dictionary).get("type", -1)) == 14:
			_end_phase.disabled = _privacy_layer.visible or _decision.visible or _network_waiting or not _pending.is_empty()
	var global_count: int = 0
	for action: Dictionary in actions:
		if str(action.get("source", "0")) == "0":
			global_count += 1
	_action_button.text = "阶段操作 · %d" % global_count
	if _selected != null:
		_show_action_list(_selected.instance_id, _selected.title + " · 操作", true)
	elif _global_popup_open:
		_show_action_list("0", "阶段操作", false)
	else:
		_action_popup.hide()

func _end_current_phase() -> void:
	if _end_phase.disabled:
		return
	_clear_card()
	var options: Array[Dictionary] = []
	for action: Dictionary in service.match_snapshot.get("actions", []):
		if int((action.get("command", {}) as Dictionary).get("type", -1)) == 14:
			options.append(action)
	if options.is_empty():
		return
	if int(service.match_snapshot.get("phase", -1)) == 2 and options.size() > 1:
		_phase_selector.open_for(_end_phase, options)
	else:
		_choose_phase_action(options[0])

func _choose_phase_action(action: Dictionary) -> void:
	if _privacy_layer.visible or _decision.visible or _network_waiting or not _pending.is_empty():
		return
	var generation: String = str(service.match_snapshot.get("generation", ""))
	var revision: String = str(service.match_snapshot.get("revision", ""))
	var result: Dictionary = service.match_request(&"select_action", [service.match_viewer, str(action.get("id", "")), generation, revision])
	if not bool(result.get("ok", false)):
		_notice.text = service.error
		_refresh()
		return
	_apply_result(service.match_request(&"confirm_action", [generation, revision]))

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
	var labels: Array[Label] = [_status_own, _status_opponent]
	for index: int in range(2):
		var player: int = service.match_viewer if index == 0 else 1 - service.match_viewer
		var values: Dictionary = players[player]
		var separator: String = "\n" if index == 0 else "   "
		var text: String = "%s  生命 %d%s%s魔素 %d   荷载 %d/%d" % ["我方" if index == 0 else "对手", int(values.get("life", 0)), " (+%d)" % int(values.get("temporaryLife", 0)) if int(values.get("temporaryLife", 0)) > 0 else "", separator, int(values.get("mana", 0)), int(values.get("load", 0)), int(values.get("capacity", 0))]
		if bool(clock.get("enabled", false)) and remaining.size() > player:
			var milliseconds: int = int(remaining[player])
			var player_running: bool = bool(running_players[player]) if running_players.size() > player else int(clock.get("runningPlayer", -1)) == player
			if bool(clock.get("running", false)) and player_running:
				milliseconds -= Time.get_ticks_msec() - _clock_received
			var seconds: int = maxi(0, ceili(float(milliseconds) / 1000.0))
			text += separator + "%02d:%02d" % [seconds / 60, seconds % 60]
			labels[index].modulate = Color("f0aaaa") if seconds <= 30 else Color.WHITE
		labels[index].text = text

func _process(delta: float) -> void:
	if service == null or not service.match_active:
		return
	_update_status()
	_clock_poll_elapsed += delta
	if _clock_poll_elapsed < 0.5 or service.match_mode == "network" or _pending_viewer >= 0:
		return
	_clock_poll_elapsed = 0.0
	var old_revision: String = str(service.match_snapshot.get("revision", ""))
	var latest: Dictionary = service.snapshot()
	if latest.is_empty():
		return
	_clock_received = Time.get_ticks_msec()
	if str(latest.get("revision", "")) != old_revision:
		_refresh()

func _hover_card(card: CardPresentation) -> void:
	if _selected != null or not _target_actions.is_empty() or _privacy_layer.visible or _decision.visible or _collection.visible or _match_menu.visible:
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
	if not bool(snapshot.get("opening", false)) or int(snapshot.get("result", -1)) != -1:
		return
	_close_match_menu()
	_phase.text = "开局 · 选择等级 1 初始阵法"
	var confirmed: Array = snapshot.get("initialFormationConfirmed", [false, false])
	if confirmed.size() > service.match_viewer and bool(confirmed[service.match_viewer]):
		_notice.text = "初始阵法已确认，等待对手确认。"
		return
	var request: DecisionPresentation = DecisionPresentation.new()
	request.generation = str(snapshot.get("generation", ""))
	request.revision = str(snapshot.get("revision", ""))
	request.decision_id = "opening"
	request.title = "选择初始阵法"
	request.description = "从副卡组选择一张等级 1 阵法。双方确认后同时公开，免费设置。"
	request.may_cancel = false
	request.docked = true
	for action: Dictionary in snapshot.get("actions", []):
		if int((action.get("command", {}) as Dictionary).get("type", -1)) != 18:
			continue
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = str(action.get("id", ""))
		choice.label = str(action.get("label", ""))
		choice.card = MatchProjection.card(snapshot, str(action.get("source", "0")))
		request.choices.append(choice)
	if request.choices.is_empty():
		return
	_decision.configure(request)
	_layout_panels()
	_show_tray()
	canvas.focus_mode = Control.FOCUS_NONE

func _show_action_list(source: String, title: String, contextual: bool) -> void:
	for child: Node in _action_list.get_children():
		_action_list.remove_child(child)
		child.queue_free()
	var snapshot: Dictionary = service.match_snapshot
	var actions: Array = snapshot.get("actions", [])
	var groups: Dictionary[String, Array] = {}
	for action: Dictionary in actions:
		if str(action.get("source", "0")) != source:
			continue
		var key: String = MatchProjection.action_group_key(action)
		if not groups.has(key):
			groups[key] = []
		groups[key].append(action)
	for key: String in groups:
		var group: Array[Dictionary] = []
		group.assign(groups[key])
		var label: String = MatchProjection.action_label(snapshot, group[0])
		var button: Button = UiFactory.button(label, _choose_action_group.bind(group))
		button.disabled = _network_waiting or (service.match_mode == "network" and bool(service.network_state.get("blocked", false)))
		button.custom_minimum_size = Vector2(196, 44)
		button.tooltip_text = label
		_action_list.add_child(button)
	if groups.is_empty():
		_action_list.add_child(UiFactory.label("暂无合法操作。" if contextual else "当前没有阶段级操作。", 14, true))
	_action_title.text = title
	_action_title.visible = not contextual
	_action_popup.reset_size()
	_action_popup.show()
	# CanvasItem z_index controls drawing, but GUI input uses scene-tree order.
	# Keep the clickable actions above the full-page detail input blocker too.
	_action_popup.move_to_front()
	_position_action_popup(contextual)

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

func _choose_action_group(actions: Array[Dictionary]) -> void:
	if actions.is_empty() or _privacy_layer.visible or _decision.visible or _network_waiting or not _pending.is_empty():
		return
	_target_actions.assign(actions)
	_target_generation = str(service.match_snapshot.get("generation", ""))
	_target_revision = str(service.match_snapshot.get("revision", ""))
	_target_decision_id = ""
	_target_picks.clear()
	var groups: Dictionary[String, Array] = {}
	for action: Dictionary in actions:
		var key: String = MatchProjection.action_group_key(action)
		if not groups.has(key):
			groups[key] = []
		groups[key].append(action)
	if groups.size() > 1:
		_show_operation_groups(groups)
		return
	_next_action_target(false)

func _show_operation_groups(groups: Dictionary[String, Array]) -> void:
	if _selected == null and not _target_actions.is_empty():
		_selected = MatchProjection.card(service.match_snapshot, str(_target_actions[0].get("source", "0")))
		if _selected != null:
			_selection_id = _selected.instance_id
			canvas.selected_id = _selected.instance_id
			canvas.queue_redraw()
	for child: Node in _action_list.get_children():
		_action_list.remove_child(child)
		child.queue_free()
	for key: String in groups:
		var group: Array[Dictionary] = []
		group.assign(groups[key])
		var label: String = MatchProjection.action_label(service.match_snapshot, group[0])
		var button: Button = UiFactory.button(label, _choose_action_group.bind(group))
		button.custom_minimum_size = Vector2(196, 44)
		_action_list.add_child(button)
	_action_title.hide()
	_action_popup.show()
	_action_popup.move_to_front()
	_position_action_popup(true)

func _next_action_target(from_board: bool) -> void:
	if _target_actions.is_empty():
		return
	# Legal commands contain every valid target/material combination. Narrow that
	# authoritative list as cards are clicked; the frontend never pays a cost.
	for field: String in ["formation", "host", "replace", "targets", "target", "discard", "materials"]:
		var candidates: Array[String] = []
		var selected: Array = _target_picks.get(field, [])
		for action: Dictionary in _target_actions:
			var command: Dictionary = action.get("command", {})
			if field == "target" and not (command.get("targets", []) as Array).is_empty():
				continue
			var values: Array = command.get(field, []) if field in ["targets", "materials"] else [str(command.get(field, "0"))]
			for value: Variant in values:
				var id: String = str(value)
				if id != "0" and not id.is_empty() and not selected.has(id) and not candidates.has(id):
					candidates.append(id)
		if candidates.is_empty():
			continue
		# Players have target tokens rather than a card hitbox. Show the exact
		# legal command variants, each labeled with its native player target.
		var has_field_target: bool = false
		for id: String in candidates:
			var candidate: CardPresentation = MatchProjection.card(service.match_snapshot, id)
			if candidate != null and canvas.card_bounds(candidate).has_area():
				has_field_target = true
				break
		if candidates.has("1073741825") or candidates.has("1073741826") or not has_field_target:
			canvas.clear_action_targets()
			_show_target_variants()
			return
		_target_field = field
		canvas.set_action_targets(candidates)
		_detail.hide()
		_action_popup.hide()
		return
	if _target_actions.size() == 1:
		var action: Dictionary = _target_actions[0]
		if from_board:
			_execute_target_action(action)
		else:
			_clear_action_targets()
			_choose_action(action)
		return
	_show_target_variants()

func _show_target_variants() -> void:
	for child: Node in _action_list.get_children():
		_action_list.remove_child(child)
		child.queue_free()
	for action: Dictionary in _target_actions:
		var button: Button = UiFactory.button(MatchProjection.action_detail(service.match_snapshot, action), _execute_target_action.bind(action))
		button.custom_minimum_size = Vector2(196, 44)
		button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		_action_list.add_child(button)
	_action_title.hide()
	_action_popup.show()
	_action_popup.move_to_front()
	_position_action_popup(true)

func _target_context_current() -> bool:
	return str(service.match_snapshot.get("generation", "")) == _target_generation and str(service.match_snapshot.get("revision", "")) == _target_revision

func _pick_action_target(id: String) -> void:
	if not _target_context_current():
		_clear_action_targets()
		_refresh()
		return
	if not canvas.target_ids.has(id):
		return
	if not _target_decision_id.is_empty():
		for action: Dictionary in _target_actions:
			if str(action.get("target", "0")) == id:
				_execute_target_action(action)
				return
		return
	var narrowed: Array[Dictionary] = []
	for action: Dictionary in _target_actions:
		var command: Dictionary = action.get("command", {})
		var values: Array = command.get(_target_field, []) if _target_field in ["targets", "materials"] else [str(command.get(_target_field, "0"))]
		if values.has(id):
			narrowed.append(action)
	if narrowed.is_empty():
		return
	if not _target_picks.has(_target_field):
		_target_picks[_target_field] = []
	_target_picks[_target_field].append(id)
	_target_actions = narrowed
	_next_action_target(true)

func _ring_selected(host_id: String, ring_index: int) -> void:
	if _target_decision_id.is_empty() or not _target_context_current():
		return
	for action: Dictionary in _target_actions:
		var command: Dictionary = action.get("command", {})
		if str(command.get("option", "0")) == host_id and int(command.get("ringIndex", -1)) == ring_index:
			_execute_target_action(action)
			return

func _render_return_selection(decision: Dictionary) -> void:
	_clear_action_targets()
	_target_generation = str(service.match_snapshot.get("generation", ""))
	_target_revision = str(service.match_snapshot.get("revision", ""))
	_target_decision_id = str(decision.get("id", ""))
	var candidates: Array[String] = []
	var rings: Dictionary[String, PackedInt32Array] = {}
	for action: Dictionary in service.match_snapshot.get("actions", []):
		var command: Dictionary = action.get("command", {})
		if int(command.get("type", -1)) != 10 or int(command.get("ringIndex", -1)) < 0:
			continue
		_target_actions.append(action)
		var host: String = str(command.get("option", "0"))
		if not candidates.has(host):
			candidates.append(host)
			rings[host] = PackedInt32Array()
		rings[host].append(int(command["ringIndex"]))
	canvas.set_action_targets(candidates, rings)
	canvas.focus_mode = Control.FOCUS_ALL
	_decision.hide()
	_shade.hide()
	_action_popup.hide()
	_detail.hide()
	_detail_shade.hide()
	_collection.hide()

func _execute_target_action(action: Dictionary) -> void:
	if _privacy_layer.visible or _network_waiting or not _pending.is_empty() or not _target_context_current():
		return
	if not _target_decision_id.is_empty():
		_decision_confirmed(_target_generation, _target_revision, _target_decision_id, [str(action.get("id", ""))])
		return
	var generation: String = _target_generation
	var revision: String = _target_revision
	var result: Dictionary = service.match_request(&"select_action", [service.match_viewer, str(action.get("id", "")), generation, revision])
	if not bool(result.get("ok", false)):
		_notice.text = service.error
		_refresh()
		return
	_clear_action_targets()
	_apply_result(service.match_request(&"confirm_action", [generation, revision]))

func _clear_action_targets() -> void:
	_target_actions.clear()
	_target_picks.clear()
	_target_field = ""
	_target_generation = ""
	_target_revision = ""
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
	var raw: Variant = snapshot.get("decision")
	if not raw is Dictionary or int(snapshot.get("result", -1)) != -1:
		_decision.hide()
		_shade.hide()
		return
	var decision: Dictionary = raw
	if int(decision.get("player", -1)) != service.match_viewer:
		_decision.hide()
		_shade.hide()
		return
	if int(decision.get("kind", -1)) == 12:
		_render_return_selection(decision)
		return
	var request: DecisionPresentation = DecisionPresentation.new()
	request.generation = str(snapshot.get("generation", ""))
	request.revision = str(snapshot.get("revision", ""))
	request.decision_id = str(decision.get("id", ""))
	var kinds: Array[String] = ["响应窗口", "施法顺序", "触发顺序", "选择弃牌", "清除荷载", "荷载溢出", "摧毁己方阵法", "效果弃牌", "追加施法", "维持专注", "摧毁盖伏", "检索牌库", "法术回位", "效果选择", "伤害类型", "清除任意荷载"]
	var kind: int = int(decision.get("kind", -1))
	request.response_mode = kind == 0
	request.docked = true
	request.title = "%s · 玩家 %d" % [kinds[kind] if kind >= 0 and kind < kinds.size() else "请选择", service.match_viewer + 1]
	request.description = "点击合法响应卡立即操作，或选择不响应。" if request.response_mode else "选择一项操作，再确认。目标和所需材料会显示在卡牌说明中。"
	request.may_cancel = false
	var response_sources: Dictionary[String, String] = {}
	for action: Dictionary in snapshot.get("actions", []):
		var command: Dictionary = action.get("command", {})
		var action_type: int = int(command.get("type", -1))
		if action_type == 11:
			continue
		if action_type == 13 or (action_type == 10 and str(command.get("option", "")) == "0"):
			request.pass_id = str(action.get("id", ""))
			continue
		var source: String = str(action.get("source", "0"))
		if request.response_mode and source != "0":
			if response_sources.has(source):
				_tray_action_groups[response_sources[source]].append(action)
				continue
			response_sources[source] = str(action.get("id", ""))
			_tray_action_groups[str(action.get("id", ""))] = [action]
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = str(action.get("id", ""))
		choice.label = MatchProjection.action_detail(snapshot, action)
		choice.card = MatchProjection.card(snapshot, str(action.get("source", "0")))
		# Choose.option belongs to the decision's option domain: damage types,
		# trigger IDs and load entries are not card IDs even when values collide.
		if action_type == 10 and kind in [2, 4, 14, 15]:
			choice.card = null
			choice.label = str(action.get("label", choice.label))
		elif choice.card == null and action_type == 10 and kind in [1, 3, 5, 6, 7, 9, 10, 11, 13]:
			choice.card = MatchProjection.card(snapshot, str(command.get("option", "0")))
		request.choices.append(choice)
	if request.choices.is_empty() and request.pass_id.is_empty():
		_notice.text = "规则决策缺少合法选项，请重新加载对局视图。"
		return
	_close_match_menu()
	_decision.configure(request)
	_layout_panels()
	_action_popup.hide()
	_detail.hide()
	_detail_shade.hide()
	_collection.hide()
	_show_tray()
	canvas.focus_mode = Control.FOCUS_NONE

func _select_card(card: CardPresentation) -> void:
	if _privacy_layer.visible or _decision.visible:
		return
	if not _target_actions.is_empty():
		_pick_action_target(card.instance_id)
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
	if _decision.visible or _privacy_layer.visible:
		return true
	if not _target_actions.is_empty():
		if not _target_decision_id.is_empty():
			return true
		_clear_action_targets()
		_render_actions()
		return true
	if _match_menu.visible:
		_close_match_menu()
		return true
	if _confirm.visible:
		_confirm.hide()
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
	if _privacy_layer.visible or _decision.visible or _network_waiting or not _pending.is_empty():
		return
	var snapshot: Dictionary = service.match_snapshot
	var result: Dictionary = service.match_request(&"select_action", [service.match_viewer, str(action.get("id", "")), str(snapshot.get("generation", "")), str(snapshot.get("revision", ""))])
	if not bool(result.get("ok", false)):
		_notice.text = service.error
		_refresh()
		return
	_pending = action.duplicate(true)
	_pending_generation = str(snapshot.get("generation", ""))
	_pending_revision = str(snapshot.get("revision", ""))
	_confirm.dialog_text = MatchProjection.action_detail(snapshot, action) + "\n\n确认执行？"
	_confirm.popup_centered()

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
	if selected.size() != 1 or str(service.match_snapshot.get("generation", "")) != generation or str(service.match_snapshot.get("revision", "")) != revision:
		_refresh()
		return
	var raw: Variant = service.match_snapshot.get("decision")
	var decision: Dictionary = raw if raw is Dictionary else {}
	var valid_opening: bool = decision_id == "opening" and bool(service.match_snapshot.get("opening", false))
	if not valid_opening and str(decision.get("id", "")) != decision_id:
		_refresh()
		return
	if _tray_action_groups.has(selected[0]) and _tray_action_groups[selected[0]].size() > 1:
		var group: Array[Dictionary] = []
		group.assign(_tray_action_groups[selected[0]])
		_tray_action_groups.clear()
		_decision.hide()
		_restore_tray_hand()
		_choose_action_group(group)
		return
	var selection: Dictionary = service.match_request(&"select_action", [service.match_viewer, selected[0], generation, revision])
	if not bool(selection.get("ok", false)):
		_notice.text = service.error
		_refresh()
		return
	_apply_result(service.match_request(&"confirm_action", [generation, revision]))

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
		_notice.text = "操作已发送，等待房主确认。"
		_render_actions()
		return
	_feedback.present(data)
	_show_cue(data)
	if not bool(data.get("replaySaved", true)):
		_notice.text = "复盘自动保存失败：" + str(data.get("saveError", "请手动保存"))
	else:
		_notice.text = "操作已执行。"
	if service.match_mode == "hotseat":
		_cover(-1)
	_refresh()

func _show_cue(data: Dictionary) -> void:
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
	var tween: Tween = create_tween()
	tween.tween_interval(minf(1.5, maxf(0.2, float(latest.get("duration", 0.7)))))
	tween.tween_property(_cue, "modulate:a", 0.0, 0.45)
	tween.tween_callback(_cue.hide)

func _cover(next_viewer: int) -> void:
	_pending_viewer = next_viewer
	_clear_card()
	canvas.set_privacy(true)
	_privacy_layer.show()
	_privacy_layer.move_to_front()
	_privacy_title.text = "请将设备交给玩家 %d" % [next_viewer + 1] if next_viewer >= 0 else "正在确认下一位玩家"
	_decision.hide()
	_shade.hide()

func _reveal() -> void:
	if _pending_viewer < 0:
		return
	service.match_viewer = _pending_viewer
	_pending_viewer = -1
	_refresh()

func _schedule_automatic() -> void:
	if _auto_busy or _pending_viewer >= 0 or _privacy_layer.visible or _decision.visible or _tray_suspended or not _target_actions.is_empty():
		return
	var snapshot: Dictionary = service.match_snapshot
	if int(snapshot.get("result", -1)) != -1 or bool(snapshot.get("paused", false)):
		return
	if bool(snapshot.get("aiTurn", false)) or snapshot.get("automaticAction") is String:
		_auto_busy = true
		_automatic_step.call_deferred(str(snapshot.get("generation", "")), str(snapshot.get("revision", "")))

func _automatic_step(generation: String, revision: String) -> void:
	await get_tree().create_timer(0.3).timeout
	_auto_busy = false
	if not is_inside_tree() or _privacy_layer.visible or service.match_snapshot.get("generation") != generation or service.match_snapshot.get("revision") != revision:
		return
	var snapshot: Dictionary = service.match_snapshot
	if bool(snapshot.get("aiTurn", false)):
		_apply_result(service.match_request(&"step_ai", [generation, revision]))
	else:
		var id: String = str(snapshot.get("automaticAction", ""))
		if id.is_empty():
			return
		var selected: Dictionary = service.match_request(&"select_action", [service.match_viewer, id, generation, revision])
		if bool(selected.get("ok", false)):
			_apply_result(service.match_request(&"confirm_action", [generation, revision]))
		else:
			_notice.text = service.error
			_refresh()

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
	if not bool(state.get("exists", state.has("room"))):
		if _leaving_network:
			_finish_leave()
			return
		_notice.text = str(state.get("notice", "房间已关闭"))
		_network_waiting = false
		return
	if bool(state.get("blocked", false)):
		_notice.text = str(state.get("notice", "连接中断，操作暂不可用"))
		_reconnect.visible = not bool(state.get("host", false))
		_force_leave.show()
		_render_actions()
		return
	_reconnect.hide()
	_force_leave.visible = _leaving_network
	if _network_waiting and str(state.get("notice", "")) != "" and str(state.get("notice", "")) != "操作已发送，等待房主确认。":
		var notice: String = str(state.get("notice", ""))
		if notice != "已连接房主" and notice != "已连接访客":
			_network_waiting = false
			_notice.text = notice
			_render_actions()
	var revision: String = str(state.get("revision", ""))
	var changed: bool = revision != str(service.match_snapshot.get("revision", "")) or str(state.get("generation", "")) != str(service.match_snapshot.get("generation", ""))
	if not revision.is_empty() and changed:
		_network_waiting = false
		if not _pending.is_empty():
			_confirm.hide()
			_cancel_pending()
			_notice.text = "场地已更新，请重新选择操作。"
		_refresh()
	else:
		# Clock packets may arrive without a gameplay revision; refresh only the clock
		# so a hover, card selection or confirmation is not disrupted.
		var updated: Dictionary = service.snapshot()
		if not updated.is_empty():
			_clock_received = Time.get_ticks_msec()
			_update_status()
	if _network_waiting:
		_notice.text = str(state.get("notice", "等待房主确认。"))

func _unhandled_key_input(event: InputEvent) -> void:
	if not event.is_action_pressed(&"ui_cancel"):
		return
	if dismiss_cancel():
		get_viewport().set_input_as_handled()
