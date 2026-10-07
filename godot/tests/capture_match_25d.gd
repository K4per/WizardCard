extends SceneTree

var client: WizardClient
var viewport: SubViewport
var output: String = ""
var reference: String = ""
var report: Dictionary = {}

func _initialize() -> void:
	run.call_deferred()

func snap(name: String) -> void:
	await process_frame
	await create_timer(0.15).timeout
	await RenderingServer.frame_post_draw
	assert(client.root_column.get_global_rect().end.y <= viewport.size_2d_override.y + 1 if viewport.size_2d_override.y > 0 else client.root_column.get_global_rect().end.y <= viewport.size.y + 1, "page overflow")
	assert(viewport.get_texture().get_image().save_png(output.path_join(name + ".png")) == OK)

func run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, args.size() - 1, 2):
		if args[index] == "--output":
			output = args[index + 1]
		elif args[index] == "--reference":
			reference = args[index + 1]
	assert(not output.is_empty() and not reference.is_empty())
	DirAccess.make_dir_recursive_absolute(output)
	viewport = SubViewport.new()
	viewport.size = Vector2i(1280, 720)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	client = (load("res://scenes/wizard_client.tscn") as PackedScene).instantiate() as WizardClient
	viewport.add_child(client)
	await process_frame
	client._timer.stop()
	var plan: Dictionary = client.bridge.read_replay_plan(reference)["data"] as Dictionary
	assert(client.start_match({"mode": "hotseat", "seed": plan["seed"], "players": plan["players"]}))
	client.uncover()
	await snap("opening-1280x720")
	for index: int in range(107):
		var row: Dictionary = (plan["commands"] as Array)[index] as Dictionary
		client.viewer = int(row["actor"])
		client.covered = false
		client.refresh_match()
		var found: bool = false
		for raw: Variant in client.view["actions"] as Array:
			var action: Dictionary = raw as Dictionary
			if action["command"] == row["command"]:
				client._flat_action(action)
				client.confirm_selection()
				found = true
				break
		assert(found)
		await process_frame
	client.covered = false
	client.viewer = 0
	client.refresh_match()
	client.match_scene.board.finish_effects()
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		viewport.size = dimensions
		await snap("match-%dx%d" % [dimensions.x, dimensions.y])
	client.match_scene.browse_zone(0, 3)
	await snap("zone-browser")
	client.match_scene.close_browser()
	client.inspected = client._find_card(client.hand_order[0]) if not client.hand_order.is_empty() else {}
	client.refresh_match()
	await snap("card-detail")
	client.match_scene.board.play_cues({"generation": client.view["generation"], "revision": client.view["revision"], "cues": [{"kind": 7, "source": "0", "player": client.viewer, "text": "生命", "amount": -3}]})
	await snap("effect-layout-synthetic")
	client.match_scene.board.finish_effects()
	# Simulated logical scaling; not a claim of changing Windows display DPI.
	viewport.size = Vector2i(1920, 1080)
	viewport.size_2d_override = Vector2i(1280, 720)
	viewport.size_2d_override_stretch = true
	await snap("logical-scale-150-percent")
	viewport.size_2d_override = Vector2i.ZERO
	await process_frame
	var samples: Array[float] = []
	for index: int in range(180):
		var start: int = Time.get_ticks_usec()
		await RenderingServer.frame_post_draw
		await process_frame
		samples.append(float(Time.get_ticks_usec() - start) / 1000.0)
	samples.sort()
	var total: float = 0
	for sample: float in samples:
		total += sample
	report = {"adapter": RenderingServer.get_video_adapter_name(), "renderer": "gl_compatibility", "resolution": [1920, 1080], "samples": samples.size(), "meanFrameMs": total / samples.size(), "p95FrameMs": samples[int(samples.size() * 0.95)], "videoMemoryBytes": Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED), "sceneCards": client.match_scene.board.cards.size(), "cardTextures": client.match_scene.board.texture_cache.size(), "note": "GPU offscreen render including host window; static replay step 107, not full gameplay or native DPI acceptance"}
	client.covered = true
	client.refresh_match()
	await snap("handoff")
	client.uncover()
	client.show_settings()
	await snap("settings")
	client._settings_back(true)
	var file: FileAccess = FileAccess.open(output.path_join("performance.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify(report, "\t"))
	print("MATCH_25D_CAPTURE ", JSON.stringify(report))
	client.queue_free()
	await process_frame
	quit(0)
