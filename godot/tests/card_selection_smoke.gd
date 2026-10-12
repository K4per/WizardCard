extends SceneTree

class RecordingService extends FrontendService:
	var selected_actions: Array[String] = []
	var confirmations: int = 0

	func match_request(method: StringName, arguments: Array = []) -> Dictionary:
		if method == &"select_action":
			selected_actions.append(str(arguments[1]))
		elif method == &"confirm_action":
			confirmations += 1
		return {"ok": true, "data": {}}

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		push_error("CARD_SELECTION: " + message)

func _card(id: String, name: String, type: int, zone: int) -> Dictionary:
	return {"id": id, "owner": 0, "zone": zone, "host": "0", "hidden": false, "definition": {"id": name, "name": name, "type": type, "rings": 3, "cost": 1, "text": ""}}

func _click(point: Vector2) -> void:
	for pressed: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.position = point
		event.global_position = point
		event.pressed = pressed
		root.push_input(event, true)

func _run() -> void:
	root.size = Vector2i(1280, 720)
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await process_frame
	var service: RecordingService = RecordingService.new()
	service.match_mode = "ai"
	service.match_snapshot = {
		"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0,
		"players": [{"life": 40}, {"life": 40}], "result": -1,
		"cards": [_card("33", "魔法飞弹", 1, 1), _card("34", "小六芒星", 3, 3), _card("35", "蓄能阵", 3, 3)],
		"actions": [
			{"id": "cover-a", "source": "33", "target": "34", "label": "盖伏 魔法飞弹 #33", "command": {"type": 15, "card": "33", "formation": "34"}},
			{"id": "cover-b", "source": "33", "target": "35", "label": "盖伏 魔法飞弹 #33", "command": {"type": 15, "card": "33", "formation": "35"}},
			{"id": "analysis-a", "source": "33", "target": "34", "label": "设置解析法术 魔法飞弹 #33", "command": {"type": 2, "card": "33", "formation": "34"}}
		]
	}
	page.configure(service)
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._select_card(MatchProjection.card(service.match_snapshot, "33"))
	await process_frame
	await process_frame
	_expect(page._action_list.get_child_count() == 2, "two placement types replace per-formation action rows")
	_expect(not page._action_title.visible, "card operations have no redundant title")
	_expect((page._action_list.get_child(0) as Button).text == "盖伏", "operation text excludes instance IDs and target details")
	_click((page._action_list.get_child(0) as Button).get_global_rect().get_center())
	await process_frame
	await process_frame
	_expect(page.canvas.target_ids.has("34") and page.canvas.target_ids.has("35") and not page.canvas.target_ids.has("33"), "cover highlights only authoritative host candidates")
	_expect(not page._confirm.visible, "choosing placement waits for a board target")
	_click(page.canvas.card_bounds(page.canvas.field.own_formations[1].card).get_center())
	await process_frame
	_expect(service.selected_actions == ["cover-b"] and service.confirmations == 1, "board host click submits exactly the chosen legal action without another dialog")
	_expect(page.canvas.target_ids.is_empty(), "successful placement clears selection highlights")
	_expect(not MatchProjection.action_detail(service.match_snapshot, service.match_snapshot["actions"][0]).contains("#33"), "confirmation details do not expose instance IDs")
	page._clear_card()
	service.match_snapshot["decision"] = {"id": "9007199254740997", "player": 0, "kind": 12}
	service.match_snapshot["actions"] = [
		{"id": "return-a", "source": "33", "target": "34", "command": {"type": 10, "decision": "9007199254740997", "option": "34", "ringIndex": 1}},
		{"id": "return-b", "source": "33", "target": "34", "command": {"type": 10, "decision": "9007199254740997", "option": "34", "ringIndex": 2}}
	]
	page._render_decision()
	await process_frame
	await process_frame
	_expect(not page._decision.visible and not page._shade.visible, "return decision keeps the battlefield visible")
	_expect(page.canvas.target_rings.get("34", PackedInt32Array()) == PackedInt32Array([1, 2]), "return exposes only projected empty ring options")
	for hit: Dictionary in page.canvas._ring_hits:
		if str(hit["host"]) == "34" and int(hit["ring"]) == 2:
			_click((hit["rect"] as Rect2).get_center())
			break
	await process_frame
	_expect(service.selected_actions == ["cover-b", "return-b"] and service.confirmations == 2, "ring click submits its exact return action with string decision context")
	service.match_snapshot["decision"] = null
	var stale_actions: Array[Dictionary] = [{"id": "stale-cover", "source": "33", "target": "35", "command": {"type": 15, "card": "33", "formation": "35"}}]
	page._choose_action_group(stale_actions)
	service.match_snapshot["revision"] = "9007199254740996"
	page._pick_action_target("35")
	_expect(service.confirmations == 2 and page.canvas.target_ids.is_empty(), "a revision change cancels board selection before any stale action is submitted")
	page.queue_free()
	await process_frame
	print("Card selection smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(0 if failures == 0 else 2)
