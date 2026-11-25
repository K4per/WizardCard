extends SceneTree

const OUT: String = "E:/Project/WizardCard/.cache/visual-audit-20261115"
const CATALOG: String = "E:/Project/WizardCard/assets/release-v260/"
var definitions: Dictionary = {}
var samples: Array[Dictionary] = []

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

func _measure(page: MatchPage, label: String) -> void:
	var canvas: FieldCanvas = page.canvas
	var record: Dictionary = {"scenario": label, "viewport": [root.size.x, root.size.y], "canvas": [canvas.size.x, canvas.size.y], "canvas_left": canvas.position.x, "analysis_width": canvas._own_region.size.x, "own": [], "opponent": [], "casting_visible": canvas._casting_page_size(canvas.field.own_casting.size(), canvas._own_casting_region)}
	for own: bool in [true, false]:
		var formations: Array[FormationPresentation] = canvas.field.own_formations if own else canvas.field.opponent_formations
		for formation: FormationPresentation in formations:
			var rect: Rect2 = canvas.card_bounds(formation.card)
			var ring: Rect2 = canvas.card_bounds(formation.occupants[0]) if formation.occupants.has(0) else Rect2()
			var ring_art: Rect2 = DeckCardPainter.artwork_rect(ring)
			var title_height: float = clampf(ring.size.y * 0.18, 20.0, 30.0) - 2.0
			var title_font: int = Typography.fitted_size(clampi(roundi(ring.size.x * 0.1), 10, 16), title_height, Typography.HEADING)
			(record["own" if own else "opponent"] as Array).append({"name": formation.card.title, "body": formation.body_size, "rings": formation.ring_count, "host_size": [rect.size.x, rect.size.y], "host_art_height": DeckCardPainter.artwork_rect(rect).size.y, "ring_size": [ring.size.x, ring.size.y], "ring_art_height": ring_art.size.y, "ring_title_font_px": title_font})
	var own_blocked: int = 0
	var opponent_blocked: int = 0
	for hit: Dictionary in canvas._ring_hits:
		var rect: Rect2 = hit["rect"]
		var center_card: CardPresentation = canvas.card_at(rect.get_center())
		if str(hit["host"]).begins_with("0-"):
			if center_card == null or center_card.instance_id.begins_with("hand-"):
				own_blocked += 1
		else:
			if center_card == null:
				opponent_blocked += 1
	record["own_ring_centers_blocked_by_hand"] = own_blocked
	record["opponent_ring_centers_blocked_by_hand"] = opponent_blocked
	samples.append(record)
	print("AUDIT ", JSON.stringify(record))

func _run() -> void:
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(CATALOG + "catalog.json"))
	for entry: Dictionary in manifest["definitions"]:
		var definition: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(CATALOG + str(entry["file"])))
		definitions[str(definition["id"])] = definition
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	await process_frame
	page._network_waiting = true
	page._status_own.set_status("我方", 0, {"life": 40, "mana": 20, "capacity": 20, "load": 8})
	page._status_opponent.set_status("对手", 1, {"life": 40, "mana": 20, "capacity": 20, "load": 8})
	var cases: Dictionary = {
		"five-body1": ["v260-formation-0004", "v260-formation-0009", "v260-formation-0004", "v260-formation-0009", "v260-formation-0004"],
		"mixed-2-2-1": ["v260-formation-0006", "v260-formation-0008", "v260-formation-0009"],
		"large-4-1": ["v260-formation-0017", "v260-formation-0009"]
	}
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1920, 1080), Vector2i(2560, 1080)]:
		root.size = dimensions
		for label: String in cases:
			page.canvas.configure(_fixture(cases[label], 14))
			await process_frame
			await process_frame
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(OUT.path_join("%s-%dx%d.png" % [label, root.size.x, root.size.y]))
			_measure(page, label)
			if dimensions == Vector2i(1280, 720) and label == "five-body1":
				page.canvas.set_hand_collapsed(true)
				await process_frame
				await RenderingServer.frame_post_draw
				root.get_texture().get_image().save_png(OUT.path_join("five-body1-hand-hidden-1280x720.png"))
				_measure(page, "five-body1-hand-hidden")
				page.canvas.set_hand_collapsed(false)
	var output: FileAccess = FileAccess.open(OUT.path_join("metrics.json"), FileAccess.WRITE)
	output.store_string(JSON.stringify(samples, "\t"))
	output.close()
	print("AUDIT COMPLETE: ", samples.size(), " samples; fixture data, current production renderer")
	quit()
