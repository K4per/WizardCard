extends SceneTree

var _bridge: WizardBridge = WizardBridge.new()
var _options: Dictionary = {}
var _checked: int = 0

func _initialize() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, args.size() - 1, 2):
		_options[args[index]] = args[index + 1]
	_run.call_deferred()

func _check(response: Dictionary, context: String) -> Dictionary:
	if not bool(response.get("ok", false)):
		push_error("%s: %s" % [context, str(response)])
		quit(2)
		return {}
	return response.get("data", {}) as Dictionary

func _run() -> void:
	var assets: String = str(_options.get("--assets", ProjectSettings.globalize_path("res://../assets")))
	var data_dir: String = str(_options.get("--user-data", ProjectSettings.globalize_path("res://../build/v15/smoke-data")))
	_check(_bridge.initialize(assets, data_dir), "initialize")
	var plan: Dictionary = _check(_bridge.read_replay_plan(str(_options["--reference"])), "reference")
	if plan.is_empty():
		return
	var mode: String = str(_options.get("--mode", "hotseat"))
	var difficulty: int = int(_options.get("--difficulty", 1))
	_check(_bridge.start_match({"mode": mode, "difficulty": difficulty, "seed": plan["seed"], "players": plan["players"]}), "start")
	var rows: Array = plan["commands"] as Array
	for raw: Variant in rows:
		var row: Dictionary = raw as Dictionary
		var viewer: int = int(row["actor"]) if mode == "hotseat" else 0
		var view: Dictionary = _check(_bridge.snapshot(viewer), "snapshot")
		if mode == "tutorial":
			var tutorial: Dictionary = view["tutorial"] as Dictionary
			while bool(tutorial["canContinue"]) and not bool(tutorial["complete"]):
				_check(_bridge.continue_tutorial(str(view["generation"]), str(view["revision"])), "lesson")
				view = _check(_bridge.snapshot(0), "lesson snapshot")
				tutorial = view["tutorial"] as Dictionary
		if mode != "hotseat" and int(row["actor"]) == 1:
			_check(_bridge.step_ai(str(view["generation"]), str(view["revision"])), "AI commit")
			_checked += 1
			continue
		var actions: Array = view["actions"] as Array
		var selected: String = ""
		for raw_action: Variant in actions:
			var action: Dictionary = raw_action as Dictionary
			if action["command"] == row["command"]:
				selected = str(action["id"])
				break
		if selected.is_empty():
			push_error("No legal action for reference step %d" % _checked)
			quit(3)
			return
		var generation: String = str(view["generation"])
		var revision: String = str(view["revision"])
		_check(_bridge.select_action(int(row["actor"]), selected, generation, revision), "select")
		var uncommitted: Dictionary = _check(_bridge.snapshot(int(row["actor"])), "selection snapshot")
		if uncommitted != view:
			push_error("Selection mutated the rule state")
			quit(4)
			return
		_check(_bridge.confirm_action(generation, revision), "confirm")
		var duplicate: Dictionary = _bridge.confirm_action(generation, revision)
		if bool(duplicate.get("ok", true)):
			push_error("Duplicate confirmation was accepted")
			quit(5)
			return
		_checked += 1
	if mode == "tutorial":
		var view: Dictionary = _check(_bridge.snapshot(0), "final tutorial")
		var tutorial: Dictionary = view["tutorial"] as Dictionary
		while bool(tutorial["canContinue"]) and not bool(tutorial["complete"]):
			_check(_bridge.continue_tutorial(str(view["generation"]), str(view["revision"])), "final lesson")
			view = _check(_bridge.snapshot(0), "final lesson snapshot")
			tutorial = view["tutorial"] as Dictionary
		if not bool(tutorial["complete"]):
			push_error("Tutorial did not complete through the application layer")
			quit(11)
			return
	var saved: Dictionary = _check(_bridge.save_replay(), "save")
	var verified: Dictionary = _check(_bridge.verify_replay(str(saved["path"])), "verify")
	var digests: Array = verified["digests"] as Array
	if digests.size() != rows.size():
		push_error("Recorded command count mismatch")
		quit(6)
		return
	for index: int in range(rows.size()):
		var row: Dictionary = rows[index] as Dictionary
		if digests[index] != row["digest"]:
			push_error("Digest diverged at step %d" % index)
			quit(7)
			return
	if verified["digest"] != plan["finalDigest"]:
		push_error("Final digest mismatch")
		quit(8)
		return
	_check(_bridge.release_session(), "release")
	if bool(_bridge.confirm_action("2", "1").get("ok", true)):
		push_error("Released handle remained valid")
		quit(9)
		return
	var report: Dictionary = {"ok": true, "steps": _checked, "digest": verified["digest"], "replay": saved["path"]}
	if _options.has("--report"):
		var file: FileAccess = FileAccess.open(str(_options["--report"]), FileAccess.WRITE)
		if file == null:
			push_error("Unable to write verification report")
			quit(10)
			return
		file.store_string(JSON.stringify(report, "\t"))
	print("BRIDGE_SMOKE ", JSON.stringify(report))
	quit(0)
