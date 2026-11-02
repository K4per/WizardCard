extends SceneTree

var _bridge: WizardBridge = WizardBridge.new()
var _options: Dictionary = {}
var _checked: int = 0
var _started_ms: int = 0
var _submitted_ms: int = 0
var _saved_ms: int = 0
var _snapshot_us: int = 0
var _submit_us: int = 0
var _duplicate_us: int = 0

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
	_started_ms = Time.get_ticks_msec()
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
	var expected_final_digest: String = str(plan["finalDigest"])
	if _options.has("--limit"):
		var limit: int = clampi(int(_options["--limit"]), 0, rows.size())
		rows = rows.slice(0, limit)
		if not rows.is_empty():
			expected_final_digest = str((rows.back() as Dictionary)["digest"])
	for raw: Variant in rows:
		var row: Dictionary = raw as Dictionary
		var viewer: int = int(row["actor"]) if mode == "hotseat" else 0
		var sample_started_us: int = Time.get_ticks_usec()
		var view: Dictionary = _check(_bridge.snapshot(viewer), "snapshot")
		_snapshot_us += Time.get_ticks_usec() - sample_started_us
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
		var generation: String = str(view["generation"])
		var revision: String = str(view["revision"])
		sample_started_us = Time.get_ticks_usec()
		_check(_bridge.submit_command(int(row["actor"]), row["command"], generation, revision), "submit reference command")
		_submit_us += Time.get_ticks_usec() - sample_started_us
		sample_started_us = Time.get_ticks_usec()
		var duplicate: Dictionary = _bridge.submit_command(int(row["actor"]), row["command"], generation, revision)
		_duplicate_us += Time.get_ticks_usec() - sample_started_us
		if bool(duplicate.get("ok", true)):
			push_error("Duplicate command accepted with stale revision")
			quit(5)
			return
		_checked += 1
	_submitted_ms = Time.get_ticks_msec()
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
	_saved_ms = Time.get_ticks_msec()
	var verified: Dictionary = _check(_bridge.verify_replay(str(saved["path"])), "verify")
	var verified_ms: int = Time.get_ticks_msec()
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
	if verified["digest"] != expected_final_digest:
		push_error("Final digest mismatch")
		quit(8)
		return
	_check(_bridge.release_session(), "release")
	if bool(_bridge.confirm_action("2", "1").get("ok", true)):
		push_error("Released handle remained valid")
		quit(9)
		return
	var report: Dictionary = {"ok": true, "steps": _checked, "digest": verified["digest"], "replay": saved["path"],
		"submitMs": _submitted_ms - _started_ms, "saveMs": _saved_ms - _submitted_ms,
		"verifyMs": verified_ms - _saved_ms, "totalMs": verified_ms - _started_ms,
		"snapshotUs": _snapshot_us, "acceptedSubmitUs": _submit_us, "duplicateSubmitUs": _duplicate_us}
	if _options.has("--report"):
		var file: FileAccess = FileAccess.open(str(_options["--report"]), FileAccess.WRITE)
		if file == null:
			push_error("Unable to write verification report")
			quit(10)
			return
		file.store_string(JSON.stringify(report, "\t"))
	print("BRIDGE_SMOKE ", JSON.stringify(report))
	quit(0)
