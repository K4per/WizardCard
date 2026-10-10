class_name FieldCanvas
extends Control

signal card_selected(card: CardPresentation)
signal placement_requested(card_id: String, host_id: String, ring_index: int)

var field: FieldPresentation = FieldPresentation.new()
var privacy: bool = false
var _card_hits: Array[Dictionary] = []
var _ring_hits: Array[Dictionary] = []
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

func _ready() -> void:
	focus_mode = Control.FOCUS_ALL
	clip_contents = true
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	resized.connect(queue_redraw)
	mouse_exited.connect(_clear_hover)

func configure(value: FieldPresentation) -> void:
	field = value
	_own_start = 0
	_opponent_start = 0
	_clear_hover()

func set_privacy(enabled: bool) -> void:
	privacy = enabled
	_dragged = null
	_drag_active = false
	_card_hits.clear()
	_ring_hits.clear()
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
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.031, 0.075, 0.122, 0.85))
	if privacy:
		draw_string(CardPainter.FONT, size * 0.5 + Vector2(-126, 0), "交接设备后，再揭开场地", HORIZONTAL_ALIGNMENT_LEFT, -1, 24, CardPainter.PAPER)
		return
	var hand_height: float = clampf(size.y * 0.19, 130, 176)
	var center_y: float = (size.y - hand_height) * 0.5
	var casting_height: float = 112
	_opponent_region = Rect2(104, 12, size.x - 208, center_y - casting_height * 0.5 - 12)
	_own_region = Rect2(104, center_y + casting_height * 0.5, size.x - 208, center_y - casting_height * 0.5 - 12)
	draw_line(Vector2(96, center_y), Vector2(size.x - 96, center_y), Color("477b80"), 1)
	draw_string(CardPainter.FONT, Vector2(28, center_y + 5), "施法区", HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color("e6bd70"))
	_draw_formations(field.opponent_formations, _opponent_region, _opponent_start)
	_draw_formations(field.own_formations, _own_region, _own_start)
	_draw_casting(field.opponent_casting, field.opponent_casting_slots, center_y - 24, false)
	_draw_casting(field.own_casting, field.own_casting_slots, center_y + 24, true)
	_draw_decks(20, 24, field.opponent_side_count, field.opponent_deck_count)
	_draw_decks(20, _own_region.position.y + 8, field.own_side_count, field.own_deck_count)
	_draw_hand(size.y - hand_height + 8, hand_height - 16)
	if _drag_active and _dragged != null:
		CardPainter.paint(self, _dragged, Rect2(_pointer - Vector2(40, 55), Vector2(80, 110)), "field", true)

func _draw_formations(formations: Array[FormationPresentation], region: Rect2, start: int) -> void:
	var visible_rows: int = maxi(1, int(region.size.y / 154.0))
	var row_height: float = region.size.y / visible_rows
	for index: int in range(start, mini(formations.size(), start + visible_rows)):
		var formation: FormationPresentation = formations[index]
		var height: float = minf(160, row_height - 24)
		var width: float = height * 128.0 / 176.0
		var y: float = region.position.y + (index - start) * row_height + 4
		var host_rect: Rect2 = Rect2(region.position.x + 4, y, width, height)
		_draw_card(formation.card, host_rect)
		var left: float = region.position.x + 142
		var available: float = region.end.x - left - 16
		var count: int = maxi(0, formation.ring_count)
		# Ring count is projected data. This layout never decides whether a play is legal.
		var stride: float = available / maxi(count, 1)
		for ring: int in range(count):
			var ring_width: float = minf(width, stride - 12)
			if ring_width < 24:
				continue
			var center: Vector2 = Vector2(left + (ring + 0.5) * stride, y + height * 0.5)
			var ring_rect: Rect2 = Rect2(center - Vector2(ring_width, ring_width * 176.0 / 128.0) * 0.5, Vector2(ring_width, ring_width * 176.0 / 128.0))
			_ring_hits.append({"rect": ring_rect, "host": formation.card.instance_id, "ring": ring})
			draw_line(Vector2(host_rect.end.x + 6, center.y), center, Color(0.28, 0.48, 0.50, 0.3), 1)
			if formation.occupants.has(ring):
				_draw_card(formation.occupants[ring], ring_rect)
			else:
				draw_circle(center, minf(24, ring_width * 0.3), Color("214957"), false, 2)
				draw_texture_rect(ArtBank.texture("c2d.icon.attribute.ring-slot"), Rect2(center - Vector2(16, 16), Vector2(32, 32)), false, Color(1, 1, 1, 0.65))
		if count == 0:
			draw_string(CardPainter.FONT, Vector2(left, y + height * 0.5), "无环位", HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color("7ba2a0"))
	if formations.size() > visible_rows:
		draw_string(CardPainter.FONT, Vector2(region.position.x + 142, region.end.y - 2), "%d / %d 阵法 · 滚轮浏览" % [start + 1, formations.size()], HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color("b6d0c1"))

func _draw_card(card: CardPresentation, rect: Rect2, view: String = "field") -> void:
	_card_hits.append({"rect": rect, "card": card})
	CardPainter.paint(self, card, rect, view, card.instance_id == _hover_id)

func _draw_casting(cards: Array[CardPresentation], slots: int, y: float, own: bool) -> void:
	var available: float = size.x - 360
	var stride: float = minf(80, available / maxi(slots, 1))
	var start_x: float = size.x * 0.5 - slots * stride * 0.5
	for index: int in range(slots):
		var point: Vector2 = Vector2(start_x + (index + 0.5) * stride, y)
		if index < cards.size():
			_draw_card(cards[index], Rect2(point - Vector2(24, 33), Vector2(48, 66)))
		else:
			draw_circle(point, 9, Color("477b80") if own else Color("715033"), false, 1)

func _draw_decks(x: float, y: float, side: int, main: int) -> void:
	# Side deck above main deck; only remaining counts are displayed.
	for index: int in range(2):
		var point: Vector2 = Vector2(x, y + index * 68)
		draw_texture_rect(ArtBank.texture("c2d.card.back.compact"), Rect2(point, Vector2(32, 44)), false)
		var label: String = ("副 %d" % side) if index == 0 else ("主 %d" % main)
		draw_string(CardPainter.FONT, point + Vector2(36, 26), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color("b6d0c1"))

func _draw_hand(y: float, height: float) -> void:
	var card_height: float = minf(160, height)
	var width: float = card_height * 160.0 / 224.0
	var total: float = field.hand.size() * (width + 12) - 12
	var start_x: float = (size.x - total) * 0.5
	for index: int in range(field.hand.size()):
		_draw_card(field.hand[index], Rect2(start_x + index * (width + 12), y, width, card_height), "hand")

func card_at(point: Vector2) -> CardPresentation:
	for index: int in range(_card_hits.size() - 1, -1, -1):
		var hit: Dictionary = _card_hits[index]
		var rect: Rect2 = hit["rect"]
		if rect.has_point(point):
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
			_scroll_rows(mouse.position, -1 if mouse.button_index == MOUSE_BUTTON_WHEEL_UP else 1)
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

func _scroll_rows(point: Vector2, direction: int) -> void:
	var region: Rect2 = _own_region if _own_region.has_point(point) else _opponent_region
	var visible_rows: int = maxi(1, int(region.size.y / 154.0))
	if _own_region.has_point(point):
		_own_start = clampi(_own_start + direction, 0, maxi(0, field.own_formations.size() - visible_rows))
	elif _opponent_region.has_point(point):
		_opponent_start = clampi(_opponent_start + direction, 0, maxi(0, field.opponent_formations.size() - visible_rows))
	_keyboard_index = -1
	_clear_hover()
