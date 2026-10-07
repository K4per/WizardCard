extends SceneTree

var client: WizardClient
var assertions: int = 0

func check(condition: bool, description: String) -> void:
	assertions += 1
	if not condition:
		push_error(description)
		quit(2)
		assert(condition, description)

func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var scene: PackedScene = load("res://scenes/wizard_client.tscn") as PackedScene
	client = scene.instantiate() as WizardClient
	root.add_child(client)
	await process_frame
	client._timer.stop()
	check(client.definitions.size() == 30, "30 live definitions")
	check(client.skin.texture("analysis_cost") != null, "analysis cost icon")
	check(client.skin.texture("master_analytic") != null, "new painted card frame")
	check(client.skin.errors.is_empty(), "no missing initial assets")
	check(client.audio.streams.size() == 17 and client.audio.errors.is_empty(), "all existing MP3 files decode")
	for event: String in client.audio.entries:
		client.audio.cue(event)
		check(client.audio.voices.size() <= WizardAudio.MAX_VOICES, "bounded audio voices")
	client.audio.apply_settings({"masterVolume": 0, "soundVolume": 80})
	check(client.audio.voices.is_empty(), "mute clears pending voices")
	client.audio.apply_settings(client.settings)
	check(client.page == "menu", "title screen")
	var menu_buttons: Array[Node] = client.find_children("*", "Button", true, false)
	check(menu_buttons.size() == 5, "title has five menu entries")
	for node: Node in menu_buttons:
		check((node as Button).custom_minimum_size == Vector2(320, 48), "readable title button hit area")
	var decorations: int = 0
	for node: Node in client.find_children("*", "Control", true, false):
		if node is WizardCardView:
			decorations += 1
	check(decorations == 0, "no decorative cards on title")
	client.show_setup("ai")
	check(client.page == "setup", "AI setup")
	client.show_decks()
	var created: Dictionary = client.bridge.create_draft("default")
	check(bool(created["ok"]), "copy preset into draft")
	client.edit_draft(created["data"] as Dictionary)
	var release: InputEventMouseButton = InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.pressed = false
	(client.editor_deck.get_child(0) as WizardCardView)._gui_input(release)
	check(not client.inspected.is_empty(), "card click drives persistent details")
	check(client.editor_deck.get_child_count() == 30, "every deck copy is a separate card")
	for card: Node in client.editor_deck.get_children():
		check(card is WizardCardView, "deck displays cards")
	var first_card: WizardCardView = client.editor_deck.get_child(0) as WizardCardView
	var first_id: String = str(first_card.definition["id"])
	var right_click: InputEventMouseButton = InputEventMouseButton.new()
	right_click.button_index = MOUSE_BUTTON_RIGHT
	right_click.pressed = true
	first_card._gui_input(right_click)
	check(client.editor_deck.get_child_count() == 29, "right click removes exactly one copy")
	client._adjust_draft(first_id, 1)
	client._adjust_draft("fireball", -1)
	check(client.editor_deck.get_child_count() == 29, "remove one card")
	client._adjust_draft("fireball", 1)
	check(client._save_draft(), "atomic deck persistence")
	var values: PackedStringArray = OS.get_cmdline_user_args()
	var directory: String = ""
	for index: int in range(0, values.size() - 1, 2):
		if values[index] == "--user-data":
			directory = values[index + 1]
	check(not directory.is_empty(), "tests always isolate user data")
	var deck_file: String = directory.path_join("decks.json")
	var backup: String = directory.path_join("decks-save-test.json")
	check(DirAccess.rename_absolute(deck_file, backup) == OK, "preserve saved draft")
	check(DirAccess.make_dir_absolute(deck_file) == OK, "inject atomic replace failure")
	client._adjust_draft("fireball", -1)
	var unsaved: Dictionary = client.draft.duplicate(true)
	check(not client._save_draft() and client.dirty and client.draft == unsaved and client.status_label.visible, "save failure retains editable draft and reports error")
	check(DirAccess.remove_absolute(deck_file) == OK, "remove isolated failure fixture")
	check(DirAccess.rename_absolute(backup, deck_file) == OK, "restore saved file")
	client._adjust_draft("fireball", 1)
	check(client._save_draft(), "retry save after failure")
	client._reload_library()
	check((client.library["drafts"] as Array).size() >= 1, "draft readable through existing library")
	client.query = {"type": 1, "name": "火球"}
	client._refresh_pool()
	check(client.editor_pool.get_child_count() == 1, "combined query uses core model")
	check(client.start_match({"mode": "hotseat", "seed": 42}), "start hotseat")
	check(not (client.definitions["fireball"] as Dictionary).is_empty(), "page transitions preserve shared card definitions")
	check(client.covered, "initial handoff hides private UI")
	client.uncover()
	# Native get_class returns the base type; inspect the scene tree directly.
	var board: WizardBattlefield = find_board(client)
	check(board != null and board.zones.size() == 10, "both players have five visible zones")
	var order: Array[String] = client.hand_order.duplicate()
	client._reorder_hand(order[-1], order[0])
	check(client.hand_order[0] == order[-1], "drag reorder inserts before another hand card")
	check(client.status_label.visible == not client.notice.is_empty(), "notice visibility")
	var first: Dictionary = (client.view["actions"] as Array)[0] as Dictionary
	var revision: String = str(client.view["revision"])
	client._flat_action(first)
	check(str(client.view["revision"]) == revision, "selection does not submit")
	client.show_settings()
	check(bool((client.bridge.snapshot(client.viewer)["data"] as Dictionary)["paused"]), "settings pause")
	client._settings_back(true)
	check(client.interaction.get("pending") != null, "pause preserves confirmation")
	client.confirm_selection()
	check(str(client.view["revision"]) != revision, "final confirmation submits once")
	var interaction_before: Dictionary = client.interaction.duplicate(true)
	client._card_if_current((client.view["cards"] as Array)[0] as Dictionary, str(client.view["generation"]), revision)
	check(client.interaction == interaction_before, "old card widgets cannot change a newer decision")
	var definition: Dictionary = client.definitions["fireball"] as Dictionary
	client.inspected = definition
	client.refresh_match()
	check(not definition.is_empty(), "handoff never clears shared definition dictionaries")
	client._leave()
	check(client.page == "menu", "leave persists and returns")
	check(client.start_match({"mode": "ai", "seed": 42, "difficulty": 2}), "hard AI session")
	check(not bool(client.bridge.snapshot(1)["ok"]), "AI opponent projection stays private")
	client._leave()
	check(client.start_match({"mode": "tutorial"}), "fixed tutorial session")
	check(client.view.get("tutorial") != null, "real tutorial prompt")
	check(client.skin.errors.is_empty(), "all used assets resolve")
	print("LOCAL_UI_SMOKE assertions=", assertions, " zones=10 deck_cards=30")
	client.queue_free()
	await process_frame
	quit(0)

func find_board(node: Node) -> WizardBattlefield:
	if node is WizardBattlefield:
		return node as WizardBattlefield
	for child: Node in node.get_children():
		var result: WizardBattlefield = find_board(child)
		if result != null:
			return result
	return null
