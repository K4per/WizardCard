extends SceneTree

class DelayedService extends QueryFixtureService:
	var delay: bool = false
	func match_request(method: StringName, arguments: Array = []) -> Dictionary:
		var result: Dictionary = super.match_request(method, arguments)
		if delay and method == &"query_actions":
			result["data"]["pending"] = true
		return result

var failures: int = 0
var completed_queries: int = 0

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, message: String) -> void:
	if not value:
		failures += 1
		push_error("V263_UI: " + message)

func _run() -> void:
	var delayed: DelayedService = DelayedService.new()
	delayed.match_snapshot = {"generation": "1", "revision": "1", "viewer": 0, "decision": {"id": "5", "kind": KernelContract.DecisionKind.Response}}
	delayed.set_actions([{"source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "5"}}])
	var delayed_selection: ActionSelectionController = ActionSelectionController.new()
	delayed_selection.service = delayed
	delayed_selection.completed.connect(func(_command: Dictionary, _intent: Dictionary) -> void: completed_queries += 1)
	delayed_selection.begin("0", KernelContract.CommandKind.PassResponse)
	completed_queries = 0
	delayed.delay = true
	delayed.match_snapshot["revision"] = "2"
	delayed_selection.revalidate()
	delayed.delay = false
	delayed_selection.poll()
	check(completed_queries == 0 and delayed_selection.options.options.size() == 1, "asynchronous revalidation waits for explicit confirmation")
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "ai"
	service.match_active = true
	service.match_snapshot = {
		"generation": "1", "revision": "2", "viewer": 0, "activePlayer": 0, "result": -1,
		"players": [{"life": 40, "castingCapacity": 2}, {"life": 40, "castingCapacity": 7}],
		"cards": [{"id": "33", "owner": 0, "zone": 1, "definition": {"id": "shield", "name": "护盾术", "type": 5, "flavor": "原文风味"}}],
		"decision": {"id": "9", "player": 0, "kind": KernelContract.DecisionKind.Response},
		"responseContext": {"text": "对方发动【魔法飞弹】", "waitingPlayer": 0},
		"actionSummary": {"hasNonPhaseAction": true, "safeAdvance": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}},
	}
	service.set_actions([
		{"id": "respond", "source": "33", "command": {"type": KernelContract.CommandKind.Respond, "card": "33", "decision": "9", "ability": "shield", "link": "7"}},
		{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}},
	])
	var projection: FieldPresentation = MatchProjection.field(service.match_snapshot)
	check(projection.own_casting_slots == 2 and projection.opponent_casting_slots == 7, "casting slots use authoritative capacities")
	check(projection.hand[0].detail_text().contains("风味") and projection.hand[0].detail_text().contains("原文风味"), "flavor has a distinct detail section")
	var selection: ActionSelectionController = ActionSelectionController.new()
	selection.service = service
	selection.begin("33", KernelContract.CommandKind.Respond)
	var selected_prefix: Dictionary = selection.intent.duplicate(true)
	service.match_snapshot["revision"] = "3"
	check(selection.has_method("revalidate"), "selection can revalidate after an unrelated state update")
	if selection.has_method("revalidate"):
		selection.call("revalidate")
		check(selection.current() and selection.intent == selected_prefix, "legal prefix survives an unrelated revision")
		service.set_actions([])
		service.match_snapshot["revision"] = "4"
		selection.call("revalidate")
		check(not selection.active, "lost candidates invalidate selection before submission")
	service.set_actions([
		{"id": "respond", "source": "33", "command": {"type": KernelContract.CommandKind.Respond, "card": "33", "decision": "9", "ability": "shield", "link": "7"}},
		{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}},
	])
	var page: MatchPage = load("res://pages/match_page.tscn").instantiate() as MatchPage
	root.add_child(page)
	await process_frame
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	page._selection.failed.connect(page._query_failed)
	page.canvas.configure(projection)
	page._render_decision()
	check(page._decision.request.description.contains("魔法飞弹"), "response explains the current event")
	check(page._decision.request.choices.size() == 2 and page._decision.request.choices[0].label == "是", "response starts with a yes/no question")
	if page._decision.request.choices.size() == 2 and page._decision.request.choices[0].id == "@yes":
		page._decision_confirmed("1", "4", "9", ["@yes"])
		check(not page._decision.visible and page.canvas.target_ids.has("33"), "yes highlights real response sources without a card collection")
		check(service.confirmations == 0, "viewing the field does not pass or pay")
		page._decision_cancelled("1", "4", "9")
		check(page._decision.visible and service.confirmations == 0, "cancel returns to the same response question")
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080), Vector2i(2560, 1440), Vector2i(1440, 900)]:
		root.size = dimensions
		page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		await process_frame
		page._layout_panels()
		check(page._detail.size.x <= maxf(244, dimensions.x * 0.20), "compact details leave more width for the battlefield")
		check(page.canvas.layout.own_analysis.y >= 0.34 and page.canvas.layout.opponent_analysis.y >= 0.30, "battlefield grows vertically while the hand remains collapsible")
		check(page._detail.position.x < dimensions.x * 0.25, "detail stays in left column")
		check(page.canvas.position.x > page._detail.position.x + page._detail.size.x, "detail never blocks the field")
		check(page._status_opponent.position.x > dimensions.x * 0.65, "opponent status remains top right")
	page.queue_free()
	await process_frame
	print("Alpha v2.6.3 UI failures: ", failures)
	quit(0 if failures == 0 else 1)
