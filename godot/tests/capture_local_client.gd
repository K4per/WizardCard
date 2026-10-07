extends SceneTree

var client: WizardClient
var viewport: SubViewport
var output: String = ""
var reference: String = ""

func _initialize() -> void:
	call_deferred("run")

func snap(name: String) -> void:
	await process_frame
	await create_timer(0.1).timeout
	await RenderingServer.frame_post_draw
	assert(viewport.get_texture().get_size() == Vector2(viewport.size))
	if client.visible and not name.begins_with("title-scale"):
		assert(client.root_column.get_global_rect().end.y <= viewport.size.y + 1, "page overflow: " + name + str(client.root_column.get_global_rect()))
		assert(client.root_column.get_global_rect().end.x <= viewport.size.x + 1, "page width overflow: " + name)
	var image: Image = viewport.get_texture().get_image()
	var result: Error = image.save_png(output.path_join(name + ".png"))
	assert(result == OK)

func run() -> void:
	var values: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(0, values.size() - 1, 2):
		if values[index] == "--output":
			output = values[index + 1]
		elif values[index] == "--reference":
			reference = values[index + 1]
	assert(not output.is_empty() and not reference.is_empty())
	DirAccess.make_dir_recursive_absolute(output)
	viewport = SubViewport.new()
	viewport.size = Vector2i(1600, 1000)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var scene: PackedScene = load("res://scenes/wizard_client.tscn") as PackedScene
	client = scene.instantiate() as WizardClient
	viewport.add_child(client)
	await process_frame
	client._timer.stop()
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		viewport.size = dimensions
		await snap("title-%dx%d" % [dimensions.x, dimensions.y])
	client.edit_draft(client.bridge.create_draft("default")["data"] as Dictionary)
	client.inspected = client.definitions["fireball"] as Dictionary
	client._clear(client.editor_details)
	client._render_details(client.editor_details, client.inspected)
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
		viewport.size = dimensions
		await snap("editor-%dx%d" % [dimensions.x, dimensions.y])
	viewport.size = Vector2i(1280, 720)
	client.show_setup("ai")
	await snap("setup-1280x720")
	client.show_settings()
	await snap("settings-1280x720")
	var plan: Dictionary = client.bridge.read_replay_plan(reference)["data"] as Dictionary
	client.start_match({"mode": "hotseat", "seed": plan["seed"], "players": plan["players"]})
	client.uncover()
	for index: int in range(107):
		replay_step(plan, index)
	client.viewer = 0
	client.inspected = (client.definitions["fireball"] as Dictionary).duplicate(true)
	client.refresh_match()
	await snap("match-1280x720")
	viewport.size = Vector2i(1600, 1000)
	await snap("match-1600x1000")
	viewport.size = Vector2i(1920, 1080)
	await snap("match-1920x1080")
	viewport.size = Vector2i(1280, 720)
	client.viewer = int(client.view["actingPlayer"])
	client.refresh_match()
	for raw: Variant in client.view["actions"] as Array:
		var action: Dictionary = raw as Dictionary
		if str(action["source"]) == "0":
			client._flat_action(action)
			await snap("confirm-1280x720")
			client.cancel_selection()
			break
	for id: String in client.hand_order.duplicate():
		client._card_selected(client._find_card(id))
		if not (client.interaction.get("groups", []) as Array).is_empty():
			client._activate(0)
			if not (client.interaction.get("candidates", []) as Array).is_empty():
				await snap("target-1280x720")
				client.cancel_selection()
				break
		client.cancel_selection()
	client.notice = "复盘保存失败，请检查目录权限并重试。"
	client.refresh_match()
	await snap("save-error-layout-1280x720")
	client.notice = ""
	client.covered = true
	client.refresh_match()
	await snap("handoff-1280x720")
	client.uncover()
	client.show_settings()
	await snap("paused-1280x720")
	client._settings_back(true)
	for index: int in range(107, (plan["commands"] as Array).size()):
		replay_step(plan, index)
	client.covered = false
	client.inspected = {}
	client.refresh_match()
	await snap("result-1280x720")
	client.hide()
	viewport.size = Vector2i(1920, 1200)
	var ids: Array = client.definitions.keys()
	for batch: int in range(3):
		var grid: GridContainer = GridContainer.new()
		grid.theme = client.skin.theme
		grid.columns = 5
		grid.add_theme_constant_override("h_separation", 12)
		grid.add_theme_constant_override("v_separation", 12)
		viewport.add_child(grid)
		for index: int in range(batch * 10, batch * 10 + 10):
			var column: VBoxContainer = client.widgets.column(client.widgets.panel(grid))
			column.custom_minimum_size = Vector2(348, 570)
			var data: Dictionary = client.definitions[ids[index]] as Dictionary
			var card: WizardCardView = client.widgets.card(column, data, "detail", Vector2(220, 330))
			card.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
			var name_label: Label = client.widgets.label(column, str(data["name"]), 20)
			name_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			var effect: RichTextLabel = client.widgets.text(column, str(data["text"]), 140)
			effect.custom_minimum_size.x = 348
		await snap("cards-%d" % (batch + 1))
		grid.free()
	client.show()
	viewport.size = Vector2i(1280, 720)
	client.show_menu()
	# Actual native window resize, followed by a 2x logical canvas render.
	viewport.remove_child(client)
	root.add_child(client)
	root.size = Vector2i(1280, 720)
	await process_frame
	await create_timer(0.2).timeout
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(output.path_join("native-window-1280x720.png"))
	root.size = Vector2i(1440, 900)
	await process_frame
	await create_timer(0.2).timeout
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(output.path_join("native-window-1440x900.png"))
	print("NATIVE_WINDOW size=", root.size, " screen_dpi=", DisplayServer.screen_get_dpi(), " screen_scale=", DisplayServer.screen_get_scale())
	root.remove_child(client)
	viewport.add_child(client)
	viewport.size = Vector2i(2560, 1440)
	viewport.size_2d_override = Vector2i(1280, 720)
	viewport.size_2d_override_stretch = true
	await snap("title-scale-200-percent")
	print("LOCAL_CAPTURE_COMPLETE")
	quit(0)

func replay_step(plan: Dictionary, index: int) -> void:
	var row: Dictionary = (plan["commands"] as Array)[index] as Dictionary
	client.viewer = int(row["actor"])
	client.view = client.bridge.snapshot(client.viewer)["data"] as Dictionary
	var found: bool = false
	for raw: Variant in client.view["actions"] as Array:
		var action: Dictionary = raw as Dictionary
		if action["command"] == row["command"]:
			assert(bool(client.bridge.select_action(client.viewer, str(action["id"]), str(client.view["generation"]), str(client.view["revision"]))["ok"]))
			assert(bool(client.bridge.confirm_action(str(client.view["generation"]), str(client.view["revision"]))["ok"]))
			found = true
			break
	assert(found, "Missing reference action at " + str(index))
