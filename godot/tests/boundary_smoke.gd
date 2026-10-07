extends SceneTree

var _bridge: WizardBridge = WizardBridge.new()

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> bool:
	if not condition:
		push_error(message)
		quit(2)
	return condition

func _run() -> void:
	var directory: String = ProjectSettings.globalize_path("res://../build/v15/boundary-data")
	var assets: String = ProjectSettings.globalize_path("res://../assets")
	var args: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, args.size() - 1, 2):
		if args[index] == "--user-data":
			directory = args[index + 1]
		elif args[index] == "--assets":
			assets = args[index + 1]
	var initialized: Dictionary = _bridge.initialize(assets, directory)
	if not _expect(bool(initialized["ok"]), "initialize failed"):
		return
	if not _expect(not bool(_bridge.start_match({"seed": 4294967296})["ok"]), "oversized seed accepted"):
		return
	if not _expect(not bool(_bridge.start_match({"mode": _bridge})["ok"]), "native object accepted as DTO"):
		return
	if not _expect(bool(_bridge.start_match({"mode": "hotseat", "seed": 42})["ok"]), "start failed"):
		return
	if not _expect(not bool(_bridge.snapshot(4294967296)["ok"]), "actor narrowed to zero"):
		return
	var view: Dictionary = _bridge.snapshot(0)["data"] as Dictionary
	var viewer: int = int(view["actingPlayer"])
	view = _bridge.snapshot(viewer)["data"] as Dictionary
	var original: Dictionary = view.duplicate(true)
	var players: Array = view["players"] as Array
	var player: Dictionary = players[0] as Dictionary
	player["life"] = -100
	if not _expect((_bridge.snapshot(viewer)["data"] as Dictionary) == original, "snapshot mutation changed C++"):
		return
	var action: Dictionary = (original["actions"] as Array)[0] as Dictionary
	var generation: String = str(original["generation"])
	var revision: String = str(original["revision"])
	if not _expect(bool(_bridge.select_action(viewer, str(action["id"]), generation, revision)["ok"]), "select failed"):
		return
	_bridge.set_paused(true)
	if not _expect(not bool(_bridge.confirm_action(generation, revision)["ok"]), "paused confirmation accepted"):
		return
	_bridge.set_paused(false)
	if not _expect(bool(_bridge.confirm_action(generation, revision)["ok"]), "paused selection lost"):
		return
	_bridge.release_session()
	if not _expect(not bool(_bridge.confirm_action(generation, revision)["ok"]), "released callback accepted"):
		return
	_bridge.initialize(assets, directory)
	_bridge.start_match({"mode": "ai"})
	if not _expect(not bool(_bridge.snapshot(1)["ok"]), "AI private view exposed"):
		return
	_bridge.release_session()
	print("BRIDGE_BOUNDARY ok · exact IDs / detached snapshots / pause / release / AI privacy")
	quit(0)
