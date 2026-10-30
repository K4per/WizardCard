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
	return {"id": id, "owner": 0, "zone": zone, "host": "0", "hidden": false, "definition": {"id": name, "illustrationId": art_id, "name": name, "type": type, "rings": 3, "cost": 1, "text": ""}}

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
	page.queue_free()
	await process_frame
	print("Card selection smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(0 if failures == 0 else 2)
