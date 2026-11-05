extends SceneTree

var _failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		_failures += 1
		push_error("ALPHA_V262_UI: " + message)

func _settle() -> void:
	await process_frame
	await process_frame

func _run() -> void:
	var card: CardPresentation = CardPresentation.from_projection({"id": "33", "owner": 0, "zone": 3, "spellState": 4, "remaining": 1, "statuses": ["【云雾术】效果生效中", "【油腻术】效果生效中"], "definition": {"id": "blessing", "name": "祝福术", "type": 1, "duration": 3}})
	_expect(card.status.contains("计时剩余 1") and card.status.contains("先触发") and card.status.contains("归零送灰"), "timer detail explains remaining count and final trigger order")
	_expect(card.detail_text().contains("【云雾术】效果生效中") and card.detail_text().contains("【油腻术】效果生效中"), "persistent target statuses reach card details")
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await _settle()
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "ai"
	service.match_viewer = 0
	service.match_snapshot = {"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0, "result": -1, "players": [{"life": 40}, {"life": 40}], "cards": [], "decision": {"id": "9007199254740997", "player": 0, "kind": KernelContract.DecisionKind.Response}}
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	var actions: Array[Dictionary] = []
	for index: int in range(80):
		actions.append({"id": "response-%d" % index, "source": str(33 + index), "command": {"type": KernelContract.CommandKind.Respond, "card": str(33 + index), "decision": "9007199254740997"}})
	service.set_actions(actions)
	page._query_is_decision = true
	page._selection.begin("0", KernelContract.CommandKind.Respond)
	await _settle()
	_expect(page._selection.options.options.size() == 64 and page._selection.options.has_more, "all response sources remain pageable")
	page._selection.choose("@next")
	await _settle()
	_expect(page._selection.options.options.size() == 16 and not page._selection.options.has_more, "last response page exposes remaining sources")
	page._pick_action_target("97")
	await _settle()
	_expect(service.confirmations == 0 and page._confirm.visible, "clicking a response source opens confirmation without submission")
	page._confirm.canceled.emit()
	_expect(service.confirmations == 0, "canceling the final dialog never pays or submits")
	page._selection.begin("0", KernelContract.CommandKind.Respond)
	await _settle()
	page._pick_action_target("33")
	await _settle()
	page._confirm.confirmed.emit()
	_expect(service.confirmations == 1, "explicit final confirmation submits exactly once")
	page._network_waiting = false
	service.match_viewer = 1
	service.match_snapshot["viewer"] = 1
	service.match_snapshot["decision"]["player"] = 1
	service.set_actions([{"id": "target", "source": "33", "queryPlayer": 1, "command": {"type": KernelContract.CommandKind.ActivateSpell, "card": "33", "target": "1073741826"}}])
	page._selection.begin("33", KernelContract.CommandKind.ActivateSpell)
	await _settle()
	_expect(page._decision.request.choices.size() == 1 and page._decision.request.choices[0].label == "己方", "player two sees its own target from its viewing perspective")
	page.queue_free()
	await process_frame
	print("Alpha v2.6.2 UI smoke: %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(0 if _failures == 0 else 2)
