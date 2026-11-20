extends SceneTree

# A presentation fixture. All point tests go through the viewport and FieldCanvas signals.
var definitions: Dictionary = {}
var samples: Array[Dictionary] = []
var picked: String = ""
var output_dir: String
var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _card(id: String, instance: String) -> CardPresentation:
	var definition: Dictionary = (definitions[id] as Dictionary).duplicate(true)
	definition["type"] = {"formation": 3, "analytic": 1, "analysis": 1, "word": 2, "incantation": 2, "rune": 4, "talisman": 5}.get(str(definition["type"]), 1)
	var card: CardPresentation = CardPresentation.from_definition(definition)
	card.instance_id = instance
	return card

func _fixture(formation_ids: Array, hand_count: int) -> FieldPresentation:
	var result: FieldPresentation = FieldPresentation.new()
	var spell_ids: Array[String] = []
	for id: String in definitions:
		if str((definitions[id] as Dictionary)["type"]) in ["analytic", "analysis"]:
			spell_ids.append(id)
	assert(not spell_ids.is_empty())
	for owner: int in range(2):
		for index: int in range(formation_ids.size()):
			var id: String = str(formation_ids[index])
			var formation: FormationPresentation = FormationPresentation.new()
			formation.card = _card(id, "%d-f%d" % [owner, index])
			formation.body_size = int(definitions[id]["body"])
			formation.ring_count = int(definitions[id]["rings"])
			for ring: int in range(formation.ring_count):
				var card: CardPresentation = _card(spell_ids[(index + ring) % spell_ids.size()], "%d-s%d-%d" % [owner, index, ring])
				card.status = "解析剩余 2" if ring % 2 == 0 else "已解析"
				if owner == 1 and ring == 1:
					card = CardPresentation.anonymous(card.instance_id)
				formation.occupants[ring] = card
			if owner == 0:
				result.own_formations.append(formation)
			else:
				result.opponent_formations.append(formation)
	for index: int in range(hand_count):
		result.hand.append(_card(spell_ids[index % spell_ids.size()], "hand-%d" % index))
	for index: int in range(12):
		result.own_casting.append(_card(spell_ids[index % spell_ids.size()], "cast-own-%d" % index))
		result.opponent_casting.append(_card(spell_ids[index % spell_ids.size()], "cast-opp-%d" % index))
	result.own_casting_slots = 12
	result.opponent_casting_slots = 12
	result.opponent_hand_count = hand_count
	result.own_deck_count = 20
	result.opponent_deck_count = 20
	result.own_side_count = 8
	result.opponent_side_count = 8
	return result

func _click(point: Vector2) -> void:
	for down: bool in [true, false]:
		var ev := InputEventMouseButton.new()
		ev.position = point
		ev.global_position = point
		ev.button_index = MOUSE_BUTTON_LEFT
		ev.pressed = down
		root.push_input(ev, true)

func _measure(canvas: FieldCanvas, layout_name: String, state: String) -> void:
	var misses: Array[String] = []
	var blocked := 0
	var checks := 0
	var min_host_art := INF
	var min_board_art := INF
	for group: Array in [canvas.field.own_formations, canvas.field.opponent_formations]:
		for host: FormationPresentation in group:
			var bounds := canvas.card_bounds(host.card)
			min_host_art = minf(min_host_art, FieldCanvas.board_artwork_rect(bounds, host.card).size.y)
	for hit: Dictionary in canvas._card_hits:
		var card: CardPresentation = hit.card
		var rect: Rect2 = hit.get("face_rect", hit.rect)
		if not card.hidden and not card.face_down:
			var art: Rect2 = hit.get("art_rect", Rect2())
			min_board_art = minf(min_board_art, minf(art.size.x, art.size.y))
		if card.instance_id.begins_with("hand-"):
			continue
		var covering := canvas.card_at(rect.get_center())
		if not canvas.hand_collapsed and covering != null and covering.instance_id.begins_with("hand-"):
			continue
		picked = ""
		_click(canvas.get_global_transform_with_canvas() * rect.get_center())
		checks += 1
		if picked != card.instance_id:
			misses.append(card.instance_id + "->" + picked)
	for hit: Dictionary in canvas._ring_hits:
		var point: Vector2 = (hit.rect as Rect2).get_center()
		var card := canvas.card_at(point)
		if canvas._opponent_hand_at(point) or (card != null and card.instance_id.begins_with("hand-")):
			blocked += 1
	var record := {"layout": layout_name, "window": [root.size.x, root.size.y], "fontPercent": Typography.scale_percent,
		"hand": canvas.field.hand.size(), "state": state, "clicks": checks, "misses": misses,
		"blockedRingCenters": blocked, "minHostArtHeight": min_host_art, "minBoardArtDimension": min_board_art}
	samples.append(record)
	if not misses.is_empty() or (canvas.hand_collapsed and blocked > 0) or min_board_art <= 0 or min_host_art <= 0:
		failures += 1
		if failures <= 4:
			printerr(JSON.stringify(record))

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	var out_i := args.find("--output")
	if out_i < 0 or out_i + 1 >= args.size():
		printerr("Expected --output <absolute evidence directory>")
		quit(2)
		return
	output_dir = args[out_i + 1]
	DirAccess.make_dir_recursive_absolute(output_dir)
	var catalog := ProjectSettings.globalize_path("res://../assets/release-v260/")
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(catalog.path_join("catalog.json")))
	for entry: Dictionary in manifest.definitions:
		var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(catalog.path_join(entry.file)))
		definitions[definition.id] = definition
	var page := (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await process_frame
	page._network_waiting = true
	page._status_own.set_status("己方", 0, {"life": 40, "mana": 20, "capacity": 20, "load": 8})
	page._status_opponent.set_status("对方", 1, {"life": 40, "mana": 20, "capacity": 20, "load": 8})
	var canvas := page.canvas
	# A display fixture has no engine action service. Observe the actual input signal.
	canvas.card_selected.disconnect(page._select_card)
	canvas.card_selected.connect(func(card: CardPresentation) -> void: picked = card.instance_id)
	var cases := {
		"five-body1": ["v260-formation-0004", "v260-formation-0009", "v260-formation-0004", "v260-formation-0009", "v260-formation-0004"],
		"mixed-2-2-1": ["v260-formation-0006", "v260-formation-0008", "v260-formation-0009"],
		"large-4-1": ["v260-formation-0017", "v260-formation-0009"]}
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080), Vector2i(2560, 1080)]:
		root.size = dimensions
		for scale_value: int in [100, 120]:
			Typography.set_scale(scale_value)
			await process_frame
			for label: String in cases:
				for count: int in [14, 20]:
					canvas.configure(_fixture(cases[label], count))
					for state: String in ["default", "hover", "selected", "collapsed"]:
						canvas.set_hand_collapsed(state == "collapsed")
						canvas._hover_id = "hand-7" if state == "hover" else ""
						canvas.selected_id = "hand-7" if state == "selected" else ""
						canvas.queue_redraw()
						await process_frame
						await RenderingServer.frame_post_draw
						if label == "mixed-2-2-1" and count == 14 and state in ["default", "collapsed"]:
							var filename := "battle-%dx%d-%d-%s.png" % [root.size.x, root.size.y, scale_value, state]
							root.get_texture().get_image().save_png(output_dir.path_join(filename))
						_measure(canvas, label, state)
	var output := FileAccess.open(output_dir.path_join("density.json"), FileAccess.WRITE)
	output.store_string(JSON.stringify(samples, "\t"))
	output.close()
	print("Battlefield fixtures: ", samples.size(), ", failed: ", failures)
	quit(1 if failures > 0 else 0)
