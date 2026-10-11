class_name MatchPage
extends Control

signal navigation_requested(page: StringName)

var service: FrontendService
var canvas: FieldCanvas
var _notice: Label
var _phase: Label
var _action_title: Label
var _action_list: VBoxContainer
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
var _show_all: bool = false
var _auto_busy: bool = false
var _quitting: bool = false
var _network_waiting: bool = false
var _confirm: ConfirmationDialog
var _leave_dialog: ConfirmationDialog
var _pending_generation: String = ""
var _pending_revision: String = ""
var _zone_picker: OptionButton
var _filter_button: Button
var _history: RichTextLabel
var _leaving_network: bool = false
var _force_leave: Button
var _force_dialog: ConfirmationDialog
var _reconnect: Button

func _ready() -> void:
	var column: VBoxContainer = VBoxContainer.new()
	UiFactory.margin(self, 12).add_child(column)
	var header: HBoxContainer = HBoxContainer.new()
	column.add_child(header)
	header.add_child(UiFactory.button("暂停 / 设置", _open_settings))
	header.add_child(UiFactory.button("保存复盘", _save_replay))
	header.add_child(UiFactory.button("查看复盘", _open_replay))
	header.add_child(UiFactory.button("结束对局", request_leave))
	_reconnect = UiFactory.button("重新连接", _reconnect_network)
	header.add_child(_reconnect)
	_reconnect.hide()
	_force_leave = UiFactory.button("强制离开", _request_force_leave)
	header.add_child(_force_leave)
	_force_leave.hide()
	_phase = UiFactory.label("", 22)
	_phase.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_phase.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(_phase)
	_notice = UiFactory.label("", 16, true)
	_notice.custom_minimum_size.y = 28
	column.add_child(_notice)
	var main: HBoxContainer = HBoxContainer.new()
	main.size_flags_vertical = Control.SIZE_EXPAND_FILL
	main.add_theme_constant_override(&"separation", 8)
	column.add_child(main)
	var field_holder: Control = Control.new()
	field_holder.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	field_holder.size_flags_vertical = Control.SIZE_EXPAND_FILL
	main.add_child(field_holder)
	canvas = FieldCanvas.new()
	field_holder.add_child(canvas)
	canvas.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	canvas.card_selected.connect(_select_card)
	canvas.selection_cleared.connect(_clear_card)
	canvas.hand_toggle_requested.connect(_toggle_hand)
	canvas.placement_requested.connect(_placement_requested)
	_cue = UiFactory.label("", 24, true)
	_cue.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_cue.mouse_filter = Control.MOUSE_FILTER_IGNORE
	field_holder.add_child(_cue)
	_cue.position = Vector2(24, 60)
	_cue.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
	_cue.offset_left = 24
	_cue.offset_right = -24
	_cue.offset_top = 60
	_cue.offset_bottom = 108
	_cue.hide()
	var side: VBoxContainer = VBoxContainer.new()
	side.custom_minimum_size.x = 380
	side.size_flags_vertical = Control.SIZE_EXPAND_FILL
	side.add_theme_constant_override(&"separation", 8)
	main.add_child(side)
	_action_title = UiFactory.label("当前合法操作", 24)
	side.add_child(_action_title)
	_filter_button = UiFactory.button("显示全部合法操作", _toggle_all)
	side.add_child(_filter_button)
	_zone_picker = OptionButton.new()
	_zone_picker.custom_minimum_size.y = 44
	_zone_picker.item_selected.connect(_zone_selected)
	side.add_child(_zone_picker)
	var scroll: ScrollContainer = ScrollContainer.new()
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	side.add_child(scroll)
	_action_list = VBoxContainer.new()
	_action_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(_action_list)
	_tutorial = UiFactory.label("", 16, true)
	side.add_child(_tutorial)
	_tutorial_button = UiFactory.button("继续教学", _continue_tutorial)
	side.add_child(_tutorial_button)
	_detail = CardDetail.new()
	_detail.battle_skin = true
	_detail.custom_minimum_size = Vector2(380, 270)
	_detail.size_flags_vertical = Control.SIZE_SHRINK_END
	side.add_child(_detail)
	_detail.closed.connect(_clear_card)
	_detail.hide()
	_history = RichTextLabel.new()
	_history.bbcode_enabled = false
	_history.custom_minimum_size.y = 90
	_history.add_theme_font_size_override(&"normal_font_size", 14)
	side.add_child(_history)
	var footer: HBoxContainer = HBoxContainer.new()
	column.add_child(footer)
	footer.add_child(UiFactory.button("收起 / 展开手牌", _toggle_hand))
	footer.add_child(UiFactory.button("取消选择", _clear_card))
	footer.add_child(UiFactory.label("点击卡牌查看操作 · 右键取消选择 · Tab 与方向键可浏览场地", 16, true))
	_feedback = MatchFeedback.new()
	add_child(_feedback)
	_build_dialogs()
	_build_privacy()
	_build_decision()
	resized.connect(_layout_panels)
	_layout_panels()

func _layout_panels() -> void:
	if _decision == null:
		return
	var extent: Vector2 = Vector2(minf(1140, size.x - 64), minf(650, size.y - 72))
	_decision.offset_left = -extent.x * 0.5
	_decision.offset_right = extent.x * 0.5
	_decision.offset_top = -extent.y * 0.5
	_decision.offset_bottom = extent.y * 0.5

func _build_dialogs() -> void:
	_confirm = ConfirmationDialog.new()
	_confirm.title = "确认操作"
	_confirm.confirmed.connect(_commit_pending)
	_confirm.canceled.connect(_cancel_pending)
	add_child(_confirm)
	_leave_dialog = ConfirmationDialog.new()
	_leave_dialog.title = "结束对局"
	_leave_dialog.dialog_text = "先保存复盘，再结束当前对局？"
	_leave_dialog.confirmed.connect(_leave)
	add_child(_leave_dialog)
	_force_dialog = ConfirmationDialog.new()
	_force_dialog.title = "强制离开房间"
	_force_dialog.dialog_text = "强制离开会中止连接，尚未接收完成的终局复盘可能无法保存。确认离开？"
	_force_dialog.confirmed.connect(_force_leave_network)
	add_child(_force_dialog)

func _build_privacy() -> void:
	_privacy_layer = ColorRect.new()
	_privacy_layer.color = Color("08131f")
	add_child(_privacy_layer)
	_privacy_layer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var center: CenterContainer = CenterContainer.new()
	_privacy_layer.add_child(center)
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var column: VBoxContainer = VBoxContainer.new()
	column.custom_minimum_size.x = 560
	center.add_child(column)
	_privacy_title = UiFactory.label("请把设备交给下一位玩家", 32, true)
	_privacy_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	column.add_child(_privacy_title)
	column.add_child(UiFactory.button("我已接手 · 揭开场地", _reveal, true))
	_privacy_layer.hide()

func _build_decision() -> void:
	_shade = ColorRect.new()
	_shade.color = Color(0.02, 0.04, 0.08, 0.76)
	_shade.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_shade)
	_shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_shade.hide()
	_decision = DecisionPanel.new()
	add_child(_decision)
	_decision.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	_decision.offset_left = -570
	_decision.offset_right = 570
	_decision.offset_top = -300
	_decision.offset_bottom = 300
	_decision.confirmed.connect(_decision_confirmed)
	_decision.hide()

func configure(value: FrontendService) -> void:
	service = value
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
	var snapshot: Dictionary = service.snapshot()
	if snapshot.is_empty():
		_notice.text = service.error
		return
	if service.match_mode == "hotseat" and int(snapshot.get("actingPlayer", service.match_viewer)) != service.match_viewer and int(snapshot.get("result", -1)) == -1:
		_cover(int(snapshot.get("actingPlayer", 0)))
		return
	var hand_collapsed: bool = canvas.hand_collapsed
	canvas.configure(MatchProjection.field(snapshot))
	canvas.hand_collapsed = hand_collapsed
	canvas.focus_mode = Control.FOCUS_ALL
	canvas.set_privacy(false)
	_privacy_layer.hide()
	_selected = null
	_detail.hide()
	_zone_picker.clear()
	_zone_picker.add_item("查看副卡组 / 灰烬区 / 附着符文")
	for row: Dictionary in snapshot.get("cards", []):
		var zone: int = int(row.get("zone", -1))
		if zone not in [6, 7, 9] or bool(row.get("hidden", false)):
			continue
		if zone == 9 and int(row.get("owner", -1)) != service.match_viewer:
			continue
		var card: CardPresentation = CardPresentation.from_projection(row)
		_zone_picker.add_item(("我方" if int(row.get("owner", -1)) == service.match_viewer else "对手") + " · " + str(row.get("zoneName", "")) + " · " + card.title)
		_zone_picker.set_item_metadata(_zone_picker.item_count - 1, card.instance_id)
	var events: Array = snapshot.get("events", [])
	var event_lines: Array[String] = []
	for event: Dictionary in events.slice(maxi(0, events.size() - 6)):
		event_lines.append(str(event.get("text", "")))
	_history.text = "\n".join(event_lines)
	var actor: int = int(snapshot.get("actingPlayer", 0))
	var phase: String = str(snapshot.get("phaseName", ""))
	_phase.text = "第 %d 位玩家 · %s" % [actor + 1, phase]
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
	_schedule_automatic()

func _render_actions() -> void:
	for child: Node in _action_list.get_children():
		child.queue_free()
	var snapshot: Dictionary = service.match_snapshot
	var actions: Array = snapshot.get("actions", [])
	var displayed: int = 0
	var source: String = _selected.instance_id if _selected != null else "0"
	for action: Dictionary in actions:
		if not _show_all and str(action.get("source", "0")) != source:
			continue
		var detail: String = MatchProjection.action_detail(snapshot, action)
		var button: Button = UiFactory.button(detail, _choose_action.bind(action))
		button.disabled = _network_waiting or (service.match_mode == "network" and bool(service.network_state.get("blocked", false)))
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		button.custom_minimum_size.y = 66
		button.tooltip_text = detail
		_action_list.add_child(button)
		displayed += 1
	if displayed == 0:
		_action_list.add_child(UiFactory.label("此处暂无合法操作。可点击场上或手中的卡牌；也可显示全部合法操作。", 16, true))
	_action_title.text = ("%s · 合法操作 %d" % [_selected.title, displayed]) if _selected != null else ("全部合法操作 %d" if _show_all else "全局操作 %d") % displayed

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
	var request: DecisionPresentation = DecisionPresentation.new()
	request.generation = str(snapshot.get("generation", ""))
	request.revision = str(snapshot.get("revision", ""))
	request.decision_id = str(decision.get("id", ""))
	var kinds: Array[String] = ["响应窗口", "选择弃牌", "清除荷载", "荷载溢出", "选择阵法", "效果弃牌", "追加施法", "维持专注", "摧毁盖伏", "检索牌库", "法术回位", "效果选择", "伤害类型", "清除荷载"]
	var kind: int = int(decision.get("kind", -1))
	request.title = "%s · 玩家 %d" % [kinds[kind] if kind >= 0 and kind < kinds.size() else "请选择", service.match_viewer + 1]
	request.description = "选择一项操作，再确认。目标和所需材料会显示在右侧。"
	request.may_cancel = false
	for action: Dictionary in snapshot.get("actions", []):
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = str(action.get("id", ""))
		choice.label = MatchProjection.action_detail(snapshot, action)
		choice.card = MatchProjection.card(snapshot, str(action.get("source", "0")))
		request.choices.append(choice)
	if request.choices.is_empty():
		_notice.text = "规则决策缺少合法选项，请重新加载对局视图。"
		return
	_decision.configure(request)
	_shade.show()
	_decision.show()
	canvas.focus_mode = Control.FOCUS_NONE

func _select_card(card: CardPresentation) -> void:
	if _privacy_layer.visible or _decision.visible:
		return
	_selected = card
	canvas.selected_id = card.instance_id
	canvas.queue_redraw()
	_detail.configure(card)
	_detail.show()
	_render_actions()

func _clear_card() -> void:
	_selected = null
	_detail.hide()
	canvas.selected_id = ""
	canvas.queue_redraw()
	if service != null and not service.match_snapshot.is_empty():
		_render_actions()

func _toggle_all() -> void:
	_show_all = not _show_all
	_filter_button.text = "仅显示选中卡牌操作" if _show_all else "显示全部合法操作"
	_render_actions()

func _zone_selected(index: int) -> void:
	if index <= 0:
		_clear_card()
		return
	var card: CardPresentation = MatchProjection.card(service.match_snapshot, str(_zone_picker.get_item_metadata(index)))
	if card != null:
		_show_all = false
		_filter_button.text = "显示全部合法操作"
		_select_card(card)

func _toggle_hand() -> void:
	if _privacy_layer.visible or _decision.visible:
		return
	canvas.set_hand_collapsed(not canvas.hand_collapsed)

func _placement_requested(card_id: String, _host_id: String, _ring_index: int) -> void:
	var card: CardPresentation = MatchProjection.card(service.match_snapshot, card_id)
	if card != null:
		_select_card(card)
	_notice.text = "请在右侧选择完整合法操作；规则将决定环位、目标与费用。"

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
	var decision: Dictionary = service.match_snapshot.get("decision", {})
	if str(decision.get("id", "")) != decision_id:
		_refresh()
		return
	var selection: Dictionary = service.match_request(&"select_action", [service.match_viewer, selected[0], generation, revision])
	if not bool(selection.get("ok", false)):
		_notice.text = service.error
		_refresh()
		return
	_apply_result(service.match_request(&"confirm_action", [generation, revision]))

func _apply_result(result: Dictionary) -> void:
	_decision.hide()
	_shade.hide()
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
	if _auto_busy or _pending_viewer >= 0 or _privacy_layer.visible or _decision.visible:
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
	if service.match_mode != "network":
		var response: Dictionary = service.match_request(&"set_paused", [true])
		if not bool(response.get("ok", false)):
			_notice.text = service.error
			return
	navigation_requested.emit(&"settings")

func _open_replay() -> void:
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
	elif _network_waiting:
		_notice.text = str(state.get("notice", "等待房主确认。"))

func _unhandled_key_input(event: InputEvent) -> void:
	if not event.is_action_pressed(&"ui_cancel"):
		return
	if _decision.visible or _privacy_layer.visible:
		get_viewport().set_input_as_handled()
	elif _confirm.visible:
		_confirm.hide()
		_cancel_pending()
		get_viewport().set_input_as_handled()
	elif _detail.visible:
		_clear_card()
		get_viewport().set_input_as_handled()
