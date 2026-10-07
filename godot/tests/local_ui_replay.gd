extends SceneTree

var client: WizardClient
var arguments: Dictionary = {}

func _initialize() -> void:
	var values: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, values.size() - 1, 2):
		arguments[values[index]] = values[index + 1]
	run.call_deferred()

func run() -> void:
	client = (load("res://scenes/wizard_client.tscn") as PackedScene).instantiate() as WizardClient
	root.add_child(client)
	await process_frame
	client._timer.stop()
	var reference: Dictionary = client.bridge.read_replay_plan(str(arguments["--reference"]))
	assert(bool(reference["ok"]), "reference readable")
	var plan: Dictionary = reference["data"] as Dictionary
	var mode: String = str(arguments["--mode"])
	assert(client.start_match({"mode": mode, "seed": plan["seed"], "players": plan["players"], "difficulty": int(arguments.get("--difficulty", 1))}))
	var rows: Array = plan["commands"] as Array
	for index: int in range(rows.size()):
		var row: Dictionary = rows[index] as Dictionary
		if mode == "hotseat" and client.covered:
			client.uncover()
		if mode == "tutorial":
			await continue_lessons()
		var revision: String = str(client.view["revision"])
		if mode != "hotseat" and int(row["actor"]) == 1:
			client.match_scene.board.finish_effects()
			client._tick()
		else:
			var found: bool = false
			for raw: Variant in client.view["actions"] as Array:
				var action: Dictionary = raw as Dictionary
				if action["command"] == row["command"]:
					client._flat_action(action)
					assert(str(client.view["revision"]) == revision, "selection must not pay")
					client.confirm_selection()
					found = true
					break
			assert(found, "missing UI action at step " + str(index))
		assert(str(client.view["revision"]) != revision, "UI must commit exactly once")
		await process_frame
	if mode == "tutorial":
		await continue_lessons()
		assert(bool((client.view["tutorial"] as Dictionary)["complete"]))
	else:
		assert(int(client.view["result"]) >= 0, "full match reaches result screen")
	client.save_replay()
	assert(client.status_label.visible, "save feedback visible")
	var saved: Dictionary = client.bridge.save_replay()["data"] as Dictionary
	var verification: Dictionary = client.bridge.verify_replay(str(saved["path"]))
	assert(bool(verification["ok"]))
	var result: Dictionary = verification["data"] as Dictionary
	assert(str(result["digest"]) == str(plan["finalDigest"]))
	var digests: Array = result["digests"] as Array
	assert(digests.size() == rows.size())
	for index: int in range(rows.size()):
		assert(str(digests[index]) == str((rows[index] as Dictionary)["digest"]), "UI digest at step " + str(index))
	assert(client.skin.errors.is_empty())
	var report: Dictionary = {"ok": true, "mode": mode, "steps": rows.size(), "digest": result["digest"], "replay": saved["path"]}
	var file: FileAccess = FileAccess.open(str(arguments["--report"]), FileAccess.WRITE)
	assert(file != null)
	file.store_string(JSON.stringify(report, "\t"))
	print("LOCAL_UI_REPLAY ", JSON.stringify(report))
	client.queue_free()
	await process_frame
	quit(0)

func continue_lessons() -> void:
	var tutorial: Dictionary = client.view["tutorial"] as Dictionary
	while bool(tutorial["canContinue"]) and not bool(tutorial["complete"]):
		client._continue_tutorial()
		await process_frame
		tutorial = client.view["tutorial"] as Dictionary
