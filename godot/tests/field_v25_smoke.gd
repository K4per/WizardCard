extends SceneTree

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, label: String) -> void:
	if not condition:
		failures += 1
		push_error("FIELD_V25: " + label)

func _run() -> void:
	var host: Control = Control.new()
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	host.theme = ArtBank.make_theme()
	root.add_child(host)
	var canvas: FieldCanvas = (load("res://ui/field_canvas.tscn") as PackedScene).instantiate() as FieldCanvas
	host.add_child(canvas)
	var definitions: Array[CardPresentation] = []
	for index: int in range(8):
		var card: CardPresentation = CardPresentation.from_definition({"id":"fireball", "name":"火球术", "type":1, "cost":2, "rank":1, "illustrationId":"fireball", "text":"造成5点火焰伤害。"})
		card.instance_id = str(index + 10)
		definitions.append(card)
	var formation: CardPresentation = CardPresentation.from_definition({"id":"hexagram", "name":"均衡之六芒星", "type":3, "illustrationId":"balance", "rings":12, "body":1, "capacity":8, "income":2, "settingConditions":"支付 2 魔素", "recipe":[{"kind":0,"amount":2}]})
	formation.instance_id = "own-slot-0"
	var group: FormationPresentation = FormationPresentation.new()
	group.card = formation
	group.ring_count = 12
	group.occupants[0] = definitions[0]
	var field: FieldPresentation = FieldPresentation.new()
	field.own_formations.append(group)
	var opponent_group: FormationPresentation = FormationPresentation.new()
	opponent_group.card = formation.duplicate() as CardPresentation
	opponent_group.card.instance_id = "opponent-slot-0"
	opponent_group.ring_count = 12
	field.opponent_formations.append(opponent_group)
	for slot: int in range(1, 5):
		var own_slot: FormationPresentation = FormationPresentation.new()
		own_slot.card = formation.duplicate() as CardPresentation
		own_slot.card.instance_id = "own-slot-%d" % slot
		own_slot.ring_count = 12
		field.own_formations.append(own_slot)
		var opponent_slot: FormationPresentation = FormationPresentation.new()
		opponent_slot.card = formation.duplicate() as CardPresentation
		opponent_slot.card.instance_id = "opponent-slot-%d" % slot
		opponent_slot.ring_count = 12
		field.opponent_formations.append(opponent_slot)
	field.hand.assign(definitions)
	field.legal_cards["11"] = 2
	field.own_deck_count = 35
	field.opponent_deck_count = 35
	field.opponent_hand_count = 5
	var own_cast: CardPresentation = definitions[0].duplicate() as CardPresentation
	own_cast.instance_id = "own-casting"
	field.own_casting.append(own_cast)
	var opponent_cast: CardPresentation = definitions[1].duplicate() as CardPresentation
	opponent_cast.instance_id = "opponent-casting"
	field.opponent_casting.append(opponent_cast)
	canvas.configure(field)
	var capture_output: String = ""
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			capture_output = arguments[index + 1]
	for dimensions: Vector2i in [Vector2i(1280,720),Vector2i(1600,1000),Vector2i(1920,1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		_expect((canvas.size - canvas._own_region.end).is_equal_approx(canvas._opponent_region.position), "analysis symmetry " + str(dimensions))
		_expect((canvas.size - canvas._own_casting_region.end).is_equal_approx(canvas._opponent_casting_region.position), "casting symmetry " + str(dimensions))
		_expect(canvas._ring_hits.size() == 120, "five independent formation lanes contain the projected ring cells")
		for slot: int in range(1, 5):
			var previous: Rect2 = canvas.card_bounds(field.own_formations[slot - 1].card)
			var current: Rect2 = canvas.card_bounds(field.own_formations[slot].card)
			_expect(is_equal_approx(previous.position.x, current.position.x) and previous.end.y < current.position.y, "formation slots occupy five vertical rows")
		for hit: Dictionary in canvas._ring_hits:
			_expect(Rect2(Vector2.ZERO, canvas.size).encloses(hit["rect"]), "rings stay inside field")
		for hit: Dictionary in canvas._zone_hits:
			_expect(Rect2(Vector2.ZERO,canvas.size).encloses(hit["rect"]), "piles stay inside field")
		var own_cast_rect: Rect2 = canvas.card_bounds(own_cast)
		var opponent_cast_rect: Rect2 = canvas.card_bounds(opponent_cast)
		_expect(canvas._own_casting_region.encloses(own_cast_rect), "casting cards stay inside the compact own track")
		_expect(canvas._opponent_casting_region.encloses(opponent_cast_rect), "casting cards stay inside the compact opponent track")
		_expect(own_cast_rect.size.x >= 100.0 and own_cast_rect.size.y >= 24.0, "casting cards retain a readable horizontal label")
		_expect((canvas.size - own_cast_rect.end).is_equal_approx(opponent_cast_rect.position), "casting cards remain center symmetric")
		if not capture_output.is_empty():
			DirAccess.make_dir_recursive_absolute(capture_output)
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(capture_output.path_join("field-%dx%d.png" % [dimensions.x, dimensions.y]))
	var original_layout: FieldLayout = canvas.layout
	canvas.layout = original_layout.duplicate() as FieldLayout
	root.size = Vector2i(1280, 720)
	await process_frame
	await process_frame
	var previous_slot_width: float = canvas.card_bounds(field.own_formations[0].card).size.x
	canvas.layout.formation_slot_width_ratio = 0.08
	await process_frame
	await process_frame
	_expect(canvas.card_bounds(field.own_formations[0].card).size.x < previous_slot_width, "exported slot-width resource changes live geometry")
	var previous_pile: Rect2 = canvas.get_node("OwnMainDeck").get_rect()
	canvas.layout.main_deck_position = Vector2(0.88, 0.72)
	canvas.layout.pile_size = Vector2(0.06, 0.12)
	await process_frame
	await process_frame
	var updated_pile: Rect2 = canvas.get_node("OwnMainDeck").get_rect()
	_expect(not previous_pile.is_equal_approx(updated_pile), "exported pile geometry updates the actual scene controls")
	var previous_analysis: Rect2 = canvas._own_region
	canvas.layout.own_analysis = Vector2(0.57, 0.27)
	await process_frame
	await process_frame
	_expect(not previous_analysis.is_equal_approx(canvas._own_region), "exported region geometry changes the live battlefield")
	canvas.scene_region_overrides = true
	var own_region_node: Control = canvas.get_node("OwnAnalysis") as Control
	own_region_node.anchor_top = 0.58
	await process_frame
	await process_frame
	_expect(is_equal_approx(canvas._own_region.position.y, canvas.size.y * 0.58), "scene anchors remain an explicit editor override")
	canvas.scene_region_overrides = false
	canvas.layout = original_layout
	var match_page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	host.add_child(match_page)
	await process_frame
	var service: FrontendService = FrontendService.new()
	service.match_viewer = 0
	service.match_snapshot = {"players":[{"life":40,"mana":1,"load":0,"capacity":2},{"life":38,"mana":3,"load":1,"capacity":8}],"clock":{"enabled":true,"remainingMs":[65000,60000],"running":true,"runningPlayer":0}}
	match_page.service = service
	match_page._hover_card(definitions[0])
	_expect(match_page._detail.visible and match_page._detail.card.instance_id == definitions[0].instance_id, "hover shows full detail without click")
	_expect(not match_page._detail_shade.visible, "hover does not block card actions")
	match_page._hover_card(null)
	_expect(not match_page._detail.visible, "hover exit closes transient details")
	match_page._clock_received = Time.get_ticks_msec()
	match_page._update_status()
	_expect(match_page._status_own.text.contains("01:05"), "authoritative clock displayed")
	_expect(formation.metadata_text.contains("设置条件：支付 2 魔素"), "formation condition shown")
	for theme_id: String in ["arcane", "slate", "ember"]:
		canvas.set_background_theme(theme_id)
		_expect(canvas.background_theme == theme_id, "built-in background selectable")
	print("Field v2.5 smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	host.queue_free()
	await process_frame
	quit(0 if failures == 0 else 2)


