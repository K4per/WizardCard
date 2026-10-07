extends SceneTree

func _initialize() -> void:
	run.call_deferred()

func run() -> void:
	var output: String = ""
	var args: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, args.size() - 1, 2):
		if args[index] == "--output":
			output = args[index + 1]
	assert(not output.is_empty())
	DirAccess.make_dir_recursive_absolute(output)
	var viewport: SubViewport = SubViewport.new()
	viewport.size = Vector2i(1920, 1080)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	viewport.own_world_3d = true
	root.add_child(viewport)
	var gallery: WizardArtGallery = (load("res://scenes/art/nonmodel_gallery.tscn") as PackedScene).instantiate() as WizardArtGallery
	viewport.add_child(gallery)
	await process_frame
	gallery.timer.stop()
	for reduced: bool in [false, true]:
		gallery.reduced = reduced
		gallery.replay()
		await create_timer(0.04 if reduced else 0.09).timeout
		gallery.paused = true
		for node: Node in gallery.effects.get_children():
			(node as WizardArtEffect3D).set_paused(true)
		await RenderingServer.frame_post_draw
		assert(viewport.get_texture().get_image().save_png(output.path_join("effects-" + ("reduced" if reduced else "normal") + ".png")) == OK)
		gallery.paused = false
	gallery.queue_free()
	await process_frame
	print("ART_EFFECTS_CAPTURE renderer=", RenderingServer.get_video_adapter_name())
	quit(0)
