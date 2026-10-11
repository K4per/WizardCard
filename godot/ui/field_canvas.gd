class_name FieldCanvas
extends Control

signal card_selected(card: CardPresentation)
signal selection_cleared()
signal hand_toggle_requested()
signal placement_requested(card_id: String, host_id: String, ring_index: int)
signal zone_clicked(kind: String, owner: int)
signal layout_changed()

var field: FieldPresentation = FieldPresentation.new()
var privacy: bool = false
var hand_collapsed: bool = false
var _card_hits: Array[Dictionary] = []
var _ring_hits: Array[Dictionary] = []
var _zone_hits: Array[Dictionary] = []
var _hover_id: String = ""
var _keyboard_index: int = -1
var _own_start: int = 0
var _opponent_start: int = 0
var _press_position: Vector2
var _dragged: CardPresentation
var _drag_active: bool = false
var _pointer: Vector2
var _own_region: Rect2
var _opponent_region: Rect2
var _own_casting_region: Rect2
var _opponent_casting_region: Rect2
var selected_id: String = ""

func card_bounds(card: CardPresentation) -> Rect2:
	for hit: Dictionary in _card_hits:
		if hit["card"] == card:
			return hit["rect"]
	return Rect2()

func _ready() -> void:
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	resized.connect(queue_redraw)
	mouse_exited.connect(_clear_hover)

func configure(value: FieldPresentation) -> void:
	field = value
	hand_collapsed = false
	_own_start = 0
	_opponent_start = 0
	_clear_hover()

func set_hand_collapsed(enabled: bool) -> void:
	hand_collapsed = enabled
	_dragged = null
	_drag_active = false
	_keyboard_index = -1
	selection_cleared.emit()
	_clear_hover()

func set_privacy(enabled: bool) -> void:
	privacy = enabled
	selected_id = ""
	_dragged = null
	_drag_active = false
	_card_hits.clear()
	_ring_hits.clear()
	_zone_hits.clear()
	_keyboard_index = -1
	tooltip_text = ""
	queue_redraw()

func _clear_hover() -> void:
	_hover_id = ""
	tooltip_text = ""
	queue_redraw()

func _draw() -> void:
	_card_hits.clear()
	_ring_hits.clear()
	_zone_hits.clear()
	_draw_cover(BattleArt.texture("dungeon_background"), Rect2(Vector2.ZERO, size), Color.WHITE)
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.031, 0.075, 0.122, 0.18))
	if privacy:
		_draw_label("交接设备后，再揭开场地", Rect2(Vector2(0, size.y * 0.5 - 24), Vector2(size.x, 48)), 26, CardPainter.PAPER, true)
		return
	# Keep deck labels clear of the first formation even when the live match
	# shares the viewport with its action panel.
	var left: float = maxf(size.x * 0.12, minf(210.0, size.x * 0.25))
	var width: float = size.x - left - maxf(30.0, size.x * 0.08)
	_opponent_region = Rect2(left, size.y * 0.105, width, size.y * 0.26)
	_own_region = Rect2(left, size.y * 0.575, width, size.y * 0.415)
	_opponent_casting_region = Rect2(left, size.y * 0.375, width, size.y * 0.09)
	_own_casting_region = Rect2(left, size.y * 0.475, width, size.y * 0.09)
	_draw_formations(field.opponent_formations, _opponent_region, _opponent_start, false)
	_draw_formations(field.own_formations, _own_region, _own_start, true)
	_draw_casting(field.opponent_casting, field.opponent_casting_slots, _opponent_casting_region, false)
	_draw_casting(field.own_casting, field.own_casting_slots, _own_casting_region, true)
	_draw_decks(size.x * 0.015, size.y * 0.14, field.opponent_side_count, field.opponent_deck_count, 1)
	_draw_decks(size.x * 0.015, size.y * 0.54, field.own_side_count, field.own_deck_count, 0)
	_draw_ash(size.x * 0.92, size.y * 0.23, field.opponent_ash_count, 1)
	_draw_ash(size.x * 0.92, size.y * 0.66, field.own_ash_count, 0)
	_draw_hud(Rect2(size.x * 0.76, 12, size.x * 0.23, 96), false)
	_draw_hud(Rect2(18, size.y - 110, size.x * 0.23, 96), true)
	var phase: Rect2 = Rect2(size.x * 0.41, 4, size.x * 0.21, 48)
	draw_texture_rect(BattleArt.texture("phase_banner"), phase, false)
	_draw_label(field.phase_label, phase.grow(-6), 24, CardPainter.PAPER, true)
	_draw_hand(size.y * 0.22)
	if _drag_active and _dragged != null:
		CardPainter.paint(self, _dragged, Rect2(_pointer - Vector2(40, 55), Vector2(80, 110)), "field", true)
	layout_changed.emit()

func _draw_cover(texture: Texture2D, rect: Rect2, modulate: Color) -> void:
	if texture == null:
		return
	var texture_size: Vector2 = texture.get_size()
	var scale_factor: float = maxf(rect.size.x / texture_size.x, rect.size.y / texture_size.y)
	var crop_size: Vector2 = rect.size / scale_factor
	var crop: Rect2 = Rect2((texture_size - crop_size) * 0.5, crop_size)
	draw_texture_rect_region(texture, rect, crop, modulate)

func _draw_label(text: String, rect: Rect2, font_size: int, color: Color = CardPainter.PAPER, centered: bool = false) -> void:
	var font: Font = Typography.HEADING
	var displayed: String = CardPainter.shortened(text, rect.size.x, font_size, font)
	var width: float = font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	var x: float = (rect.size.x - width) * 0.5 if centered else 0.0
	var baseline: Vector2 = rect.position + Vector2(x, (rect.size.y - font.get_height(font_size)) * 0.5 + font.get_ascent(font_size))
	draw_string_outline(font, baseline, displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, 3, Color("06121e"))
	draw_string(font, baseline, displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)

func _analysis_content(region: Rect2) -> Rect2:
	# The texture has an inset floor border; its outer rectangle is not playable space.
	var inset: Vector2 = region.size * Vector2(0.065, 0.12)
	return Rect2(region.position + inset + Vector2(0, 40), region.size - inset * 2 - Vector2(0, 40))

func _draw_formations(formations: Array[FormationPresentation], region: Rect2, start: int, own: bool) -> void:
	# One floor boundary per side; invisible lanes preserve formation/ring positions.
	draw_texture_rect(BattleArt.texture("analysis_floor"), region, false, Color(1, 1, 1, 0.58))
	var inset: Vector2 = region.size * Vector2(0.065, 0.12)
	var header: Rect2 = Rect2(region.position + inset, Vector2(region.size.x - inset.x * 2, 32))
	_draw_label(("我方" if own else "对手") + " · 解析区", header, 22, Color("f1dfb3"))
	var capacity: int = _visible_groups(region)
	var shown: int = mini(capacity, maxi(0, formations.size() - start))
	var content: Rect2 = _analysis_content(region)
	var stride: float = _tile_side() + 16
	for offset: int in range(shown):
		var cluster: Rect2 = Rect2(content.position + Vector2(0, offset * stride), Vector2(content.size.x, _tile_side()))
		_draw_formation_cluster(formations[start + offset], cluster)
	if formations.size() > capacity:
		var counter_width: float = minf(360, header.size.x * 0.5)
		_draw_label("%d–%d / %d 阵法 · 滚轮浏览" % [start + 1, start + shown, formations.size()], Rect2(Vector2(header.end.x - counter_width, header.position.y), Vector2(counter_width, 32)), 16, Color("d9e4da"))

func _draw_formation_cluster(formation: FormationPresentation, region: Rect2) -> void:
	var count: int = maxi(0, formation.ring_count)
	var host_side: float = _tile_side()
	var gap: float = host_side * 0.30
	var host_rect: Rect2 = Rect2(region.position, Vector2.ONE * host_side)
	var grid_origin: Vector2 = Vector2(host_rect.end.x + gap, host_rect.position.y)
	var available: float = maxf(1, region.end.x - grid_origin.x)
	# Every ring comes from the projection, including currently empty rings.
	var stride: float = minf(host_side + gap, available / maxi(1, count))
	var side: float = minf(host_side, stride * host_side / (host_side + gap))
	for ring: int in range(count):
		var ring_rect: Rect2 = Rect2(grid_origin + Vector2(ring * stride, (host_side - side) * 0.5), Vector2.ONE * side)
		_ring_hits.append({"rect": ring_rect, "host": formation.card.instance_id, "ring": ring})
		if formation.occupants.has(ring) or _drag_active:
			draw_line(host_rect.get_center(), ring_rect.get_center(), Color(0.36, 0.65, 0.63, 0.19), 1)
		if formation.occupants.has(ring):
			_draw_card(formation.occupants[ring], ring_rect)
		elif _drag_active:
			draw_rect(ring_rect, Color(0.28, 0.65, 0.67, 0.4), false, 1)
	_draw_card(formation.card, host_rect)

func _draw_card(card: CardPresentation, rect: Rect2, view: String = "field") -> void:
	_card_hits.append({"rect": rect, "card": card})
	if view == "field":
		CardPainter.paint_art(self, card, rect, card.instance_id == _hover_id or card.instance_id == selected_id)
	else:
		CardPainter.paint(self, card, rect, view, card.instance_id == _hover_id or card.instance_id == selected_id)

func _draw_casting(cards: Array[CardPresentation], _slots: int, region: Rect2, own: bool) -> void:
	draw_texture_rect(BattleArt.texture("casting_floor"), region, false, Color(1, 1, 1, 0.55))
	if cards.is_empty():
		_draw_label("施法区（%s）" % ("我方" if own else "对手"), region.grow(-8), 22, Color("f1dfb3"), true)
	var side: float = minf(_tile_side(), minf(region.size.y - 16, (region.size.x - 80) / maxi(cards.size(), 1) - 8))
	for index: int in range(cards.size()):
		_draw_card(cards[index], Rect2(region.position + Vector2(32 + index * (side + 16), (region.size.y - side) * 0.5), Vector2.ONE * side))

func _draw_decks(x: float, y: float, side: int, main: int, owner: int) -> void:
	# Side deck above main deck; only remaining counts are displayed.
	for index: int in range(2):
		var extent: float = minf(92, size.y * 0.10)
		var point: Vector2 = Vector2(x, y + index * (extent + 18))
		var rect: Rect2 = Rect2(point, Vector2.ONE * extent)
		_zone_hits.append({"kind": "side_deck" if index == 0 else "main_deck", "owner": owner, "rect": rect})
		draw_texture_rect(BattleArt.texture("side_deck_plate" if index == 0 else "main_deck_plate"), rect, false)
		draw_texture_rect(ArtBank.texture("c2d.card.back.ai"), CardPainter.fitted_rect(Rect2(point + Vector2(12, 9), Vector2(extent - 34, extent - 22))), false)
		var label: String = ("副卡组 %d" % side) if index == 0 else ("牌库 %d" % main)
		_draw_label(label, Rect2(point + Vector2(extent + 8, extent * 0.5 - 16), Vector2(128, 32)), 18, Color("d9e4da"))

func _draw_ash(x: float, y: float, count: int, owner: int) -> void:
	var extent: float = minf(76, size.y * 0.09)
	var rect: Rect2 = Rect2(x, y, extent, extent)
	_zone_hits.append({"kind": "ash", "owner": owner, "rect": rect})
	draw_texture_rect(BattleArt.texture("ash_plate"), rect, false)
	_draw_label("灰烬区 %d" % count, Rect2(x - 12, y + extent + 4, minf(112, size.x - x + 4), 32), 18, CardPainter.PAPER, true)

func _draw_hud(rect: Rect2, own: bool) -> void:
	draw_texture_rect(BattleArt.texture("player_hud"), rect, false)
	var values: Array[String] = [str(field.own_life if own else field.opponent_life), str(field.own_mana if own else field.opponent_mana), "%d/%d" % [field.own_load if own else field.opponent_load, field.own_load_limit if own else field.opponent_load_limit]]
	var ids: Array[String] = ["resource_life", "resource_mana", "resource_load"]
	var cell_width: float = (rect.size.x - 32) / 3
	var icon_side: float = clampf(cell_width * 0.34, 32, 46)
	for index: int in range(3):
		var origin: Vector2 = rect.position + Vector2(16 + index * cell_width, 12)
		var icon: Texture2D = BattleArt.texture(ids[index])
		draw_texture_rect(icon, BattleArt.fitted(Rect2(origin + Vector2(0, (48 - icon_side) * 0.5), Vector2.ONE * icon_side), icon), false)
		var value_rect: Rect2 = Rect2(origin + Vector2(icon_side + 6, 0), Vector2(cell_width - icon_side - 8, 48))
		var value_size: int = clampi(roundi(cell_width * 0.20), 20, 26)
		while value_size > 12 and Typography.HEADING.get_string_size(values[index], HORIZONTAL_ALIGNMENT_LEFT, -1, value_size).x > value_rect.size.x:
			value_size -= 1
		_draw_label(values[index], value_rect, value_size)
	var track: Rect2 = Rect2(rect.position + Vector2(20, 74), Vector2(rect.size.x - 40, 10))
	draw_texture_rect(BattleArt.texture("load_track"), track, false)
	var limit: int = field.own_load_limit if own else field.opponent_load_limit
	var load_value: int = field.own_load if own else field.opponent_load
	var fraction: float = clampf(float(load_value) / limit, 0, 1) if limit > 0 else (1.0 if load_value > 0 else 0.0)
	if fraction > 0:
		var fill: Texture2D = BattleArt.texture("load_fill_warning" if load_value > limit else "load_fill_normal")
		draw_texture_rect_region(fill, Rect2(track.position + Vector2(5, 2), Vector2((track.size.x - 10) * fraction, 6)), Rect2(Vector2.ZERO, Vector2(fill.get_width() * fraction, fill.get_height())))

func _visible_groups(region: Rect2) -> int:
	return maxi(1, floori((_analysis_content(region).size.y + 16) / (_tile_side() + 16)))

func _tile_side() -> float:
	return minf(80, size.y * 0.086)

func _draw_hand(height: float) -> void:
	var card_height: float = minf(224, height)
	var width: float = card_height * 160.0 / 224.0
	var count: int = field.hand.size()
	if count == 0 or hand_collapsed:
		return
	var stride: float = minf(width * 0.66, (size.x * 0.70 - width) / maxi(1, count - 1))
	var total: float = width + (count - 1) * stride
	var start_x: float = (size.x - total + width) * 0.5
	var selected_index: int = -1
	for index: int in range(count):
		if field.hand[index].instance_id == selected_id:
			selected_index = index
		else:
			_draw_hand_card(index, start_x, stride, Vector2(width, card_height), false)
	# A raised card is painted last so neighbours cannot cover its face or hit area.
	if selected_index >= 0:
		_draw_hand_card(selected_index, start_x, stride, Vector2(width, card_height), true)

func _draw_hand_card(index: int, start_x: float, stride: float, extent: Vector2, raised: bool) -> void:
	var card: CardPresentation = field.hand[index]
	var fraction: float = (2.0 * index / (field.hand.size() - 1) - 1.0) if field.hand.size() > 1 else 0.0
	var angle: float = deg_to_rad(minf(22, (field.hand.size() - 1) * 4.5) * fraction)
	var arch: float = 0.21 if field.hand.size() > 1 else 0.0
	var center: Vector2 = Vector2(start_x + index * stride, size.y - extent.y * (0.5 + arch) - 12 + fraction * fraction * extent.y * 0.16)
	if raised:
		center += Vector2(0, -extent.y * 0.34).rotated(angle)
		angle *= 0.2
	var pose: Transform2D = Transform2D(angle, center)
	var rect: Rect2 = Rect2(-extent * 0.5, extent)
	_card_hits.append({"rect": pose * rect, "card": card, "pose": pose, "local_rect": rect})
	draw_set_transform_matrix(pose)
	draw_rect(Rect2(rect.position + Vector2(3, 5), rect.size), Color(0, 0, 0, 0.3))
	CardPainter.paint(self, card, rect, "hand", raised or card.instance_id == _hover_id)
	draw_set_transform_matrix(Transform2D.IDENTITY)

func card_at(point: Vector2) -> CardPresentation:
	for index: int in range(_card_hits.size() - 1, -1, -1):
		var hit: Dictionary = _card_hits[index]
		var rect: Rect2 = hit["rect"]
		if not rect.has_point(point):
			continue
		if hit.has("pose"):
			var pose: Transform2D = hit["pose"]
			var local_rect: Rect2 = hit["local_rect"]
			if not local_rect.has_point(pose.affine_inverse() * point):
				continue
		return hit["card"] as CardPresentation
	return null

func _gui_input(event: InputEvent) -> void:
	if privacy:
		return
	if event is InputEventMouseMotion:
		var motion: InputEventMouseMotion = event
		_pointer = motion.position
		if _dragged != null and motion.button_mask & MOUSE_BUTTON_MASK_LEFT and _press_position.distance_to(_pointer) > 8:
			_drag_active = true
		var hovered: CardPresentation = card_at(motion.position)
		_hover_id = hovered.instance_id if hovered != null else ""
		tooltip_text = hovered.title if hovered != null else ""
		queue_redraw()
	elif event is InputEventMouseButton:
		var mouse: InputEventMouseButton = event
		if mouse.pressed and mouse.button_index in [MOUSE_BUTTON_WHEEL_UP, MOUSE_BUTTON_WHEEL_DOWN]:
			_scroll_formations(mouse.position, -1 if mouse.button_index == MOUSE_BUTTON_WHEEL_UP else 1)
		elif mouse.pressed and mouse.button_index == MOUSE_BUTTON_RIGHT:
			_dragged = null
			_drag_active = false
			if selected_id.is_empty():
				hand_toggle_requested.emit()
			else:
				selection_cleared.emit()
			accept_event()
		elif mouse.button_index == MOUSE_BUTTON_LEFT:
			if mouse.pressed:
				grab_focus()
				_press_position = mouse.position
				_dragged = card_at(mouse.position)
			elif _drag_active and _dragged != null:
				for hit: Dictionary in _ring_hits:
					var rect: Rect2 = hit["rect"]
					if rect.has_point(mouse.position):
						placement_requested.emit(_dragged.instance_id, str(hit["host"]), int(hit["ring"]))
						break
			else:
				var selected: CardPresentation = card_at(mouse.position)
				if selected != null:
					card_selected.emit(selected)
				else:
					for hit: Dictionary in _zone_hits:
						if hit["rect"].has_point(mouse.position):
							zone_clicked.emit(str(hit["kind"]), int(hit["owner"]))
							accept_event()
							break
			if not mouse.pressed:
				_dragged = null
				_drag_active = false
				queue_redraw()
	elif event.is_action_pressed(&"ui_left") or event.is_action_pressed(&"ui_right"):
		if _card_hits.is_empty():
			return
		_keyboard_index = wrapi(_keyboard_index + (-1 if event.is_action_pressed(&"ui_left") else 1), 0, _card_hits.size())
		var card: CardPresentation = _card_hits[_keyboard_index]["card"]
		_hover_id = card.instance_id
		queue_redraw()
		accept_event()
	elif event.is_action_pressed(&"ui_accept") and _keyboard_index >= 0 and _keyboard_index < _card_hits.size():
		card_selected.emit(_card_hits[_keyboard_index]["card"] as CardPresentation)
		accept_event()

func _scroll_formations(point: Vector2, direction: int) -> void:
	var region: Rect2 = _own_region if _own_region.has_point(point) else _opponent_region
	var capacity: int = _visible_groups(region)
	if _own_region.has_point(point):
		_own_start = clampi(_own_start + direction, 0, maxi(0, field.own_formations.size() - capacity))
	elif _opponent_region.has_point(point):
		_opponent_start = clampi(_opponent_start + direction, 0, maxi(0, field.opponent_formations.size() - capacity))
	_keyboard_index = -1
	_clear_hover()

func zone_at(point: Vector2) -> Dictionary:
	for index: int in range(_zone_hits.size() - 1, -1, -1):
		var hit: Dictionary = _zone_hits[index]
		if hit["rect"].has_point(point):
			return hit
	return {}
