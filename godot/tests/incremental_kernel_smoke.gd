extends SceneTree

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _check(value: bool, description: String) -> bool:
	if not value:
		push_error(description)
		failures += 1
	return value

func _run() -> void:
	var service: FrontendService = FrontendService.new()
	if not _check(service.initialize(ProjectSettings.globalize_path("res://../assets"), ProjectSettings.globalize_path("res://../build/architecture-baseline/frontend-smoke")), "initialize current native bridge"):
		quit(1)
		return
	_check(service.start_match({"mode": "hotseat", "seed": 42}), "start current rules match")
	for step: int in range(10):
		var view: Dictionary = service.snapshot()
		service.match_viewer = int(view.get("actingPlayer", 0))
		view = service.snapshot(service.match_viewer)
		_check(service.match_state != null and service.match_state.data == view, "typed stable snapshot installs")
		_check(not view.has("actions") and not view.has("events"), "stable view omits combinations and history")
		var intent: Dictionary = {"source": "0", "selections": []}
		var command: Dictionary = {}
		for depth: int in range(32):
			var query: Dictionary = service.match_request(&"query_actions", [service.match_viewer, intent, str(view.generation), str(view.revision)])
			if not _check(bool(query.get("ok", false)), "query accepted: " + str(query)):
				break
			var parsed: ActionOptions = ActionOptions.parse(query.get("data", {}))
			if not _check(parsed != null and parsed.matches(view, service.match_viewer), "query binds exact viewer and context"):
				break
			if not parsed.complete.is_empty():
				command = parsed.complete
				break
			if not _check(not parsed.options.is_empty(), "query returns next field step=%s intent=%s query=%s" % [step, intent, query]):
				break
			var option: Dictionary = parsed.options[0]
			if parsed.field == KernelContract.ActionField.Operation:
				for candidate: Dictionary in parsed.options:
					if int(candidate["value"]) != KernelContract.CommandKind.Surrender:
						option = candidate
						break
			intent["selections"].append({"field": parsed.field, "value": option["value"]})
		_check(not command.is_empty(), "bounded query reaches complete command")
		var selected: Dictionary = service.match_request(&"select_intent", [service.match_viewer, intent, str(view.generation), str(view.revision)])
		_check(bool(selected.get("ok", false)), "stage full intent")
		_check(service.snapshot() == view, "selection does not mutate projection")
		var accepted: Dictionary = service.match_request(&"confirm_action", [str(view.generation), str(view.revision)])
		_check(bool(accepted.get("ok", false)), "confirm current plan")
		_check(not bool(service.match_request(&"confirm_action", [str(view.generation), str(view.revision)]).get("ok", false)), "reject duplicate confirmation")
		service.poll_session()
		if failures > 0:
			break
	var now: Dictionary = service.snapshot(service.match_viewer)
	var idle: Dictionary = service.match_request(&"poll_match", [service.match_viewer, str(now.generation), str(now.revision)])
	_check(not idle.get("data", {}).has("snapshot"), "idle poll carries clock only")
	_check(service.save_replay(), "save format 5 replay")
	var frame: Dictionary = service.match_request(&"replay_frame", [service.last_replay_path, 0, 0])
	_check(bool(frame.get("ok", false)), "seek current replay")
	var page: MatchPage = load("res://pages/match_page.tscn").instantiate() as MatchPage
	root.add_child(page)
	page.configure(service)
	await process_frame
	_check(page.canvas != null, "typed frontend page initializes")
	page.queue_free()
	await process_frame
	service.release()
	print("Incremental kernel frontend smoke failures: ", failures)
	quit(0 if failures == 0 else 1)
