extends SceneTree

var client: WizardClient
var assertions: int = 0
var last_mouse: Vector2 = Vector2.ZERO
var picked_count: int = 0
var dropped_count: int = 0

func check(condition: bool, description: String) -> void:
	assertions += 1
	if not condition:
		push_error(description)
		quit(2)
		assert(condition, description)

func _initialize() -> void:
	run.call_deferred()

func frames(count: int = 3) -> void:
	for index: int in range(count):
		await process_frame

func mouse(at: Vector2, button: int = 0, pressed: bool = false, held: bool = false) -> void:
	var event: InputEventMouse
	if button == 0:
		var motion: InputEventMouseMotion = InputEventMouseMotion.new()
		motion.button_mask = MOUSE_BUTTON_MASK_LEFT if held else 0
		motion.relative = at - last_mouse
		event = motion
	else:
		var click: InputEventMouseButton = InputEventMouseButton.new()
		click.button_index = button
		click.pressed = pressed
		click.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed and button == MOUSE_BUTTON_LEFT else 0
		event = click
	event.position = at
	event.global_position = at
	last_mouse = at
	Input.parse_input_event(event)
	await frames(2)

func run() -> void:
	root.size = Vector2i(1280, 720)
	client = (load("res://scenes/wizard_client.tscn") as PackedScene).instantiate() as WizardClient
	root.add_child(client)
	await frames()
	client._timer.stop()
	check(client.start_match({"mode": "hotseat", "seed": 42}), "start scene match")
	client.uncover()
	await frames()
	var scene: WizardMatchView = client.match_scene
	var board: WizardBattlefield = scene.board
	board.selected.connect(func(_card: Dictionary) -> void: picked_count += 1)
	board.dropped.connect(func(_data: Dictionary, _destination: int, _target: String) -> void: dropped_count += 1)
	check(board.zones.size() == 10, "ten authored zones")
	check((board.zones["0:3"] as WizardZone3D).get_node("Slots").get_child_count() == 5, "five authored formation slots")
	var id: String = client.hand_order[0]
	var card: WizardCard3D = board.cards[id] as WizardCard3D
	var identity: int = card.get_instance_id()
	client.refresh_match()
	await frames()
	check((board.cards[id] as WizardCard3D).get_instance_id() == identity, "snapshot refresh preserves identity")
	var on_table: WizardCard3D
	for node: WizardCard3D in board.cards.values():
		if node.visible and not str(node.data["id"]).contains(":") and int(node.data["owner"]) == client.viewer:
			on_table = node
			break
	check(on_table != null, "real base formation in world")
	var point: Vector2 = board.camera.unproject_position(on_table.global_position)
	check(str((board.pick(point).get("card", {}) as Dictionary).get("id", "")) == str(on_table.data["id"]), "camera ray selects real 3D card")
	await mouse(board.global_position + point)
	await mouse(board.global_position + point, MOUSE_BUTTON_LEFT, true)
	await mouse(board.global_position + point, MOUSE_BUTTON_LEFT, false)
	check(str(client.inspected.get("id", "")) == str(on_table.data["id"]), "viewport input reaches 3D card")
	var picked_before: int = picked_count
	var panel: Control = scene.get_node("HUD/Content/DetailsPanel") as Control
	var panel_position: Vector2 = panel.position
	panel.global_position = board.global_position + point - Vector2(60, 60)
	panel.show()
	await frames()
	await mouse(board.global_position + point, MOUSE_BUTTON_LEFT, true)
	await mouse(board.global_position + point, MOUSE_BUTTON_LEFT, false)
	check(picked_count == picked_before, "floating detail panel blocks clicks through to world")
	panel.position = panel_position
	panel.hide()
	client.cancel_selection()
	await frames()
	var hand_card: WizardCardView = scene.hand.cards[id] as WizardCardView
	var hand_point: Vector2 = hand_card.get_global_transform() * Vector2(20, 65)
	await mouse(hand_point)
	# Hover changes the fan transform: use its actual raised location.
	hand_point = hand_card.get_global_transform() * Vector2(20, 65)
	await mouse(hand_point, MOUSE_BUTTON_LEFT, true)
	await mouse(hand_point + Vector2(0, -35), 0, false, true)
	check(root.gui_is_dragging(), "real mouse motion begins foreground card drag")
	var revision: String = str(client.view["revision"])
	await mouse(Vector2(2, 2), 0, false, true)
	await mouse(Vector2(2, 2), MOUSE_BUTTON_LEFT, false)
	check(not root.gui_is_dragging() and str(client.view["revision"]) == revision, "invalid drop changes no rules")
	var payload: Dictionary = {"wizard_card": true, "context": "match", "id": id, "generation": client.view["generation"], "revision": revision}
	var area: WizardZone3D = board.zones["0:3"] as WizardZone3D
	var area_point: Vector2 = board.camera.unproject_position(area.global_position + Vector3(2.9, 0.1, 0.7))
	check(board.surface._can_drop_data(area_point, payload), "foreground payload targets own 3D zone")
	payload["revision"] = "0"
	check(not board.surface._can_drop_data(area_point, payload), "stale drag rejected before bridge")
	# Actual drop goes through Surface -> controller -> C++ without auto-confirmation.
	await mouse(hand_card.get_global_transform() * Vector2(20, 65))
	hand_point = hand_card.get_global_transform() * Vector2(20, 65)
	await mouse(hand_point, MOUSE_BUTTON_LEFT, true)
	await mouse(hand_point + Vector2(0, -35), 0, false, true)
	await mouse(board.global_position + area_point, 0, false, true)
	await mouse(board.global_position + area_point, MOUSE_BUTTON_LEFT, false)
	check(str(client.view["revision"]) == revision, "3D drop never pays before confirmation")
	check(dropped_count == 1, "actual drop crossed surface signal into C++ interaction")
	client.cancel_selection()
	await frames()
	# Cancellation from outside the card itself, including focus and right click.
	for cancel_mode: int in range(3):
		hand_point = hand_card.get_global_transform() * Vector2(20, 65)
		await mouse(hand_point)
		hand_point = hand_card.get_global_transform() * Vector2(20, 65)
		await mouse(hand_point, MOUSE_BUTTON_LEFT, true)
		await mouse(hand_point + Vector2(0, -35), 0, false, true)
		check(root.gui_is_dragging(), "cancel fixture uses a real drag")
		if cancel_mode == 0:
			await mouse(board.global_position + area_point, MOUSE_BUTTON_RIGHT, true)
		elif cancel_mode == 1:
			var escape: InputEventKey = InputEventKey.new()
			escape.keycode = KEY_ESCAPE
			escape.pressed = true
			Input.parse_input_event(escape)
		else:
			scene.notification(Node.NOTIFICATION_WM_WINDOW_FOCUS_OUT)
		await frames()
		check(not root.gui_is_dragging() and str(client.view["revision"]) == revision, "cancel never submits a command")
		await mouse(Vector2(2, 2), MOUSE_BUTTON_LEFT, false)
	scene.browse_zone(0, 3)
	await frames()
	check(is_instance_valid(scene.browser) and scene.browser.visible, "crowded zones have independent card browser")
	scene.close_browser()
	# Successful operation, cue deduplication, settings pause and retained scene.
	var action: Dictionary = (client.view["actions"] as Array)[0] as Dictionary
	client._flat_action(action)
	client.confirm_selection()
	if client.covered:
		client.uncover()
	await frames()
	var batch: Dictionary = {"generation": client.view["generation"], "revision": client.view["revision"], "cues": [{"kind": 10, "source": "0", "player": client.viewer, "text": "荷载", "amount": 1}]}
	board.play_cues(batch)
	var effect_count: int = board.effects.get_child_count()
	board.play_cues(batch)
	check(board.effects.get_child_count() == effect_count, "same committed cue batch is not repeated")
	client.show_settings()
	await frames()
	check(client.match_scene == scene and bool(client.bridge.snapshot(client.viewer)["data"]["paused"]), "settings retains and pauses scene")
	client._settings_back(true)
	await frames()
	check(client.match_scene == scene and not bool(client.bridge.snapshot(client.viewer)["data"]["paused"]), "settings returns to same scene")
	client.covered = true
	client.refresh_match()
	check(board.cards.is_empty() and scene.hand.cards.is_empty() and board.effects.get_child_count() == 0, "handoff immediately clears private cards and effects")
	client.uncover()
	await frames()
	for node: WizardCard3D in board.cards.values():
		if bool(node.data.get("hidden", false)):
			check(node.title.text.is_empty() and node.state_label.text.is_empty() and not node.data.has("definition"), "hidden card has no face or definition")
	var scene_ref: WeakRef = weakref(scene)
	client._leave()
	await frames()
	check(scene_ref.get_ref() == null, "leaving frees the scene")
	print("MATCH_SCENE_SMOKE assertions=", assertions)
	client.queue_free()
	await frames()
	quit(0)
