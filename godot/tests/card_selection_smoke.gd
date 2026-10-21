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
	# Synthetic identities use explicit published art so resource diagnostics stay
	# reserved for missing resources rather than fixture-only Chinese IDs.
	var art_id: String = "balance" if type == 3 else ("ring" if type == 4 else "fireball")
	return {"id": id, "owner": 0, "zone": zone, "host": "0", "hidden": false, "definition": {"id": name, "illustrationId": art_id, "name": name, "type": type, "rings": 3, "cost": 1, "text": ""}}

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
	var healing_actions: Array[Dictionary] = [
		{"id": "heal-own", "source": "33", "command": {"type": 17, "card": "33", "target": "1073741825"}},
		{"id": "heal-other", "source": "33", "command": {"type": 17, "card": "33", "target": "1073741826"}}
	]
	page._select_card(MatchProjection.card(service.match_snapshot, "33"))
	page._choose_action_group(healing_actions)
	await process_frame
	await process_frame
	_expect(page._action_popup.visible and page.canvas.target_ids.is_empty(), "player targets appear as selectable legal commands without nonexistent card hitboxes")
	_expect((page._action_list.get_child(0) as Button).text.contains("己方玩家") and (page._action_list.get_child(1) as Button).text.contains("对方玩家"), "healing targets identify each player")
	_click((page._action_list.get_child(1) as Button).get_global_rect().get_center())
	await process_frame
	_expect(service.selected_actions.back() == "heal-other" and service.confirmations == 3, "player target choice submits the exact native action")
	service.match_snapshot["cards"].append(_card("90", "灰烬中的火球术", 1, 6))
	page._select_card(MatchProjection.card(service.match_snapshot, "33"))
	var ash_actions: Array[Dictionary] = [{"id": "recover-ash", "source": "33", "command": {"type": 17, "card": "33", "target": "90"}}]
	page._choose_action_group(ash_actions)
	await process_frame
	await process_frame
	_expect(page._action_popup.visible and page.canvas.target_ids.is_empty(), "ash targets remain selectable through labeled commands")
	_expect((page._action_list.get_child(0) as Button).text.contains("灰烬中的火球术"), "ash target retains its card identity")
	_click((page._action_list.get_child(0) as Button).get_global_rect().get_center())
	await process_frame
	_expect(service.selected_actions.back() == "recover-ash" and service.confirmations == 4, "ash target submits its exact legal command")
	page._clear_card()
	service.match_snapshot["cards"].append(_card("91", "承载法术", 1, 3).merged({"host": "34"}, true))
	service.match_snapshot["cards"].append(_card("92", "阵法符文", 4, 7).merged({"host": "34"}, true))
	service.match_snapshot["cards"].append(_card("93", "法术符文", 4, 7).merged({"host": "91"}, true))
	var removal: Dictionary = {"id": "remove-formation", "source": "34", "command": {"type": 1, "card": "34"}}
	service.match_snapshot["actions"] = [removal]
	page._select_card(MatchProjection.card(service.match_snapshot, "34"))
	await process_frame
	_expect(page._action_list.get_child_count() == 1 and (page._action_list.get_child(0) as Button).text == "移除阵法", "authoritative formation removal is exposed with the approved label")
	page._choose_action_group([removal])
	_expect(page._confirm.visible and service.confirmations == 4, "formation removal requires explicit confirmation")
	_expect(page._confirm.dialog_text.contains("承载法术") and page._confirm.dialog_text.contains("符文 2 张") and page._confirm.dialog_text.contains("灰烬区"), "removal confirmation explains carried spells and nested rune departures")
	page._cancel_pending()
	page._confirm.hide()
	_expect(service.confirmations == 4, "cancelling formation removal never submits it")
	page.queue_free()
	await process_frame
	print("Card selection smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(0 if failures == 0 else 2)
