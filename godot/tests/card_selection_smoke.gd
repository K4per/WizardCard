extends SceneTree

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error("CARD_SELECTION: " + message)

func _card(id: String, name: String, type: int, zone: int) -> Dictionary:
	var art_id: String = "balance" if type == 3 else "fireball"
	return {"id": id, "owner": 0, "zone": zone, "host": "0", "hidden": false, "definition": {"id": name, "illustrationId": art_id, "name": name, "type": type, "body": 1, "rings": 3, "cost": 1, "text": ""}}

func _run() -> void:
	root.size = Vector2i(1280, 720)
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await process_frame
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "ai"
	service.match_snapshot = {
		"schema": KernelContract.SCHEMA, "generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0,
		"players": [{"life": 40}, {"life": 40}], "result": -1,
		"cards": [_card("33", "魔法飞弹", 1, 1), _card("34", "小六芒星", 3, 3), _card("35", "蓄能阵", 3, 3)]
	}
	page.configure(service)
	service.fixture_actions.assign([
		{"id": "cover-a", "source": "33", "label": "盖伏 魔法飞弹", "command": {"type": KernelContract.CommandKind.SetAmbush, "card": "33", "formation": "34"}},
		{"id": "cover-b", "source": "33", "label": "盖伏 魔法飞弹", "command": {"type": KernelContract.CommandKind.SetAmbush, "card": "33", "formation": "35"}},
		{"id": "analysis-a", "source": "33", "label": "设置解析法术 魔法飞弹", "command": {"type": KernelContract.CommandKind.StartAnalysis, "card": "33", "formation": "34"}}
	])
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._select_card(MatchProjection.card(service.match_snapshot, "33"))
	await process_frame
	await process_frame
	_expect(page._action_list.get_child_count() >= 1, "query menu groups placement operations without exposing full combinations")
	var cover_button: Button
	for child: Node in page._action_list.get_children():
		if child is Button and (child as Button).text == "盖伏":
			cover_button = child
	_expect(cover_button != null, "cover option is present")
	if cover_button != null:
		cover_button.pressed.emit()
	await process_frame
	await process_frame
	_expect(page.canvas.target_ids.has("34") and page.canvas.target_ids.has("35") and not page.canvas.target_ids.has("33"), "query highlights only authoritative host candidates")
	_expect(not page._confirm.visible, "choosing placement waits for a board target")
	page._pick_action_target("35")
	await process_frame
	_expect(service.selected_actions == ["cover-b"] and service.confirmations == 1, "board target stages and confirms the exact queried command")
	_expect(page.canvas.target_ids.is_empty(), "successful placement clears target highlights")
	_expect(service.match_snapshot.get("cards", []).size() == 3 and not service.match_snapshot.has("actions"), "query and confirmation do not edit the fixture projection")

	service.match_snapshot["decision"] = {"id": "9007199254740997", "player": 0, "kind": KernelContract.DecisionKind.ReturnSpell}
	service.fixture_actions.assign([{"id": "return-b", "source": "0", "command": {"type": KernelContract.CommandKind.Choose, "decision": "9007199254740997", "option": {"kind": KernelContract.ChoiceKind.Card, "value": "34"}, "ringIndex": 2}}])
	page._render_decision()
	await process_frame
	await process_frame
	_expect(not page._decision.visible and page.canvas.target_rings.get("34", PackedInt32Array()) == PackedInt32Array([2]), "return decision highlights its exact legal ring")
	page._ring_selected("34", 2)
	await process_frame
	_expect(service.selected_actions == ["cover-b", "return-b"] and service.confirmations == 2, "ring click selects and confirms the typed return choice")

	service.match_snapshot["decision"] = null
	service.match_snapshot["revision"] = "9007199254740996"
	_expect(not page.canvas.target_rings.has("34"), "completed decision clears ring highlights")

	service.match_snapshot["cards"] = [_card("70", "基础法阵", 3, 3), _card("71", "恩格亚斯之星", 3, 9)]
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	service.fixture_actions.assign([
		{"id": "formation-set", "source": "71", "label": "设置阵法", "command": {"type": KernelContract.CommandKind.SetFormation, "card": "71", "replace": "0", "materials": []}},
		{"id": "formation-replace", "source": "71", "label": "替换阵法", "command": {"type": KernelContract.CommandKind.SetFormation, "card": "71", "replace": "70", "materials": []}}
	])
	page._query_title = "设置阵法"
	page._query_is_decision = false
	page._query_confirmed = false
	page._selection.begin("0", KernelContract.CommandKind.SetFormation)
	await process_frame
	await process_frame
	_expect(page._tray_active and page._decision.visible, "global setup first exposes its legal source card")
	page._selection.choose("71")
	await process_frame
	await process_frame
	_expect(not page._decision.visible, "independent setup does not open a replacement popup")
	_expect(not page._tray_active, "field selection restores the hand layout after source selection")
	_expect(page.canvas.formation_slot_target == 1 and page.canvas.target_ids.has("70"), "independent setup highlights the next slot and replacement target")
	_expect(page.canvas._formation_slot_hits.size() == 1, "the highlighted empty formation slot is clickable")
	if page.canvas._formation_slot_hits.size() == 1:
		var click: InputEventMouseButton = InputEventMouseButton.new()
		click.button_index = MOUSE_BUTTON_LEFT
		click.position = (page.canvas._formation_slot_hits[0]["rect"] as Rect2).get_center()
		click.pressed = true
		page.canvas._gui_input(click)
		click.pressed = false
		page.canvas._gui_input(click)
		await process_frame
		_expect(service.selected_actions.back() == "formation-set" and service.confirmations == 3, "clicking the highlighted slot selects independent setup")
		_expect(page.canvas.formation_slot_target == -1, "slot highlight clears after selection")

	service.match_snapshot["revision"] = "9007199254740998"
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._query_confirmed = false
	page._selection.begin("0", KernelContract.CommandKind.SetFormation)
	await process_frame
	await process_frame
	page._selection.choose("71")
	await process_frame
	await process_frame
	var enter: InputEventAction = InputEventAction.new()
	enter.action = &"ui_accept"
	enter.pressed = true
	page.canvas._gui_input(enter)
	await process_frame
	_expect(service.selected_actions.back() == "formation-set" and service.confirmations == 4, "keyboard confirm selects the highlighted slot")

	service.match_snapshot["revision"] = "9007199254740999"
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._query_confirmed = false
	page._selection.begin("71", KernelContract.CommandKind.SetFormation)
	await process_frame
	await process_frame
	_expect(page.canvas.target_ids.has("70"), "existing formations remain highlighted as replacement targets")
	page._pick_action_target("70")
	await process_frame
	_expect(service.selected_actions.back() == "formation-replace" and service.confirmations == 5, "clicking the highlighted formation still selects replacement")
	page.queue_free()
	await process_frame
	print("Card selection smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(0 if failures == 0 else 2)
