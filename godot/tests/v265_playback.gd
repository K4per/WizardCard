extends SceneTree

class ReplayFixture extends FrontendService:
	var bad: bool = false
	var frames: Array[Vector2i] = []
	var commands: Array[Dictionary] = [{}, {}, {}]

	func match_request(method: StringName, arguments: Array = []) -> Dictionary:
		if method == &"read_replay_plan":
			if bad:
				error = "bad replay"
				return {"ok": false}
			return {"ok": true, "data": {"schema": KernelContract.SCHEMA, "commands": commands}}
		frames.append(Vector2i(int(arguments[1]), int(arguments[2])))
		return {"ok": true, "data": {"schema": KernelContract.SCHEMA}}

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var page: ReplayPage = load("res://pages/replay_page.tscn").instantiate()
	root.add_child(page)
	var fixture: ReplayFixture = ReplayFixture.new()
	page.service = fixture
	page._paths.append("record")
	page._files.add_item("record")
	page._load()
	page._load()
	if page._command.get_parsed_text() != "对局开始":
		push_error("Reloading at step zero must show the opening command caption")
		quit(1)
		return
	if not page.has_method("_toggle_playback"):
		push_error("Replay page is missing playback controls")
		quit(1)
		return
	page.call("_toggle_playback")
	var timer: Timer = page.get_node("PlaybackTimer")
	assert(not timer.is_stopped() and is_equal_approx(timer.wait_time, 1.0))
	page.call("_change_speed", 2)
	assert(is_equal_approx(timer.wait_time, 0.25))
	timer.timeout.emit()
	assert(page._step == 1)
	page.call("_begin_seek")
	assert(timer.is_stopped())
	page.call("_end_seek", true)
	page.call("_seek", 3)
	assert(page._step == 3 and timer.is_stopped())
	page.call("_seek", 1)
	assert(page._step == 1)
	page._viewer.select(1)
	page._viewer_changed(1)
	assert(fixture.frames.back() == Vector2i(1, 1))
	fixture.bad = true
	page._load()
	assert(page._path.is_empty() and page._frame.is_empty() and timer.is_stopped())
	fixture.bad = false
	fixture.commands.clear()
	page._load()
	assert(page._step == 0 and not page._path.is_empty() and page._play.disabled and not page._progress.editable)
	quit()
