extends SceneTree

var failures: int = 0
var _ring_selection: Array = []
var _card_selection: String = ""

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, label: String) -> void:
	if not condition:
		failures += 1
		push_error("FIELD_V25: " + label)

func _record_ring(host: String, ring: int) -> void:
	_ring_selection = [host, ring]

func _record_card(card: CardPresentation) -> void:
	_card_selection = card.instance_id

func _click_at(point: Vector2) -> void:
	for pressed: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.position = point
		event.global_position = point
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		root.push_input(event, true)
	await process_frame

func _body_fixture(body: int, include_small: bool) -> FieldPresentation:
	var formation_id: String = "hexagram" if body == 2 else "sun"
	var ring_count: int = 3 if body == 2 else 7
	var rows: Array[Dictionary] = []
	for owner: int in range(2):
		var host: String = "body-%d-player-%d" % [body, owner]
		rows.append({"id": host, "owner": owner, "zone": 3, "fieldOrder": "1", "definition": {"id": formation_id, "illustrationId": "alpha-v252-" + formation_id, "name": "均衡之六芒星" if body == 2 else "至圣之太阳星", "type": 3, "body": body, "rings": ring_count}})
		rows.append({"id": host + "-spell", "owner": owner, "zone": 3, "host": host, "ringIndex": 0, "definition": {"id": "fireball", "name": "火球术", "type": 1, "cost": 3, "castCost": 1}})
		rows.append({"id": host + "-rune", "owner": owner, "zone": 7, "host": host, "definition": {"id": "ring", "name": "宿主符文", "type": 4}})
		rows.append({"id": host + "-spell-rune", "owner": owner, "zone": 7, "host": host + "-spell", "definition": {"id": "ring", "name": "法术符文", "type": 4}})
		if include_small:
			rows.append({"id": "small-player-%d" % owner, "owner": owner, "zone": 3, "fieldOrder": "2", "definition": {"id": "small-hexagram", "illustrationId": "alpha-v252-small-hexagram", "name": "运行之三角星", "type": 3, "body": 1, "rings": 1}})
	return MatchProjection.field({"viewer": 0, "players": [{}, {}], "cards": rows})

func _body_checks(canvas: FieldCanvas, body: int, include_small: bool, output: String) -> void:
	var fixture: FieldPresentation = _body_fixture(body, include_small)
	canvas.configure(fixture)
	await process_frame
	await process_frame
	var own: FormationPresentation = fixture.own_formations[0]
	var opponent: FormationPresentation = fixture.opponent_formations[0]
	_expect(own.body_size == body and opponent.body_size == body, "native body %d reaches both formation presentations" % body)
	var first_slot: Rect2 = canvas._formation_slot(canvas._own_region, 0, true)
	var last_slot: Rect2 = canvas._formation_slot(canvas._own_region, body - 1, true)
	var occupied: Rect2 = first_slot.merge(last_slot)
	var host: Rect2 = canvas.card_bounds(own.card)
	var opponent_host: Rect2 = canvas.card_bounds(opponent.card)
	_expect(is_equal_approx(host.size.y, occupied.size.y - 10.0) and is_equal_approx(host.position.y, occupied.position.y + 5.0), "body %d host fills exactly %d consecutive slots, including intervening gaps" % [body, body])
	_expect((canvas.size - host.end).is_equal_approx(opponent_host.position) and host.size.is_equal_approx(opponent_host.size), "body %d host bounds remain center symmetric" % body)
	_expect(canvas.card_at(host.get_center()) == own.card and canvas.card_at(opponent_host.get_center()) == opponent.card, "multi-slot hosts retain separate inspectable card targets")
	var used: int = body + (1 if include_small else 0)
	if include_small:
		var small: Rect2 = canvas.card_bounds(fixture.own_formations[1].card)
		var next_slot: Rect2 = canvas._formation_slot(canvas._own_region, body, true)
		_expect(is_equal_approx(small.position.y, next_slot.position.y + 5.0) and host.end.y < small.position.y, "the next formation begins after all body %d slots" % body)
	for slot: int in range(used, 5):
		var empty: Rect2 = canvas._formation_slot(canvas._own_region, slot, true)
		var opponent_empty: Rect2 = canvas._formation_slot(canvas._opponent_region, slot, false)
		_expect((canvas.size - empty.end).is_equal_approx(opponent_empty.position), "unused body slot remains center symmetric")
		for hit: Dictionary in canvas._card_hits:
			_expect(not empty.intersects(hit["rect"]) and not opponent_empty.intersects(hit["rect"]), "unused body slot has no phantom host, spell or attachment")
	var expected_rings: int = own.ring_count + (1 if include_small else 0)
	_expect(canvas._ring_hits.size() == expected_rings * 2, "body-span layout renders only the actual projected ring count")
	var ring_rows: Dictionary[float, bool] = {}
	var target_hit: Dictionary = {}
	for hit: Dictionary in canvas._ring_hits:
		var rect: Rect2 = hit["rect"]
		if str(hit["host"]) != own.card.instance_id:
			continue
		ring_rows[rect.position.y] = true
		_expect(occupied.encloses(rect), "every body %d ring remains inside its own formation span" % body)
		for mirrored_hit: Dictionary in canvas._ring_hits:
			if str(mirrored_hit["host"]) == opponent.card.instance_id and int(mirrored_hit["ring"]) == int(hit["ring"]):
				_expect((canvas.size - rect.end).is_equal_approx((mirrored_hit["rect"] as Rect2).position), "ring index remains attached to the same center-mirrored host")
		if int(hit["ring"]) == own.ring_count - 1:
			target_hit = hit
	_expect(ring_rows.size() == body, "body %d rings use the full occupied vertical span" % body)
	for owner_group: FormationPresentation in [own, opponent]:
		for host_id: String in [owner_group.card.instance_id, owner_group.occupants[0].instance_id]:
			var attached: CardPresentation = fixture.attachments[host_id][0] as CardPresentation
			var attached_rect: Rect2 = canvas.card_bounds(attached)
			var attached_host: CardPresentation = owner_group.card if host_id == owner_group.card.instance_id else owner_group.occupants[0]
			_expect(canvas.card_bounds(attached_host).encloses(attached_rect) and canvas.card_at(attached_rect.get_center()) == attached, "body-span attachment stays on its own host and retains selection priority")
	for suffix: String in ["-rune", "-spell-rune"]:
		var own_rune: CardPresentation = fixture.attachments[own.card.instance_id if suffix == "-rune" else own.occupants[0].instance_id][0] as CardPresentation
		var opponent_rune: CardPresentation = fixture.attachments[opponent.card.instance_id if suffix == "-rune" else opponent.occupants[0].instance_id][0] as CardPresentation
		var own_rune_rect: Rect2 = canvas.card_bounds(own_rune)
		var opponent_rune_rect: Rect2 = canvas.card_bounds(opponent_rune)
		_expect((canvas.size - own_rune_rect.end).is_equal_approx(opponent_rune_rect.position), "host and spell attachment boxes remain center symmetric")
	var targets: Dictionary[String, PackedInt32Array] = {}
	targets[own.card.instance_id] = PackedInt32Array([own.ring_count - 1])
	canvas.set_action_targets([], targets)
	_ring_selection.clear()
	await _click_at(canvas.global_position + (target_hit["rect"] as Rect2).get_center())
	_expect(_ring_selection == [own.card.instance_id, own.ring_count - 1], "real mouse click selects the exact legal ring in a body %d span" % body)
	canvas.clear_action_targets()
	_card_selection = ""
	await _click_at(canvas.global_position + host.get_center())
	_expect(_card_selection == own.card.instance_id, "real mouse click selects the body %d formation host" % body)
	var clickable_rune: CardPresentation = fixture.attachments[own.card.instance_id][0] as CardPresentation
	_card_selection = ""
	await _click_at(canvas.global_position + canvas.card_bounds(clickable_rune).get_center())
	_expect(_card_selection == clickable_rune.instance_id, "real mouse click selects the attachment instead of its larger body-span host")
	if not output.is_empty():
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png(output.path_join("field-body-%d-%dx%d.png" % [body, root.size.x, root.size.y]))

func _run() -> void:
	var dynamic: CardPresentation = CardPresentation.from_projection({"id": "99", "definition": {"id": "sunburst", "illustrationId": "v2-sunburst", "name": "阳炎爆", "type": 1, "cost": 20, "castCost": 2}, "effectiveSettingCost": 17, "effectiveCastCost": 2})
	_expect(dynamic.primary_value == "17/2", "analytic face uses live setting and casting costs")
	_expect(dynamic.metadata_text.contains("设置费用：17　施法费用：2"), "details use live costs")
	var storm: CardPresentation = CardPresentation.from_projection({"id": "42", "stormPoints": 6, "definition": {"id": "storm-crest", "name": "风暴纹章", "type": 3, "tags": ["风暴"], "body": 1, "rings": 1, "capacity": 5, "income": 1}})
	_expect(storm.detail_text().contains("风暴点数：6"), "visible storm card details show its projected point count")
	var wind: CardPresentation = CardPresentation.from_projection({"id": "44", "stormPoints": 0, "definition": {"id": "storm-crest", "name": "风契·风暴纹章", "type": 3, "tags": ["风契"], "body": 2, "rings": 1, "capacity": 5, "income": 1}})
	_expect(wind.wind_contract and wind.token_label() == "风契指示物：0" and wind.detail_text().contains("风契指示物：0"), "wind-contract formations expose an explicit zero token count")
	var blood_snapshot: Dictionary = {"cards": [
		{"id": "spell", "owner": 0, "zone": 5, "definition": {"id": "scorching-ray", "type": 1, "effects": [{"kind": "ray_volley"}]}},
		{"id": "rune", "owner": 0, "zone": 7, "host": "spell", "definition": {"id": "blood-heartmark", "type": 4, "traits": ["blood_heartmark"]}}
	]}
	var blood_action: Dictionary = {"source": "spell", "command": {"type": 17, "card": "spell"}}
	_expect(MatchProjection.blood_heartmark_hint(blood_snapshot, blood_action).contains("支付 2 点生命值"), "blood-heartmark hint comes from the projected legal spell action")
	var hidden_storm: CardPresentation = CardPresentation.from_projection({"id": "43", "hidden": true, "stormPoints": 99, "definition": {"id": "storm-crest", "tags": ["风暴"]}})
	_expect(not hidden_storm.detail_text().contains("风暴点数"), "hidden storm card details do not reveal point counts")
	var talisman: CardPresentation = CardPresentation.from_definition({"type": 5, "cost": 5, "castCost": 0})
	_expect(talisman.primary_value == "5" and talisman.metadata_text.contains("施法费用：5（盖伏免费）") and not talisman.metadata_text.contains("设置费用"), "talisman has only a casting cost")
	var dynamic_talisman: CardPresentation = CardPresentation.from_definition({"type": 5, "cost": 5, "dynamicCost": true})
	_expect(dynamic_talisman.primary_value == "X" and dynamic_talisman.metadata_text.contains("施法费用：X（盖伏免费）"), "dynamic talisman cost is shown as X")
	var preview_dynamic_talisman: CardPresentation = CardPresentation.from_preview_definition({"id": "v2-counter-spell", "name": "法术反制", "typeKey": "talisman", "typeLabel": "咒符法术", "level": 5, "school": "防护", "costText": "X", "costParts": [5], "analysisTurns": 0, "speed": 4, "text": "", "illustrationId": "counter-spell", "summonCondition": ""})
	_expect(preview_dynamic_talisman.primary_value == "X" and preview_dynamic_talisman.metadata_text.contains("费用：X"), "preview talisman cost is shown as X")
	var covered: CardPresentation = CardPresentation.from_projection({"id": "98", "faceDown": true, "definition": {"type": 1, "name": "火球术", "cost": 3}})
	_expect(covered.face_down and not covered.hidden and covered.title == "火球术", "own facedown card retains inspectable identity with covered face")
	var highlight_projection: FieldPresentation = MatchProjection.field({"viewer": 0, "players": [{}, {}], "cards": [], "actions": [
		{"source": "set", "command": {"type": 2}},
		{"source": "attach", "command": {"type": 6}},
		{"source": "cast", "command": {"type": 17}},
		{"source": "covered", "command": {"type": 15}},
		{"source": "both", "command": {"type": 5}},
		{"source": "both", "command": {"type": 15}},
		{"source": "remove", "command": {"type": 1}},
		{"source": "formation", "command": {"type": 0}}
	]})
	_expect(highlight_projection.legal_cards.get("set", 0) == FieldPresentation.LEGAL_GOLD and highlight_projection.legal_cards.get("attach", 0) == FieldPresentation.LEGAL_GOLD and highlight_projection.legal_cards.get("cast", 0) == FieldPresentation.LEGAL_GOLD, "native setting, attachment and activation operations receive gold")
	_expect(highlight_projection.legal_cards.get("covered", 0) == FieldPresentation.LEGAL_AMBUSH, "only native cover operation receives bright blue")
	_expect(highlight_projection.legal_cards.get("both", 0) == (FieldPresentation.LEGAL_GOLD | FieldPresentation.LEGAL_AMBUSH), "a card preserves simultaneous setting and cover availability")
	_expect(not highlight_projection.legal_cards.has("remove") and highlight_projection.can_set_formation, "removal does not imply setting and legal formation setting highlights the side deck")
	var attachment_projection: FieldPresentation = MatchProjection.field({"viewer": 0, "players": [{}, {}], "cards": [
		{"id": "host", "owner": 0, "zone": 3, "definition": {"name": "阵法", "type": 3, "rings": 1}},
		{"id": "rune", "owner": 0, "zone": 7, "host": "host", "definition": {"name": "符文", "type": 4}}
	]})
	_expect(attachment_projection.attachments.has("host") and (attachment_projection.attachments["host"][0] as CardPresentation).instance_id == "rune" and attachment_projection.own_casting.is_empty(), "projected attachment belongs to its host rather than a spell ring or casting area")
	var host: Control = Control.new()
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	host.theme = ArtBank.make_theme()
	root.add_child(host)
	var fixture_notice: Label = Label.new()
	fixture_notice.text = "布局压力测试 · 五张阵体1合成卡，槽位占用合法"
	fixture_notice.position = Vector2(12, 12)
	fixture_notice.mouse_filter = Control.MOUSE_FILTER_IGNORE
	fixture_notice.z_index = 10
	host.add_child(fixture_notice)
	var canvas: FieldCanvas = (load("res://ui/field_canvas.tscn") as PackedScene).instantiate() as FieldCanvas
	host.add_child(canvas)
	canvas.ring_selected.connect(_record_ring)
	canvas.card_selected.connect(_record_card)
	var definitions: Array[CardPresentation] = []
	for index: int in range(8):
		var card: CardPresentation = CardPresentation.from_definition({"id":"fireball", "name":"火球术", "type":1, "cost":3, "castCost":1, "rank":3, "illustrationId":"fireball", "text":"对对方玩家造成10点火焰伤害。"})
		card.instance_id = str(index + 10)
		definitions.append(card)
	dynamic.instance_id = "16"
	talisman.instance_id = "17"
	definitions[6] = dynamic
	definitions[7] = talisman
	var formation: CardPresentation = CardPresentation.from_definition({"id":"synthetic-body-one", "name":"阵体1压力卡", "type":3, "illustrationId":"balance", "rings":12, "body":1, "capacity":8, "income":2, "settingConditions":"支付 2 魔素", "recipe":[{"kind":0,"amount":2}]})
	formation.instance_id = "own-slot-0"
	var group: FormationPresentation = FormationPresentation.new()
	group.card = formation
	group.body_size = 1
	group.ring_count = 12
	group.occupants[0] = definitions[0]
	group.occupants[1] = covered
	var field: FieldPresentation = FieldPresentation.new()
	field.own_formations.append(group)
	var opponent_group: FormationPresentation = FormationPresentation.new()
	opponent_group.card = formation.duplicate() as CardPresentation
	opponent_group.card.instance_id = "opponent-slot-0"
	opponent_group.body_size = 1
	opponent_group.ring_count = 12
	field.opponent_formations.append(opponent_group)
	for slot: int in range(1, 5):
		var own_slot: FormationPresentation = FormationPresentation.new()
		own_slot.card = formation.duplicate() as CardPresentation
		own_slot.card.instance_id = "own-slot-%d" % slot
		own_slot.body_size = 1
		own_slot.ring_count = 12
		field.own_formations.append(own_slot)
		var opponent_slot: FormationPresentation = FormationPresentation.new()
		opponent_slot.card = formation.duplicate() as CardPresentation
		opponent_slot.card.instance_id = "opponent-slot-%d" % slot
		opponent_slot.body_size = 1
		opponent_slot.ring_count = 12
		field.opponent_formations.append(opponent_slot)
	field.hand.assign(definitions)
	field.legal_cards["11"] = FieldPresentation.LEGAL_GOLD
	field.legal_cards["12"] = FieldPresentation.LEGAL_AMBUSH
	field.legal_cards["13"] = FieldPresentation.LEGAL_GOLD | FieldPresentation.LEGAL_AMBUSH
	var rune: CardPresentation = attachment_projection.attachments["host"][0] as CardPresentation
	field.attachments[formation.instance_id] = [rune]
	for index: int in range(1, 9):
		var extra_rune: CardPresentation = rune.duplicate() as CardPresentation
		extra_rune.instance_id = "rune-%d" % index
		field.attachments[formation.instance_id].append(extra_rune)
	field.can_set_formation = true
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
	# The headless renderer does not emit frame_post_draw; it can perform every
	# geometry/input assertion but must not await a screenshot callback.
	if DisplayServer.get_name() == "headless":
		capture_output = ""
	for dimensions: Vector2i in [Vector2i(1280,720),Vector2i(1600,1000),Vector2i(1920,1080)]:
		root.size = dimensions
		await process_frame
		await process_frame
		_expect((canvas.size - canvas._own_region.end).is_equal_approx(canvas._opponent_region.position), "analysis symmetry " + str(dimensions))
		_expect((canvas.size - canvas._own_casting_region.end).is_equal_approx(canvas._opponent_casting_region.position), "casting symmetry " + str(dimensions))
		_expect(canvas._ring_hits.size() == 120, "five legal body-one pressure slots contain the projected ring cells")
		for slot: int in range(1, 5):
			var previous: Rect2 = canvas.card_bounds(field.own_formations[slot - 1].card)
			var current: Rect2 = canvas.card_bounds(field.own_formations[slot].card)
			_expect(is_equal_approx(previous.position.x, current.position.x) and previous.end.y < current.position.y, "five body-one formations occupy five consecutive slots")
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
		var attachment_bounds: Rect2 = canvas.card_bounds(rune)
		_expect(attachment_bounds.has_area() and canvas.card_at(attachment_bounds.get_center()) == rune, "attached rune has a visible, selectable box on its host")
		var rune_boxes: Array[Rect2] = []
		for attached: CardPresentation in field.attachments[formation.instance_id]:
			var box: Rect2 = canvas.card_bounds(attached)
			_expect(box.has_area() and canvas.card_bounds(formation).encloses(box) and canvas.card_at(box.get_center()) == attached, "every stacked rune retains its own bounded selectable box")
			for previous_box: Rect2 in rune_boxes:
				_expect(not previous_box.intersects(box), "stacked rune boxes never overlap")
			rune_boxes.append(box)
		var hand_extent: Vector2 = canvas._hand_card_size(canvas.size.y * canvas.layout.hand_height_ratio)
		var hand_stride: float = canvas._hand_stride(hand_extent.x, field.hand.size())
		var hand_start: float = (canvas.size.x - (field.hand.size() - 1) * hand_stride) * 0.5
		var left_pose: Transform2D = canvas._hand_pose(0, field.hand.size(), hand_start, hand_stride, hand_extent, false)
		var right_pose: Transform2D = canvas._hand_pose(field.hand.size() - 1, field.hand.size(), hand_start, hand_stride, hand_extent, false)
		_expect(is_equal_approx(left_pose.origin.y, right_pose.origin.y) and is_equal_approx(left_pose.get_rotation(), -right_pose.get_rotation()), "left and right fan endpoints stay level and symmetric")
		_expect(canvas._opponent_hand_hits.size() == field.opponent_hand_count, "opponent fan uses the full public hand count")
		for silhouette: Dictionary in canvas._opponent_hand_hits:
			_expect((silhouette["local_rect"] as Rect2).size.is_equal_approx(hand_extent), "opponent hand uses the same card size as the own fan")
			_expect(canvas.card_at((silhouette["rect"] as Rect2).get_center()) == null, "opponent silhouette never exposes a card identity or selects the ring behind it")
		var covered_point: Vector2 = (canvas._opponent_hand_hits[0]["rect"] as Rect2).get_center()
		var covered_ring: Dictionary = {"rect": Rect2(covered_point - Vector2.ONE * 4.0, Vector2.ONE * 8.0), "host": "silhouette-covered-ring", "ring": 0}
		canvas._ring_hits.append(covered_ring)
		var covered_targets: Dictionary[String, PackedInt32Array] = {"silhouette-covered-ring": PackedInt32Array([0])}
		canvas.set_action_targets([], covered_targets)
		_ring_selection.clear()
		await _click_at(canvas.global_position + covered_point)
		_expect(_ring_selection.is_empty(), "real mouse input cannot select a legal ring through the opponent hand silhouette")
		canvas.clear_action_targets()
		await process_frame
		await process_frame
		var hand_card: CardPresentation = definitions[4]
		var rest_bounds: Rect2 = canvas.card_bounds(hand_card)
		var rest_pointer: Vector2 = rest_bounds.get_center()
		canvas._hover_id = hand_card.instance_id
		canvas.queue_redraw()
		await process_frame
		await process_frame
		var lifted_bounds: Rect2 = canvas.card_bounds(hand_card)
		_expect(lifted_bounds.position.y < rest_bounds.position.y - hand_extent.y * 0.40, "hover visibly lifts a hand card")
		_expect(canvas.card_at(rest_pointer) == hand_card, "hover remains stable over the original footprint after lifting")
		if not capture_output.is_empty():
			DirAccess.make_dir_recursive_absolute(capture_output)
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(capture_output.path_join("field-hover-%dx%d.png" % [dimensions.x, dimensions.y]))
		canvas.selected_id = hand_card.instance_id
		canvas._clear_hover()
		await process_frame
		await process_frame
		_expect(canvas.card_bounds(hand_card).is_equal_approx(rest_bounds), "pointer exit restores the fan even when the card remains selected")
		canvas.selected_id = ""
		if not capture_output.is_empty():
			DirAccess.make_dir_recursive_absolute(capture_output)
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(capture_output.path_join("field-%dx%d.png" % [dimensions.x, dimensions.y]))
		fixture_notice.text = "阵体2 · 占两槽；后续阵体1占一槽；剩余两槽空"
		await _body_checks(canvas, 2, true, capture_output)
		fixture_notice.text = "阵体4 · 占四槽；剩余一槽空"
		await _body_checks(canvas, 4, false, capture_output)
		fixture_notice.text = "布局压力测试 · 五张阵体1合成卡，槽位占用合法"
		canvas.configure(field)
		await process_frame
		await process_frame
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
	_expect(match_page._status_own._name.text.contains("01:05"), "authoritative clock displayed")
	_expect(formation.metadata_text.contains("设置条件：支付 2 魔素"), "formation condition shown")
	for theme_id: String in ["arcane", "slate", "ember"]:
		canvas.set_background_theme(theme_id)
		_expect(canvas.background_theme == theme_id, "built-in background selectable")
	print("Field v2.5 smoke: %s" % ("PASS" if failures == 0 else "FAIL"))
	host.queue_free()
	await process_frame
	quit(0 if failures == 0 else 2)
