extends SceneTree

var _failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		_failures += 1
		push_error("RESPONSE_COLLECTION: " + message)

func _settle() -> void:
	await process_frame
	await process_frame

func _run() -> void:
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await _settle()
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "ai"
	service.match_snapshot = {
		"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0, "result": -1,
		"players": [{"life": 40}, {"life": 40}], "cards": [],
		"decision": {"id": "9007199254740997", "player": 0, "kind": 0},
		"actionSummary": {"hasNonPhaseAction": true, "safeAdvance": {"type": KernelContract.CommandKind.PassResponse, "decision": "9007199254740997"}},
	}
	service.set_actions([
			{"id": "respond", "source": "33", "command": {"type": 12, "decision": "9007199254740997"}},
			{"id": "pass", "source": "0", "command": {"type": 13, "decision": "9007199254740997"}},
		])
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._status_own.set_status("我方", 0, {"life": 26, "lifeLimit": 34, "temporaryLife": 7, "sevenMode": 1})
	_expect(page._status_own._life.text == "生命 26/34", "life limit is projected independently of ordinary life")
	_expect(page._status_own._temporary_life.text == "临时生命 +7" and page._status_own._name.text.contains("耀"), "temporary life and seven-star mode have separate markers")
	page._status_own.set_status("我方", 0, {"life": 26, "lifeLimit": 34, "sevenMode": 2})
	_expect(page._status_own._name.text.contains("黯") and not page._status_own._name.text.contains("耀"), "mode switch replaces the old marker")
	for zone: String in ["ash", "side_deck"]:
		page._render_decision()
		await _settle()
		page._decision_confirmed("9007199254740993", "9007199254740995", "9007199254740997", ["@yes"])
		page._zone_clicked(zone, 0)
		await _settle()
		if page._decision.request.browse_mode:
			page.dismiss_cancel()
		page._return_to_response()
		_expect(page._decision.request.response_mode and page._decision.request.decision_id == "9007199254740997", "response survives %s browsing" % zone)
		_expect(page._decision.request.choices[1].id == "@no", "decline remains reachable after %s" % zone)
		_expect(service.selected_actions.is_empty(), "browsing does not submit")
	page._decision_confirmed("9007199254740993", "9007199254740995", "9007199254740997", ["@yes"])
	page._query_is_decision = false
	page._selection.begin("33", KernelContract.CommandKind.Respond)
	await _settle()
	_expect(service.selected_actions == ["respond"], "response stages its exact legal action")
	_expect(service.confirmations == 0 and page._confirm.visible, "response waits for final confirmation")
	page._confirm.confirmed.emit()
	_expect(service.confirmations == 1, "final response confirmation submits once")
	page.queue_free()
	await process_frame
	print("Response collection smoke: %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(0 if _failures == 0 else 2)

