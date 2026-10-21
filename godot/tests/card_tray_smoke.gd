extends SceneTree

class RecordingService extends FrontendService:
	var submitted: Array[String] = []
	func match_request(method: StringName, arguments: Array = []) -> Dictionary:
		if method == &"select_action":
			submitted.append(str(arguments[1]))
		return {"ok": true, "data": {}}

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

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			output = arguments[index + 1]
	if not output.is_empty():
		DirAccess.make_dir_recursive_absolute(output)
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await _settle()
	var service: RecordingService = RecordingService.new()
	service.match_mode = "ai"
	service.match_snapshot = {"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0, "result": -1, "players": [{"life": 40}, {"life": 40}], "cards": [_card("33", "魔法飞弹", 1, 1), _card("34", "小六芒星", 3), _card("35", "蓄能阵", 9), _card("36", "均衡之六芒星", 9), _card("37", "火球术", 6, 1)], "actions": []}
	page.service = service
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
		_expect(viewport.encloses(phase_hint) and phase_hint.end.y < phase_button.position.y, "turn and phase hint sits above the phase button")
		_expect(is_equal_approx(phase_hint.position.x, phase_button.position.x), "phase hint aligns with phase switching")
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
		_expect(service.submitted.is_empty(), "collapsing does not submit a rule command")
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
	service.match_snapshot["actions"] = [{"id": "respond", "source": "33", "command": {"type": 12, "decision": "9007199254740997"}}, {"id": "pass", "source": "0", "command": {"type": 13, "decision": "9007199254740997"}}]
	page._render_decision()
	await _settle()
	_expect(page._decision._confirm.visible and page._decision._cancel.visible, "response exposes confirm and pass footer")
	_expect(page._decision._confirm.disabled, "response needs selection before confirmation")
	await _capture("response")
	_click(page._decision._collapse.get_global_rect().get_center())
	await _settle()
	_expect(service.submitted.is_empty() and page._tray_suspended, "collapsing mandatory response keeps it pending")
	page._toggle_hand()
	await _settle()
	_click(page._decision._options["respond"].get_global_rect().get_center())
	await _settle()
	_expect(service.submitted.is_empty() and not page._decision._confirm.disabled, "response selection waits for explicit confirmation")
	_click(page._decision._cancel.get_global_rect().get_center())
	await _settle()
	_expect(service.submitted == ["pass"], "pass button submits exactly the native pass action")
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
		service.match_snapshot["actions"] = [{"id": "scalar", "source": "0", "label": scalar["label"], "command": {"type": 10, "option": "1", "decision": "9007199254740999"}}]
		page._render_decision()
		await _settle()
		_expect(page._decision.request.choices[0].card == null, "scalar decision does not render a colliding card instance")
		_expect(page._decision._labels["scalar"].text == scalar["label"], "scalar decision displays its exact native option label")
	service.match_snapshot["decision"] = {"id": "9007199254740999", "player": 0, "kind": 11}
	service.match_snapshot["actions"] = [{"id": "search", "source": "0", "label": "选择 蓄能阵 #2", "command": {"type": 10, "option": "2", "decision": "9007199254740999"}}]
	page._render_decision()
	await _settle()
	_expect(page._decision.request.choices[0].card != null and page._decision.request.choices[0].card.instance_id == "2", "search options still resolve a genuine card ID")
	var damage_names: Array[String] = ["火焰", "寒冷", "光耀", "黯蚀", "毒素", "闪电", "心灵", "雷鸣", "力场", "挥砍", "钝击", "穿刺", "强酸"]
	service.match_snapshot["decision"] = {"id": "9007199254740999", "player": 0, "kind": 14}
	service.match_snapshot["actions"] = []
	for index: int in range(damage_names.size()):
		service.match_snapshot["actions"].append({"id": "damage-%d" % index, "source": "0", "label": "选择" + damage_names[index] + "伤害", "command": {"type": 10, "option": str(index + 1), "decision": "9007199254740999"}})
	page._render_decision()
	await _settle()
	for index: int in range(damage_names.size()):
		_expect(page._decision.request.choices[index].card == null and page._decision._labels["damage-%d" % index].text == "选择" + damage_names[index] + "伤害", "all damage options show their own type")
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await _settle()
		await _capture("damage-types")
	page._decision._candidate_scroll.scroll_horizontal = 0
	await _settle()
	_click(page._decision._options["damage-0"].get_global_rect().get_center())
	await _settle()
	_expect(not page._decision._confirm.disabled and page._decision._selected == ["damage-0"], "damage selection enables explicit confirmation")
	_click(page._decision._confirm.get_global_rect().get_center())
	await _settle()
	_expect(service.submitted == ["pass", "damage-0"], "damage confirmation submits the exact native action ID")
	page.queue_free()
	await process_frame
	print("Card tray smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(0 if failures == 0 else 2)
