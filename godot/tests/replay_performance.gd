extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var assets: String = ""
	var path: String = ""
	var user_directory: String = ""
	for index: int in range(args.size() - 1):
		if args[index] == "--assets":
			assets = args[index + 1]
		elif args[index] == "--replay":
			path = args[index + 1]
		elif args[index] == "--user-data":
			user_directory = args[index + 1]
	var bridge: RefCounted = ClassDB.instantiate(&"WizardBridge") as RefCounted
	var initialized: Dictionary = bridge.call(&"initialize", assets, user_directory)
	if not bool(initialized.get("ok", false)):
		push_error(str(initialized))
		quit(2)
		return
	var started: int = Time.get_ticks_usec()
	var plan: Dictionary = bridge.call(&"read_replay_plan", path)
	var load_ms: float = (Time.get_ticks_usec() - started) / 1000.0
	if not bool(plan.get("ok", false)):
		push_error(str(plan))
		quit(2)
		return
	var data: Dictionary = plan["data"]
	var commands: Array = data["commands"]
	var timings: Array[float] = []
	for step: int in [commands.size(), 0, commands.size() / 2, commands.size() / 2 - 1, commands.size() / 2 + 1, 0, commands.size()]:
		started = Time.get_ticks_usec()
		var frame: Dictionary = bridge.call(&"replay_frame", path, step, 0)
		timings.append((Time.get_ticks_usec() - started) / 1000.0)
		var expected: String = str(data["finalDigest"]) if step == commands.size() else str(commands[step - 1]["digest"]) if step > 0 else ""
		if not bool(frame.get("ok", false)) or (step > 0 and str(frame["data"]["digest"]) != expected):
			push_error("Seek digest mismatch at " + str(step))
			quit(1)
			return
	print(JSON.stringify({"commands": commands.size(), "loadMs": load_ms, "seekMs": timings, "digest": data["finalDigest"]}))
	bridge.call(&"release_session")
	quit(0)
