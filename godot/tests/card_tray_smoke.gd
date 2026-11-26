extends SceneTree

var failures: int = 0
var output: String = ""

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error("CARD_TRAY: " + message)

func _card(id: String, name: String, zone: int, type: int = 3) -> Dictionary:
	var art_id: String = "balance" if type == 3 else "fireball"
	return {"id": id, "owner": 0, "zone": zone, "hidden": false, "definition": {"id": name, "illustrationId": art_id, "name": name, "type": type, "rings": 3, "cost": 1}}

func _click(point: Vector2) -> void:
	for pressed: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.position = point
		event.global_position = point
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		root.push_input(event, true)

func _settle() -> void:
	await process_frame
	await process_frame

func _capture(label: String) -> void:
	if output.is_empty():
		return
	await RenderingServer.frame_post_draw
	var capture: Image = root.get_texture().get_image()
	_expect(capture.save_png(output.path_join(label + "-%dx%d.png" % [root.size.x, root.size.y])) == OK, "capture saves")

func _activation_targets(single: bool) -> void:
	root.size = Vector2i(1280, 720)
	var page: MatchPage = preload("res://pages/match_page.tscn").instantiate()
	page.theme = ArtBank.make_theme()
	root.add_child(page)
	var service := QueryFixtureService.new()
	service.match_snapshot = {"generation": "10", "revision": "20", "viewer": 0, "result": -1,
		"players": [{"castingCapacity": 4}, {}], "cards": [_card("33", "火球术", 5, 1)]}
	var opponent := "1073741826"
	var own := "1073741825"
	var targets: Array[String] = [opponent]
	if not single:
		targets.push_front(own)
	for target: String in targets:
		service.fixture_actions.append({"source": "33", "id": target, "queryPlayer": 1 if target == opponent else 0, "costQuote": "发动费用：2 魔素",
			"command": {"type": KernelContract.CommandKind.ActivateSpell, "card": "33", "target": target}})
	page.configure(service)
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	await _settle()
	_click(page.canvas.global_position + page.canvas.card_bounds(page.canvas.field.own_casting[0]).get_center())
	await _settle()
	for attempt: int in range(2):
		var activate := page._action_list.get_node_or_null("ActivateSpell") as Button
		_expect(activate != null and activate.is_visible_in_tree(), "casting card exposes its activation action")
		if activate == null or not activate.is_visible_in_tree():
			break
		_click(activate.get_global_rect().get_center())
		await _settle()
		var panel := page._decision
		_expect(panel.is_visible_in_tree() and panel._options.has(opponent), "opening activation exposes the legal opponent target")
		if not panel._options.has(opponent):
			break
		for button: Button in [panel._confirm, panel._cancel]:
			_expect(button.is_visible_in_tree() and root.get_visible_rect().encloses(button.get_global_rect()),
				"first and reopened target trays fit action buttons inside the viewport: %s" % button.get_global_rect())
		_expect(service.confirmations == 0 and service.selected_actions.is_empty(), "opening a target query never submits it")
		if single:
			_expect(panel._selected == [opponent] and not panel._confirm.disabled, "the only legal target is ready for explicit confirmation")
			_click(panel._options[opponent].get_global_rect().get_center())
			_expect(panel._selected == [opponent] and not panel._confirm.disabled, "clicking the sole target keeps it ready to confirm")
		else:
			_expect(panel._selected.is_empty() and panel._confirm.disabled, "multiple legal targets require an explicit choice")
			_click(panel._options[own].get_global_rect().get_center())
			_click(panel._options[opponent].get_global_rect().get_center())
			_expect(panel._selected == [opponent] and not panel._confirm.disabled, "switching player targets enables the chosen one")
		if attempt == 0:
			_click(panel._cancel.get_global_rect().get_center())
			await _settle()
			_expect(not panel.visible and service.confirmations == 0, "back dismisses the first activation without committing")
		else:
			var confirm_point := panel._confirm.get_global_rect().get_center()
			_click(confirm_point)
			_click(confirm_point)
			await _settle()
			_expect(service.selected_actions == [opponent] and service.confirmations == 1, "confirming the reopened activation commits its target exactly once")
	page.queue_free()
	await _settle()

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			output = arguments[index + 1]
	if not output.is_empty():
		DirAccess.make_dir_recursive_absolute(output)
	await _activation_targets(true)
	await _activation_targets(false)
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await _settle()
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "ai"
	service.match_snapshot = {"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0, "result": -1, "players": [{"life": 40}, {"life": 40}], "cards": [_card("33", "魔法飞弹", 1, 1), _card("34", "小六芒星", 3), _card("35", "蓄能阵", 9), _card("36", "均衡之六芒星", 9), _card("37", "火球术", 6, 1)]}
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._phase.text = "第 12 回合\n玩家 1 · 主要"
	page._update_status()
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await _settle()
		var viewport: Rect2 = Rect2(Vector2.ZERO, dimensions)
		var own_status: Rect2 = page._status_own.get_global_rect()
		var phase_hint: Rect2 = page._phase.get_global_rect()
		var phase_button: Rect2 = page._end_phase.get_global_rect()
		_expect(viewport.encloses(own_status) and own_status.position.x < 24 and own_status.end.y > dimensions.y - 16, "own status stays in the lower-left corner")
		_expect(page._status_opponent.get_global_rect().position.y < 64, "opponent status stays at the top")
		_expect(viewport.encloses(phase_hint) and phase_hint.end.y < phase_button.position.y, "turn and phase hint sits above the phase button at %s (%s, %s)" % [dimensions, phase_hint, phase_button])
		_expect(phase_hint.position.x >= page.canvas.get_global_rect().end.x, "turn and waiting player are in the right heading")
		await _capture("match-hud")
		page._open_collection("side_deck", 0)
		await _settle()
		_expect(page._collection == page._decision, "collections use the decision tray")
		_expect(not page._shade.visible and not page._detail_shade.visible, "tray leaves the battlefield unobscured")
		_expect(page.canvas.hand_collapsed and page._decision.visible, "tray replaces the hand")
		_expect(Rect2(Vector2.ZERO, dimensions).encloses(page._decision.get_global_rect()), "tray stays within viewport")
		_expect(page._decision.get_global_rect().position.y > dimensions.y * 0.60, "tray stays below casting area")
		_expect(page._collection_grid.get_child_count() == 2, "side deck shows projected cards")
		_expect(page._collection_grid.get_child(0).position.y == page._collection_grid.get_child(1).position.y, "cards share one horizontal row")
		await _capture("side-deck")
		_click(page._decision._collapse.get_global_rect().get_center())
		await _settle()
		_expect(not page._decision.visible and not page.canvas.hand_collapsed, "collapse restores hand without submission")
		_expect(service.selected_actions.is_empty(), "collapsing does not submit a rule command")
		page._toggle_hand()
		await _settle()
		_click((page._collection_grid.get_child(0) as Button).get_global_rect().get_center())
		await _settle()
		_expect(page._detail.visible and page._detail.card.instance_id == "35", "side-deck card opens its detail after closing tray")
		page._clear_card()
		page._open_collection("ash", 0)
		await _settle()
		_expect(page._decision.request.choices.size() == 1 and page._decision.request.choices[0].card.instance_id == "37", "ash uses the same tray with its own cards")
		page.dismiss_cancel()
		_expect(not page._decision.visible, "Escape closes collection")
	service.match_snapshot["decision"] = {"id": "9007199254740997", "player": 0, "kind": 0}
	service.fixture_actions.assign([{"id": "respond", "source": "33", "command": {"type": 12, "decision": "9007199254740997"}}, {"id": "pass", "source": "0", "command": {"type": 13, "decision": "9007199254740997"}}])
	service.match_snapshot["actionSummary"] = {"hasNonPhaseAction": true}
	page._render_decision()
	await _settle()
	_expect(page._confirm.visible, "response shows the shared confirmation dialog")
	await _capture("response")
	_click(page._confirm.get_node("%Confirm").get_global_rect().get_center())
	await _settle()
	_expect(service.selected_actions.is_empty() and page._response_browsing, "yes keeps response pending while browsing")
	page._return_to_response()
	await _settle()
	_click(page._confirm.get_node("%Cancel").get_global_rect().get_center())
	await _settle()
	_expect(service.selected_actions == ["pass"], "no submits exactly the native pass action")
	page._network_waiting = false
	# Damage types, load entries and trigger IDs share numeric values with card
	# instances, but their native decision options do not identify those cards.
	service.match_snapshot["cards"].append(_card("1", "小六芒星", 3))
	service.match_snapshot["cards"].append(_card("2", "蓄能阵", 9))
	var scalar_choices: Array[Dictionary] = [
		{"kind": 14, "label": "选择火焰伤害"},
		{"kind": 4, "label": "移除临时荷载来源 #1 的1点荷载"},
		{"kind": 15, "label": "清除荷载来源 #1 的1点荷载"},
		{"kind": 2, "label": "结算被动触发 魔法飞弹"},
	]
	for scalar: Dictionary in scalar_choices:
		service.match_snapshot["decision"] = {"id": "9007199254740999", "player": 0, "kind": scalar["kind"]}
		service.fixture_actions.assign([{"id": "scalar", "source": "0", "label": scalar["label"], "command": {"type": 10, "option": "1", "decision": "9007199254740999"}}])
		page._render_decision()
		await _settle()
		_expect(page._decision.request.choices[0].card == null, "scalar decision does not render a colliding card instance")
		_expect(page._decision._labels["1"].text == scalar["label"], "scalar decision displays its exact native option label")
	service.match_snapshot["decision"] = {"id": "9007199254740999", "player": 0, "kind": 11}
	service.fixture_actions.assign([{"id": "search", "source": "0", "label": "选择 蓄能阵 #2", "queryCard": "2", "command": {"type": 10, "option": "2", "decision": "9007199254740999"}}])
	page._render_decision()
	await _settle()
	_expect(page._decision.request.choices[0].card != null and page._decision.request.choices[0].card.instance_id == "2", "search options still resolve a genuine card ID")
	var damage_names: Array[String] = ["火焰", "寒冷", "光耀", "黯蚀", "毒素", "闪电", "心灵", "雷鸣", "力场", "挥砍", "钝击", "穿刺", "强酸"]
	service.match_snapshot["decision"] = {"id": "9007199254740999", "player": 0, "kind": 14}
	service.fixture_actions.assign([])
	for index: int in range(damage_names.size()):
		service.fixture_actions.append({"id": "damage-%d" % index, "source": "0", "label": "选择" + damage_names[index] + "伤害", "command": {"type": 10, "option": str(index + 1), "decision": "9007199254740999"}})
	page._render_decision()
	await _settle()
	for index: int in range(damage_names.size()):
		_expect(page._decision.request.choices[index].card == null and page._decision._labels[str(index + 1)].text == "选择" + damage_names[index] + "伤害", "all damage options show their own type")
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await _settle()
		await _capture("damage-types")
		if dimensions == Vector2i(1280, 720):
			var panel := page._decision
			for tick: int in range(30):
				var wheel := InputEventMouseButton.new()
				wheel.position = panel._candidate_scroll.get_global_rect().get_center()
				wheel.global_position = wheel.position
				wheel.button_index = MOUSE_BUTTON_WHEEL_RIGHT
				wheel.pressed = true
				root.push_input(wheel, true)
				wheel = wheel.duplicate() as InputEventMouseButton
				wheel.pressed = false
				root.push_input(wheel, true)
			await _settle()
			var last := panel._options["13"].get_global_rect().get_center()
			_expect(panel._candidate_scroll.get_global_rect().has_point(last) and panel._candidate_scroll.scroll_horizontal > 0, "the last candidate scrolls into view at 720p")
			_click(last)
			_expect(panel._selected == ["13"] and root.get_visible_rect().encloses(panel._confirm.get_global_rect()), "scrolling keeps the last choice and its confirmation accessible")
	page._decision._candidate_scroll.scroll_horizontal = 0
	await _settle()
	_click(page._decision._options["1"].get_global_rect().get_center())
	await _settle()
	_expect(not page._decision._confirm.disabled and page._decision._selected == ["1"], "damage selection enables explicit confirmation")
	_click(page._decision._confirm.get_global_rect().get_center())
	await _settle()
	_expect(service.selected_actions == ["pass", "damage-0"], "damage confirmation submits the exact native action ID")
	page.queue_free()
	await process_frame
	print("Card tray smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(0 if failures == 0 else 2)
