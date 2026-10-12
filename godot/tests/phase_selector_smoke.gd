extends SceneTree

var _chosen: Array[Dictionary] = []

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> bool:
	if not condition:
		push_error("PHASE_SELECTOR: " + message)
		quit(2)
	return condition

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	var output: String = ""
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			output = arguments[index + 1]
	if not output.is_empty():
		DirAccess.make_dir_recursive_absolute(output)
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	page.service = FrontendService.new()
	page.service.match_snapshot = {
		"phase": 2,
		"actions": [
			{"id": "cast", "source": "0", "command": {"type": 14, "gate": "3"}},
			{"id": "end", "source": "0", "command": {"type": 14, "gate": "3", "skipCast": true}},
		],
	}
	page._phase_selector.action_chosen.disconnect(page._choose_phase_action)
	page._phase_selector.action_chosen.connect(_chosen.append)
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		var casting: Rect2 = page.canvas._own_casting_region
		var button: Rect2 = page._end_phase.get_global_rect()
		if not _expect(button.position.x >= casting.end.x, "phase control sits to the right of own casting at %s" % dimensions):
			return
		if not _expect(Rect2(Vector2.ZERO, dimensions).encloses(button), "phase control stays inside viewport"):
			return
		page._end_phase.disabled = false
		page._end_current_phase()
		await process_frame
		if not _expect(page._phase_selector.visible and page._phase_selector._choices.get_child_count() == 2, "main phase offers two explicit destinations"):
			return
		var cast: Button = page._phase_selector._choices.get_child(0) as Button
		var end: Button = page._phase_selector._choices.get_child(1) as Button
		if not _expect(cast.text == "切换到施法阶段" and end.text == "切换到结束阶段", "phase destination labels"):
			return
		if not output.is_empty():
			await RenderingServer.frame_post_draw
			var capture: Image = root.get_texture().get_image()
			if not _expect(capture.save_png(output.path_join("phase-%dx%d.png" % [dimensions.x, dimensions.y])) == OK, "save phase menu capture"):
				return
		end.pressed.emit()
		if not _expect(not page._phase_selector.visible and bool((_chosen.back()["command"] as Dictionary).get("skipCast", false)), "choosing end preserves the authoritative skip flag"):
			return
	page.queue_free()
	await process_frame
	quit(0)
