extends SceneTree

class RecordingService extends FrontendService:
	var submitted: Array[String] = []
	func match_request(method: StringName, arguments: Array = []) -> Dictionary:
		if method == &"select_action":
			submitted.append(str(arguments[1]))
		return {"ok": true, "data": {}}

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
	var service: RecordingService = RecordingService.new()
	service.match_mode = "ai"
	service.match_snapshot = {
		"generation": "9007199254740993", "revision": "9007199254740995", "viewer": 0, "result": -1,
		"players": [{"life": 40}, {"life": 40}], "cards": [],
		"decision": {"id": "9007199254740997", "player": 0, "kind": 0},
		"actions": [
			{"id": "respond", "source": "33", "command": {"type": 12, "decision": "9007199254740997"}},
			{"id": "pass", "source": "0", "command": {"type": 13, "decision": "9007199254740997"}},
		],
	}
	page.service = service
	page.canvas.configure(MatchProjection.field(service.match_snapshot))
	page._status_own.set_status("我方", 0, {"life": 26, "lifeLimit": 34, "temporaryLife": 7, "sevenMode": 1})
	_expect(page._status_own._life.text == "生命 26/34", "life limit is projected independently of ordinary life")
	_expect(page._status_own._temporary_life.text == "临时生命 +7" and page._status_own._name.text.contains("耀"), "temporary life and seven-star mode have separate markers")
	page._status_own.set_status("我方", 0, {"life": 26, "lifeLimit": 34, "sevenMode": 2})
	_expect(page._status_own._name.text.contains("黯") and not page._status_own._name.text.contains("耀"), "mode switch replaces the old marker")
	for zone: String in ["ash", "side_deck"]:
		page._render_decision()
		await _settle()
		page._collapse_tray()
		page._zone_clicked(zone, 0)
		await _settle()
		if page._decision.request.browse_mode:
			page.dismiss_cancel()
		page._toggle_hand()
		await _settle()
		_expect(page._decision.request.response_mode and page._decision.request.decision_id == "9007199254740997", "collapsed response survives %s browsing" % zone)
		_expect(page._decision.request.pass_id == "pass", "native pass remains reachable after %s" % zone)
		_expect(service.submitted.is_empty(), "browsing and collapse do not submit an action")
	# The exact legal response still confirms after both collection clicks.
	page._render_decision()
	await _settle()
	page._decision._options["respond"].button_pressed = true
	page._decision._confirm.pressed.emit()
	_expect(service.submitted == ["respond"], "response confirms its exact legal action")
	page.queue_free()
	await process_frame
	print("Response collection smoke: %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(0 if _failures == 0 else 2)

